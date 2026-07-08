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

#include "../../src/audio/audio_wav_load.c"
#include "../../src/network/sha256.h"

#define DATA_BASE_DIR "unit/audio/wav"

static void check_sha256(struct wav_info *wav, const char *filename, const char *expected)
{
  struct SHA256_ctx ctx;
  char buf[65];

  SHA256_init(&ctx);
  SHA256_update(&ctx, wav->wav_data, wav->data_length);
  SHA256_final(&ctx);

  snprintf(buf, sizeof(buf), SHA256_PRINT_FMT,
   ctx.H[0], ctx.H[1], ctx.H[2], ctx.H[3], ctx.H[4], ctx.H[5], ctx.H[6], ctx.H[7]);

  ASSERTCMP(buf, expected, "%s", filename);
}

static void check_sam_load(const char *filename, enum wav_format fmt, int chn,
 size_t rate, size_t bytes, size_t loop_start, size_t loop_end, const char *sha256)
{
  struct wav_info wav;
  boolean ret;
  vfile *vf;

  vf = vfopen_unsafe(filename, "rb");
  ASSERT(vf, "%s", filename);

  ret = audio_load_sam(&wav, vf, filename);
  vfclose(vf);
  ASSERTEQ(ret, true, "%s", filename);

  ASSERTEQ(wav.format, fmt, "%s", filename);
  ASSERTEQ(wav.channels, (unsigned)chn, "%s", filename);
  ASSERTEQ(wav.freq, rate, "%s", filename);
  ASSERTEQ(wav.data_length, bytes, "%s", filename);
  ASSERTEQ(wav.loop_start, loop_start, "%s", filename);
  ASSERTEQ(wav.loop_end, loop_end, "%s", filename);
  ASSERTEQ(wav.enable_sam_frequency_hack, true, "%s", filename);
  check_sha256(&wav, filename, sha256);

  audio_free_wav(&wav);
}

static void check_wav_load(const char *filename, enum wav_format fmt, int chn,
 size_t rate, size_t bytes, size_t loop_start, size_t loop_end,
 const char *sha256le, const char *sha256be)
{
  struct wav_info wav;
  boolean ret;
  vfile *vf;

  vf = vfopen_unsafe(filename, "rb");
  ASSERT(vf, "%s", filename);

  ret = audio_load_wav(&wav, vf, filename);
  vfclose(vf);
  ASSERTEQ(ret, true, "%s", filename);

  ASSERTEQ(wav.format, fmt, "%s", filename);
  ASSERTEQ(wav.channels, (unsigned)chn, "%s", filename);
  ASSERTEQ(wav.freq, rate, "%s", filename);
  ASSERTEQ(wav.data_length, bytes, "%s", filename);
  ASSERTEQ(wav.loop_start, loop_start, "%s", filename);
  ASSERTEQ(wav.loop_end, loop_end, "%s", filename);
  ASSERTEQ(wav.enable_sam_frequency_hack, false, "%s", filename);

  const char *expected_sha256;
#if PLATFORM_BYTE_ORDER == PLATFORM_LIL_ENDIAN
  expected_sha256 = sha256le;
  (void)sha256be;
#else
  expected_sha256 = sha256be;
  (void)sha256le;
#endif
  check_sha256(&wav, filename, expected_sha256);

  audio_free_wav(&wav);
}

static void check_wav_fail(const char *filename,
 const char *sha256le, const char *sha256be)
{
  struct wav_info wav;
  boolean ret;
  vfile *vf;

  vf = vfopen_unsafe(filename, "rb");
  ASSERT(vf, "%s", filename);

  ret = audio_load_wav(&wav, vf, filename);
  vfclose(vf);
  ASSERTEQ(ret, false, "%s", filename);

  /* Not testable */
  (void)sha256le;
  (void)sha256be;
}


UNITTEST(SAM)
{
  check_sam_load(DATA_BASE_DIR "/m_s8.sam", SAMPLE_S8, 1, 8363, 1673, 0, 0,
   "4bb18c2a2aaefd2e63b1f5afea93613d229d9cf347512c9154132a3f0a9c5800");

  check_wav_fail(DATA_BASE_DIR "/m_s8.sam",
   "4bb18c2a2aaefd2e63b1f5afea93613d229d9cf347512c9154132a3f0a9c5800",
   "4bb18c2a2aaefd2e63b1f5afea93613d229d9cf347512c9154132a3f0a9c5800");
}

