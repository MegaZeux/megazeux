/* MegaZeux
 *
 * Copyright (C) 2008 Alistair John Strachan <alistair@devzero.co.uk>
 * Copyright (C) 2018-2026 Alice Rowan <petrifiedrowan@gmail.com>
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

#ifndef MEGAZEUX_SOCKET_HPP
#define MEGAZEUX_SOCKET_HPP

#include "../compat.h"
#include "../util.h"

#include <errno.h>
#include <stdio.h>

#ifndef _MSC_VER
#include <unistd.h>
#endif

#ifdef _WIN32

// Winsock symbols are dynamically loaded, so disable the inline versions.
#define UNIX_INLINE(x)
#ifndef __WIN64__
// Windows XP WinSock2 needed for getaddrinfo() API
#ifdef CONFIG_GETADDRINFO
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0501
#endif
#endif // !__WIN64__
// Windows Vista WinSock2 is needed for poll().
#ifdef CONFIG_POLL
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>

#elif defined(CONFIG_WII)

// See Socket_wii.cpp.
#include <network.h>
#include <fcntl.h>
#define UNIX_INLINE(x)

/* libogc moved the poll defines to poll.h with no replacement net_poll
 * defines. poll.h declares a pollfd incompatible with network.h pollsd.
 * Interrim compile-time workaround: */
#ifdef CONFIG_POLL
#include <poll.h>
#define pollfd net_pollfd

struct net_pollfd
{
  s32 fd;
  u32 events;
  u32 revents;
};
#endif

#else // !_WIN32 && !CONFIG_WII

#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#ifdef CONFIG_POLL
#include <poll.h>
#endif
// Not clear what the intent of including this was but it's not present for PSP.
// Commenting out for now until it's needed (if at all).
//#include <net/if.h>

#endif // _WIN32

#if defined(CONFIG_GETADDRINFO) && (!defined(EAI_AGAIN) && !defined(EAI_FAMILY))
#error "Missing getaddrinfo() support; configure with --disable-getaddrinfo."
#endif

#if defined(CONFIG_POLL) && !defined(POLLPRI)
#error "Missing poll support; configure with --disable-poll."
#endif

#if defined(CONFIG_IPV6) && !defined(AF_INET6)
#error "Missing IPv6 support; configure with --disable-ipv6."
#endif

#if !defined(CONFIG_GETADDRINFO)

// Amiga doesn't have getaddrinfo and needs to use a fallback implementation.
// This affects various other legacy platforms and some console SDKs as well.
#define GETADDRINFO_MAYBE_INLINE(x)

#if !defined(EAI_AGAIN)
#define EAI_NONAME -2
#define EAI_AGAIN  -3
#define EAI_FAIL   -4
#define EAI_FAMILY -6

struct addrinfo
{
  int ai_flags;
  int ai_family;
  int ai_socktype;
  int ai_protocol;
  size_t ai_addrlen;
  struct sockaddr *ai_addr;
  char *ai_canonname;
  struct addrinfo *ai_next;
};
#endif
#endif

#if !defined(CONFIG_POLL)

// PSP and probably a lot of older systems don't have poll and need to fall
// back to using select().
#define POLL_MAYBE_INLINE(x)

#if !defined(POLLPRI)
#define POLLIN   0x001
#define POLLPRI  0x002
#define POLLOUT  0x004
#define POLLERR  0x008
#define POLLHUP  0x010
#define POLLNVAL 0x020

struct pollfd
{
  int fd;
  short events;
  short revents;
};
#endif
#endif

#if defined(CONFIG_WII) || defined(CONFIG_3DS) || defined(CONFIG_SWITCH)
#define SOCKET_INIT_MAYBE_INLINE(x)
#endif

#if !defined(EAI_AGAIN) && defined(EAI_NONAME)
#define EAI_AGAIN EAI_NONAME
#endif

#ifndef AI_V4MAPPED
#define AI_V4MAPPED (0)
#endif

#ifndef AI_ADDRCONFIG
#define AI_ADDRCONFIG (0)
#endif

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL (0)
#endif

#ifndef UNIX_INLINE
#define UNIX_INLINE(x) x
#endif

#ifndef SOCKET_INIT_MAYBE_INLINE
#define SOCKET_INIT_MAYBE_INLINE(x) UNIX_INLINE(x)
#endif

#ifndef GETADDRINFO_MAYBE_INLINE
#define GETADDRINFO_MAYBE_INLINE(x) UNIX_INLINE(x)
#endif

#ifndef POLL_MAYBE_INLINE
#define POLL_MAYBE_INLINE(x) UNIX_INLINE(x)
#endif

struct config_info;

/**
 * Low-level abstraction for misc. socket symbols.
 * Don't use directly outside of src/network/.
 */
class Socket final
{
private:
  static int init_ref_count;

  Socket() {}

  static boolean platform_init(struct config_info *conf) SOCKET_INIT_MAYBE_INLINE
  ({
    return true;
  });

  static boolean platform_init_late() SOCKET_INIT_MAYBE_INLINE
  ({
    return true;
  });

  static void platform_exit() SOCKET_INIT_MAYBE_INLINE
  ({
    return;
  });

  // Potentially non thread-safe. Only allow internally in getaddrinfo_alt.
  static struct hostent *gethostbyname(const char *name) UNIX_INLINE
  ({
    return ::gethostbyname(name);
  });

  static int getaddrinfo_alt(const char *node, const char *service,
   const struct addrinfo *hints, struct addrinfo **res);
  static void freeaddrinfo_alt(struct addrinfo *res);
  static const char *gai_strerror_alt(int errcode);

