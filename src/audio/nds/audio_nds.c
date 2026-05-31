/* MegaZeux
 *
 * Copyright (C) 2016 Adrian Siekierka <asiekierka@gmail.com>
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

#include "../audio.h"
#include "../audio_struct.h"
#include "../sfx.h"

#include "../../util.h"
#include "../../io/fsafeopen.h"
#include "../../platform/nds/platform_nds.h"

#include <nds.h>
#include <maxmod9.h>

// Utility logic

static u8 nds_vol_table[] =
{
	0, 8, 16, 26, 36, 48, 61, 75, 91, 108, 127
};

#define NDS_VOLUME(vol) nds_vol_table[CLAMP((vol), 0, 10)]

// FIFO calls

static inline void nds_sound_volume(int volume)
{
  fifoSendValue32(FIFO_MZX, CMD_MZX_SOUND_VOLUME | (NDS_VOLUME(volume) << 24));
}

static inline void nds_pcs_sound(int freq, int volume)
{
  fifoSendValue32(FIFO_MZX, CMD_MZX_PCS_TONE | (((freq) & 0xFFFF) << 8) | (NDS_VOLUME(volume) << 24));
}

static inline int nds_mm_get_position(void)
{
  fifoSendValue32(FIFO_MZX, CMD_MZX_MM_GET_POSITION);
  while(!fifoCheckValue32(FIFO_MZX));
  return fifoGetValue32(FIFO_MZX);
}

// PC speaker implementation

#define PCS_DURATION_PER_VBL 9

static int pcs_playing;
static int pcs_frequency;
static int pcs_duration;

static inline void nds_pcs_tick(int duration)
{
  int last_playing = pcs_playing;
  int last_frequency = pcs_frequency;
  int ticks = 0;
  int ticked;

  if(!audio_get_pcs_on() || sfx_should_cancel_note())
    pcs_playing = 0;

  while(ticks < duration)
  {
    if(pcs_playing)
    {
      if(pcs_duration <= 0)
      {
        pcs_playing = 0;
      }
      else
      {
        ticked = MIN(duration - ticks, pcs_duration);
        ticks += ticked;
        pcs_duration -= ticked;
      }
      continue;
    }

    // Update PC speaker.
    sfx_next_note(&pcs_playing, &pcs_frequency, &pcs_duration);

    if(!pcs_playing)
      break;
  }

  if(pcs_playing != last_playing)
  {
    nds_pcs_sound(pcs_playing ? pcs_frequency : 0, NDS_VOLUME(audio_get_pcs_volume()));
  }
  else

  if(pcs_playing && (pcs_frequency != last_frequency))
  {
    nds_pcs_sound(pcs_frequency, NDS_VOLUME(audio_get_pcs_volume()));
  }
}

// Maxmod glue code

#define MAX_NUM_SAMPLES 8

static mm_ds_system maxmod_conf;
static int maxmod_effective_frequency;
static char *maxmod_sample_filenames[MAX_NUM_SAMPLES];

static void nds_maxmod_init(void)
{
  u32 i;

  maxmod_conf.mod_count = 1;
  maxmod_conf.samp_count = isDSiMode() ? MAX_NUM_SAMPLES : 4;
  maxmod_conf.mem_bank = malloc(sizeof(mm_word) * (maxmod_conf.mod_count + maxmod_conf.samp_count));
  for(i = 0; i < (maxmod_conf.mod_count + maxmod_conf.samp_count); i++)
  {
    maxmod_conf.mem_bank[i] = 0;
  }
  for(i = 0; i < maxmod_conf.samp_count; i++)
  {
    maxmod_sample_filenames[i] = NULL;
  }
  maxmod_conf.fifo_channel = FIFO_MAXMOD;

  DC_FlushAll();

  mmInit(&maxmod_conf);
  mmSelectMode(MM_MODE_C); // extended mixing
  mmLockChannels(1 << 8); // sound channel 8 is used for PC speaker
}

void audio_set_module_order(int order)
{
  mmPosition(order);
}

int audio_get_module_order(void)
{
  return nds_mm_get_position();
}

void audio_set_module_position(int position)
{
  // FIXME: position is not supported by maxmod
  mmPosition(position);
}

int audio_get_module_position(void)
{
  // TODO
  return 0;
}

void audio_set_module_volume(int volume) // 0..255
{
  int base_volume = NDS_VOLUME(audio_get_music_volume()); // 0..127
  mmSetModuleVolume((base_volume * volume) >> 5); // 0..1012
}

void audio_set_module_frequency(int frequency)
{
  int mm_freq;

  if(frequency == 0)
    frequency = 44100;
  maxmod_effective_frequency = frequency;
  mm_freq = CLAMP(div32(frequency * 0x400, 44100), 0x200, 0x800);

  mmSetModuleTempo(mm_freq);
  mmSetModulePitch(mm_freq);
}

int audio_get_module_frequency(void)
{
  return maxmod_effective_frequency;
}

int audio_get_module_length(void)
{
  // TODO
  return 0;
}

void audio_set_module_loop_start(int loop_start)
{
  // TODO
}

int audio_get_module_loop_start(void)
{
  // TODO
  return 0;
}

void audio_set_module_loop_end(int loop_end)
{
  // TODO
}

int audio_get_module_loop_end(void)
{
  // TODO
  return 0;
}

static u8 *audio_load_mas_file(char *filename)
{
  u32 mas_size;
  u8 *mas_buffer;
  FILE *mas_file;

  // load the maxmod soundbank file
  mas_file = fopen_unsafe(filename, "rb");
  if(mas_file == NULL)
  {
    return NULL;
  }

  // get soundbank filesize
  fread(&mas_size, 4, 1, mas_file);

  // read buffer
  if(mas_size <= 0)
  {
    fclose(mas_file);
    return NULL;
  }

  mas_buffer = malloc(mas_size + 8);
  if(mas_buffer == NULL)
  {
    fclose(mas_file);
    return NULL;
  }

  if(!fread(mas_buffer + 4, mas_size + 4, 1, mas_file))
  {
    free(mas_buffer);
    fclose(mas_file);
    return NULL;
  }

  fclose(mas_file);
  return mas_buffer;
}

#define MAXMOD_SAMPLE_FREQ(f) (((f) * 1024 + (1 << 14)) >> 15)

static u8 *audio_load_sam_file(char *filename)
{
  u32 sam_size;
  u32 sam_size_aligned;
  u8 *mas_buffer;
  FILE *sam_file;

  // load the SAM file
  sam_file = fopen_unsafe(filename, "rb");
  if(sam_file == NULL)
  {
    return NULL;
  }

  // get SAM filesize
  fseek(sam_file, 0, SEEK_END);
  sam_size = ftell(sam_file);
  fseek(sam_file, 0, SEEK_SET);
  if(sam_size <= 0)
  {
    fclose(sam_file);
    return NULL;
  }

  // generate a .MAS-format sample on the fly
  sam_size_aligned = (sam_size + 3) & (~3);
  mas_buffer = malloc(8 /* preamble */ + 16 /* header */ + sam_size_aligned + 4 /* padded data */);
  if(mas_buffer == NULL)
  {
    fclose(sam_file);
    return NULL;
  }

  if(!fread(mas_buffer + 8 + 16, sam_size, 1, sam_file))
  {
    free(mas_buffer);
    fclose(sam_file);
    return NULL;
  }

  fclose(sam_file);

  // clear up and convert data to signed 8-bit
  memset(mas_buffer, 0, 8 + 16);
  memset(mas_buffer + 8 + 16 + sam_size, 0, sam_size_aligned - sam_size + 4);

  // generate header
  mas_buffer[4] = 2; // type: NDS sample
  mas_buffer[5] = 0x18; // version
  *((u32*) &mas_buffer[8 + 4]) = sam_size_aligned >> 2; // length
  mas_buffer[8 + 8] = 0; // format: signed 8-bit
  mas_buffer[8 + 9] = 2; // repeat: no
  *((u16*) &mas_buffer[8 + 10]) = MAXMOD_SAMPLE_FREQ(audio_get_real_frequency(SAM_DEFAULT_PERIOD)); // length

  return mas_buffer;
}

