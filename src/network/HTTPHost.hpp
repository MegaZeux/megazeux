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

#ifndef MEGAZEUX_HTTPHOST_HPP
#define MEGAZEUX_HTTPHOST_HPP

#include "../compat.h"
#include "../io/vfile.h"
#include "Host.hpp"

#include <stdio.h> // for FILE

/**
 * Enum for HTTP host error statuses.
 */
enum HTTPHostStatus
{
  HOST_SUCCESS,
  HOST_INIT_FAILED = -99,
  HOST_FREAD_FAILED,
  HOST_FWRITE_FAILED,
  HOST_SEND_FAILED,
  HOST_RECV_FAILED,
  HOST_ALLOC_FAILED,
  HOST_HTTP_EXCEEDED_BUFFER,
  HOST_HTTP_INFO,
  HOST_HTTP_REDIRECT,
  HOST_HTTP_CLIENT_ERROR,
  HOST_HTTP_SERVER_ERROR,
  HOST_HTTP_INVALID_STATUS,
  HOST_HTTP_INVALID_HEADER,
  HOST_HTTP_INVALID_CONTENT_LENGTH,
  HOST_HTTP_INVALID_TRANSFER_ENCODING,
  HOST_HTTP_INVALID_CONTENT_TYPE,
  HOST_HTTP_INVALID_CONTENT_ENCODING,
  HOST_HTTP_INVALID_CHUNK_LENGTH,
  HOST_ZLIB_INVALID_DATA,
  HOST_ZLIB_INVALID_GZIP_HEADER,
  HOST_ZLIB_DEFLATE_FAILED,
  HOST_ZLIB_INFLATE_FAILED,
  HOST_STATUS_ERROR_MAX
};

/**
 * Struct for HTTP request and response data.
 */
struct UPDATER_LIBSPEC HTTPRequestInfo
{
  static const int URL_BUF_SIZE = 256;
  static const int STATUS_BUF_SIZE = 32;
  static const int CONTENT_TYPE_BUF_SIZE = 64;
  static const int ENC_BUF_SIZE = 32;

  static const char * const plaintext_types[];
  static const char * const binary_types[];

  enum HTTPEncodingType
  {
    EN_UNSUPPORTED,
    EN_NORMAL,
    EN_CHUNKED,
    EN_GZIP
  };

  /**
   * This field must be set to the remote URL to request.
   * It may be overwritten in the case of a redirect.
   */
  char url[URL_BUF_SIZE];

  /**
   * Set this field to a nullptr-terminated list of strings to filter the
   * Content-Type of the response. Leaving it nullptr allows all content types.
   */
  const char * const *allowed_types;

  // Response fields.
  int status_type;
  int status_code;
  char status_message[STATUS_BUF_SIZE];
  char content_type[CONTENT_TYPE_BUF_SIZE];
  char content_type_params[CONTENT_TYPE_BUF_SIZE];
  char content_encoding[ENC_BUF_SIZE];
  char transfer_encoding[ENC_BUF_SIZE];
  size_t content_length;
  size_t final_length;
  HTTPEncodingType transfer_encoding_type;
  HTTPEncodingType content_encoding_type;

  void clear();
  void clear_response();
  void print_response() const;
};

/**
 * Class for network connections over the HTTP protocol.
 */
class UPDATER_LIBSPEC HTTPHost: public Host
{
protected:
  ssize_t http_receive_line(char *buffer, size_t len);
  ssize_t http_send_line(const char *message);
  HTTPHostStatus http_read_status(HTTPRequestInfo &dest, const char *status,
   size_t status_len);
  HTTPHostStatus http_read_header_line(HTTPRequestInfo &dest, char *h,
   size_t h_len);
  HTTPHostStatus http_filter_content_type(const HTTPRequestInfo &request) const;
  boolean http_skip_headers();

public:
  /**
   * See `Host()`.
   */
  HTTPHost(enum host_type type, enum host_family family): Host(type, family) {}

  /**
   * Get a description string for an `HTTPHostStatus` value.
   *
   * @param status        `HTTPHostStatus` value to get a description for.
   *
   * @return description of the provided status.
   */
  static const char *get_error_string(HTTPHostStatus status);

  /**
   * Send a HEAD request and return the response header info (if any).
   *
   * @param request       HTTP request to send; returns response data.
   *
   * @return see HTTPHostStatus.
   */
  HTTPHostStatus head(HTTPRequestInfo &request);

  /**
   * Send a GET request and stream the response (if any) to a file.
   *
   * @param request       HTTP request to send; returns response data.
   * @param file          File pointer to stream to.
   *
   * @return see HTTPHostStatus.
   */
  HTTPHostStatus get(HTTPRequestInfo &request, vfile *file);

  /**
   * Send a GET request and stream the response (if any) to a buffer.
   * Use request.final_length to determine the actual length of the response.
   *
   * @param request       HTTP request to send; returns response data.
   * @param buffer        Buffer to stream to.
   * @param buffer_len    Size of buffer.
   *
   * @return see HTTPHostStatus.
   */
  HTTPHostStatus get(HTTPRequestInfo &request, char *buffer, size_t buffer_len);

#ifdef NETWORK_DEADCODE

  /**
   * Transmit a response to a GET request.
   * Streams a file from disk to the network socket.
   *
   * @param file        File to stream to socket (must already exist).
   * @param mime_type   MIME type of payload.
   *
   * @return see HTTPHostStatus.
   */
  HTTPHostStatus send_file(vfile *file, const char *mime_type);

  /**
   * Handle an incoming HTTP request.
   *
   * @return `true` if the request was successfully handled, otherwise `false`.
   */
  boolean handle_request();

#endif

};

#endif /* MEGAZEUX_HTTPHOST_HPP */
