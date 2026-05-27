/* MegaZeux
 *
 * Copyright (C) 2024-2026 Alice Rowan <petrifiedrowan@gmail.com>
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
#include "../UnitIO.hpp"

#include "../../src/audio/sampled_stream.cpp"

#include <math.h>
#include <time.h>

//#define GENERATE_TEST_FILES

#define DATA_BASE_DIR "unit/audio/mixer"

void destruct_audio_stream(struct audio_stream *a_src)
{
  // nop to shut up linker
}

struct audio audio; // shut up linker

struct sequence
{
  double delta;
  unsigned volume;
};

template<int N>
void adjust_path(char (&buffer)[N], const char *filename)
{
  int ret = snprintf(buffer, sizeof(buffer), DATA_BASE_DIR "/%s", filename);
  ASSERT(ret < N, "wtf");
}

template<mixer_channels SRC_CHANNELS>
class mixer_input
{
  const char *filename;
  std::vector<int16_t> input;
  size_t input_frames = 0;
  bool did_init = false;

public:
  mixer_input(const char *f): filename(f) {}

  void init()
  {
    if(did_init)
      return;

    char path[512];
    adjust_path(path, filename);

    std::vector<uint8_t> in = unit::io::load(path);
    input_frames = in.size() / (SRC_CHANNELS * 2);
    input.resize(input_frames * SRC_CHANNELS + PROLOGUE_LENGTH + EPILOGUE_LENGTH);

    size_t i, j;
    for(i = 0, j = PROLOGUE_LENGTH; i < in.size(); i += 2, j++)
      input[j] = static_cast<int16_t>(unit::io::read16le(in, i));

    for(j = 0; j < PROLOGUE_LENGTH; j++)
      input[j] = 0;
    for(j = 0; j < EPILOGUE_LENGTH; j++)
      input[input.size() - j - 1] = 0;

    did_init = true;
  }

  const std::vector<int16_t> &get() const
  {
    ASSERT(did_init, "run init() before calling get()");
    return input;
  }

  const int16_t *start() const
  {
    ASSERT(did_init, "run init() before calling start()");
    return input.data() + PROLOGUE_LENGTH;
  }

  size_t frames() const
  {
    ASSERT(did_init, "run init() before calling frames()");
    return input_frames;
  }
};

/**
 * Each test steps through its input sequence of notes, mixing each note
 * at the specified frequency-delta and volume and appending the result to
 * a buffer. This buffer is compared against the expected output file.
 * Each frequency-delta is (playback rate / sample rate), so 2.0 will play
 * the input sample at double speed.
 *
 * Each individual note is mixed into an initial pseudo-randomized noise
 * background, which is immediately subtracted out of the result after
 * mixing. ceil(input_samples / delta) are requested from the mix function.
 */
template<mixer_channels DEST_CHANNELS, mixer_channels SRC_CHANNELS,
 mixer_volume VOLUME, mixer_resample RESAMPLE>
class mixer_tester
{
  static constexpr int sample_rate = 8192;
  struct sampled_stream strm{};
  const mixer_input<SRC_CHANNELS> &input;
  uint64_t seed = 0;
  uint64_t stored_seed = 0;

  uint32_t random(uint32_t range)
  {
    if(!seed)
      seed = 1u;

    uint64_t x = seed;
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    return (((x * 0x2545f4914f6cdd1dull) >> 32u) * range) >> 32u;
  }

  void init_noise_background(std::vector<int32_t> &dest)
  {
    stored_seed = seed;
    for(int32_t &smp : dest)
      smp = random(65536) - 32768;
  }

  void remove_noise_background(std::vector<int32_t> &dest)
  {
    seed = stored_seed;
    for(int32_t &smp : dest)
      smp -= random(65536) - 32768;
  }

