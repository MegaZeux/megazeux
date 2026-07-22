/* MegaZeux
 *
 * Copyright (C) 2008 Alistair John Strachan <alistair@devzero.co.uk>
 * Copyright (C) 2020-2024 Alice Rowan <petrifiedrowan@gmail.com>
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

#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <zlib.h>

#include "HTTPHost.hpp"
#include "Scoped.hpp"
#include "../util.h"
#include "../io/vio.h"
#include "../platform/log.h"

#ifdef IS_CXX_11
#include <type_traits>
#endif

#define BLOCK_SIZE    4096UL
#define LINE_BUF_LEN  256

static ssize_t zlib_skip_gzip_header(Bytef *initial, unsigned long len)
{
  Bytef *gzip = initial;
  uint8_t flags;

  if(len < 10)
    return HOST_ZLIB_INVALID_DATA;

  /* Unfortunately, Apache (and probably other httpds) send deflated
   * data in the gzip format. Internally, gzip is identical to zlib's
   * DEFLATE format, but it has some additional headers that must be
   * skipped before we can proceed with the inflation.
   *
   * RFC 1952 details the gzip headers.
   */
  if(*gzip++ != 0x1F || *gzip++ != 0x8B || *gzip++ != Z_DEFLATED)
    return HOST_ZLIB_INVALID_GZIP_HEADER;

  // Grab gzip flags from header and skip MTIME+XFL+OS
  flags = *gzip++;
  gzip += 6;

  // Header "reserved" bits must not be set
  if(flags & 0xE0)
    return HOST_ZLIB_INVALID_GZIP_HEADER;

  // Skip extended headers
  if(flags & 0x4)
    gzip += 2 + *gzip + (*(gzip + 1) << 8);

  // Skip filename (null terminated string)
  if(flags & 0x8)
    while(*gzip++);

  // Skip comment
  if(flags & 0x10)
    while(*gzip++);

  // FIXME: Skip CRC16 (should verify if exists)
  if(flags & 0x2)
    gzip += 2;

  // Return number of bytes to skip from buffer
  return gzip - initial;
}

#ifdef NETWORK_DEADCODE

static int zlib_forge_gzip_header(char *buffer)
{
  // GZIP magic (see RFC 1952)
  buffer[0] = 0x1F;
  buffer[1] = 0x8B;
  buffer[2] = Z_DEFLATED;

  // Flags (no flags required)
  buffer[3] = 0x0;

  // Zero mtime etc.
  memset(&buffer[4], 0, 6);

  // GZIP header is 10 bytes
  return 10;
}

#endif

const char * const HTTPRequestInfo::plaintext_types[] =
{
  "text/plain",
  "text/plain;charset=us",
  "text/plain;charset=utf-8",
  nullptr
};

const char * const HTTPRequestInfo::binary_types[] =
{
  "application/octet-stream;*",
  nullptr
};

void HTTPRequestInfo::clear()
{
  memset(this, 0, sizeof(HTTPRequestInfo));
}

void HTTPRequestInfo::clear_response()
{
  status_type = 0;
  status_code = 0;
  status_message[0] = '\0';
  content_type[0] = '\0';
  content_type_params[0] = '\0';
  content_encoding[0] = '\0';
  transfer_encoding[0] = '\0';
  content_length = 0;
  final_length = 0;
  transfer_encoding_type = HTTPRequestInfo::EN_NORMAL;
  content_encoding_type = HTTPRequestInfo::EN_NORMAL;
}

void HTTPRequestInfo::print_response() const
{
  boolean params = this->content_type_params[0] != '\0';
  fprintf(mzxerr,
    "  URL               : %s\n"
    "  Status            : %d %s\n"
    "  Content-Type      : %s%s%s\n"
    "  Content-Encoding  : %s\n"
    "  Transfer-Encoding : %s\n"
    "  Content-Length    : %zu\n",
    this->url,
    this->status_code, this->status_message,
    this->content_type,
    (params ? "; " : ""),
    (params ? this->content_type_params : ""),
    this->content_encoding,
    this->transfer_encoding,
    this->content_length
  );
  fflush(mzxerr);
}

