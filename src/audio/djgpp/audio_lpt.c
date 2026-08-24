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

#include <stdint.h>
#include <stdlib.h>
#include <dpmi.h>

#include "../audio.h"
#include "../audio_drivers.h"
#include "../audio_players.h"
#include "../audio_struct.h"

#include "../../util.h"
#include "../../platform/djgpp/platform_djgpp.h"
#include "../../platform/djgpp/interrupt.h"

static void lpt_audio_callback(void)
{
  unsigned cur = dos_audio_buffer_pos >= dos_audio_buffer + dos_audio_buffer_size;

  if(cur != dos_audio_buffer_next)
  {
    uint8_t fpustate[108];
    unsigned next;

    next = dos_audio_buffer_next;
    dos_audio_buffer_next ^= 1;

    djgpp_save_x87(fpustate);

    audio_mixer_render_frames(dos_audio_buffer + next * dos_audio_buffer_size,
     dos_audio_buffer_frames, dos_audio_channels, SAMPLE_U8);

    djgpp_restore_x87(fpustate);
  }

  djgpp_rtc_ack();
  djgpp_irq_ack(8);
}

static boolean lpt_init_buffer(unsigned frames, unsigned channels)
{
  uint32_t size = frames * channels * 2;

  dos_audio_buffer = (uint8_t *)cmalloc(size);
  if(!dos_audio_buffer)
    return false;
  memset(dos_audio_buffer, 0x80, size);

  _go32_dpmi_lock_data(dos_audio_buffer, size);

  dos_audio_buffer_end = dos_audio_buffer + size;
  dos_audio_buffer_pos = dos_audio_buffer;
  dos_audio_buffer_size = size >> 1;
  dos_audio_buffer_next = 0;
  dos_audio_buffer_frames = frames;
  dos_audio_channels = channels;
  return true;
}

static void lpt_free_buffer(void)
{
  /* TODO: this is possible, but annoying, to unlock + the benefit is minimal. */
  free(dos_audio_buffer);
  dos_audio_buffer = NULL;
}

static boolean lpt_init_callback(void)
{
  /* Attempt to guarantee at least 4 buffer fill checks within the time it
   * takes to consume a buffer. */
  int rate = (audio.output_frequency * 4) / audio.buffer_frames;

  _go32_dpmi_lock_code((void *)lpt_audio_callback, 4096);

  return djgpp_set_irq8_handler(rate, lpt_audio_callback);
}

static void lpt_quit_callback(void)
{
  djgpp_reset_irq8_handler();
}

/* Limit rate for unbuffered DACs to a preselected set of rates that
 * should result in relatively consistent global timer updates. */
static uint16_t lpt_get_rate(int requested_rate)
{
  static const uint16_t supported_rates[] =
  {
    LPT_4K_HZ,  LPT_5K_HZ,  LPT_6K_HZ,  LPT_7K_HZ,  LPT_8K_HZ,
    LPT_10K_HZ, LPT_11K_HZ, LPT_15K_HZ, LPT_17K_HZ, LPT_22K_HZ,
    LPT_26K_HZ, LPT_28K_HZ, LPT_32K_HZ, LPT_38K_HZ, LPT_44K_HZ, LPT_48K_HZ,
  };
  size_t i;

  for(i = 0; i < ARRAY_SIZE(supported_rates) - 1; i++)
    if(requested_rate < ((int)supported_rates[i] + supported_rates[i + 1]) >> 1)
      break;

  return supported_rates[i];
}

static boolean lpt_read_config(const char *config, boolean is_dual_port)
{
  unsigned l = 1;
  unsigned r = 2;
  int pos;

  if(sscanf(config, "%x%n", &l, &pos) == 1)
  {
    if(l > UINT16_MAX)
      return false;

    if(is_dual_port)
    {
      if(sscanf(config + pos, ",%x", &r) != 1)
        return false;
      if(r > UINT16_MAX)
        return false;
    }
  }

  lpt_left_port = (l >= 0xa) ? (uint16_t)l : djgpp_get_lpt_base_port(l);
  if(lpt_left_port == 0)
    return false;

  if(is_dual_port)
  {
    lpt_right_port = (r >= 0xa) ? (uint16_t)r : djgpp_get_lpt_base_port(r);
    if(lpt_right_port == 0 || lpt_left_port == lpt_right_port)
      return false;
  }
  else
    lpt_right_port = 0;

  return true;
}

