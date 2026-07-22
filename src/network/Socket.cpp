/* MegaZeux
 *
 * Copyright (C) 2008 Alistair John Strachan <alistair@devzero.co.uk>
 * Copyright (C) 2018-2024 Alice Rowan <petrifiedrowan@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "Socket.hpp"

#include "../util.h"
#include "../platform/log.h"
#include "../platform/platform_dso.h"
#include "../platform/platform_thread.h"

#include <assert.h>
#include <inttypes.h>
#include <stdlib.h>

#ifndef _MSC_VER
#include <sys/time.h>
#endif

#if defined(__WIN32__) || !defined(CONFIG_GETADDRINFO)

/**
 * gethostbyname may use a static struct that is shared across threads
 * in some implementations. For safety, acquire a lock until the call to it
 * and all use of its return value is complete.
 */
static platform_mutex gai_lock;

int Socket::getaddrinfo_alt(const char *node, const char *service,
 const struct addrinfo *hints, struct addrinfo **res)
{
  struct addrinfo *r = NULL, *r_head = NULL;
  struct hostent *hostent;
  int i;

  trace("--SOCKET-- Socket::getaddrinfo_alt\n");

  // This relies on hints being provided to initialize the addrinfo structs...
  if(!hints)
    return EAI_FAIL;

  /**
   * If hints.ai_family is provided as AF_UNSPEC, getaddrinfo is expected to
   * return anything found (including AF_INET), so it's acceptable here.
   * https://pubs.opengroup.org/onlinepubs/9699919799/functions/freeaddrinfo.html
   */
  if(hints->ai_family != AF_INET && hints->ai_family != AF_UNSPEC)
    return EAI_FAMILY;

  platform_mutex_lock(&gai_lock);

  hostent = Socket::gethostbyname(node);
  if(!hostent || hostent->h_addrtype != AF_INET)
  {
    platform_mutex_unlock(&gai_lock);
    return EAI_NONAME;
  }

  /* Walk the h_addr_list and create faked addrinfo structures
   * corresponding to the addresses. We don't support non-IPV4
   * addresses or any other magic, but that's an acceptable
   * fallback, and it won't affect users on XP or newer.
   */
  for(i = 0; hostent->h_addr_list[i]; i++)
  {
    struct sockaddr_in *addr;

    if(r_head)
    {
      r->ai_next = (struct addrinfo *)cmalloc(sizeof(struct addrinfo));
      r = r->ai_next;
    }
    else
      r_head = r = (struct addrinfo *)cmalloc(sizeof(struct addrinfo));

    // Zero the fake addrinfo and fill out the essential fields
    memset(r, 0, sizeof(struct addrinfo));
    r->ai_family = hostent->h_addrtype;
    r->ai_socktype = hints->ai_socktype;
    r->ai_protocol = hints->ai_protocol;
    r->ai_addrlen = sizeof(struct sockaddr_in);
    r->ai_addr = (struct sockaddr *)cmalloc(r->ai_addrlen);

    // Zero the fake ipv4 addr and fill our all of the fields
    addr = (struct sockaddr_in *)r->ai_addr;
    memcpy(&addr->sin_addr.s_addr, hostent->h_addr_list[i], sizeof(uint32_t));
    addr->sin_family = r->ai_family;
    addr->sin_port = Socket::hton_short(atoi(service));
  }

  platform_mutex_unlock(&gai_lock);
  *res = r_head;
  return 0;
}

void Socket::freeaddrinfo_alt(struct addrinfo *res)
{
  struct addrinfo *r, *r_next;
  for(r = res; r; r = r_next)
  {
    r_next = r->ai_next;
    free(r->ai_addr);
    free(r);
  }
}

const char *Socket::gai_strerror_alt(int errcode)
{
  switch(errcode)
  {
    case EAI_NONAME:
      return "Node or service is not known.";
    case EAI_AGAIN:
      return "Temporary failure in name resolution";
    case EAI_FAIL:
      return "Non-recoverable failure in name resolution";
    case EAI_FAMILY:
      return "Address family is not supported.";
    default:
      return "Unknown error.";
  }
}

#ifndef __WIN32__
int Socket::getaddrinfo(const char *node, const char *service,
 const struct addrinfo *hints, struct addrinfo **res)
{
  return Socket::getaddrinfo_alt(node, service, hints, res);
}

void Socket::freeaddrinfo(struct addrinfo *res)
{
  return Socket::freeaddrinfo_alt(res);
}