int audio_play_module(char *filename, boolean safely, int volume)
{
  char mas_filename[MAX_PATH];
  char translated_filename[MAX_PATH];
  char *mas_ext_pos;
  u8 *mas_buffer;

  if(!audio_get_music_on())
    return 1;

  // we can only play pre-converted .MAS files
  strcpy(mas_filename, filename);
  mas_ext_pos = strrchr(mas_filename, '.');
  if(mas_ext_pos == NULL)
  {
    return 0;
  }
  strcpy(mas_ext_pos, ".mas");

  if(safely)
  {
    if(fsafetranslate(mas_filename, translated_filename, MAX_PATH) != FSAFE_SUCCESS)
    {
      return 0;
    }

    strcpy(mas_filename, translated_filename);
  }

  mas_buffer = audio_load_mas_file(mas_filename);
  if(mas_buffer == NULL)
  {
    return 0;
  }
  audio_end_module();

  maxmod_conf.mem_bank[0] = (mm_word) mas_buffer;
  DC_FlushAll();

  // play module
  mmStart(0, MM_PLAY_LOOP);

  audio_set_module_volume(volume);
  audio_set_module_frequency(0);

  return 1;
}

void audio_end_module(void)
{
  if(!mmActive())
    return;

  mmStop();

  while(mmActive())
    delay(1);

  if(maxmod_conf.mem_bank[0] != 0)
  {
    free((void*) maxmod_conf.mem_bank[0]);
    maxmod_conf.mem_bank[0] = 0;
  }
}

