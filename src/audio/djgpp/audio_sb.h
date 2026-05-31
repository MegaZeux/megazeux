/* MegaZeux
 *
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

#ifndef __AUDIO_SB_H
#define __AUDIO_SB_H

#define SB_VERSION_ATLEAST(cfg, ver) \
 (((cfg.version_major << 8) | (cfg.version_minor)) >= (ver))

#define SB_VERSION_SB1            0x100
#define SB_VERSION_SB1_REV        0x200 /* SB 1.x with DSP 2.00 */
#define SB_VERSION_SB2            0x201
#define SB_VERSION_SBPRO1         0x300
#define SB_VERSION_SBPRO2         0x302
#define SB_VERSION_SB16           0x400

#define SB_PORT_DMA8_ACK          0xe
#define SB_PORT_DMA16_ACK         0xf

#define SBPRO_MIXER_STEREO        0x02
#define SBPRO_MIXER_FILTER        0x20

#define SB_DSP_SET_TIME_CONSTANT  0x40
#define SB_DSP_SET_SAMPLE_RATE    0x41
#define SB_DSP_SET_DMA_BLOCK_SIZE 0x48
#define SB_DSP_GET_VERSION        0xE1

#define SB_DSP_SINGLE_8_BIT       0x14
#define SB_DSP_AUTOINIT_8_BIT     0x1c
#define SB_DSP_AUTOINIT_8_BIT_HI  0x90
#define SB_DSP_PAUSE_8_BIT        0xD0
#define SB_DSP_PAUSE_16_BIT       0xD5
#define SB_DSP_CONTINUE_8_BIT     0xD4
#define SB_DSP_CONTINUE_16_BIT    0xD6
#define SB_DSP_EXIT_AUTO_8_BIT    0xDA
#define SB_DSP_EXIT_AUTO_16_BIT   0xD9
#define SB_DSP_SPEAKER_ON         0xD1
#define SB_DSP_SPEAKER_OFF        0xD3

#define SB16_DSP_AUTOINIT_16_BIT  0xB6
#define SB16_DSP_AUTOINIT_8_BIT   0xC6
#define SB16_FORMAT_SIGNED        0x10
#define SB16_FORMAT_STEREO        0x20

#endif /* __AUDIO_SB_H */