void Socket::getaddrinfo_perror(const char *message, int errcode)
{
  warn("%s (code %d): %s\n", message, errcode,
   Socket::gai_strerror_alt(errcode));
}
#endif
#endif // __WIN32__ || !CONFIG_GETADDRINFO

#if defined(__WIN32__) || !defined(CONFIG_POLL)

static void reset_timeout(struct timeval *tv, int timeout)
{
  /* Because of the way select() works on Unix platforms, this needs to
   * be reset every time select() is used on it. */
  tv->tv_sec = (timeout / 1000);
  tv->tv_usec = (timeout % 1000) * 1000;
}

/**
 * Alternative implementation of poll() that uses select() internally.
 * Only use this if poll() isn't available (examples: couldn't load WSAPoll;
 * POSIX <2001; broken poll() in some Mac OS X versions; some console SDKs).
 */
int Socket::poll_alt(struct pollfd *fds, size_t nfds, int timeout_ms)
{
  struct timeval tv;
  fd_set readfds;
  fd_set writefds;
  fd_set exceptfds;
  int maxfd = -1;
  int ret;

  FD_ZERO(&readfds);
  FD_ZERO(&writefds);
  FD_ZERO(&exceptfds);

  if(nfds >= FD_SETSIZE)
  {
    warn(
      "--SOCKET-- Socket::poll_alt: provided number of fds for select() %d >= "
      "FD_SETSIZE %d\n",
      (int)nfds, (int)FD_SETSIZE
    );
  }

  for(size_t i = 0; i < nfds; i++)
  {
    struct pollfd &p = fds[i];

#ifndef __WIN32__
    /**
     * Reject any fds over FD_SETSIZE. These can't be supported and attempting
     * to work around this is prone to security issues.
     *
     * This doesn't matter for Windows, which uses a list instead of a bitmask.
     */
    if(p.fd >= FD_SETSIZE)
    {
      warn(
        "--SOCKET-- Socket::poll_alt: ignoring fd %d >= FD_SETSIZE %d for "
        "select()\n",
        (int)p.fd, (int)FD_SETSIZE
      );
      continue;
    }
#endif

    maxfd = MAX(maxfd, (int)p.fd);
    p.revents = 0;
    if(p.events & POLLIN)
      FD_SET(p.fd, &readfds);
    if(p.events & POLLOUT)
      FD_SET(p.fd, &writefds);
    if(p.events & POLLPRI)
      FD_SET(p.fd, &exceptfds);
  }

  reset_timeout(&tv, timeout_ms);
  ret = Socket::select(maxfd + 1, &readfds, &writefds, &exceptfds, &tv);
  if(ret < 0)
    return -1;

  if(ret > 0)
  {
    for(size_t i = 0; i < nfds; i++)
    {
      struct pollfd &p = fds[i];

      if((p.events & POLLIN) && FD_ISSET(p.fd, &readfds))
        p.revents |= POLLIN;
      if((p.events & POLLOUT) && FD_ISSET(p.fd, &writefds))
        p.revents |= POLLOUT;
      if((p.events & POLLPRI) && FD_ISSET(p.fd, &exceptfds))
        p.revents |= POLLPRI;
    }
    return ret;
  }
  return 0;
}

#ifndef __WIN32__
int Socket::poll(struct pollfd *fds, unsigned int nfds, int timeout_ms)
{
  return Socket::poll_alt(fds, nfds, timeout_ms);
}
#endif
#endif /* __WIN32__ || !CONFIG_POLL */

#ifdef __WIN32__

/* MegaZeux needs ws2_32.dll for getaddrinfo() and friends, which are
 * relatively new POSIX APIs added to simplify the lookup of (and
 * association with) a host. They passively support IPv6 lookups,
 * for example, and can deal with DNS extensions like SRV for
 * protocol specific resolves. This is useful stuff for the future.
 *
 * Therefore, it was decided to rely on Winsock2, specifically the
 * version introduced in Windows XP. If the new XP-added functions cannot
 * be loaded, the old gethostbyname() method will be wrapped to produce
 * an interface identical to that of getaddrinfo(). This allows us to
 * support all Winsock 2.0 platforms, which was officially 98 onwards,
 * but 95 was also supported with an additional download.
 */

