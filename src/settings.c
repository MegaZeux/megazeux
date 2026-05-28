/* MegaZeux
 *
 * Copyright (C) 1996 Alexis Janson
 * Copyright (C) 1999 Charles Goetzman
 * Copyright (C) 2004 Gilead Kutnick <exophase@adelphia.net>
 * Copyright (C) 2018-2024 Alice Rowan <petrifiedrowan@gmail.com>
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

#include "configure.h"
#include "core.h"
#include "error.h"
#include "game.h"
#include "graphics.h"
#include "settings.h"
#include "util.h"
#include "window.h"
#include "world_struct.h"

#include "audio/audio.h"
#include "event/event.h"

#include <string.h>

// Settings dialog-

//----------------------------
//
//    Speed- [   NN][^][v]
//
//   ( ) Digitized music on
//   ( ) Digitized music off
//
//   ( ) PC speaker SFX on
//   ( ) PC speaker SFX off
//
//  Sound card volumes-       //
//  Overall volume- 12345678  //
//  SoundFX volume- 12345678  //
//  PC Speaker SFX- 12345678  //
//
//      [OK]     [Cancel]
//
//----------------------------//

//  [OK]  [Cancel]  [Shader]  //

enum
{
  RESULT_CANCEL,
  RESULT_CONFIRM,
  RESULT_KEEP_OPEN,
  RESULT_SET_VIDEO_OUTPUT,
  RESULT_SET_SHADER
};

#ifdef CONFIG_RENDER_GL_PROGRAM
// Note- we search for .frags, then load the matching .vert too if present.
static const char *shader_exts[] = { ".frag", NULL };
static char shader_default_text[20];
static char shader_path[MAX_PATH];
static char shader_name[MAX_PATH];
static boolean is_glsl = false;
#endif

static int choose_video_output(struct world *mzx_world, const char **vo,
 int num_vo, int current_vo)
{
  int retval;
  struct dialog di;
  struct element *elements[] =
  {
    construct_list_box(2, 1, vo, num_vo, 12, 20, 0, &current_vo, NULL, false)
  };

  construct_dialog(&di, "Choose renderer...", 28, 5, 24, 14,
   elements, ARRAY_SIZE(elements), 0);

  retval = run_dialog(mzx_world, &di);
  destruct_dialog(&di);

  if(!retval)
    return current_vo;
  else
    return -1;
}

void game_settings(struct world *mzx_world)
{
  struct board *src_board = mzx_world->current_board;
  int mzx_speed, music, pcs;
  int prev_music;
  int music_volume, sound_volume, pcs_volume;
  struct dialog di;
  int dialog_result;
  int y_offset;
  int y_offset_buttons;
  int window_height;
  int num_elements;
  int start_option = -1;

  int ok_pos;

  const char *radio_strings_1[2] =
  {
    "Music/sample SFX on", "Music/sample SFX off"
  };
  const char *radio_strings_2[2] =
  {
    "PC speaker SFX on", "PC speaker SFX off"
  };
  struct element *elements[11];

  struct config_info *conf = get_config();

  const char *available_video_outputs[32];
  int num_available_video_outputs = 0;
  int cur_video_output = get_current_video_output();

  num_available_video_outputs =
   get_available_video_output_list(available_video_outputs,
    ARRAY_SIZE(available_video_outputs));

  mzx_speed = mzx_world->mzx_speed;
  music = !audio_get_music_on();
  pcs = !audio_get_pcs_on();
  music_volume = audio_get_music_volume();
  sound_volume = audio_get_sound_volume();
  pcs_volume = audio_get_pcs_volume();
  prev_music = !music;

  // Prevent previous keys from carrying through.
  force_release_all_keys();

  set_context(CTX_CONFIGURE);

  do
  {
    ok_pos = 6;
    num_elements = 8;
    y_offset = 0;
    y_offset_buttons = 0;

    if(!mzx_world->lock_speed)
    {
      y_offset += 2;
      y_offset_buttons += 2;
      num_elements++;
    }

#if !defined(CONFIG_WII) && !defined(CONFIG_SWITCH) && \
 !defined(__EMSCRIPTEN__) && !defined(CONFIG_PSVITA)
    // Emscripten's SDL port crashes on re-entry attempts.
    // Wii has multiple renderers but shouldn't display this option.
    // FIXME this is a hack. Fix the Wii renderers so they're switchable.
    // FIXME: Switch has undiagnosed crash bugs related to renderer switching.
    if(num_available_video_outputs > 1)
    {
      elements[ok_pos] = construct_button(4, 13 + y_offset_buttons,
       " Select renderer... ", RESULT_SET_VIDEO_OUTPUT);

      ok_pos++;
      num_elements++;
      y_offset_buttons += 2;
    }
#endif

#ifdef CONFIG_RENDER_GL_PROGRAM
    {
      const char *cur_output_name = conf->video_output;

      if(num_available_video_outputs)
        cur_output_name = available_video_outputs[cur_video_output];

      is_glsl = strstr(cur_output_name, "glsl") ? true : false;
    }

    if(is_glsl)
    {
      if(!shader_default_text[0])
      {
        if(conf->gl_scaling_shader[0])
          snprintf(shader_default_text, 20, "<conf: %.11s>",
           conf->gl_scaling_shader);

        else
          strcpy(shader_default_text, "<default: semisoft>");
      }
      strcpy(shader_path, mzx_res_get_by_id(GLSL_SHADER_SCALER_DIRECTORY));

      elements[ok_pos] = construct_file_selector(3, 13 + y_offset_buttons,
       "Scaling shader-", "Choose a scaling shader...", shader_exts,
       shader_default_text, 21, 0, shader_path, shader_name, RESULT_SET_SHADER);

      ok_pos++;
      num_elements++;
      y_offset_buttons += 3;
    }
#endif

    if(!mzx_world->lock_speed)
    {
      elements[ok_pos + 2] = construct_number_box(2, 2, "Speed- ", 1, 16,
       NUMBER_SLIDER, &mzx_speed);

      // Start at the Speed setting if enabled.
      if(start_option < 0)
        start_option = ok_pos + 2;
    }

    // Start at the music/samples toggle by default.
    if(start_option < 0)
      start_option = 0;

    elements[0] = construct_radio_button(4, 2 + y_offset,
     radio_strings_1, 2, 20, &music);
    elements[1] = construct_radio_button(4, 5 + y_offset,
     radio_strings_2, 2, 18, &pcs);
    elements[2] = construct_label(2, 8 + y_offset,
     "Audio volumes-");
    elements[3] = construct_number_box(7, 9 + y_offset,
     "Music- ", 0, 10, NUMBER_SLIDER, &music_volume);
    elements[4] = construct_number_box(5, 10 + y_offset,
     "Samples- ", 0, 10, NUMBER_SLIDER, &sound_volume);
    elements[5] = construct_number_box(2, 11 + y_offset,
     "PC Speaker- ", 0, 10, NUMBER_SLIDER, &pcs_volume);

    elements[ok_pos] = construct_button(7, 13 + y_offset_buttons,
     "OK", RESULT_CONFIRM);

    elements[ok_pos + 1] = construct_button(15, 13 + y_offset_buttons,
     "Cancel", RESULT_CANCEL);

    window_height = (16 + y_offset_buttons);
    construct_dialog(&di, "Game Settings", 25, (25 - window_height) / 2,
     30, window_height, elements, num_elements, start_option);

    dialog_result = run_dialog(mzx_world, &di);
    start_option = di.current_element;

    // Prevent UI keys from carrying through.
    force_release_all_keys();

    if(dialog_result == RESULT_CONFIRM)
    {
      audio_set_pcs_volume(pcs_volume);

      if(music_volume != audio_get_music_volume())
      {
        audio_set_music_volume(music_volume);

        if(mzx_world->active)
          audio_set_module_volume(src_board->volume);
      }

      if(sound_volume != audio_get_sound_volume())
        audio_set_sound_volume(sound_volume);

      music ^= 1;
      pcs ^= 1;

      audio_set_pcs_on(pcs);
      audio_set_music_on(music);

      if(!music && prev_music && mzx_world->active)
      {
        // Turn off music and sound effects.
        audio_end_module();
        audio_end_sample();
      }
      else

      if(music && !prev_music && mzx_world->active)
      {
        // Turn on music.
        load_game_module(mzx_world, mzx_world->real_mod_playing, false);
      }

      mzx_world->mzx_speed = mzx_speed;
    }
    else

    if(dialog_result == RESULT_SET_VIDEO_OUTPUT)
    {
      int new_video_output = choose_video_output(mzx_world,
       available_video_outputs, num_available_video_outputs, cur_video_output);

      if(new_video_output >= 0)
      {
        if(!change_video_output(conf, available_video_outputs[new_video_output]))
          error("Failed to set renderer.", ERROR_T_ERROR, ERROR_OPT_OK, 0x6000);

        cur_video_output = get_current_video_output();

        // HACK: after creating a new X11 window SDL seems to like to send the
        // previous frame's events again sometimes, so just flush the events
        update_event_status();
      }
    }

#ifdef CONFIG_RENDER_GL_PROGRAM
    else

    // Shader selection
    if(is_glsl && (dialog_result == RESULT_SET_SHADER) && shader_name[0])
    {
      size_t offset = strlen(shader_path) + 1;
      boolean shader_res;
      char *pos;

      if(strlen(shader_name) > offset)
      {
        pos = strrchr(shader_name + offset, '.');

        if(pos) *pos = 0;
        shader_res = switch_shader(shader_name + offset);
        if(pos) *pos = '.';

        if(!shader_res)
        {
          // If the shader failed to load, the default should have been loaded.
          strcpy(shader_default_text, "<default: semisoft>");
          shader_name[0] = 0;
        }
      }
    }
#endif
    destruct_dialog(&di);
  }
  while(dialog_result >= RESULT_KEEP_OPEN);

  pop_context();
}
