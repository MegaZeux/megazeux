/* This implementation of SHA-256 is based on an implementation in the
 * public domain, which was in turn based on another public domain
 * implementation of SHA-1 by Adam Back.
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

#ifndef MEGAZEUX_SHA256_H
#define MEGAZEUX_SHA256_H

#include "../compat.h"

MEGAZEUX_BEGIN_DECLS

#include <stdint.h>

struct SHA256_ctx
{
  uint32_t H[8];
  uint32_t hbits;
  uint32_t lbits;
  uint8_t M[64];
  uint8_t mlen;
};

void SHA256_init(struct SHA256_ctx *ctx);
void SHA256_update(struct SHA256_ctx *ctx, const void *vdata, size_t data_len);
void SHA256_final(struct SHA256_ctx *ctx);

MEGAZEUX_END_DECLS

#endif /* MEGAZEUX_SHA256_H */