UNITTEST(PCM)
{
  SECTION(MonoU8)
    check_wav_load(DATA_BASE_DIR "/m_u8.wav", SAMPLE_U8, 1, 8000, 1600, 0, 0,
     "6f3daf7234aa90f4dc0ab322dd46ae0bc3c448954ae92bf1eba49ab0c5807813",
     "6f3daf7234aa90f4dc0ab322dd46ae0bc3c448954ae92bf1eba49ab0c5807813");

  SECTION(MonoS16)
    check_wav_load(DATA_BASE_DIR "/m_s16.wav", SAMPLE_S16LE, 1, 8000, 3200, 0, 0,
     "42de7dbb03a331d544ee518e44aa99c69c554ed0e13d6460b5dca7d86d504a9a",
     "42de7dbb03a331d544ee518e44aa99c69c554ed0e13d6460b5dca7d86d504a9a");

  SECTION(MonoS24)
    check_wav_fail(DATA_BASE_DIR "/m_s24.wav",
     "17a638510c96e08e57d4e0a2c5a5c54f40eeed8284b24f702872bf7fac2619a6",
     "17a638510c96e08e57d4e0a2c5a5c54f40eeed8284b24f702872bf7fac2619a6");

  SECTION(MonoS32)
    check_wav_fail(DATA_BASE_DIR "/m_s32.wav",
     "95d91bb62e9e478be6f7a1b9a48f14f77e698815d2842a39cf657acc54b7e351",
     "95d91bb62e9e478be6f7a1b9a48f14f77e698815d2842a39cf657acc54b7e351");

  SECTION(StereoU8)
    check_wav_load(DATA_BASE_DIR "/s_u8.wav", SAMPLE_U8, 2, 8000, 3200, 0, 0,
     "0b94ec41f6360d7f35c82c99f7a2edabe8ec298a2fa4236e608732be4577aabc",
     "0b94ec41f6360d7f35c82c99f7a2edabe8ec298a2fa4236e608732be4577aabc");

  SECTION(StereoS16)
    check_wav_load(DATA_BASE_DIR "/s_s16.wav", SAMPLE_S16LE, 2, 8000, 6400, 0, 0,
     "1fb4e713c0f109fae610ce45c1a7a4b6be5692b7e42b431ce9653db3d29446aa",
     "1fb4e713c0f109fae610ce45c1a7a4b6be5692b7e42b431ce9653db3d29446aa");

  SECTION(StereoS24)
    check_wav_fail(DATA_BASE_DIR "/s_s24.wav",
     "51589ae6ca0de9bc409cc2798f203d64ef2eff89b3e18f686f6f2424fda4e09e",
     "51589ae6ca0de9bc409cc2798f203d64ef2eff89b3e18f686f6f2424fda4e09e");

  SECTION(StereoS32)
    check_wav_fail(DATA_BASE_DIR "/s_s32.wav",
     "81b574519cb904ccec6d40cd9ca49f5ced6a471203097b7f6e550353d69ff9e2",
     "81b574519cb904ccec6d40cd9ca49f5ced6a471203097b7f6e550353d69ff9e2");

  SECTION(MonoU8_smpl)
    check_wav_load(DATA_BASE_DIR "/loop_m_u8.wav", SAMPLE_U8, 1, 44100, 2004, 200, 1804,
     "7664f14049704a9a74cf950adb973acd4088d9948d39eacbb0790c916600e19d",
     "7664f14049704a9a74cf950adb973acd4088d9948d39eacbb0790c916600e19d");

  SECTION(MonoS16_smpl)
    check_wav_load(DATA_BASE_DIR "/loop_m_s16.wav", SAMPLE_S16LE, 1, 44100, 4008, 400, 3608,
     "6a1144671cb8f99b48f4d327d2a7b00f7d06fa090e1e9251894f5ed0c66882e1",
     "6a1144671cb8f99b48f4d327d2a7b00f7d06fa090e1e9251894f5ed0c66882e1");

  SECTION(StereoU8_smpl)
    check_wav_load(DATA_BASE_DIR "/loop_s_u8.wav", SAMPLE_U8, 2, 44100, 4008, 400, 3608,
     "43c7598725e60b1fb919542549f37dbc5a7f4b6e49538c5fae982d09f2df6460",
     "43c7598725e60b1fb919542549f37dbc5a7f4b6e49538c5fae982d09f2df6460");

  SECTION(StereoS16_smpl)
    check_wav_load(DATA_BASE_DIR "/loop_s_s16.wav", SAMPLE_S16LE, 2, 44100, 8016, 800, 7216,
     "c34e6333e08cd62648bb08d1f71172e7cc0d6409f20022478022e3eba2205d29",
     "c34e6333e08cd62648bb08d1f71172e7cc0d6409f20022478022e3eba2205d29");

}