static struct
{
  /* These are Winsock 2.0 functions that should be present all the
   * way back to 98 (and 95 with the additional Winsock 2.0 update).
   */
  int (PASCAL *accept)(SOCKET, const struct sockaddr *, int *);
  int (PASCAL *bind)(SOCKET, const struct sockaddr *, int);
  int (PASCAL *closesocket)(SOCKET);
  int (PASCAL *connect)(SOCKET, const struct sockaddr *, int);
  struct hostent *(PASCAL *gethostbyname)(const char*);
  u_short (PASCAL *htons)(u_short);
  int (PASCAL *ioctlsocket)(SOCKET, long, u_long *);
  int (PASCAL *listen)(SOCKET, int);
  int (PASCAL *select)(int, fd_set *, fd_set *, fd_set *,
   const struct timeval *);
  int (PASCAL *send)(SOCKET, const char *, int, int);
  int (PASCAL *sendto)(SOCKET, const char*, int, int,
   const struct sockaddr *, int);
  int (PASCAL *getsockopt)(SOCKET, int, int, char *, int *);
  int (PASCAL *setsockopt)(SOCKET, int, int, const char *, int);
  SOCKET (PASCAL *socket)(int, int, int);
  int (PASCAL *recv)(SOCKET, char *, int, int);
  int (PASCAL *recvfrom)(SOCKET, char*, int, int,
   struct sockaddr *, int *);

  // Similar to above but these are extensions of Berkeley sockets
  int (PASCAL *WSACancelBlockingCall)(void);
  int (PASCAL *WSACleanup)(void);
  int (PASCAL *WSAGetLastError)(void);
  int (PASCAL *WSAStartup)(WORD, LPWSADATA);
  int (PASCAL *__WSAFDIsSet)(SOCKET, fd_set *);

  // These functions were only implemented as of Windows XP (5.1)
  void (WSAAPI *freeaddrinfo)(struct addrinfo *);
  int (WSAAPI *getaddrinfo)(const char *, const char *,
   const struct addrinfo *, struct addrinfo **);

  // These functions were only implemented as of Windows Vista.
  int (WSAAPI *WSAPoll)(struct pollfd *fds, unsigned long nfds, int timeout);

  struct dso_library *handle;
}
socksyms;

static const struct dso_syms_map socksyms_map[] =
{
  { "accept",                { &socksyms.accept }},
  { "bind",                  { &socksyms.bind }},
  { "closesocket",           { &socksyms.closesocket }},
  { "connect",               { &socksyms.connect }},
  { "gethostbyname",         { &socksyms.gethostbyname }},
  { "getsockopt",            { &socksyms.getsockopt }},
  { "htons",                 { &socksyms.htons }},
  { "ioctlsocket",           { &socksyms.ioctlsocket }},
  { "listen",                { &socksyms.listen }},
  { "select",                { &socksyms.select }},
  { "send",                  { &socksyms.send }},
  { "sendto",                { &socksyms.sendto }},
  { "setsockopt",            { &socksyms.setsockopt }},
  { "socket",                { &socksyms.socket }},
  { "recv",                  { &socksyms.recv }},
  { "recvfrom",              { &socksyms.recvfrom }},

  { "WSACancelBlockingCall", { &socksyms.WSACancelBlockingCall }},
  { "WSACleanup",            { &socksyms.WSACleanup }},
  { "WSAGetLastError",       { &socksyms.WSAGetLastError }},
  { "WSAStartup",            { &socksyms.WSAStartup }},
  { "__WSAFDIsSet",          { &socksyms.__WSAFDIsSet }},

  { "freeaddrinfo",          { &socksyms.freeaddrinfo }},
  { "getaddrinfo",           { &socksyms.getaddrinfo }},

  { "WSAPoll",               { &socksyms.WSAPoll }},

  DSO_MAP_END
};

#define WINSOCK2 "ws2_32.dll"
#define WINSOCK  "wsock32.dll"

static void socket_free_syms(void)
{
  if(socksyms.handle)
  {
    platform_unload_library(socksyms.handle);
    socksyms.handle = NULL;
  }
}

static boolean socket_load_syms(void)
{
  int i;

  socksyms.handle = platform_load_library(WINSOCK2);
  if(!socksyms.handle)
  {
    warn("Failed to load Winsock 2.0, falling back to Winsock\n");
    socksyms.handle = platform_load_library(WINSOCK);
    if(!socksyms.handle)
    {
      warn("Failed to load Winsock fallback\n");
      return false;
    }
  }

  for(i = 0; socksyms_map[i].name; i++)
  {
    if(!platform_load_function(socksyms.handle, &(socksyms_map[i])))
    {
      // Skip these NT 5.1 WS2 extensions; we can fall back
      if((strcmp(socksyms_map[i].name, "freeaddrinfo") == 0) ||
         (strcmp(socksyms_map[i].name, "getaddrinfo") == 0) ||
         (strcmp(socksyms_map[i].name, "WSAPoll") == 0))
        continue;

      // However all other Winsock symbols must be loaded, or we fail hard
      warn("Failed to load Winsock symbol '%s'\n", socksyms_map[i].name);
      socket_free_syms();
      return false;
    }
  }

  return true;
}