const char *HTTPHost::get_error_string(HTTPHostStatus status)
{
#ifdef IS_CXX_11
  static_assert(HOST_STATUS_ERROR_MAX < 0, "HOST_STATUS_ERROR_MAX not < 0!");
#endif
  switch(status)
  {
    case HOST_SUCCESS:
      return "Operation completed successfully.";
    case HOST_INIT_FAILED:
      return "FIXME not used";
    case HOST_FREAD_FAILED:
      return "Failed to fread file.";
    case HOST_FWRITE_FAILED:
      return "Failed to fwrite file.";
    case HOST_SEND_FAILED:
      return "Connection issue occurred (send() failed).";
    case HOST_RECV_FAILED:
      return "Connection issue occurred (receive() failed).";
    case HOST_ALLOC_FAILED:
      return "Failed to allocate buffer memory.";
    case HOST_HTTP_EXCEEDED_BUFFER:
      return "Operation would exceed the provided buffer"
       " (INTERNAL ERROR: REPORT THIS!)";
    case HOST_HTTP_INFO:
      return "Unexpected response of type 1xx (informational).";
    case HOST_HTTP_REDIRECT:
      return "Unexpected response of type 3xx (redirect).";
    case HOST_HTTP_CLIENT_ERROR:
      return "Unexpected response of type 4xx (client error).";
    case HOST_HTTP_SERVER_ERROR:
      return "Unexpected response of type 5xx (server error).";
    case HOST_HTTP_INVALID_STATUS:
      return "Response status is invalid.";
    case HOST_HTTP_INVALID_HEADER:
      return "Response header is invalid.";
    case HOST_HTTP_INVALID_CONTENT_LENGTH:
      return "Response 'Content-Length' value is invalid.";
    case HOST_HTTP_INVALID_TRANSFER_ENCODING:
      return "Response 'Transfer-Encoding' value is invalid"
       " (only 'chunked' is accepted).";
    case HOST_HTTP_INVALID_CONTENT_TYPE:
      return "Response 'Content-Type' does not match the expected value(s).";
    case HOST_HTTP_INVALID_CONTENT_ENCODING:
      return "Response 'Content-Encoding' value is invalid"
       " (only 'gzip' is accepted).";
    case HOST_HTTP_INVALID_CHUNK_LENGTH:
      return "Response chunk length is invalid.";
    case HOST_ZLIB_INVALID_DATA:
      return "Response gzip header is missing.";
    case HOST_ZLIB_INVALID_GZIP_HEADER:
      return "Response gzip header is invalid.";
    case HOST_ZLIB_DEFLATE_FAILED:
      return "Failed to compress response data.";
    case HOST_ZLIB_INFLATE_FAILED:
      return "Failed to decompress response data.";
    case HOST_STATUS_ERROR_MAX:
      return "INTERNAL ERROR: REPORT THIS!";
  }
  return "invalid status code!";
}

ssize_t HTTPHost::http_receive_line(char *buffer, size_t len)
{
  size_t pos;

  // Grab data bytewise until we find CRLF characters
  for(pos = 0; pos < len; pos++)
  {
    // If recv() times out or fails, abort
    if(!this->receive(buffer + pos, 1))
    {
      trace("--HOST-- HTTPHost::http_receive_line (failed)\n");
      return HOST_RECV_FAILED;
    }

    // Erase terminating CRLF and fix up count
    if(buffer[pos] == '\n')
    {
      if(pos < 1)
        return HOST_HTTP_INVALID_HEADER;

      buffer[pos--] = 0;
      buffer[pos] = 0;
      break;
    }
  }

  // We didn't find CRLF; this is bad
  if(pos == len)
    return HOST_HTTP_EXCEEDED_BUFFER;

  trace("--HOST-- HTTPHost::http_receive_line: %.*s\n", (int)pos, buffer);
  return pos;
}

ssize_t HTTPHost::http_send_line(const char *message)
{
  char line[LINE_BUF_LEN];
  ssize_t len;

  trace("--HOST-- HTTPHost::http_send_line( %s )\n", message);
  snprintf(line, LINE_BUF_LEN, "%s\r\n", message);
  len = (ssize_t)strlen(line);

  if(!this->send(line, len))
    return HOST_SEND_FAILED;

  return len;
}

