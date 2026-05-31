/* MegaZeux
 *
 * Copyright (C) 2018-2021 Alice Rowan <petrifiedrowan@gmail.com>
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

#ifndef MEGAZEUX_DNS_HPP
#define MEGAZEUX_DNS_HPP

#include "../compat.h"

#include "Socket.hpp"

#include <stdint.h>

class DNS final
{
private:
  DNS() {}

  // Default thread cap for lookups (note: update host count can override).
  static const int DEFAULT_MAX_THREADS = 3;

public:
  static boolean init(struct config_info *conf);
  static void exit();

  /**
   * Wrapper for `Socket::getaddrinfo`. The `getaddrinfo` call will be performed
   * asynchronously and this function will block for up to a provided timeout
   * duration for the call to finish. This is useful as `getaddrinfo` calls
   * block for an OS-specified amount of time (for Windows, up to 10 seconds)
   * that may be unacceptable in some situations.
   *
   * @param node      Hostname.
   * @param service   Port number string.
   * @param hints     Hints to provide to `getaddrinfo`.
   * @param res       Pointer for the return value of `getaddrinfo`.
   * @param timeout   Duration (ms) to wait for `getaddrinfo` to resolve.
   *
   * @return a `getaddrinfo` return code (see `getaddrinfo`, `gai_strerror`).
   */
  static int lookup(const char *node, const char *service,
   const struct addrinfo *hints, struct addrinfo **res, uint32_t timeout);
};

#endif /* MEGAZEUX_DNS_HPP */