UNITTEST(Float)
{
  SECTION(MonoF32)
    check_wav_fail(DATA_BASE_DIR "/m_f32.wav",
     "a4eb62babb05fd3078f69bcc56520471ec3c7ba1ce6520988536f6926b489241",
     "a4eb62babb05fd3078f69bcc56520471ec3c7ba1ce6520988536f6926b489241");

  SECTION(MonoF64)
    check_wav_fail(DATA_BASE_DIR "/m_f64.wav",
     "",
     "");

  SECTION(StereoF32)
    check_wav_fail(DATA_BASE_DIR "/s_f32.wav",
     "d8b2011f71ccb4efbf4bbe148d805f70cb8a74f00755ac7cd5b0ea19d697f6f2",
     "d8b2011f71ccb4efbf4bbe148d805f70cb8a74f00755ac7cd5b0ea19d697f6f2");

  SECTION(StereoF64)
    check_wav_fail(DATA_BASE_DIR "/s_f64.wav",
     "",
     "");
}

UNITTEST(ULaw)
{
#ifndef CONFIG_SDL
  SKIP();
#endif

  SECTION(MonoULaw)
    check_wav_load(DATA_BASE_DIR "/m_ulaw.wav", SAMPLE_S16, 1, 8000, 3200, 0, 0,
     "771c9845180bba23a147b7d86e2d1aca6fa6f07222ddc34e40f14903ad3ab976",
     "610694769beca8aa55f23b6c1050cbdc9a2350c2cb17a263fedf5f7a0b8b4419");

  SECTION(StereoULaw)
    check_wav_load(DATA_BASE_DIR "/s_ulaw.wav", SAMPLE_S16, 2, 8000, 6400, 0, 0,
     "3886366672ceb11e98223e47d4f24f2efa77db5274fccca961beba5f5f905d45",
     "977e2ec4f0e78fb82320e4ae3cd4a6fd11e1462180da4b16470c02ac8d27b406");

  SECTION(MonoULaw_smpl)
    check_wav_load(DATA_BASE_DIR "/loop_m_ulaw.wav", SAMPLE_S16, 1, 44100, 4008, 400, 3608,
     "86613075a0327a96adf4f488c6ed3e8017f8c8e20bb0ff1339f88966788238ab",
     "83265b810fad2e987f20ce7d4d6727d007eede6b88a0584f8f51567f8a0b97a4");

  SECTION(StereoULaw_smpl)
    check_wav_load(DATA_BASE_DIR "/loop_s_ulaw.wav", SAMPLE_S16, 2, 44100, 8016, 800, 7216,
     "fc4de372c4dbc251bb0c67abcc6a82f25e26c68890b2ce85e1a563aadd73fdb2",
     "d04bc88774a4fca0df0cebbb15abb5e897f384b5cb94cf3c5225508485c66204");
}

UNITTEST(ALaw)
{
#ifndef CONFIG_SDL
  SKIP();
#endif

  SECTION(MonoALaw)
    check_wav_load(DATA_BASE_DIR "/m_alaw.wav", SAMPLE_S16, 1, 8000, 3200, 0, 0,
     "9e1de6b18593786f14bfb3fb4476a28e9c22ebecb5204ce38dabfdf8de00dc5f",
     "83645b1bdebd1bcdd5fcbea2bae81c767691ba50aa453efd339f21d60a1e2cd8");

  SECTION(StereoALaw)
    check_wav_load(DATA_BASE_DIR "/s_alaw.wav", SAMPLE_S16, 2, 8000, 6400, 0, 0,
     "565b1f2e8bec286d3d51e64547b8a71dee3663d5791062002a42b1c98f3b3360",
     "d2927259eba2871359c55add643376752386ed8f5704228ca4d3d7d50a43f3de");

  SECTION(MonoALaw_smpl)
    check_wav_load(DATA_BASE_DIR "/loop_m_alaw.wav", SAMPLE_S16, 1, 44100, 4008, 400, 3608,
     "cbd3410c8deecc74f377d6c9a5062428f1ff614a98a0e7a0063b1a626900fedd",
     "e4730f780903f770c805a436142dfe92acd55a120e9297d9ac07ea87bbf88883");

  SECTION(StereoALaw_smpl)
    check_wav_load(DATA_BASE_DIR "/loop_s_alaw.wav", SAMPLE_S16, 2, 44100, 8016, 800, 7216,
     "e49a4629ecc57753706f77da851d8ea1eeac0f8576022f1d3d97269b88d4f82b",
     "282df728adc3a5752dd7a65c4a94150bab2ff626875513983353b02156a7fc26");
}