void audio_spot_sample(int period, int which)
{
  // not implemented
}

// Sample playback code

void audio_play_sample(char *filename, boolean safely, int period)
{
  char mas_filename[MAX_PATH];
  char translated_filename[MAX_PATH];
  char *mas_ext_pos;
  u8 *mas_buffer;
  mm_sound_effect sfx;
  int freq_desired, freq_real;
  u32 sample_id, mas_fn_len;

  if(!audio_get_music_on())
    return;

  // we can only play pre-converted .SAM files
  strcpy(mas_filename, filename);
  mas_ext_pos = strrchr(mas_filename, '.');
  if(mas_ext_pos == NULL)
  {
    return;
  }
  strcpy(mas_ext_pos, ".sam");

  if(safely)
  {
    if(fsafetranslate(mas_filename, translated_filename, MAX_PATH) != FSAFE_SUCCESS)
    {
      return;
    }

    strcpy(mas_filename, translated_filename);
  }

  sample_id = 0;
  for(sample_id = 0; sample_id < maxmod_conf.samp_count; sample_id++)
  {
    if(maxmod_sample_filenames[sample_id] != NULL)
    {
      if(!strcmp(maxmod_sample_filenames[sample_id], mas_filename))
      {
        break;
      }
    }
  }

  if(sample_id == maxmod_conf.samp_count)
  {
    // deallocate all samples
    audio_end_sample();
    sample_id = 0;
  }

  if(maxmod_sample_filenames[sample_id] == NULL)
  {
    mas_buffer = audio_load_sam_file(mas_filename);
    if(mas_buffer == NULL)
    {
      return;
    }

    maxmod_conf.mem_bank[1 + sample_id] = (mm_word) mas_buffer;
    DC_FlushAll();

    mas_fn_len = strlen(mas_filename) + 1;
    maxmod_sample_filenames[sample_id] = malloc(mas_fn_len);
    strcpy(maxmod_sample_filenames[sample_id], mas_filename);
  }

  freq_real = audio_get_real_frequency(SAM_DEFAULT_PERIOD);
  freq_desired = period == 0 ? freq_real : audio_get_real_frequency(period);

  // play sample
  sfx.id = 0;
  sfx.rate = CLAMP(divf32(freq_desired << 12, freq_real << 12) >> 2, 0, 0xFFFF);
  sfx.handle = 0;
  sfx.volume = NDS_VOLUME(audio_get_sound_volume()) << 1;
  sfx.panning = 128;

  mmEffectEx(&sfx);
}

void audio_end_sample(void)
{
  u32 i;

  // TODO: mmEffectActive() is not present on ARM9 currently
  mmEffectCancelAll();
  delay(1);

  for(i = 0; i < maxmod_conf.samp_count; i++)
  {
    if(maxmod_conf.mem_bank[1 + i] != 0)
    {
      free((void*) maxmod_conf.mem_bank[1 + i]);
      maxmod_conf.mem_bank[1 + i] = 0;

      free(maxmod_sample_filenames[i]);
      maxmod_sample_filenames[i] = NULL;
    }
  }
}

// Audio glue code

int nds_max_samples;

int audio_get_max_samples(void)
{
  return nds_max_samples;
}

void audio_set_max_samples(int max_samples)
{
  if(max_samples < 0 || (u32)max_samples > maxmod_conf.samp_count)
    max_samples = maxmod_conf.samp_count;
  nds_max_samples = max_samples;
}

void nds_audio_vblank(void)
{
  nds_pcs_tick(PCS_DURATION_PER_VBL);
}

void init_audio(struct config_info *conf)
{
  audio_set_music_volume(conf->music_volume);
  audio_set_sound_volume(conf->sam_volume);
  audio_set_music_on(conf->music_on);
  audio_set_pcs_on(conf->pc_speaker_on);
  audio_set_pcs_volume(conf->pc_speaker_volume);

  init_audio_platform(conf);

  audio_set_max_samples(-1);
}

void quit_audio(void)
{
  quit_audio_platform();
}

void init_audio_platform(struct config_info *conf)
{
  // PC speaker init
  pcs_playing = 0;
  pcs_frequency = 0;
  pcs_duration = 0;

  // global volume init
  nds_sound_volume(10);

  // maxmod init
  nds_maxmod_init();
}

void quit_audio_platform(void)
{
  audio_end_module();

  nds_sound_volume(0);
  nds_pcs_sound(0, 0);
}