HTTPHostStatus HTTPHost::http_read_status(HTTPRequestInfo &dest,
 const char *status, size_t status_len)
{
  /* These conditionals check the status line is formatted:
   *   "HTTP/1.? XXX ..." (where ? is 0 or 1, XXX is the status code,
   *                      and ... is the status message)
   *
   * This is because partially HTTP/1.1 capable servers supporting
   * pipelining may not advertise full HTTP/1.1 compliance (e.g.
   * `perlbal' httpd).
   *
   * MegaZeux only cares about pipelining.
   */
  char ver[2];
  char code[4];
  int res;
  int c;

  dest.status_message[0] = 0;

  res = sscanf(status, "HTTP/1.%1s %3s %31[^\r\n]", ver, code,
   dest.status_message);

  trace("--HOST-- HTTPHost::http_read_status: %s (%s, %s, %s)\n",
   status, ver, code, dest.status_message);
  if(res != 3 || (ver[0] != '0' && ver[0] != '1'))
    return HOST_HTTP_INVALID_STATUS;

  // Make sure the code is a 3-digit number
  c = strtol(code, NULL, 10);
  if(c < 100)
    return HOST_HTTP_INVALID_STATUS;

  dest.status_code = c;
  dest.status_type = c / 100;

  return HOST_SUCCESS;
}

HTTPHostStatus HTTPHost::http_read_header_line(HTTPRequestInfo &dest, char *h,
 size_t h_len)
{
  char *next = h;
  char *key = strsep(&next, ":");
  char *value = strsep(&next, ":");

  if(!key || !value)
    return HOST_HTTP_INVALID_HEADER;

  // Skip common prefix space if present.
  while(isspace(*value))
    value++;

  /* Parse pertinent headers. These are:
   *
   *   Content-Length     Necessary to determine payload length
   *   Transfer-Encoding  Instead of Content-Length, can only be "chunked"
   *   Content-Type       Text or binary; also used for sanity checks
   *   Content-Encoding   Present and set to 'gzip' if deflated
   *
   * NOTE: all header field names are case-insensitive.
   * https://www.w3.org/Protocols/rfc2616/rfc2616-sec4.html#sec4.2
   * https://tools.ietf.org/html/rfc7230#section-3.2
   *
   * NOTE: all content coding names are case-insensitive.
   * https://www.w3.org/Protocols/rfc2616/rfc2616-sec3.html#sec3.5
   * https://tools.ietf.org/html/rfc7231#section-3.1.2.1
   *
   * NOTE: all transfer coding names are case-insensitive.
   * https://www.w3.org/Protocols/rfc2616/rfc2616-sec3.html#sec3.6
   * https://tools.ietf.org/html/rfc7230#section-4
   */

  if(strcasecmp(key, "Content-Length") == 0)
  {
    char *endptr;

    dest.content_length = strtoul(value, &endptr, 10);
    if(endptr[0])
      return HOST_HTTP_INVALID_CONTENT_LENGTH;

    dest.transfer_encoding_type = HTTPRequestInfo::EN_NORMAL;
  }
  else

  if(strcasecmp(key, "Transfer-Encoding") == 0)
  {
    const size_t len = ARRAY_SIZE(dest.transfer_encoding);
    snprintf(dest.transfer_encoding, len, "%s", value);
    dest.transfer_encoding[len - 1] = '\0';

    dest.transfer_encoding_type = HTTPRequestInfo::EN_UNSUPPORTED;
    if(strcasecmp(value, "chunked") == 0)
      dest.transfer_encoding_type = HTTPRequestInfo::EN_CHUNKED;
  }
  else

  if(strcasecmp(key, "Content-Type") == 0)
  {
    const char *type = strsep(&value, ";");

    size_t len = ARRAY_SIZE(dest.content_type);
    snprintf(dest.content_type, len, "%s", type);
    dest.content_type[len - 1] = '\0';
    if(value)
    {
      while(isspace(*value))
        value++;

      len = ARRAY_SIZE(dest.content_type_params);
      snprintf(dest.content_type_params, len, "%s", value);
      dest.content_type_params[len - 1] = '\0';
    }
    else
      dest.content_type_params[0] = '\0';
  }
  else

  if(strcasecmp(key, "Content-Encoding") == 0)
  {
    const size_t len = ARRAY_SIZE(dest.content_encoding);
    snprintf(dest.content_encoding, len, "%s", value);
    dest.content_encoding[len - 1] = '\0';

    dest.content_encoding_type = HTTPRequestInfo::EN_UNSUPPORTED;
    if(strcasecmp(value, "gzip") == 0 || strcasecmp(value, "x-gzip") == 0)
      dest.content_encoding_type = HTTPRequestInfo::EN_GZIP;
  }
  return HOST_SUCCESS;
}