  void mix(std::vector<int32_t> &_dest, size_t dest_frames)
  {
    int32_t *dest = _dest.data();
    const int16_t *src = input.start();
    int volume = strm.a.volume;
    switch(RESAMPLE)
    {
      case FLAT:
        flat_mix_loop<DEST_CHANNELS, SRC_CHANNELS, VOLUME>(
         &strm, dest, dest_frames, src, volume);
        break;
      case NEAREST:
        resample_mix_loop<DEST_CHANNELS, SRC_CHANNELS, VOLUME, nearest_mix<SRC_CHANNELS>>(
         &strm, dest, dest_frames, src, volume);
        break;
      case LINEAR:
        resample_mix_loop<DEST_CHANNELS, SRC_CHANNELS, VOLUME, linear_mix<SRC_CHANNELS>>(
         &strm, dest, dest_frames, src, volume);
        break;
      case CUBIC:
        resample_mix_loop<DEST_CHANNELS, SRC_CHANNELS, VOLUME, cubic_mix<SRC_CHANNELS>>(
         &strm, dest, dest_frames, src, volume);
        break;
    }
  }


public:
  mixer_tester(const mixer_input<SRC_CHANNELS> &in): input(in)
  {
    seed = (uint64_t)time(NULL) << 32u;
  }

  template<int N>
  void test(const sequence (&seq)[N], const char *expected_file,
   bool allow_generate = false)
  {
    std::vector<int16_t> render;

    char expected_path[512];
    adjust_path(expected_path, expected_file);

    strm.channels = SRC_CHANNELS; // TODO: surround
    strm.dest_channels = DEST_CHANNELS; // TODO: surround
    {
      std::vector<int32_t> dest;
      size_t dest_frames;

      for(const sequence &s : seq)
      {
        if(RESAMPLE == FLAT)
          ASSERT(s.delta == 1.0, "invalid frequency delta for FLAT resampler");

        strm.a.volume = s.volume;
        strm.sample_index = 0;
        //strm.frequency = static_cast<size_t>(sample_rate * s.delta);
        strm.frequency_delta = static_cast<int64_t>((1 << FP_SHIFT) * s.delta);

        dest_frames = static_cast<size_t>(ceil(input.frames() / s.delta));
        dest.resize(dest_frames * strm.dest_channels);

        init_noise_background(dest);
        mix(dest, dest_frames);
        remove_noise_background(dest);

        size_t pos = render.size();
        render.resize(pos + dest.size());
        for(size_t i = 0; i < dest.size(); i++)
          render[pos + i] = Unit::clamp((int)dest[i], -32768, 32767);
      }
    }

    std::vector<uint8_t> raw;
    for(size_t i = 0; i < render.size(); i++)
      unit::io::put16le(raw, render[i]);

#ifdef GENERATE_TEST_FILES
    if(allow_generate)
      unit::io::save(raw, expected_path);
#else
    std::vector<uint8_t> expected = unit::io::load(expected_path);
    ASSERTEQ(raw.size(), expected.size(), "size mismatch");
    for(size_t j = 0; j < raw.size(); j++)
    {
      int diff = raw[j] - expected[j];
      ASSERT(diff >= -1 && diff <= 1, "data mismatch @ %zu: %d != %d",
       j, raw[j], expected[j]);
    }
#endif
  }

  void done()
  {
#ifdef GENERATE_TEST_FILES
    FAIL("generation code enabled");
#endif
  }
};

/**
 * Each target is tested twice--first with a sequence where every volume is
 * 256, and then with a sequence where the volume varies for every note.
 * "Full" targets should render both identical to all 256; "dynamic" should
 * render the same as "full" for all 256, but apply the input volume for
 * the second sequence.
 *
 * Since the "flat" resample mode does no actual resampling, only one rate
 * ratio is tested for it (1.0). All others test various ratios from 0.5 to
 * 2.0, but theoretically any ratio should be valid.
 */

static constexpr sequence flat_full[] =
{
  { 1.0, 256 },
};

static constexpr sequence flat_dynamic[] =
{
  { 1.0, 192 },
};