boolean Socket::platform_init(struct config_info *conf)
{
  WORD version = MAKEWORD(1, 0);
  WSADATA ws_data;

  if(!socket_load_syms())
    return false;

  if(socksyms.WSAStartup(version, &ws_data) < 0)
    return false;

  return true;
}

boolean Socket::platform_init_late()
{
  return true;
}

void Socket::platform_exit(void)
{
  if(socksyms.WSACleanup() == SOCKET_ERROR)
  {
    if(socksyms.WSAGetLastError() == WSAEINPROGRESS)
    {
      socksyms.WSACancelBlockingCall();
      socksyms.WSACleanup();
    }
  }
  socket_free_syms();
}

boolean Socket::is_last_error_fatal(void)
{
  /**
   * NOTE: WSA error codes are not intercompatible with errno, so there's no
   * point in calling is_last_errno_fatal from this function. Just handle the
   * WSA error codes directly.
   */
  switch(Socket::get_errno())
  {
    case 0:
    case WSAEINPROGRESS:
    case WSAEWOULDBLOCK:
    case WSAEINTR:
      return false;
    default:
      return true;
  }
}

/**
 * It turns out perror is completely useless for Winsock error messages, so
 * as a workaround, use Win32 functions instead (these should be safe back to
 * Windows 95).
 */
static void winsock_perror(const char *message, int code)
{
  LPSTR err_message = NULL;

  FormatMessage(
    FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER,
    0,
    code,
    0,
    (LPSTR)&err_message,
    0,
    NULL
  );

  warn("%s (code %d): %s", message, code, err_message);
  LocalFree(err_message);
}

int Socket::get_errno()
{
  return socksyms.WSAGetLastError();
}

void Socket::perror(const char *message)
{
  winsock_perror(message, Socket::get_errno());
}

int Socket::accept(int sockfd,
 struct sockaddr *addr, socklen_t *addrlen)
{
  return socksyms.accept(sockfd, addr, addrlen);
}

int Socket::bind(int sockfd,
 const struct sockaddr *addr, socklen_t addrlen)
{
  return socksyms.bind(sockfd, addr, addrlen);
}

void Socket::close(int fd)
{
  socksyms.closesocket(fd);
}

int Socket::connect(int sockfd,
 const struct sockaddr *serv_addr, socklen_t addrlen)
{
  return socksyms.connect(sockfd, serv_addr, addrlen);
}

struct hostent *Socket::gethostbyname(const char *name)
{
  return socksyms.gethostbyname(name);
}

int Socket::getsockopt(int sockfd, int level, int optname, void *optval,
 socklen_t *optlen)
{
  return socksyms.getsockopt(sockfd, level, optname, (char *)optval, optlen);
}

uint16_t Socket::hton_short(uint16_t hostshort)
{
  return socksyms.htons(hostshort);
}

void Socket::freeaddrinfo(struct addrinfo *res)
{
  if(socksyms.freeaddrinfo)
    socksyms.freeaddrinfo(res);
  else
    Socket::freeaddrinfo_alt(res);
}

int Socket::getaddrinfo(const char *node, const char *service,
 const struct addrinfo *hints, struct addrinfo **res)
{
  if(socksyms.getaddrinfo)
    return socksyms.getaddrinfo(node, service, hints, res);
  return Socket::getaddrinfo_alt(node, service, hints, res);
}

/**
 * A Win32 version of gai_strerror is available but apparently uses a 1K
 * static buffer for the error message, making it non-thread safe.
 * Fortunately, the workaround for this is the same as for perror.
 */
void Socket::getaddrinfo_perror(const char *message, int errcode)
{
  winsock_perror(message, errcode);
}

int Socket::listen(int sockfd, int backlog)
{
  return socksyms.listen(sockfd, backlog);
}

int Socket::poll(struct pollfd *fds, unsigned int nfds, int timeout_ms)
{
  if(socksyms.WSAPoll)
    return socksyms.WSAPoll(fds, nfds, timeout_ms);
  return Socket::poll_alt(fds, nfds, timeout_ms);
}