/**
 * Check the content_type and content_type_params fields of a request response
 * against the allowed_types array provided with the request.
 *
 * NOTE: all media types, subtypes, and parameter names are case-insensitive.
 * https://tools.ietf.org/html/rfc7231#section-3.1.1.1
 *
 * NOTE: all charset tokens are case-insensitive.
 * https://www.w3.org/Protocols/rfc2616/rfc2616-sec3.html#sec3.4
 * https://tools.ietf.org/html/rfc7231#section-3.1.1.2
 */
HTTPHostStatus HTTPHost::http_filter_content_type(const HTTPRequestInfo &request) const
{
  if(request.allowed_types)
  {
    size_t i;
    for(i = 0; request.allowed_types[i]; i++)
    {
      /**
       * The provided type may have a list of params. If the type has params
       * (denoted by ;), the request params must exactly match the allowed type
       * params. If the allowed type has no params, the response must also not
       * have any params. If the allowed type has "*" provided in place of
       * params, any or no params are accepted.
       */
      const char *type = request.allowed_types[i];
      const char *params = strchr(type, ';');
      if(params)
      {
        size_t type_len = (params - type);
        if(type_len >= ARRAY_SIZE(request.content_type))
        {
          warn("Content-Type filter '%s' exceeds buffer length (REPORT THIS).\n",
           type);
          continue;
        }

        if(strncasecmp(type, request.content_type, type_len) == 0 &&
         request.content_type[type_len] == '\0')
        {
          params++;
          while(isspace(*params))
            params++;

          if(strcmp(params, "*") == 0)
            return HOST_SUCCESS;

          if(strcasecmp(params, request.content_type_params) == 0)
            return HOST_SUCCESS;
        }
      }
      else

      if(request.content_type_params[0] == '\0')
      {
        if(strcasecmp(type, request.content_type) == 0)
          return HOST_SUCCESS;
      }
    }
    return HOST_HTTP_INVALID_CONTENT_TYPE;
  }
  return HOST_SUCCESS;
}

boolean HTTPHost::http_skip_headers()
{
  char buffer[LINE_BUF_LEN];

  while(true)
  {
    int len = this->http_receive_line(buffer, LINE_BUF_LEN);

    // We read the final newline (empty, with CRLF)
    if(len == 0)
      return true;

    // Connection probably failed; fail hard
    if(len < 0)
      return false;
  }
}

HTTPHostStatus HTTPHost::head(HTTPRequestInfo &request)
{
  char line[LINE_BUF_LEN];
  ssize_t line_len;

  trace("--HOST-- HTTPHost::head\n");
  request.clear_response();

  snprintf(line, LINE_BUF_LEN, "HEAD %s HTTP/1.1", request.url);
  line[LINE_BUF_LEN - 1] = 0;
  if(http_send_line(line) < 0)
    return HOST_SEND_FAILED;

  snprintf(line, LINE_BUF_LEN, "Host: %s", this->get_host_name());
  line[LINE_BUF_LEN - 1] = 0;
  if(http_send_line(line) < 0)
    return HOST_SEND_FAILED;

  if(http_send_line("") < 0)
    return HOST_SEND_FAILED;

  line_len = http_receive_line(line, LINE_BUF_LEN);
  if(line_len < 0)
  {
    warn("No response for url '%s': %zd!\n", request.url, line_len);
    return (HTTPHostStatus)line_len;
  }

  if(http_read_status(request, line, line_len) != HOST_SUCCESS)
  {
    warn("Invalid status: %s\nFailed for url '%s'\n", line, request.url);
    return HOST_HTTP_INVALID_STATUS;
  }

  while(true)
  {
    line_len = http_receive_line(line, LINE_BUF_LEN);
    if(line_len < 0)
      return (HTTPHostStatus)line_len;

    if(line_len == 0)
      break;

    HTTPHostStatus result = http_read_header_line(request, line, line_len);
    if(result != HOST_SUCCESS)
      return result;
  }

  return HOST_SUCCESS;
}

