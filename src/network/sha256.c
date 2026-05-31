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

#include "sha256.h"

#include "../util.h"
#include "../platform/platform_endian.h"

#include <string.h>

#define S(x,n)     (((x) >> (n)) | ((x) << (32 - (n))))
#define R(x,n)     ((x) >> (n))

#define Ch(x,y,z)  (((x) & (y)) | (~(x) & (z)))
#define Maj(x,y,z) (((x) & (y)) |  ((x) & (z)) | ((y) & (z)))

#define SIG0(x)    (S(x, 2) ^ S(x,13) ^ S(x,22))
#define SIG1(x)    (S(x, 6) ^ S(x,11) ^ S(x,25))
#define sig0(x)    (S(x, 7) ^ S(x,18) ^ R(x, 3))
#define sig1(x)    (S(x,17) ^ S(x,19) ^ R(x,10))

static const uint32_t K[] =
{
  0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
  0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
  0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
  0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
  0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
  0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
  0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
  0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
  0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
  0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
  0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
  0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
  0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
  0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
  0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
  0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

static const uint32_t H_initial[] = {
  0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
  0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
};

#if PLATFORM_BYTE_ORDER == PLATFORM_LIL_ENDIAN

static void convert_to_bigendian(void *data, int len)
{
  uint32_t *data_as_words = (uint32_t *)data;
  uint8_t *data_as_bytes;
  uint32_t temp;
  uint8_t *temp_as_bytes = (uint8_t *)&temp;
  int i;

  for(i = 0; i < len / 4; i++)
  {
    temp = data_as_words[i];
    data_as_bytes = (uint8_t *)&data_as_words[i];

    data_as_bytes[0] = temp_as_bytes[3];
    data_as_bytes[1] = temp_as_bytes[2];
    data_as_bytes[2] = temp_as_bytes[1];
    data_as_bytes[3] = temp_as_bytes[0];
  }
}

#else

static inline void convert_to_bigendian(void *data, int len) { }

#endif

static void SHA256_transform(struct SHA256_ctx *ctx)
 {
  uint32_t A = ctx->H[0];
  uint32_t B = ctx->H[1];
  uint32_t C = ctx->H[2];
  uint32_t D = ctx->H[3];
  uint32_t E = ctx->H[4];
  uint32_t F = ctx->H[5];
  uint32_t G = ctx->H[6];
  uint32_t H = ctx->H[7];
  uint32_t T1, T2;
  uint32_t W[64];
  int t;

  memcpy(W, ctx->M, 64);

  for(t = 16; t < 64; t++)
    W[t] = sig1(W[t - 2]) + W[t - 7] + sig0(W[t - 15]) + W[t - 16];

  for(t = 0; t < 64; t++)
  {
    T1 = H + SIG1(E) + Ch(E,F,G) + K[t] + W[t];
    T2 = SIG0(A) + Maj(A,B,C);
    H = G;
    G = F;
    F = E;
    E = D + T1;
    D = C;
    C = B;
    B = A;
    A = T1 + T2;
  }

  ctx->H[0] += A;
  ctx->H[1] += B;
  ctx->H[2] += C;
  ctx->H[3] += D;
  ctx->H[4] += E;
  ctx->H[5] += F;
  ctx->H[6] += G;
  ctx->H[7] += H;
}

void SHA256_init(struct SHA256_ctx *ctx)
{
  memcpy(ctx->H, H_initial, 8 * sizeof(uint32_t));
  ctx->lbits = 0;
  ctx->hbits = 0;
  ctx->mlen = 0;
}

void SHA256_update(struct SHA256_ctx *ctx, const void *vdata, size_t data_len)
{
  const uint8_t *data = (const uint8_t *)vdata;
  uint32_t low_bits;
  size_t use;

  /* convert data_len to bits and add to the 64 bit word
   * formed by lbits and hbits
   */

  ctx->hbits += data_len >> 29;
  low_bits = data_len << 3;
  ctx->lbits += low_bits;

  if(ctx->lbits < low_bits)
    ctx->hbits++;

  /* deal with first block */

  use = MIN((size_t)(64 - ctx->mlen), data_len);
  memcpy(ctx->M + ctx->mlen, data, use);
  ctx->mlen += use;
  data_len -= use;
  data += use;

  while(ctx->mlen == 64)
  {
    convert_to_bigendian(ctx->M, 64);
    SHA256_transform(ctx);
    use = MIN(64, data_len);
    memcpy(ctx->M, data, use);
    ctx->mlen = use;
    data_len -= use;
    data += use; /* was missing */
  }
}

void SHA256_final(struct SHA256_ctx *ctx)
{
  if(ctx->mlen < 56)
  {
    ctx->M[ctx->mlen++] = 0x80;
    memset(ctx->M + ctx->mlen, 0x00, 56 - ctx->mlen);
    convert_to_bigendian(ctx->M, 56);
  }
  else
  {
    ctx->M[ctx->mlen++] = 0x80;
    memset(ctx->M + ctx->mlen, 0x00, 64 - ctx->mlen);
    convert_to_bigendian(ctx->M, 64);
    SHA256_transform(ctx);
    memset(ctx->M, 0x00, 56);
  }

  memcpy(ctx->M + 56, &ctx->hbits, 8);
  SHA256_transform(ctx);
}

#ifdef SHA256TEST

int main(void)
{
  struct SHA256_ctx ctx;
  int i;

  const char *test1 = "abc";
  const uint32_t result1[8] = {
    0xba7816bf, 0x8f01cfea, 0x414140de, 0x5dae2223,
    0xb00361a3, 0x96177a9c, 0xb410ff61, 0xf20015ad
  };

  const char *test2 =
    "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
  const uint32_t result2[8] = {
    0x248d6a61, 0xd20638b8, 0xe5c02693, 0x0c3e6039,
    0xa33ce459, 0x64ff2167, 0xf6ecedd4, 0x19db06c1
  };

  printf("test 1\n\n");
  SHA256_init(&ctx);
  SHA256_update(&ctx, test1, strlen(test1));
  SHA256_final(&ctx);

  printf("SHA256(\"%s\") = \n\t", test1);

  for(i = 0; i < 8; i++)
    printf("%08x", ctx.H[i]);

  if(memcmp(ctx.H, result1, 32) == 0)
    printf(" test ok\n\n");
  else
    printf(" test failed\n\n");

  printf("test 2\n\n");
  SHA256_init(&ctx);
  SHA256_update(&ctx, test2, strlen(test2));
  SHA256_final(&ctx);

  printf("SHA256(\"%s\") = \n\t", test2);

  for(i = 0; i < 8; i++)
    printf("%08x", ctx.H[i]);

  if(memcmp(ctx.H, result2, 32) == 0)
    printf(" test ok\n\n");
  else
    printf(" test failed\n\n");

  return 0;
}

#endif // SHA256TEST
