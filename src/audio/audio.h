/* MegaZeux
 *
 * Copyright (C) 2004 Gilead Kutnick <exophase@adelphia.net>
 * Copyright (C) 2007 Alistair John Strachan <alistair@devzero.co.uk>
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

/* Audio subsystem externally visible functions. */

#ifndef MEGAZEUX_AUDIO_H
#define MEGAZEUX_AUDIO_H

#include "../compat.h"

MEGAZEUX_BEGIN_DECLS

#ifdef CONFIG_AUDIO

#include <stdint.h>

// Default period for .SAM files.
#define SAM_DEFAULT_PERIOD 428

struct config_info;
struct audio_stream;
struct audio_stream_spec;

CORE_LIBSPEC void init_audio(struct config_info *conf);
CORE_LIBSPEC void quit_audio(void);
CORE_LIBSPEC int audio_play_module(char *filename, boolean safely, int volume);
CORE_LIBSPEC void audio_end_module(void);
CORE_LIBSPEC void audio_play_sample(char *filename, boolean safely, int period);
CORE_LIBSPEC void audio_spot_sample(int period, int which);

CORE_LIBSPEC void audio_set_module_volume(int volume);
void audio_set_module_order(int order);
int audio_get_module_order(void);
void audio_set_module_position(int pos);
int audio_get_module_position(void);
void audio_set_module_frequency(int freq);
int audio_get_module_frequency(void);
int audio_get_module_length(void);
void audio_set_module_loop_start(int pos);
int audio_get_module_loop_start(void);
void audio_set_module_loop_end(int pos);
int audio_get_module_loop_end(void);

void audio_end_sample(void);
int audio_get_max_samples(void);
void audio_set_max_samples(int max_samples);

int audio_get_music_on(void);
int audio_get_pcs_on(void);
int audio_get_music_volume(void);
int audio_get_sound_volume(void);
int audio_get_pcs_volume(void);
void audio_set_music_on(int val);
void audio_set_pcs_on(int val);
void audio_set_music_volume(int volume);
void audio_set_sound_volume(int volume);
void audio_set_pcs_volume(int volume);

int audio_legacy_translate(const char *path, char *newpath, size_t buffer_len);

// Internal functions
int audio_get_real_frequency(int period);
void destruct_audio_stream(struct audio_stream *a_src);
void initialize_audio_stream(struct audio_stream *a_src,
 struct audio_stream_spec *a_spec, unsigned int volume, boolean repeat);

size_t audio_mixer_render_frames(void *stream, unsigned frames,
 unsigned channels, unsigned format);
boolean audio_mixer_init(unsigned rate, unsigned frames, unsigned channels);
void audio_mixer_free(void);

// Platform-related functions.
void init_audio_platform(struct config_info *conf);
void quit_audio_platform(void);

#else // !CONFIG_AUDIO

static inline void init_audio(struct config_info *conf) {}
static inline void quit_audio(void) {}
static inline int audio_play_module(char *filename, boolean safely, int volume)
 { return 1; }
static inline void audio_end_module(void) {}
static inline void audio_play_sample(char *filename, boolean safely, int period)
 {}
static inline void audio_spot_sample(int period, int which) {}

static inline void audio_set_module_volume(int vol) {}
static inline void audio_set_module_order(int order) {}
static inline int audio_get_module_order(void) { return 0; }
static inline void audio_set_module_position(int pos) {}
static inline int audio_get_module_position(void) { return 0; }
static inline void audio_set_module_frequency(int freq) {}
static inline int audio_get_module_frequency(void) { return 0; }
static inline int audio_get_module_length(void) { return 0; }
static inline void audio_set_module_loop_start(int pos) {}
static inline int audio_get_module_loop_start(void) { return 0; }
static inline void audio_set_module_loop_end(int pos) {}
static inline int audio_get_module_loop_end(void) { return 0; }

static inline void audio_end_sample(void) {}
static inline void audio_set_max_samples(int max_samples) {}
static inline int audio_get_max_samples(void) { return 0; }

static inline int audio_get_music_on(void) { return 0; }
static inline int audio_get_pcs_on(void) { return 0; }
static inline int audio_get_music_volume(void) { return 0; }
static inline int audio_get_sound_volume(void) { return 0; }
static inline int audio_get_pcs_volume(void) { return 0; }
static inline void audio_set_music_on(int val) {}
static inline void audio_set_pcs_on(int val) {}
static inline void audio_set_music_volume(int volume) {}
static inline void audio_set_sound_volume(int volume) {}
static inline void audio_set_pcs_volume(int volume) {}

static inline int audio_legacy_translate(const char *path,
 char *newpath, size_t buffer_len) { return -1; }

#endif // CONFIG_AUDIO

MEGAZEUX_END_DECLS

#endif /* MEGAZEUX_AUDIO_H */