static constexpr sequence resample_full[] =
{
  { 0.5, 256 },
  { 0.75, 256 },
  { 1.0, 256 },
  { 1.5, 256 },
  { 2.0, 256 },
};

static constexpr sequence resample_dynamic[] =
{
  { 0.5, 224 },
  { 0.75, 256 },
  { 1.0, 192 },
  { 1.5, 157 },
  { 2.0, 219 },
};

static mixer_input<MONO> mono("m.raw");
static mixer_input<STEREO> stereo("s.raw");

#define GENERATE_SECTIONS(seq_full, seq_dynamic, resample) \
do { \
  mono.init(); \
  stereo.init(); \
  \
  SECTION(MonoToMono)                                           \
  {                                                             \
    mixer_tester<MONO, MONO, FULL, resample> t(mono);           \
    t.test(seq_full, #resample "_mm.raw");                      \
    t.test(seq_dynamic, #resample "_mm.raw", true);             \
    t.done();                                                   \
  }                                                             \
  SECTION(MonoToMonoDynamic)                                    \
  {                                                             \
    mixer_tester<MONO, MONO, DYNAMIC, resample> t(mono);        \
    t.test(seq_full, #resample "_mm.raw");                      \
    t.test(seq_dynamic, #resample "_mm_dyn.raw", true);         \
    t.done();                                                   \
  }                                                             \
  SECTION(MonoToStereo)                                         \
  {                                                             \
    mixer_tester<STEREO, MONO, FULL, resample> t(mono);         \
    t.test(seq_full, #resample "_ms.raw");                      \
    t.test(seq_dynamic, #resample "_ms.raw", true);             \
    t.done();                                                   \
  }                                                             \
  SECTION(MonoToStereoDynamic)                                  \
  {                                                             \
    mixer_tester<STEREO, MONO, DYNAMIC, resample> t(mono);      \
    t.test(seq_full, #resample "_ms.raw");                      \
    t.test(seq_dynamic, #resample "_ms_dyn.raw", true);         \
    t.done();                                                   \
  }                                                             \
  SECTION(StereoToMono)                                         \
  {                                                             \
    mixer_tester<MONO, STEREO, FULL, resample> t(stereo);       \
    t.test(seq_full, #resample "_sm.raw");                      \
    t.test(seq_dynamic, #resample "_sm.raw", true);             \
    t.done();                                                   \
  }                                                             \
  SECTION(StereoToMonoDynamic)                                  \
  {                                                             \
    mixer_tester<MONO, STEREO, DYNAMIC, resample> t(stereo);    \
    t.test(seq_full, #resample "_sm.raw");                      \
    t.test(seq_dynamic, #resample "_sm_dyn.raw", true);         \
    t.done();                                                   \
  }                                                             \
  SECTION(StereoToStereo)                                       \
  {                                                             \
    mixer_tester<STEREO, STEREO, FULL, resample> t(stereo);     \
    t.test(seq_full, #resample "_ss.raw");                      \
    t.test(seq_dynamic, #resample "_ss.raw", true);             \
    t.done();                                                   \
  }                                                             \
  SECTION(StereoToStereoDynamic)                                \
  {                                                             \
    mixer_tester<STEREO, STEREO, DYNAMIC, resample> t(stereo);  \
    t.test(seq_full, #resample "_ss.raw");                      \
    t.test(seq_dynamic, #resample "_ss_dyn.raw", true);         \
    t.done();                                                   \
  }                                                             \
} while(0)

UNITTEST(Flat)
{
  GENERATE_SECTIONS(flat_full, flat_dynamic, FLAT);
}

UNITTEST(Nearest)
{
  GENERATE_SECTIONS(resample_full, resample_dynamic, NEAREST);
}

UNITTEST(Linear)
{
  GENERATE_SECTIONS(resample_full, resample_dynamic, LINEAR);
}

UNITTEST(Cubic)
{
  GENERATE_SECTIONS(resample_full, resample_dynamic, CUBIC);
}