UNITTEST(IMA_ADPCM)
{
#ifndef CONFIG_SDL
  SKIP();
#endif

  SECTION(MonoIMAADPCM)
    check_wav_load(DATA_BASE_DIR "/m_ima_adpcm.wav", SAMPLE_S16, 1, 8000, 4040, 0, 0,
     "43b3e70e2eeb3cf96ff5c532bbb377305ae2b6c9485fc86dfb9a07182175f138",
     "641092095039535ee58559648da90e1753a5b540a43ed0e10ce28971c003cfc8");

  SECTION(StereoIMAADPCM)
    check_wav_load(DATA_BASE_DIR "/s_ima_adpcm.wav", SAMPLE_S16, 2, 8000, 8080, 0, 0,
     "4036c0c18654614928d436f1936940520d6f5d15a0ba41e455247fdebd8791c7",
     "8ae078be6fb881953c2080b6ca5f361fb11a62e6eec7c033340489e2a8c7dd83");

  SECTION(MonoIMAADPCM_smpl)
    check_wav_load(DATA_BASE_DIR "/loop_m_ima_adpcm.wav", SAMPLE_S16, 1, 44100, 8178, 400, 3608,
     "125b58f1e7610caa4396710b1a4f7e3727d9b18b43938dabe74c76d3f6c1ce47",
     "a48ce845d272662a6ba488b44d1ffaf30c4b78768ecd5c9d415698576142aae2");

  SECTION(StereoIMAADPCM_smpl)
    check_wav_load(DATA_BASE_DIR "/loop_s_ima_adpcm.wav", SAMPLE_S16, 2, 44100, 8164, 800, 7216,
     "faf213999ff7ebf8efcc025ab964c55cd3ac5fb3dbff7607604ce6fd522b9f40",
     "cfcba051ae8e884c36fff91aea081dafa941f1084b0a0edeef531e71986e82b8");

}

UNITTEST(MS_ADPCM)
{
#ifndef CONFIG_SDL
  SKIP();
#endif

  SECTION(MonoMSADPCM)
    check_wav_load(DATA_BASE_DIR "/m_ms_adpcm.wav", SAMPLE_S16, 1, 8000, 4000, 0, 0,
     "d928beac8e3b3c296ce5edf94f5fdd2cb0cd686dc9783ef502c4d75d74a6e45d",
     "a7498d85745ce92b3d4f8cbb134e52ffc345b160fd70237717d2821c2c08b2f4");

  SECTION(StereoMSADPCM)
    check_wav_load(DATA_BASE_DIR "/s_ms_adpcm.wav", SAMPLE_S16, 2, 8000, 8000, 0, 0,
     "a4a611613955f145259c573501eb9363b22dd84541a134dcee51e4c1815d4240",
     "9b216e43a18c97611164ed79ec110738290407b99ab658f444ca83c9bf0fefc5");

  SECTION(MonoMSADPCM_smpl)
    check_wav_load(DATA_BASE_DIR "/loop_m_ms_adpcm.wav", SAMPLE_S16, 1, 44100, 8168, 400, 3608,
     "c3fdd9cd0237d9aaa8b252b67fd3bcee0b7adeb47da1c6223f7b42301dc21955",
     "5449a210b81ba32aab03553153a232257c1af5fd1129be0d6ea1c1669330f156");

  SECTION(StereoMSADPCM_smpl)
    check_wav_load(DATA_BASE_DIR "/loop_s_ms_adpcm.wav", SAMPLE_S16, 2, 44100, 8144, 800, 7216,
     "3e4befd105d74d72f9090404aaeea184529c44e06d85c02b2d683dfd146d25a7",
     "b56cdeab6bf7c485a1c0b076e5beb951a047a8c911ff034b7be888d0201ac19c");
}