int Socket::select(int nfds, fd_set *readfds,
 fd_set *writefds, fd_set *exceptfds, struct timeval *timeout)
{
  return socksyms.select(nfds, readfds, writefds, exceptfds, timeout);
}

ssize_t Socket::send(int sockfd, const void *buf, size_t len,
 int flags)
{
  return socksyms.send(sockfd, (const char *)buf, (int)len, flags);
}

ssize_t Socket::sendto(int sockfd, const void *buf, size_t len,
 int flags, const struct sockaddr *to, socklen_t tolen)
{
  return socksyms.sendto(sockfd, (const char *)buf, (int)len, flags, to, tolen);
}

int Socket::setsockopt(int sockfd, int level, int optname,
 const void *optval, socklen_t optlen)
{
  return socksyms.setsockopt(sockfd, level, optname, (const char *)optval, optlen);
}

int Socket::socket(int af, int type, int protocol)
{
  return (int)socksyms.socket(af, type, protocol);
}

void Socket::set_blocking(int sockfd, boolean blocking)
{
  unsigned long mode = !blocking;
  socksyms.ioctlsocket(sockfd, FIONBIO, &mode);
}

ssize_t Socket::recv(int sockfd, void *buf, size_t len, int flags)
{
  return socksyms.recv(sockfd, (char *)buf, (int)len, flags);
}

ssize_t Socket::recvfrom(int sockfd, void *buf, size_t len,
 int flags, struct sockaddr *from, socklen_t *fromlen)
{
  return socksyms.recvfrom(sockfd, (char *)buf, (int)len, flags, from, fromlen);
}

int Socket::__WSAFDIsSet(int sockfd, fd_set *set)
{
  return socksyms.__WSAFDIsSet(sockfd, set);
}

#endif /*__WIN32__*/


#ifdef CONFIG_3DS

#include <3ds.h>
#include <malloc.h>

// Just going off what the examples use for the buffer size here...
#define SOC_ALIGN       0x1000
#define SOC_BUFFERSIZE  0x100000

static boolean socket_3ds_init = false;
static u32 *socket_ctx_buffer = nullptr;

boolean Socket::platform_init(struct config_info *conf)
{
  return true;
}

boolean Socket::platform_init_late()
{
  if(!socket_3ds_init)
  {
    void *buf = memalign(SOC_ALIGN, SOC_BUFFERSIZE);
    if(!buf)
      return false;

    socket_ctx_buffer = static_cast<u32 *>(buf);

    Result ret = socInit(socket_ctx_buffer, SOC_BUFFERSIZE);
    if(ret != 0)
    {
      free(socket_ctx_buffer);
      socket_ctx_buffer = nullptr;
      return false;
    }
    socket_3ds_init = true;
  }
  return true;
}

void Socket::platform_exit()
{
  if(socket_3ds_init)
  {
    socExit();
    free(socket_ctx_buffer);
    socket_3ds_init = false;
    socket_ctx_buffer = nullptr;
  }
}

#endif /* CONFIG_3DS */


#ifdef CONFIG_SWITCH

#include <switch.h>

static boolean socket_switch_init = false;

boolean Socket::platform_init(struct config_info *conf)
{
  return true;
}

boolean Socket::platform_init_late()
{
  if(!socket_switch_init)
  {
    Result ret = socketInitializeDefault();
    if(ret != 0)
      return false;

    socket_switch_init = true;
  }
  return true;
}

void Socket::platform_exit()
{
  if(socket_switch_init)
  {
    socketExit();
    socket_switch_init = false;
  }
}

#endif /* CONFIG_SWITCH */


int Socket::init_ref_count = 0;

boolean Socket::init(struct config_info *conf)
{
  if(!init_ref_count)
  {
    if(!Socket::platform_init(conf))
      return false;

#if defined(__WIN32__) || !defined(CONFIG_GETADDRINFO)
    platform_mutex_init(&gai_lock);
#endif
  }
  init_ref_count++;
  return true;
}

boolean Socket::init_late()
{
  return Socket::platform_init_late();
}

void Socket::exit()
{
  assert(init_ref_count > 0);
  init_ref_count--;
  if(!init_ref_count)
  {
    Socket::platform_exit();

#if defined(__WIN32__) || !defined(CONFIG_GETADDRINFO)
    platform_mutex_destroy(&gai_lock);
    gai_lock = 0;
#endif
  }
}
