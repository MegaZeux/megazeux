/* MegaZeux
 *
 * Copyright (C) 2010 Alan Williams <mralert@gmail.com>
 * Copyright (C) 2019 Adrian Siekierka <kontakt@asie.pl>
 * Copyright (C) 2024 Alice Rowan <petrifiedrowan@gmail.com>
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

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#define delay delay_dos
#include <bios.h>
#include <dpmi.h>
#include <go32.h>
#include <sys/nearptr.h>
#undef delay

#include "audio_sb.h"
#include "../audio.h"
#include "../audio_drivers.h"
#include "../audio_players.h"
#include "../audio_struct.h"
#include "../../util.h"
#include "../../platform/djgpp/platform_djgpp.h"

#define BUFFER_BLOCKS 2

struct sb_config
{
  // from environment variable, config, ...
  uint16_t port;
  uint8_t irq, dma8, dma16, type;
  // from DSP
  uint8_t version_major, version_minor;
  // from allocation
  int buffer_selector, buffer_segment;
  uint8_t *buffer;
  unsigned buffer_block;
  unsigned buffer_channels;
  unsigned buffer_format;
  unsigned buffer_frames;
  unsigned buffer_size;
  unsigned active_dma;
  unsigned active_dma_ack;
  unsigned active_dma_size;
  // driver-specific:
  // - to restore on deinit
  struct irq_state old_irq_state;
  _go32_dpmi_seginfo old_irq_handler;
  // - if nearptr enabled
  boolean nearptr_enabled;
  __dpmi_meminfo nearptr_mapping;
  int nearptr_type;
};

static struct sb_config sb_cfg;

static void audio_sb_clear_buffer(void)
{
  int zero = (sb_cfg.buffer_format == SAMPLE_U8) ? 0x80 : 0;
  memset(sb_cfg.buffer, zero, sb_cfg.buffer_size * BUFFER_BLOCKS);

  if(!sb_cfg.nearptr_enabled)
  {
    dosmemput(sb_cfg.buffer, sb_cfg.buffer_size * BUFFER_BLOCKS,
     sb_cfg.buffer_segment << 4);
  }
}

static void audio_sb_fill_block(void)
{
  size_t offset = sb_cfg.buffer_block * sb_cfg.buffer_size;

  audio_mixer_render_frames(sb_cfg.buffer + offset,
   sb_cfg.buffer_frames, sb_cfg.buffer_channels, sb_cfg.buffer_format);

  if(!sb_cfg.nearptr_enabled)
  {
    dosmemput(sb_cfg.buffer + offset, sb_cfg.buffer_size,
     (sb_cfg.buffer_segment << 4) + offset);
  }
}

static void audio_sb_next_block(void)
{
  audio_sb_fill_block();
  sb_cfg.buffer_block = (sb_cfg.buffer_block + 1) % BUFFER_BLOCKS;
}

static void audio_sb_interrupt(void)
{
  uint8_t fpustate[108];
  djgpp_save_x87(fpustate);

  audio_sb_next_block();
  inportb(sb_cfg.port + sb_cfg.active_dma_ack); // ack (sb)
  djgpp_irq_ack(sb_cfg.irq); // ack (pic)

  djgpp_restore_x87(fpustate);
}

static void audio_sb_parse_env(struct sb_config *conf, char *env)
{
  char *token;
  while(env != NULL)
  {
    token = strsep(&env, " ");
    switch(token[0])
    {
      case 'A':
        conf->port = strtol(token + 1, NULL, 16);
        break;
      case 'D':
        conf->dma8 = strtol(token + 1, NULL, 10);
        break;
      case 'H':
        conf->dma16 = strtol(token + 1, NULL, 10);
        break;
      case 'I':
        conf->irq = strtol(token + 1, NULL, 10);
        break;
      case 'T':
        conf->type = strtol(token + 1, NULL, 10);
        break;
    }
  }
}

static uint8_t audio_sb_dsp_read(void)
{
  int i;
  for(i = 0; i < 8192; i++)
    if(inportb(sb_cfg.port + 0xE) & 0x80)
      break;

  return inportb(sb_cfg.port + 0xA);
}

static void audio_sb_dsp_write(uint8_t val)
{
  int i;
  for(i = 0; i < 8192; i++)
    if(!(inportb(sb_cfg.port + 0xC) & 0x80))
      break;

  outportb(sb_cfg.port + 0xC, val);
}

static boolean audio_sb_dsp_reset(void)
{
  uint16_t i;
  outportb(sb_cfg.port + 0x6, 1);
  usleep(3);
  outportb(sb_cfg.port + 0x6, 0);
  for(i = 65535; i > 0; i--)
    if(inportb(sb_cfg.port + 0xE) & 0x80)
      break;
  if(i == 0)
    return false;
  for(i = 65535; i > 0; i--)
    if(inportb(sb_cfg.port + 0xA) == 0xAA)
      break;
  return i > 0;
}

static void audio_sb_mixer_set_stereo(boolean enable)
{
  int tmp;
  if(sb_cfg.version_major != 3)
    return;

  tmp = inportb(sb_cfg.port + 0x5);
  tmp = enable ? (tmp | SBPRO_MIXER_STEREO) : (tmp & ~SBPRO_MIXER_STEREO);

  outportb(sb_cfg.port + 0x4, 0xe);
  outportb(sb_cfg.port + 0x5, tmp);
}

static boolean init_audio_driver_sb(struct config_info *conf)
{
  _go32_dpmi_seginfo new_irq_handler;

  // Try to find a Sound Blaster
  // TODO: manual configuration
  char *sb_env = getenv("BLASTER");
  if(sb_env != NULL)
    audio_sb_parse_env(&sb_cfg, sb_env);

  if(sb_cfg.port == 0)
    goto err;

  if(!audio_sb_dsp_reset())
    goto err;

  audio_sb_dsp_write(SB_DSP_GET_VERSION);
  sb_cfg.version_major = audio_sb_dsp_read();
  sb_cfg.version_minor = audio_sb_dsp_read();

  info("Sound Blaster DSP %d.%02d - using A%03x I%d D%d H%d\n",
   sb_cfg.version_major, sb_cfg.version_minor,
   sb_cfg.port, sb_cfg.irq, sb_cfg.dma8, sb_cfg.dma16);

  if(!SB_VERSION_ATLEAST(sb_cfg, SB_VERSION_SB1_REV))
    goto err;

  if(sb_cfg.irq >= 16)
    goto err;

  if(sb_cfg.dma8 >= 4) // TODO: support DMA >3?
    goto err;

  if(SB_VERSION_ATLEAST(sb_cfg, SB_VERSION_SB16) &&
   (sb_cfg.dma16 < 5 || sb_cfg.dma16 >= 8))
    goto err;

  {
    unsigned frames = conf->audio_buffer_samples;
    unsigned rate = conf->audio_sample_rate;
    unsigned sb16_format = 0;
    unsigned time_constant = 0;
    unsigned irq_vector;
    boolean is_16_bit = false;

    rate = CLAMP(rate, 5000, 44100);

    if(SB_VERSION_ATLEAST(sb_cfg, SB_VERSION_SB16))
    {
      sb_cfg.buffer_format = SAMPLE_S16;
      sb_cfg.buffer_channels = CLAMP(conf->audio_output_channels, 1, 2);

      if(sb_cfg.buffer_format == SAMPLE_S16)
        is_16_bit = true;
      if(sb_cfg.buffer_format != SAMPLE_U8)
        sb16_format |= SB16_FORMAT_SIGNED;
      if(sb_cfg.buffer_channels == 2)
        sb16_format |= SB16_FORMAT_STEREO;
    }
    else // pre-SB16
    {
      sb_cfg.buffer_format = SAMPLE_U8;
      sb_cfg.buffer_channels = 1;
      // Sound Blaster Pro and up support stereo.
      if(SB_VERSION_ATLEAST(sb_cfg, SB_VERSION_SBPRO1))
        sb_cfg.buffer_channels = CLAMP(conf->audio_output_channels, 1, 2);

      // Sound Blaster Pro stereo and all DSPs prior to 2.01 have a
      // maximum rate of around 22050.
      if(sb_cfg.buffer_channels == 2 || !SB_VERSION_ATLEAST(sb_cfg, SB_VERSION_SB2))
        rate = CLAMP(rate, 5000, 22050);

      time_constant = 256 - 1000000 / (sb_cfg.buffer_channels * rate);
      rate = 1000000 / (sb_cfg.buffer_channels * (256 - time_constant));
    }

    if(!audio_mixer_init(rate, frames, sb_cfg.buffer_channels))
      goto err;

    sb_cfg.buffer_frames = audio.buffer_frames;
    sb_cfg.buffer_size = sb_cfg.buffer_frames * sb_cfg.buffer_channels *
     (is_16_bit ? sizeof(int16_t) : sizeof(uint8_t));

    sb_cfg.buffer_segment = djgpp_malloc_boundary(
     sb_cfg.buffer_size * BUFFER_BLOCKS, DMA_BOUNDARY, &sb_cfg.buffer_selector);
    if(sb_cfg.buffer_segment < 0)
      goto err;

    sb_cfg.buffer = (uint8_t *)djgpp_map_physical_memory(
     sb_cfg.buffer_segment << 4, sb_cfg.buffer_size,
     &sb_cfg.nearptr_mapping, &sb_cfg.nearptr_type);

    sb_cfg.nearptr_enabled = (sb_cfg.buffer != NULL);
    if(!sb_cfg.nearptr_enabled)
    {
      sb_cfg.buffer = (uint8_t *)cmalloc(sb_cfg.buffer_size * BUFFER_BLOCKS);
      if(!sb_cfg.buffer)
        goto err2;
    }

    audio_sb_clear_buffer();

    // lock C irq handler
    // (TODO: rewrite handler in ASM?)
    _go32_dpmi_lock_code(audio_sb_interrupt, 1024);
    _go32_dpmi_lock_data(&sb_cfg, sizeof(struct sb_config));
    sb_cfg.buffer_block = 0;

    // configure irq
    irq_vector = djgpp_irq_vector(sb_cfg.irq);
    _go32_dpmi_get_protected_mode_interrupt_vector(irq_vector, &sb_cfg.old_irq_handler);
    new_irq_handler.pm_offset = (int) audio_sb_interrupt;
    new_irq_handler.pm_selector = _go32_my_cs();
    _go32_dpmi_chain_protected_mode_interrupt_vector(irq_vector, &new_irq_handler);

    djgpp_irq_enable(sb_cfg.irq, &sb_cfg.old_irq_state);

    // configure dma
    if(is_16_bit)
    {
      sb_cfg.active_dma = sb_cfg.dma16;
      sb_cfg.active_dma_ack = SB_PORT_DMA16_ACK;
    }
    else
    {
      sb_cfg.active_dma = sb_cfg.dma8;
      sb_cfg.active_dma_ack = SB_PORT_DMA8_ACK;
    }
    sb_cfg.active_dma_size = (sb_cfg.buffer_frames * sb_cfg.buffer_channels) - 1;

    djgpp_enable_dma(sb_cfg.active_dma, DMA_WRITE | DMA_AUTOINIT,
     sb_cfg.buffer_segment << 4, sb_cfg.buffer_size * BUFFER_BLOCKS);

    audio_sb_dsp_write(SB_DSP_SPEAKER_ON);

    // configure dsp
    if(SB_VERSION_ATLEAST(sb_cfg, SB_VERSION_SB16))
    {
      audio_sb_dsp_write(SB_DSP_SET_SAMPLE_RATE);
      audio_sb_dsp_write(rate >> 8);
      audio_sb_dsp_write(rate & 0xFF);

      // configure transfer
      audio_sb_dsp_write(is_16_bit ? SB16_DSP_AUTOINIT_16_BIT : SB16_DSP_AUTOINIT_8_BIT);
      audio_sb_dsp_write(sb16_format); // mono/stereo, signed/unsigned
      audio_sb_dsp_write(sb_cfg.active_dma_size);
      audio_sb_dsp_write(sb_cfg.active_dma_size >> 8);
    }
    else

    if(SB_VERSION_ATLEAST(sb_cfg, SB_VERSION_SB1_REV))
    {
      audio_sb_mixer_set_stereo(sb_cfg.buffer_channels == 2);

      audio_sb_dsp_write(SB_DSP_SET_TIME_CONSTANT);
      audio_sb_dsp_write(time_constant);

      audio_sb_dsp_write(SB_DSP_SET_DMA_BLOCK_SIZE);
      audio_sb_dsp_write(sb_cfg.active_dma_size);
      audio_sb_dsp_write(sb_cfg.active_dma_size >> 8);

      // start transfer
      if(SB_VERSION_ATLEAST(sb_cfg, SB_VERSION_SB2))
        audio_sb_dsp_write(SB_DSP_AUTOINIT_8_BIT_HI);
      else
        audio_sb_dsp_write(SB_DSP_AUTOINIT_8_BIT);
    }
#if 0
    else
    {
      audio_sb_dsp_write(SB_DSP_SET_TIME_CONSTANT);
      audio_sb_dsp_write(time_constant);

      audio_sb_dsp_write(SB_DSP_SINGLE_8_BIT); // single block transfer
      audio_sb_dsp_write(sb_cfg.active_dma_size);
      audio_sb_dsp_write(sb_cfg.active_dma_size >> 8);
    }
#endif
  }
  return true;

err2:
  __dpmi_free_dos_memory(sb_cfg.buffer_selector);
err:
  sb_cfg.port = 0;
  return false;
}

static void quit_audio_driver_sb(void)
{
  // Deinitialize audio
  if(sb_cfg.port != 0)
  {
    unsigned irq_vector = djgpp_irq_vector(sb_cfg.irq);

    audio_sb_dsp_write(SB_DSP_SPEAKER_OFF);

    if(SB_VERSION_ATLEAST(sb_cfg, SB_VERSION_SB1_REV))
    {
      // Stop autoinit DMA.
      // This is usually done in the interrupt with a nonblocking write.
      if(sb_cfg.active_dma == sb_cfg.dma16)
        audio_sb_dsp_write(SB_DSP_EXIT_AUTO_16_BIT);
      else
        audio_sb_dsp_write(SB_DSP_EXIT_AUTO_8_BIT);

      // Disable stereo (SBPro only).
      audio_sb_mixer_set_stereo(false);
    }

    // undo interrupt change
    djgpp_irq_restore(&sb_cfg.old_irq_state);

    // undo vector change
    _go32_dpmi_set_protected_mode_interrupt_vector(irq_vector, &sb_cfg.old_irq_handler);

    djgpp_disable_dma(sb_cfg.active_dma);

    if(sb_cfg.nearptr_enabled)
      djgpp_unmap_physical_memory(&sb_cfg.nearptr_mapping, &sb_cfg.nearptr_type);
    else
      free(sb_cfg.buffer);

    __dpmi_free_dos_memory(sb_cfg.buffer_selector);
  }
}

const struct audio_driver audio_driver_sb =
{
  "Sound Blaster 16, Pro 2, Pro, 2, or DSP 2.0",
  "sb",

  NULL,
  &audio_player_pcs,
  NULL, /* &audio_player_pcs_pit */

  init_audio_driver_sb,
  quit_audio_driver_sb,
};
