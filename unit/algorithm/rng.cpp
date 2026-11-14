/* MegaZeux
 *
 * Copyright (C) 2026 Alice Rowan <petrifiedrowan@gmail.com>
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

#include "../Unit.hpp"
#include "../../src/algorithm/rng.c"

struct rng_data
{
  uint64_t seed;
  uint64_t range;
  uint32_t retrange;
  uint32_t retmax;
};

/* Tester should manually set the seed every time. */
static constexpr rng_data rng_set_sequence[] =
{
  { 0x0000000000000000ull, UINT32_MAX,  0x47e4ce4au,  0x47e4ce4bu },
  { 0x0000000000012345ull, UINT32_MAX,  0x74caeba0u,  0x74caeba1u },
  { 0x0123456789abcdefull, UINT32_MAX,  0x7c948246u,  0x7c948247u },
  { 0xfedcba9876543210ull, UINT32_MAX,  0x85fd2a77u,  0x85fd2a78u },
  { 0x13579bdf13579bdfull, 0,           0x00000000u,  0x20356cb4u },
  { 0x13579bdf13579bdfull, 1,           0x00000000u,  0x20356cb4u },
  { 0x13579bdf13579bdfull, 2,           0x00000000u,  0x20356cb4u },
  { 0x13579bdf13579bdfull, 12345,       0x00000611u,  0x20356cb4u },
  { 0x13579bdf13579bdfull, UINT32_MAX,  0x20356cb3u,  0x20356cb4u },
  { 0x02468ace02468aceull, 0,           0x00000000u,  0x9152b190u },
  { 0x02468ace02468aceull, 1,           0x00000000u,  0x9152b190u },
  { 0x02468ace02468aceull, 2,           0x00000001u,  0x9152b190u },
  { 0x02468ace02468aceull, 54321,       0x00007874u,  0x9152b190u },
  { 0x02468ace02468aceull, UINT32_MAX,  0x9152b18fu,  0x9152b190u },
  { 0x5eb5c4dc10e8cbecull, MZX_RNG_MAX, 0xffffffffu,  0xffffffffu },
  { 0x5eb5c4dc10e8cbecull, UINT32_MAX,  0xfffffffeu,  0xffffffffu },
};

/* This sequence of seeds should naturally occur. */
static constexpr rng_data rng_auto_sequence[] =
{
  { 0xc6f15104b24e0619ull,  176,  0x00000044u,  0x63f264f5u },
  { 0xe5b93448e723a873ull,  188,  0x0000007au,  0xa77cf35bu },
  { 0x52f034656fab57afull,  236,  0x0000006du,  0x7693588eu },
  { 0x9ea6e02fc921aa99ull,  44,   0x00000022u,  0xc9795c4eu },
  { 0x1d38b0336acb3e9aull,  145,  0x0000008bu,  0xf594e0f2u },
  { 0x6dea989186ae81aeull,  31,   0x00000000u,  0x046f6bf1u },
  { 0x1df32bed3dd39696ull,  65,   0x0000003cu,  0xed5ce91eu },
  { 0xa2f4f45c839ec2b4ull,  166,  0x00000079u,  0xbb236e42u },
  { 0x857276f5d81827f6ull,  254,  0x0000003au,  0x3ac1e3a6u },
  { 0x2014aa9a5dd0f5bfull,  233,  0x000000abu,  0xbc5cc6bbu },
  { 0x81fe4191abbd1882ull,  21,   0x00000011u,  0xdab0ab55u },
  { 0x6a93113e46c58531ull,  111,  0x00000025u,  0x5572bcc7u },
  { 0x743ffbd300de92d9ull,  143,  0x00000081u,  0xe8363d9au },
  { 0x2d43651775823d6bull,  112,  0x00000003u,  0x07bac51fu },
  { 0x6f085be655fe183eull,  229,  0x000000d7u,  0xf0b240d4u },
  { 0xa8d99df94ea8fa55ull,  109,  0x00000069u,  0xf6cf75c7u },
};

/* Another natural sequence of seeds in the algorithm. */
static constexpr rng_data rng_auto_sequence_2[] =
{
  { 0xf7263e58ec311c20ull,  47504,      0x00002122u,  0x2db66940u },
  { 0x813a33954cf9af9cull,  38797,      0x000058d0u,  0x9606d7cau },
  { 0xedd97aebc2823ec9ull,  28765,      0x00004b4fu,  0xab94c3afu },
  { 0x150fdb5319c57cddull,  2046,       0x00000079u,  0x0f27f521u },
  { 0x48576266b218ad7dull,  49924,      0x000092a8u,  0xc084e24du },
  { 0x697b000455138dd8ull,  17479,      0x00003560u,  0xc8239625u },
  { 0x01d73a0dafb19d52ull,  49593,      0x00001d40u,  0x26a80ad3u },
  { 0xfdc9f1ad20555018ull,  37422,      0x00002b2fu,  0x4ba0d524u },
  { 0x99b3238b96e32659ull,  385134853,  0x131c19f7u,  0xd51bff19u },
  { 0xeae60c34249bcd44ull,  3260761158, 0x55f024b6u,  0x7131dd6du },
  { 0x43271155733baf5aull,  2984773568, 0x68721fd2u,  0x964b11b4u },
  { 0x0beffa1cdb93df54ull,  816076611,  0x0e5c3eaau,  0x4b93e0eau },
  { 0xcd1bf8360b21639cull,  1507833713, 0x51c0a230u,  0xe8dddda6u },
  { 0xde07aa315cb494d9ull,  4239773073, 0xd85881b0u,  0xdb298368u },
  { 0x49f508fde100410bull,  1328721241, 0x1072517bu,  0x352975eau },
  { 0x132c2b0d155b30e1ull,  3527012086, 0x5d90a6adu,  0x71effd6du },
};

