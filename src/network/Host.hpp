/* MegaZeux
 *
 * Copyright (C) 2008 Alistair John Strachan <alistair@devzero.co.uk>
 * Copyright (C) 2020-2021 Alice Rowan <petrifiedrowan@gmail.com>
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

#ifndef MEGAZEUX_HOST_HPP
#define MEGAZEUX_HOST_HPP

#include "../compat.h"
#include "../configure.h"

#include "Socket.hpp"

#include <stdint.h>
#include <stdio.h> // for FILE

enum host_type
{
  /** Use TCP protocol for sockets */
  HOST_TYPE_TCP,
  /** Use UDP protocol for sockets */
  HOST_TYPE_UDP,
  NUM_HOST_TYPES
};

enum proxy_status
{
  PROXY_SUCCESS,
  PROXY_INVALID_CONFIG,
  PROXY_CONNECTION_FAILED,
  PROXY_AUTH_FAILED,
  PROXY_AUTH_VERSION_UNSUPPORTED,
  PROXY_AUTH_METHOD_UNSUPPORTED,
  PROXY_SEND_ERROR,
  PROXY_HANDSHAKE_FAILED,
  PROXY_REFLECTION_FAILED,
  PROXY_TARGET_REFUSED,
  PROXY_ADDRESS_TYPE_UNSUPPORTED,
  PROXY_ADDRESS_INVALID,
  PROXY_ACCESS_DENIED,
  PROXY_UNKNOWN_ERROR
};

/**
 * Base class for network connections.
 * Most uses will want to use a specialized host class instead (see HTTPHost).
 */
class UPDATER_LIBSPEC Host
{
private:
  enum host_state
  {
    HOST_UNINITIALIZED, // No socket or connection.
    HOST_INITIALIZED,   // Has a socket, but no connection.
    HOST_CONNECTED,     // Has a connection.
    HOST_BOUND,         // Is bound to a port.
    NUM_HOST_STATES
  };
  enum host_state state;
  enum host_type type;
  enum host_family preferred_family;
  int hint_af;
  int hint_socktype;
  int hint_proto;
  int hint_flags;

  const char *name;
  const char *endpoint;
  boolean proxied;
  boolean trace_raw;
  int af;
  int sockfd;
  int proto;
  uint32_t timeout_ms;

protected:
  // TODO send_callback
  void (*receive_callback)(long offset);
  boolean (*cancel_callback)(void);

private:
  // No copies (or moves since they aren't as portable). Use Host::swap.
  Host(Host &h) {}
  Host &operator=(const Host &h) { return *this; }
#ifdef IS_CXX_11
  Host(Host &&h) {};
  Host &operator=(Host &&h) { return *this; };
#endif

  boolean create_socket(enum host_type type, enum host_family family);
  boolean address_op(const char *hostname, int port, void *priv,
   struct addrinfo *(Host::*op)(struct addrinfo *ais, void *priv));

  boolean create_connection(struct addrinfo *ai, enum host_family family);
  struct addrinfo *create_connection_op(struct addrinfo *ais, void *priv);
  boolean connect_direct(const char *hostname, int port);
  enum proxy_status connect_socks4a(const char *hostname, int port);
  enum proxy_status connect_socks4(struct addrinfo *ai);
  enum proxy_status connect_socks5(struct addrinfo *ai);
  enum proxy_status connect_proxy(const char *hostname, int port);

  struct addrinfo *bind_op(struct addrinfo *ais, void *priv);

  struct addrinfo *receive_from_op(struct addrinfo *ais, void *priv);
  struct addrinfo *send_to_op(struct addrinfo *ais, void *priv);

public:
  /**
   * Event bitmask flags for poll(). These are defined to be the same as the
   * poll event flags for convenience, but these should be referenced instead.
   */
  enum host_poll
  {
    POLL_READ       = POLLIN,
    POLL_WRITE      = POLLOUT,
    POLL_EXCEPT     = POLLPRI,
    POLL_ERROR      = POLLERR, // Return value only.
    POLL_DISCONNECT = POLLHUP, // Return value only.
    POLL_INVALID    = POLLNVAL // Return value only.
  };