HTTPHostStatus HTTPHost::get(HTTPRequestInfo &request, vfile *file)
{
  boolean mid_inflate = false;
  boolean mid_chunk = false;
  size_t len = 0;
  size_t pos = 0;
  char line[LINE_BUF_LEN];
  z_stream stream;

  trace("--HOST-- HTTPHost::get\n");
  request.clear_response();

  // Tell the server that we support pipelining
  snprintf(line, LINE_BUF_LEN, "GET %s HTTP/1.1", request.url);
  line[LINE_BUF_LEN - 1] = 0;
  if(http_send_line(line) < 0)
    return HOST_SEND_FAILED;

  snprintf(line, LINE_BUF_LEN, "Host: %s", this->get_host_name());

  line[LINE_BUF_LEN - 1] = 0;
  if(http_send_line(line) < 0)
    return HOST_SEND_FAILED;

  // We support DEFLATE/GZIP payloads
  if(http_send_line("Accept-Encoding: gzip") < 0)
    return HOST_SEND_FAILED;

  // Blank line tells server we are done
  if(http_send_line("") < 0)
    return HOST_SEND_FAILED;

  // Read in the HTTP status line
  ssize_t line_len = http_receive_line(line, LINE_BUF_LEN);
  if(line_len < 0)
  {
    warn("No response for url '%s': %zd!\n", request.url, line_len);
    return (HTTPHostStatus)line_len;
  }

  if(http_read_status(request, line, line_len) != HOST_SUCCESS)
  {
    warn("Invalid status: %s\nFailed for url '%s'\n", line, request.url);
    return HOST_HTTP_INVALID_STATUS;
  }

  // Unhandled status categories
  switch(request.status_type)
  {
    case 1:
      return HOST_HTTP_INFO;

    case 3:
      return HOST_HTTP_REDIRECT;

    case 4:
      return HOST_HTTP_CLIENT_ERROR;

    case 5:
      return HOST_HTTP_SERVER_ERROR;
  }

  // Now parse the HTTP headers, extracting only the pertinent fields

  while(true)
  {
    ssize_t len = http_receive_line(line, LINE_BUF_LEN);

    if(len < 0)
      return HOST_HTTP_INVALID_HEADER;

    if(len == 0)
      break;

    HTTPHostStatus result = http_read_header_line(request, line, len);
    if(result != HOST_SUCCESS)
      return result;
  }

  // Filter by Content-Type (if requested).
  HTTPHostStatus result = http_filter_content_type(request);
  if(result != HOST_SUCCESS)
    return result;

  if(request.transfer_encoding_type == HTTPRequestInfo::EN_UNSUPPORTED)
    return HOST_HTTP_INVALID_TRANSFER_ENCODING;

  if(request.content_encoding_type == HTTPRequestInfo::EN_UNSUPPORTED)
    return HOST_HTTP_INVALID_CONTENT_ENCODING;

  // Use Bytef for these buffers since they'll mostly interact with zlib...
  // outbuf will be expanded when/if it's needed.
  ScopedBuffer<Bytef> block(BLOCK_SIZE);
  ScopedBuffer<Bytef> outbuf;
  if(!block)
    return HOST_ALLOC_FAILED;

  while(true)
  {
    unsigned long block_size;

    /* Both transfer mechanisms need preambles. For NORMAL, this will
     * happen only once, because we have a predetermined length for
     * transfer. However, for CHUNKED we don't know the total payload
     * size, so this will be invoked each time we exhaust a chunk.
     *
     * The CHUNKED handling basically involves chopping away the
     * headers and determining the next chunk size.
     */
    if(!mid_chunk)
    {
      if(request.transfer_encoding_type == HTTPRequestInfo::EN_NORMAL)
      {
        len = request.content_length;
      }
      else

      if(request.transfer_encoding_type == HTTPRequestInfo::EN_CHUNKED)
      {
        char *endptr, *length, *buf = line;

        // Get a chunk_length;parameters formatted line (CRLF terminated)
        if(http_receive_line(line, LINE_BUF_LEN) <= 0)
          return HOST_HTTP_INVALID_CHUNK_LENGTH;

        // HTTP 1.1 says we can ignore trailing parameters
        length = strsep(&buf, ";");
        if(!length)
          return HOST_HTTP_INVALID_CHUNK_LENGTH;

        // Convert hex length to unsigned long; check for conversion errors
        len = strtoul(length, &endptr, 16);
        if(endptr[0])
          return HOST_HTTP_INVALID_CHUNK_LENGTH;
      }

      mid_chunk = true;
      pos = 0;
    }

    /* For NORMAL transfers, this indicates that there was a zero byte
     * payload. This is unusual but we can handle it safely by aborting.
     *
     * For CHUNKED transfers, zero indicates that there are no more chunks
     * to process, and that final footer handling should occur. We then
     * abort as with NORMAL.
     */
    if(len == 0)
    {
      if(request.transfer_encoding_type == HTTPRequestInfo::EN_CHUNKED)
        if(!http_skip_headers())
          return HOST_HTTP_INVALID_HEADER;
      break;
    }

    /* For a NORMAL transfer, the block_size computation should yield
     * BLOCK_SIZE until the final block, which will be len % BLOCK_SIZE.
     *
     * For CHUNKED, this block_size can be more volatile. In most cases it
     * will be BLOCK_SIZE if chunk size > BLOCK_SIZE, until the final block.
     *
     * However for very small chunks (which are unlikely) this will always
     * be shorter than BLOCK_SIZE.
     */
    block_size = MIN(BLOCK_SIZE, len - pos);

    /* In either case, all headers and block computation has now been done,
     * and the buffer can be streamed to disk.
     */
    if(!this->receive(block, block_size))
      return HOST_RECV_FAILED;

    if(request.content_encoding_type == HTTPRequestInfo::EN_GZIP)
    {
      /* This is the first block requiring inflation. In this case, we must
       * parse the GZIP header in order to compute an offset to the DEFLATE
       * formatted data.
       */
      if(!mid_inflate)
      {
        ssize_t deflate_offset = 0;

        /* Compute the offset within this block to begin the inflation
         * process. For all but the first block, deflate_offset will be
         * zero.
         */
        deflate_offset = zlib_skip_gzip_header(block, block_size);
        if(deflate_offset < 0)
          return (HTTPHostStatus)deflate_offset;

        /* Now we can initialize the decompressor. Pass along the block
         * without the GZIP header (and for a GZIP, this is also without
         * the DEFLATE header too, which is what the -MAX_WBITS trick is for).
         */
        stream.avail_in = block_size - (unsigned long)deflate_offset;
        stream.next_in = &block[deflate_offset];
        stream.zalloc = Z_NULL;
        stream.zfree = Z_NULL;
        stream.opaque = Z_NULL;

        if(inflateInit2(&stream, -MAX_WBITS) != Z_OK)
          return HOST_ZLIB_INFLATE_FAILED;

        mid_inflate = true;
      }
      else
      {
        stream.avail_in = block_size;
        stream.next_in = block;
      }

      while(true)
      {
        outbuf.resize(BLOCK_SIZE);
        if(!outbuf)
          return HOST_ALLOC_FAILED;

        // Each pass, only decompress a maximum of BLOCK_SIZE
        stream.avail_out = BLOCK_SIZE;
        stream.next_out = outbuf;

        /* Perform the inflation (this will modify avail_in and
         * next_in automatically.
         */
        int ret = inflate(&stream, Z_NO_FLUSH);
        if(ret != Z_OK && ret != Z_STREAM_END)
          return HOST_ZLIB_INFLATE_FAILED;

        size_t amount = BLOCK_SIZE - stream.avail_out;
        if(amount && vfwrite(outbuf, 1, amount, file) != amount)
          return HOST_FWRITE_FAILED;

        // If the stream has terminated, flag it and break out
        if(ret == Z_STREAM_END)
        {
          mid_inflate = false;
          break;
        }

        /* The stream hasn't terminated but we've exhausted input
         * data for this pass.
         */
        if(stream.avail_in == 0)
          break;
      }

      // The stream terminated, so we should free associated data-structures
      if(!mid_inflate)
        inflateEnd(&stream);
    }
    else
    {
      /* If the transfer is not deflated, we can simply write out
       * block_size bytes to the file now.
       */
      if(vfwrite(block, 1, block_size, file) != block_size)
        return HOST_FWRITE_FAILED;
    }

    pos += block_size;

    if(this->receive_callback)
      this->receive_callback(vftell(file));

    /* For NORMAL transfers we can now abort since we have reached the end
     * of our payload.
     *
     * For CHUNKED transfers, we remove the trailing newline and flag that
     * a new set of chunk headers should be read.
     */
    if(len == pos)
    {
      if(request.transfer_encoding_type == HTTPRequestInfo::EN_NORMAL)
      {
        break;
      }
      else

      if(request.transfer_encoding_type == HTTPRequestInfo::EN_CHUNKED)
      {
        if(http_receive_line(line, LINE_BUF_LEN) != 0)
          return HOST_HTTP_INVALID_HEADER;
        mid_chunk = false;
      }
    }
  }
  request.final_length = vftell(file);
  return HOST_SUCCESS;
}