  static int poll_alt(struct pollfd *fds, size_t nfds, int timeout_ms);

public:
  static boolean init(struct config_info *conf);
  static boolean init_late();
  static void exit();

  static int getaddrinfo(const char *node, const char *service,
   const struct addrinfo *hints, struct addrinfo **res) GETADDRINFO_MAYBE_INLINE
  ({
    return ::getaddrinfo(node, service, hints, res);
  });

  static void freeaddrinfo(struct addrinfo *res) GETADDRINFO_MAYBE_INLINE
  ({
    ::freeaddrinfo(res);
  });

  static void getaddrinfo_perror(const char *message, int errcode) GETADDRINFO_MAYBE_INLINE
  ({
    warn("%s (code %d): %s\n", message, errcode, ::gai_strerror(errcode));
  });

  static int get_errno() UNIX_INLINE
  ({
    return errno;
  });

  static void perror(const char *message) UNIX_INLINE
  ({
    ::perror(message);
  });

  static int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen) UNIX_INLINE
  ({
    return ::accept(sockfd, addr, addrlen);
  });

  static int bind(int sockfd, const struct sockaddr *addr,
   socklen_t addrlen) UNIX_INLINE
  ({
    return ::bind(sockfd, addr, addrlen);
  });

  static void close(int fd) UNIX_INLINE
  ({
    ::close(fd);
  });

  static int connect(int sockfd, const struct sockaddr *serv_addr,
   socklen_t addrlen) UNIX_INLINE
  ({
    return ::connect(sockfd, serv_addr, addrlen);
  });

  static int getsockopt(int sockfd, int level, int optname, void *optval,
   socklen_t *optlen) UNIX_INLINE
  ({
    return ::getsockopt(sockfd, level, optname, (char *)optval, optlen);
  });

  /**
   * Convert a short from host byte order to network byte order (big endian).
   * Don't use "htons" as the function name; BSD defines it as a macro.
   */
  static uint16_t hton_short(uint16_t hostshort) UNIX_INLINE
  ({
    return htons(hostshort);
  });

  static int listen(int sockfd, int backlog) UNIX_INLINE
  ({
    return ::listen(sockfd, backlog);
  });

  static int poll(struct pollfd *fds, unsigned int nfds, int timeout_ms) POLL_MAYBE_INLINE
  ({
    return ::poll(fds, nfds, timeout_ms);
  });

  static int select(int nfds, fd_set *readfds, fd_set *writefds,
   fd_set *exceptfds, struct timeval *timeout) UNIX_INLINE
  ({
    return ::select(nfds, readfds, writefds, exceptfds, timeout);
  });

  static ssize_t send(int sockfd, const void *buf, size_t len, int flags) UNIX_INLINE
  ({
    return ::send(sockfd, (const char *)buf, len, flags);
  });

  static ssize_t sendto(int sockfd, const void *buf, size_t len, int flags,
   const struct sockaddr *to, socklen_t tolen) UNIX_INLINE
  ({
    return ::sendto(sockfd, (const char *)buf, len, flags, to, tolen);
  });

  static int setsockopt(int sockfd, int level, int optname,
   const void *optval, socklen_t optlen) UNIX_INLINE
  ({
    return ::setsockopt(sockfd, level, optname, (const char *)optval, optlen);
  });

  static int socket(int af, int type, int protocol) UNIX_INLINE
  ({
    return ::socket(af, type, protocol);
  });

  static ssize_t recv(int sockfd, void *buf, size_t len, int flags) UNIX_INLINE
  ({
    return ::recv(sockfd, (char *)buf, len, flags);
  });

  static ssize_t recvfrom(int sockfd, void *buf, size_t len, int flags,
   struct sockaddr *from, socklen_t *fromlen) UNIX_INLINE
  ({
    return ::recvfrom(sockfd, (char *)buf, len, flags, from, fromlen);
  });

#ifdef _WIN32
  static int __WSAFDIsSet(int sockfd, fd_set *set);
#undef  FD_ISSET
#define FD_ISSET(fd,set) Socket::__WSAFDIsSet((int)fd, (fd_set *)(set))
#endif

   /**
    * These functions don't correspond to POSIX/Berkeley socket functions but
    * are provided for convenience.
    */

  static boolean is_last_error_fatal() UNIX_INLINE
  ({
    return is_last_errno_fatal();
  });

  static void set_blocking(int sockfd, boolean blocking) UNIX_INLINE
  ({
    int flags = fcntl(sockfd, F_GETFL);

    if(!blocking)
      flags |= O_NONBLOCK;
    else
      flags &= ~O_NONBLOCK;

    fcntl(sockfd, F_SETFL, flags);
  });

  /**
   * Class to disable blocking until this object exits scope.
   */
  class scoped_nonblocking final
  {
  private:
    int sockfd;
  public:
    scoped_nonblocking(int _sockfd): sockfd(_sockfd)
    {
      Socket::set_blocking(sockfd, false);
    }
    ~scoped_nonblocking()
    {
      Socket::set_blocking(sockfd, true);
    }
  };

private:
#ifndef _WIN32
  static boolean is_last_errno_fatal()
  {
    switch(Socket::get_errno())
    {
      case 0:
      case EINPROGRESS:
      case EAGAIN:
      case EINTR:
        return false;
      default:
        return true;
    }
  }
#endif
};

#endif /* MEGAZEUX_SOCKET_HPP */
