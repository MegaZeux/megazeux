/* MegaZeux
 *
 * Copyright (C) 2020 Alice Rowan <petrifiedrowan@gmail.com>
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

#include <network.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>

#ifdef IS_CXX_11
#include <type_traits>
#endif

/**
 * Wii essentially uses standard BSD socket functions with different names.
 * Note while BSD socket fd functions are compatible with other operations for
 * fds, the Wii socket functions are not. That doesn't really matter as far
 * as MegaZeux is concerned, though.
 */

/**
 * Track whether or not the internal Wii network layer is initialized.
 */
static boolean net_is_initialized = false;

/**
 * Set this thread's errno value.
 * If the provided value is negative, errno will be set to `-value`.
 * If the provided value is zero or positive, errno will be set to `0`.
 *
 * @param value   Return value from a network.h function.
 * @return        `value`.
 */
static int set_net_errno(int value)
{
  errno = value < 0 ? -value : 0;
  return value;
}

boolean Socket::platform_init(struct config_info *conf)
{
  return true;
}

boolean Socket::platform_init_late()
{
  if(!net_is_initialized)
  {
    int res = net_init();
    set_net_errno(res);
    if(res != 0)
    {
      Socket::perror("platform_socket_init_late() -> net_init()");
      return false;
    }
  }
  net_is_initialized = true;
  return true;
}

void Socket::platform_exit()
{
  if(net_is_initialized)
  {
    net_is_initialized = false;
    net_deinit();
  }
}

struct hostent *Socket::gethostbyname(const char *name)
{
  // This one actually does set errno...
  return net_gethostbyname(name);
}

int Socket::get_errno()
{
  return errno;
}

void Socket::perror(const char *message)
{
  char buf[256];
  buf[0] = '\0';

  // NOTE: return value not portable.
  strerror_r(errno, buf, ARRAY_SIZE(buf));
  warn("--SOCKET-- %s: %s\n", message, buf);
}

int Socket::accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen)
{
  int res = net_accept(sockfd, addr, addrlen);
  return set_net_errno(res);
}

int Socket::bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
{
  int res = net_bind(sockfd, (struct sockaddr *)addr, addrlen);
  return set_net_errno(res);
}

void Socket::close(int fd)
{
  int res = net_close(fd);
  set_net_errno(res);
}

int Socket::connect(int sockfd, const struct sockaddr *serv_addr,
 socklen_t addrlen)
{
  int res = net_connect(sockfd, const_cast<struct sockaddr *>(serv_addr), addrlen);
  return set_net_errno(res);
}

int Socket::getsockopt(int sockfd, int level, int optname, void *optval,
 socklen_t *optlen)
{
  warn("--SOCKET-- getsockopt() is not implemented by libogc.\n");
  if(level == SOL_SOCKET && optname == SO_ERROR && optval && optlen &&
   *optlen == sizeof(int))
  {
    // Just pretend everything is ok since there's no way to get the error ;-(
    int *optval_i = reinterpret_cast<int *>(optval);
    *optval_i = 0;
    return set_net_errno(0);
  }
  return set_net_errno(-EINVAL);
}

uint16_t Socket::hton_short(uint16_t hostshort)
{
  return htons(hostshort);
}

int Socket::listen(int sockfd, int backlog)
{
  int res = net_listen(sockfd, backlog);
  return set_net_errno(res);
}

int Socket::poll(struct pollfd *fds, unsigned int nfds, int timeout_ms)
{
  /**
   * libogc supports poll but uses its own struct with u32s instead of shorts
   * for some reason. Note "pollsd" instead of "pollfd". Socket.hpp defines a
   * pollfd that should be compatible with "pollsd".
   */
#ifdef IS_CXX_11
  struct pollfd tmpf;
  struct pollsd tmps;
  // Make sure pollsd wasn't modified in libogc...
  static_assert(sizeof(pollfd) == sizeof(pollsd), "");
  static_assert(sizeof(tmpf.fd) == sizeof(tmps.socket), "");
  static_assert(sizeof(tmpf.events) == sizeof(tmps.events), "");
  static_assert(sizeof(tmpf.revents) == sizeof(tmps.revents), "");
  static_assert(offsetof(pollfd, fd) == offsetof(pollsd, socket), "");
  static_assert(offsetof(pollfd, events) == offsetof(pollsd, events), "");
  static_assert(offsetof(pollfd, revents) == offsetof(pollsd, revents), "");
#endif

  int res = net_poll(reinterpret_cast<struct pollsd *>(fds), nfds, timeout_ms);
  return set_net_errno(res);
}

int Socket::select(int nfds, fd_set *readfds, fd_set *writefds,
 fd_set *exceptfds, struct timeval *timeout)
{
  warn("--SOCKET-- select() is not implemented by libogc.\n");
  int res = net_select(nfds, readfds, writefds, exceptfds, timeout);
  return set_net_errno(res);
}

ssize_t Socket::send(int sockfd, const void *buf, size_t len, int flags)
{
  int res = net_send(sockfd, buf, len, flags);
  return set_net_errno(res);
}

ssize_t Socket::sendto(int sockfd, const void *buf, size_t len, int flags,
 const struct sockaddr *to, socklen_t tolen)
{
  int res = net_sendto(sockfd, buf, len, flags, (struct sockaddr *)to, tolen);
  return set_net_errno(res);
}

int Socket::setsockopt(int sockfd, int level, int optname,
 const void *optval, socklen_t optlen)
{
  int res = net_setsockopt(sockfd, level, optname, optval, optlen);
  return set_net_errno(res);
}

int Socket::socket(int af, int type, int protocol)
{
  /**
   * Doesn't recognize IPPROTO_TCP, but IPPROTO_IP is fine... :/
   * https://github.com/devkitPro/wii-examples/blob/master/devices/network/sockettest/source/sockettest.c
   */
  if((type == SOCK_STREAM && protocol == IPPROTO_TCP) ||
   (type == SOCK_DGRAM && protocol == IPPROTO_UDP))
    protocol = IPPROTO_IP;

  int res = net_socket(af, type, protocol);
  return set_net_errno(res);
}

int Socket::recv(int sockfd, void *buf, size_t len, int flags)
{
  int res = net_recv(sockfd, buf, len, flags);
  return set_net_errno(res);
}

int Socket::recvfrom(int sockfd, void *buf, size_t len, int flags,
 struct sockaddr *from, socklen_t *fromlen)
{
  int res = net_recvfrom(sockfd, buf, len, flags, from, fromlen);
  return set_net_errno(res);
}

boolean Socket::is_last_error_fatal()
{
  return is_last_errno_fatal();
}

void Socket::set_blocking(int sockfd, boolean blocking)
{
  int flags = net_fcntl(sockfd, F_GETFL, 0);

  if(!blocking)
    flags |= O_NONBLOCK;
  else
    flags &= ~O_NONBLOCK;

  net_fcntl(sockfd, F_SETFL, flags);
}