  /**
   * Default time to wait for requests before aborting.
   */
  static const int TIMEOUT_DEFAULT = (10 * 1000);

  /**
   * Initializes the host layer. Must be called before all other host functions.
   *
   * @return Whether initialization succeeded, or not
   */
  static boolean host_layer_init(struct config_info *conf);

  /**
   * Check the host layer initialization status. If the current platform needs
   * to perform any late initialization (e.g. when initializing the network
   * would take too long for it to be worth performing on startup), this
   * function will handle that.
   *
   * @return `true` if the host layer is ready, otherwise `false`.
   */
  static boolean host_layer_init_check();

  /**
   * Shuts down the host layer.
   * Frees any associated operating system resources.
   */
  static void host_layer_exit(void);

  /**
   * Swap the connection data of two Host objects.
   *
   * @param a        Object to swap.
   * @param b        Object to swap.
   */
  static void swap(Host &a, Host &b);

  /**
   * Initialize a host object.
   *
   * @param type     IP protocol to use (HOST_TYPE_TCP or HOST_TYPE_UDP).
   * @param family   Preferred address family (HOST_FAMILY_IPV4 or HOST_FAMILY_IPV6).
   *                 Any resulting connection will not necessarily use this.
   */
  Host(enum host_type type, enum host_family family);

  /**
   * Destructor (close the socket and free any data/resources if needed).
   */
  ~Host();

  /**
   * Resets this host object to its initial state.
   * Closes the connection (if connected) and releases the socket (if created).
   * This function doesn't change the callbacks assigned to this Host.
   */
  void close();

  /**
   * Gets the hostname of this connection. For proxied connections, this will
   * return the endpoint name.
   *
   * @return the hostname of the current connection, or `NULL` if there is none.
   */
  const char *get_host_name();

  /**
   * Connects to the specified remote host and port.
   *
   * @param hostname Remote hostname or IP address to connect to.
   * @param port     Target port (service) to use.
   *
   * @return `true` on a successful connection, otherwise `false`.
   */
  boolean connect(const char *hostname, int port);

  /**
   * Sets the timeout for sending and receiving packets (default is 10s).
   *
   * @param timeout_ms  Timeout (in ms).
   */
  void set_timeout_ms(uint32_t timeout_ms);

  /**
   * Set send/recv callbacks which will be called (potentially many times) as
   * the library fills the send/recv buffers for "block transfers". HTTP headers
   * and other preambles are explicitly ignored. Raw transfers are always fully
   * accounted. This code is primarily used by UI widgets like progress meters.
   *
   * Additionally, include a callback for cancellation. This will cause the
   * send/recv functions to fail and subsequently abort the transfer process.
   * This is again useful for cancelling long or slow transfers with UI widgets.
   *
   * Please note that this code does not abstract away protocol specific
   * knowledge. For example, CHUNKED HTTP transfers will appear as many
   * incrementally filled, fixed size buffers. Therefore, you should not use
   * `len' below to compute the transfer amount remaining. If you do not have
   * any expectations about transfer size, these callbacks may in fact not be
   * very useful.
   *
   * TODO maybe replace this?
   *
   * @param send_cb   Implementation of a send callback (or NULL for none)
   * @param recv_cb   Implementation of a recv callback (or NULL for none)
   * @param cancel_cb Implementation of a cancel callback (or NULL for none)
   */
  void set_callbacks(void (*send_cb)(long offset), void (*recv_cb)(long offset),
   boolean (*cancel_cb)(void));

  /**
   * Some socket operations "fail" for non-fatal reasons. If using non-blocking
   * sockets, they may fail with EAGAIN, EINTR, or in some other
   * platform-specific way, if there was no data available at that time.
   *
   * @return `true` if the last socket error was fatal, otherwise `false`.
   */
  boolean is_last_error_fatal();