HTTPHostStatus HTTPHost::get(HTTPRequestInfo &request, char *buffer, size_t len)
{
  vfile *vf = vfile_init_mem(buffer, len, "wb");
  HTTPHostStatus result = this->get(request, vf);
  vfclose(vf);
  return result;
}

#ifdef NETWORK_DEADCODE

HTTPHostStatus HTTPHost::send_file(vfile *file, const char *mime_type)
{
  boolean mid_deflate = false;
  char line[LINE_BUF_LEN];
  uint32_t crc, uSize;
  z_stream stream;
  long size;

  trace("--HOST-- HTTPHost::send_file\n");

  // Tell the client that we're going to use HTTP/1.1 features
  if(http_send_line("HTTP/1.1 200 OK") < 0)
    return HOST_SEND_FAILED;

  /* To bring ourselves into complete HTTP 1.1 compliance, send
   * some headers that we know our client doesn't actually need.
   */
  if(http_send_line("Accept-Ranges: bytes") < 0)
    return HOST_SEND_FAILED;
  if(http_send_line("Vary: Accept-Encoding") < 0)
    return HOST_SEND_FAILED;

  // Always zlib deflate content; keeps code simple
  if(http_send_line("Content-Encoding: gzip") < 0)
    return HOST_SEND_FAILED;

  // We'll just send everything chunked, unconditionally
  if(http_send_line("Transfer-Encoding: chunked") < 0)
    return HOST_SEND_FAILED;

  // Pass along a type hint for the client (mandatory sanity check for MZX)
  snprintf(line, LINE_BUF_LEN, "Content-Type: %s", mime_type);
  line[LINE_BUF_LEN - 1] = 0;
  if(http_send_line(line) < 0)
    return HOST_SEND_FAILED;

  // Terminate the headers with a blank line
  if(http_send_line("") < 0)
    return HOST_SEND_FAILED;

  // Initialize CRC for GZIP footer
  crc = crc32(0L, Z_NULL, 0);

  // Record uncompressed size for GZIP footer
  size = vfilelength(file, true);
  uSize = (uint32_t)size;
  if(size < 0)
    return HOST_FREAD_FAILED;

  while(true)
  {
    int deflate_offset = 0, ret = Z_OK, deflate_flag = Z_SYNC_FLUSH;
    char block[BLOCK_SIZE], zblock[BLOCK_SIZE];
    size_t block_size;

    /* Read a block from the disk source and compute a CRC32 on the
     * fly. This CRC will be dumped at the end of the deflated data
     * and is required for RFC 1952 compliancy.
     */
    block_size = vfread(block, 1, BLOCK_SIZE, file);
    crc = crc32(crc, (Bytef *)block, block_size);

    /* The fread() above pretty much guarantees that block_size will
     * be BLOCK_SIZE up to the final block. However, fread() also
     * returns a short count if there was an I/O error, and we must
     * detect this. If it's legitimately the final block, give the
     * compressor this information.
     */
    if(block_size != BLOCK_SIZE)
    {
      /* FIXME
      if(!feof(file))
        return HOST_FREAD_FAILED;
      */
      deflate_flag = Z_FINISH;
    }

    /* We exhausted input at a block boundary. This is unlikely,
     * but in the event that it happens we can simply ignore
     * the deflate stage and write out the terminal chunk signature.
     */
    if(block_size == 0)
      break;

    /* Regardless of whether we are initializing the compressor in
     * the next section or not, we always have BLOCK_SIZE aligned
     * input data available.
     */
    stream.avail_in = block_size;
    stream.next_in = (Bytef *)block;

    if(!mid_deflate)
    {
      deflate_offset = zlib_forge_gzip_header(zblock);

      stream.avail_out = BLOCK_SIZE - (unsigned long)deflate_offset;
      stream.next_out = (Bytef *)&zblock[deflate_offset];
      stream.zalloc = Z_NULL;
      stream.zfree = Z_NULL;
      stream.opaque = Z_NULL;

      ret = deflateInit2(&stream, Z_BEST_COMPRESSION,
       Z_DEFLATED, -MAX_WBITS, 8, Z_DEFAULT_STRATEGY);
      if(ret != Z_OK)
        return HOST_ZLIB_DEFLATE_FAILED;

      mid_deflate = true;
    }
    else
    {
      stream.avail_out = BLOCK_SIZE;
      stream.next_out = (Bytef *)zblock;
    }

    while(true)
    {
      unsigned long chunk_size;

      // Deflate a chunk of the input (partially or fully)
      ret = deflate(&stream, deflate_flag);
      if(ret != Z_OK && ret != Z_STREAM_END)
        return HOST_ZLIB_INFLATE_FAILED;

      // Compute chunk length (final chunk includes GZIP footer)
      chunk_size = BLOCK_SIZE - stream.avail_out;
      if(ret == Z_STREAM_END)
        chunk_size += 2 * sizeof(uint32_t);

      // Dump compressed chunk length
      snprintf(line, LINE_BUF_LEN, "%lx", chunk_size);
      if(http_send_line(line) < 0)
        return HOST_SEND_FAILED;

      // Send the compressed output block over the socket
      if(!this->send(zblock, BLOCK_SIZE - stream.avail_out))
        return HOST_SEND_FAILED;

      /* We might not have finished the entire stream, but the
       * available input is likely to have been exhausted. With
       * Z_SYNC_FLUSH this will commonly result in zero bytes
       * remaining in the input source. Additionally, if this is
       * the final chunk (and Z_FINISH was flagged), Z_STREAM_END
       * will be set. In either case, we must break out.
       */
      if((ret == Z_OK && stream.avail_in == 0) || ret == Z_STREAM_END)
        break;

      // Output has been flushed; start over for the remaining input (if any)
      stream.avail_out = BLOCK_SIZE;
      stream.next_out = (Bytef *)zblock;
    }

    /* Z_FINISH was flagged, stream ended
     * Terminate compression
     */
    if(ret == Z_STREAM_END)
    {
      // Free any zlib allocated resources
      deflateEnd(&stream);

      // Write out GZIP `CRC32' footer
      if(!this->send(&crc, sizeof(uint32_t)))
        return HOST_SEND_FAILED;

      // Write out GZIP `ISIZE' footer
      if(!this->send(&uSize, sizeof(uint32_t)))
        return HOST_SEND_FAILED;

      mid_deflate = false;
    }

    // Newline after chunk's data
    if(http_send_line("") < 0)
      return HOST_SEND_FAILED;

    // Final block; can break out
    if(block_size != BLOCK_SIZE)
      break;
  }

  // Terminal chunk signature, so called "trailer"
  if(http_send_line("0") < 0)
    return HOST_SEND_FAILED;

  // Post-trailer newline
  if(http_send_line("") < 0)
    return HOST_SEND_FAILED;

  return HOST_SUCCESS;
}