static boolean init_audio_driver_lpt(unsigned mix_rate, unsigned irq_rate,
 unsigned frames, unsigned channels, const int *irq_handler)
{
  if(!audio_mixer_init(mix_rate, frames, channels))
    return false;
  /* Shouldn't happen */
  if(audio.output_frequency != mix_rate || audio.buffer_channels != channels)
    return false;

  if(!lpt_init_buffer(audio.buffer_frames, channels))
    return false;

  if(!lpt_init_callback())
  {
    lpt_free_buffer();
    return false;
  }

  if(!djgpp_set_irq0_handler(irq_rate, irq_handler))
  {
    lpt_quit_callback();
    lpt_free_buffer();
    return false;
  }

  info("LPT audio initialized: %xh/%xh %uFr %uCh %zuHz (%uHz)", lpt_left_port,
   lpt_right_port, audio.buffer_frames, channels, audio.output_frequency, irq_rate);
  return true;
}

static boolean init_audio_driver_lpt_mono(struct config_info *conf)
{
  unsigned rate = lpt_get_rate(conf->audio_sample_rate);
  unsigned frames = MIN(conf->audio_buffer_samples, 32768);

  if(!lpt_read_config(conf->audio_config_lpt, false))
    return false;

  return init_audio_driver_lpt(rate, rate, frames, 1, &lpt_mono_handler);
}

static boolean init_audio_driver_lpt_stereo1(struct config_info *conf)
{
  /* Note: DOSBox Staging claims a maximum rate of 30kHz, which seems wrong. */
  unsigned rate = lpt_get_rate(conf->audio_sample_rate);
  unsigned frames = MIN(conf->audio_buffer_samples, 16384);

  if(!lpt_read_config(conf->audio_config_lpt, false))
    return false;

  return init_audio_driver_lpt(rate, rate, frames, 2, &lpt_stereo1_handler);
}

static boolean init_audio_driver_lpt_stereo2(struct config_info *conf)
{
  unsigned rate = lpt_get_rate(conf->audio_sample_rate);
  unsigned frames = MIN(conf->audio_buffer_samples, 16384);

  if(!lpt_read_config(conf->audio_config_lpt, true))
    return false;

  return init_audio_driver_lpt(rate, rate, frames, 2, &lpt_stereo2_handler);
}

static boolean init_audio_driver_lpt_dss(struct config_info *conf)
{
  unsigned frames = MIN(conf->audio_buffer_samples, 32768);

  if(!lpt_read_config(conf->audio_config_lpt, false))
    return false;

  return init_audio_driver_lpt(LPT_DSS_HZ, TIMER_HZ, frames, 1, &lpt_dss_handler);
}

static void quit_audio_driver_lpt(void)
{
  djgpp_reset_irq0_handler();
  lpt_quit_callback();
  lpt_free_buffer();
}

const struct audio_driver audio_driver_lpt_mono =
{
  "LPT DAC (Mono)",
  "lptmono",
  false, /* Must be explicitly requested */

  NULL,
  &audio_player_pcs,
  NULL, /* &audio_player_pcs_pit */

  init_audio_driver_lpt_mono,
  quit_audio_driver_lpt,
};

const struct audio_driver audio_driver_lpt_stereo1 =
{
  "LPT DAC (Stereo-on-1)",
  "lptstereo1",
  false, /* Must be explicitly requested */

  NULL,
  &audio_player_pcs,
  NULL, /* &audio_player_pcs_pit */

  init_audio_driver_lpt_stereo1,
  quit_audio_driver_lpt,
};

const struct audio_driver audio_driver_lpt_stereo2 =
{
  "LPT DAC (Left DAC + Right DAC)",
  "lptstereo2",
  false, /* Must be explicitly requested */

  NULL,
  &audio_player_pcs,
  NULL, /* &audio_player_pcs_pit */

  init_audio_driver_lpt_stereo2,
  quit_audio_driver_lpt,
};

const struct audio_driver audio_driver_lpt_dss =
{
  "LPT DAC (DSS)",
  "lptdss",
  false, /* Must be explicitly requested */

  NULL,
  &audio_player_pcs,
  NULL, /* &audio_player_pcs_pit */

  init_audio_driver_lpt_dss,
  quit_audio_driver_lpt,
};