UNITTEST(mzx_rng_init)
{
  struct mzx_rng_state rng{};
  ASSERTEQ(rng.state, 0, "");
  mzx_rng_init(&rng);
  ASSERT(rng.state != 0, "");
}

UNITTEST(mzx_rng_get_seed)
{
  struct mzx_rng_state rng{};

  auto fn = [&rng](const rng_data &d)
  {
    rng.state = d.seed;
    ASSERTEQ(mzx_rng_get_seed(&rng), d.seed, "");
  };

  test_data_set(fn, rng_set_sequence);
  test_data_set(fn, rng_auto_sequence);
  test_data_set(fn, rng_auto_sequence_2);
}

UNITTEST(mzx_rng_set_seed)
{
  struct mzx_rng_state rng{};

  auto fn = [&rng](const rng_data &d)
  {
    mzx_rng_set_seed(&rng, d.seed);
    ASSERTEQ(rng.state, d.seed, "");
  };

  test_data_set(fn, rng_set_sequence);
  test_data_set(fn, rng_auto_sequence);
  test_data_set(fn, rng_auto_sequence_2);
}

UNITTEST(mzx_rng_get_random)
{
  struct mzx_rng_state rng{};
  uint32_t x;

  SECTION(SeedUpdate)
  {
    mzx_rng_init(&rng);
    uint64_t seed_init = mzx_rng_get_seed(&rng);
    mzx_rng_get_random(&rng, MZX_RNG_MAX);
    uint64_t seed_new = mzx_rng_get_seed(&rng);
    ASSERT(seed_init != seed_new, "%" PRIx64 " -> %" PRIx64, seed_init, seed_new);

    /* Arbitrary seed: calling mzx_rng_get_random should ALWAYS update the seed */
    auto fn = [&rng](const rng_data &d)
    {
      mzx_rng_set_seed(&rng, d.seed);
      mzx_rng_get_random(&rng, UINT32_MAX);

      uint64_t seed = mzx_rng_get_seed(&rng);
      ASSERT(seed != d.seed, "%" PRIx64 " -> %" PRIx64, seed, d.seed);
    };

    test_data_set(fn, rng_set_sequence);
    test_data_set(fn, rng_auto_sequence);
    test_data_set(fn, rng_auto_sequence_2);
  }

  SECTION(ReturnMax)
  {
    auto fn = [&rng](const rng_data &d)
    {
      mzx_rng_set_seed(&rng, d.seed);

      uint32_t val = mzx_rng_get_random(&rng, MZX_RNG_MAX);
      ASSERTEQ(val, d.retmax, "%" PRIx64, d.seed);
    };

    test_data_set(fn, rng_set_sequence);
    test_data_set(fn, rng_auto_sequence);
    test_data_set(fn, rng_auto_sequence_2);
  }

  SECTION(ReturnRange)
  {
    auto fn = [&rng](const rng_data &d)
    {
      mzx_rng_set_seed(&rng, d.seed);

      uint32_t val = mzx_rng_get_random(&rng, d.range);
      ASSERTEQ(val, d.retrange, "%" PRIx64, d.seed);
    };

    test_data_set(fn, rng_set_sequence);
    test_data_set(fn, rng_auto_sequence);
    test_data_set(fn, rng_auto_sequence_2);
  }

  SECTION(SequenceMax)
  {
    auto fn = [&rng](const rng_data &d)
    {
      uint32_t val = mzx_rng_get_random(&rng, MZX_RNG_MAX);
      ASSERTEQ(val, d.retmax, "%" PRIx64, d.seed);
    };

    mzx_rng_set_seed(&rng, rng_auto_sequence[0].seed);
    test_data_set(fn, rng_auto_sequence);
    mzx_rng_set_seed(&rng, rng_auto_sequence_2[0].seed);
    test_data_set(fn, rng_auto_sequence_2);
  }

  SECTION(SequenceRange)
  {
    auto fn = [&rng](const rng_data &d)
    {
      uint32_t val = mzx_rng_get_random(&rng, d.range);
      ASSERTEQ(val, d.retrange, "%" PRIx64, d.seed);
    };

    mzx_rng_set_seed(&rng, rng_auto_sequence[0].seed);
    test_data_set(fn, rng_auto_sequence);
    mzx_rng_set_seed(&rng, rng_auto_sequence_2[0].seed);
    test_data_set(fn, rng_auto_sequence_2);
  }

  SECTION(ZeroSeed)
  {
    /* The xorshift64* generator from Vigna will get stuck in a fixed state
     * if the seed is ever set to 0. The MegaZeux implementation internally
     * translates a seed of 0 to 1 when updating the state to prevent this. */
    mzx_rng_set_seed(&rng, 0ull);
    ASSERTEQ(mzx_rng_get_seed(&rng), 0ull, "");

    x = mzx_rng_get_random(&rng, MZX_RNG_MAX);
    ASSERTEQ(x, 0x47e4ce4bu, "");
    ASSERTEQ(mzx_rng_get_seed(&rng), 0x0000000002000001ull, "");

    mzx_rng_set_seed(&rng, 1ull);
    ASSERTEQ(mzx_rng_get_seed(&rng), 1ull, "");

    x = mzx_rng_get_random(&rng, MZX_RNG_MAX);
    ASSERTEQ(x, 0x47e4ce4bu, "");
    ASSERTEQ(mzx_rng_get_seed(&rng), 0x0000000002000001ull, "");
  }
}