  /**
   * Sets the socket blocking mode on the given host.
   * Should only be used after connect() or bind().
   *
   * @param blocking `true` if the socket should block, `false` otherwise.
   */
  void set_blocking(boolean blocking);

  /**
   * Accepts a connection from a host processed by `host_bind` and
   * `host_listen` previously. The new connection is assigned to the
   * provided `Host` instance. The new connection will be blocking by default.
   *
   * @param client_host `Host` instance to accept incoming client connection.
   *
   * @return `true` on a successful connection to the client, otherwise `false`.
   */
  boolean accept(Host &client_host);

  /**
   * Binds the managed host to the specified host and port.
   *
   * @param hostname Hostname or IP address to bind to
   * @param port     Target port (service) to use
   *
   * @return `true` if the bind was successful, otherwise `false`.
   */
  boolean bind(const char *hostname, int port);

  /**
   * Prepares a socket processed with HostServer::bind to listen for
   * connections. Only applies if the managed socket is HOST_TYPE_TCP.
   *
   * @return `true` if listening is successful, otherwise `false`.
   */
  boolean listen();

  /**
   * Polls a host via raw socket access.
   *
   * @param flags       Bitmask of events to poll for (see `enum host_poll`).
   * @param timeout_ms  Timeout in milliseconds for poll.
   *
   * @return <0 if there was a failure, 0 if there was no activity,
   *         or a >0 bitmask of event flags indicating the type(s) of activity.
   */
  int poll(int flags, uint32_t timeout_ms);

  /**
   * Check if a poll() result is an error.
   *
   * @param poll_result Return value from poll().
   *
   * @return `true` if the result is <0 or Host::POLL_ERROR, Host::POLL_INVALID,
   *         or Host::POLL_DISCONNECT is set; otherwise, `false`.
   */
  static boolean is_poll_error(int poll_result)
  {
    return poll_result < 0 ||
     (poll_result & (POLL_ERROR | POLL_DISCONNECT | POLL_INVALID));
  }

protected:
  /**
   * These functions are not exposed externally as use of them might interfere
   * with an active stream.
   */

  /**
   * Receive a buffer on a host via raw socket access.
   *
   * @param buffer    Buffer to receive data into (pre-allocated).
   * @param len       Amount of data to receive into the buffer.
   *
   * @return `true` if the buffer was received, otherwise `false`.
   */
  boolean receive(void *buffer, size_t len);

  /**
   * Send a buffer on a host via raw socket access.
   *
   * @param buffer    Buffer to send from (pre-allocated).
   * @param len       Length of data to send.
   *
   * @return `true` if the buffer was sent, otherwise `false`.
   */
  boolean send(const void *buffer, size_t len);

#ifdef NETWORK_DEADCODE

  /**
   * Connect to and receive a buffer from a remote host.
   *
   * @param buffer    Buffer to receive data into (pre-allocated).
   * @param len       Maximum amount of data that can be received into the buffer.
   * @param hostname  Remote hostname or IP address to receive from.
   * @param port      Remote host port to receive from.
   *
   * @return `true` if a buffer was successfully received, otherwise `false`.
   */
  boolean receive_from(char *buffer, size_t len, const char *hostname, int port);

  /**
   * Connect to and send a buffer to a remote host.
   *
   * @param buffer    Buffer to send from (pre-allocated).
   * @param len       Length of data to send.
   * @param hostname  Remote hostname or IP address to send to.
   * @param port      Remote host port to send to.
   *
   * @return `true` if the buffer was successfully sent, otherwise `false`.
   */
  boolean send_to(const char *buffer, size_t len, const char *hostname, int port);

#endif // NETWORK_DEADCODE
};

namespace std
{
  inline void swap(Host &a, Host &b)
  {
    Host::swap(a, b);
  }
}

#endif /* MEGAZEUX_HOST_HPP */