static const char resp_404[] =
 "<html>"
  "<head><title>404</title></head>"
  "<body><pre>404 ;-(</pre></body>"
 "</html>";

boolean HTTPHost::handle_request()
{
  const char *mime_type = "application/octet-stream";
  char buffer[LINE_BUF_LEN], *buf = buffer;
  char *cmd_type, *path, *proto;
  HTTPHostStatus ret;
  size_t path_len;
  vfile *f;

  trace("--HOST-- HTTPHost::handle_request\n");

  if(http_receive_line(buffer, LINE_BUF_LEN) < 0)
  {
    warn("Failed to receive HTTP request\n");
    return false;
  }

  cmd_type = strsep(&buf, " ");
  if(!cmd_type)
    return false;

  if(strcmp(cmd_type, "GET") != 0)
    return false;

  path = strsep(&buf, " ");
  if(!path)
    return false;

  proto = strsep(&buf, " ");
  if(!proto)
    return false;

  if(strncmp("HTTP/1.1", proto, 8) != 0)
  {
    warn("Client must support HTTP 1.1, rejecting\n");
    return false;
  }

  if(!http_skip_headers())
  {
    warn("Failed to skip HTTP headers\n");
    return false;
  }

  path++;
  trace("Received request for '%s'\n", path);

  f = vfopen_unsafe(path, "rb");
  if(!f)
  {
    warn("Failed to open file '%s', sending 404\n", path);

    snprintf(buffer, LINE_BUF_LEN,
     "HTTP/1.1 404 Not Found\r\n"
     "Content-Length: %zd\r\n"
     "Content-Type: text/html\r\n"
     "Connection: close\r\n\r\n", strlen(resp_404));

    if(this->send(buffer, strlen(buffer)))
    {
      if(!this->send(resp_404, strlen(resp_404)))
        warn("Failed to send 404 payload\n");
    }
    else
      warn("Failed to send 404 status code\n");

    return false;
  }

  path_len = strlen(path);
  if(path_len >= 4 && strcasecmp(&path[path_len - 4], ".txt") == 0)
    mime_type = "text/plain";

  ret = this->send_file(f, mime_type);
  vfclose(f);
  if(ret != HOST_SUCCESS)
  {
    warn("Failed to send file '%s' over HTTP (error %d)\n", path, ret);
    return false;
  }

  return true;
}

#endif
