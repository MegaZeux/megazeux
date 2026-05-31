/* MegaZeux
 *
 * Copyright (C) 2008 Alistair John Strachan <alistair@devzero.co.uk>
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

/**
 * Easiest way to get this test code to run:
 *
 * 1) Add "#define NETWORK_DEADCODE" to Host.hpp.
 *
 * 2) add this rule to src/Makefile.in (NOTE: needs tabs instead of spaces)

${network_obj}/server.o: ${network_src}/server.cpp
        $(if ${V},,@echo "  CXX     " $<)
        ${CXX} -MD ${core_cxxflags} ${core_flags} -c $< -o $@

 * then include "${network_obj}/server.o" in ${mzx_objs} instead of main.o.
 *
 * 3) ./megazeux
 *
 * 4) Test by sending a request to: http://localhost:5656/config.txt
 */

#include "HTTPHost.hpp"

#include "../const.h"
#include "../util.h"
#include "../platform/sdl/SDLmzx.h"

#include <assert.h>

#define INBOUND_PORT 5656

int main(int argc, char *argv[])
{
  struct config_info conf;
  memset(&conf, 0, sizeof(struct config_info));

  if(Host::host_layer_init(&conf))
  {
    Host server(HOST_TYPE_TCP, HOST_FAMILY_IPV4);
    HTTPHost http_client(HOST_TYPE_TCP, HOST_FAMILY_IPV4);

    if(!server.bind("localhost", INBOUND_PORT))
    {
      warn("Failed to bind host\n");
      goto exit_socket_layer;
    }

    server.set_blocking(false);
    if(!server.listen())
    {
      warn("Failed to listen with host\n");
      goto exit_socket_layer;
    }

    while(true)
    {
      if(server.accept(http_client))
      {
        while(true)
        {
          if(!http_client.handle_request())
          {
            warn("Failure handling HTTP request\n");
            http_client.close();
            break;
          }
        }
      }
      SDL_Delay(10);
    }
  }
  else
    warn("Error initializing socket layer\n");

  return 0;

exit_socket_layer:
  Host::host_layer_exit();
  return 0;
}
