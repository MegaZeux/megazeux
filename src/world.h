/* MegaZeux
 *
 * Copyright (C) 2004 Gilead Kutnick <exophase@adelphia.net>
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

#ifndef MEGAZEUX_WORLD_H
#define MEGAZEUX_WORLD_H

#include "compat.h"

MEGAZEUX_BEGIN_DECLS

#include "world_struct.h"
#include "io/vfile.h"

/* When making new versions, change the number below, and
 * change the number in the version strings to follow. From now on,
 * be sure that the last two characters are the hex equivalent of the
 * MINOR version number.
 */

/**
 * READ THIS -- MAGIC INFO
 *
 * World files:
 *
 *  MZX - Ver 1.x MegaZeux                  (mzx_world->version == 0x0100)
 *  MZ2 - Ver 2.x MegaZeux                  (mzx_world->version == 0x0205)
 *  MZA - Ver 2.51S1 Megazeux               (mzx_world->version == 0x0208)
 *  M\x02\x09 - 2.5.1spider2+
 *  M\x02\x11 - Used for decrypted worlds.  (spider octal "\011" misread as hex)
 *  M\x02\x32 - MZX 2.62 and MZX 2.62b      (from octal "\062")
 *  M\x02\x41 - MZX 2.65                    (from decimal "65")
 *  M\x02\x44 - MZX 2.68                    (from decimal "68")
 *  M\x02\x45 - MZX 2.69                    (from decimal "69")
 *  M\x02\x46 - MZX 2.69b
 *  M\x02\x48 - MZX 2.69c
 *  M\x02\x49 - MZX 2.70
 *  M\x02\x50 - MZX 2.80 (non-DOS)          (from decimal "80")
 *  M\x02\x51 - MZX 2.81                    (and so on...)
 *  M\x02\x52 - MZX 2.82
 *  M\x02\x53 - MZX 2.83
 *  M\x02\x54 - MZX 2.84
 *  M\x02\x5A - MZX 2.90
 *  M\x02\x5B - MZX 2.91
 *  M\x02\x5C - MZX 2.92
 *  M\x02\x5D - MZX 2.93
 *
 * Save files:
 *
 *  MZSV2 - Ver 2.x MegaZeux                (mzx_world->version == 0x0205)
 *  MZXSA - Ver 2.51S1 MegaZeux             (mzx_world->version == 0x0208)
 *  MZS\x02\x09 - 2.5.1spider2+
 *  MZS\x02\x32 - MZX 2.62 and MZX 2.62b
 *  MZS\x02\x41 - MZX 2.65
 *  MZS\x02\x44 - MZX 2.68
 *  MZS\x02\x45 - MZX 2.69
 *  MZS\x02\x46 - MZX 2.69b
 *  MZS\x02\x48 - MZX 2.69c
 *  MZS\x02\x49 - MZX 2.70
 *  MZS\x02\x50 - MZX 2.80 (non-DOS)
 *  MZS\x02\x51 - MZX 2.81
 *  MZS\x02\x52 - MZX 2.82
 *  MZS\x02\x53 - MZX 2.83
 *  MZS\x02\x54 - MZX 2.84
 *  MZS\x02\x5A - MZX 2.90
 *  MZS\x02\x5B - MZX 2.91
 *  MZS\x02\x5C - MZX 2.92
 *  MZS\x02\x5D - MZX 2.93
 *
 * Board files follow a similar pattern to world files. Versions prior to
 * 2.51S1 are "MB2". For versions greater than 2.51S1, they match the
 * world file magic. For all boards, the first byte is always 0xFF.
 *
 * As of 2.80+, all letters after the name denote bug fixes/minor additions
 * and may NOT have different save formats. If a save format change is
 * necessitated, a numerical change must be enacted.
 */

enum mzx_version
{
  V100            = 0x0100, // Magic: MZX
  V200            = 0x0205, // Magic: MZ2
  V251            = 0x0205,
  V251s1          = 0x0208, // Magic: MZA
  V251s2          = 0x0209,
  V251s3          = 0x0209,
  V260            = 0x0209,
  V261            = 0x0209,
  VERSION_DECRYPT = 0x0211, // Special version used for decrypted worlds only.
  V262            = 0x0232,
  V265            = 0x0241, // NOTE: 2.68 uses 0x241 for worlds and 0x244 for
  V268            = 0x0244, // saves. Leave any 2.68 version checks at V268.
  V269            = 0x0245,
  V269b           = 0x0246,
  V269c           = 0x0248,
  V270            = 0x0249,
  VERSION_PORT    = 0x0250, // For checks explicitly differentiating DOS and port
  V280            = 0x0250,
  V281            = 0x0251,
  V282            = 0x0252,
  V283            = 0x0253,
  V284            = 0x0254,
  V290            = 0x025A,
  V291            = 0x025B,
  V292            = 0x025C,
  V293            = 0x025D,
  V294            = 0x025E,
#ifdef CONFIG_DEBYTECODE
  VERSION_SOURCE  = 0x0300, // For checks dependent on Robotic source changes
  V300            = 0x0300,
#endif
};

/* The magic stamp for worlds, boards or SAV files created with this version
 * of MegaZeux. If you introduce a counter or any other incompatible change,
 * such as altering semantics or actually changing the binary format, this
 * value MUST be bumped.
 */
#define MZX_VERSION      (V293)

/* The world version that worlds will be saved as when Export Downver. World
 * is used from the editor. This function is also fulfilled by the downver util.
 *
 * If you increase MZX_VERSION, always make sure this is updated with its
 * previous value; this way, users can always downgrade their work to an
 * older version (if it at all makes sense to do so).
 */
#define MZX_VERSION_PREV (V292)

// This is the last version of MegaZeux to use the legacy world format.
#define MZX_LEGACY_FORMAT_VERSION (V284)

// FIXME: hack
#ifdef CONFIG_DEBYTECODE
#undef  MZX_VERSION_PREV
#define MZX_VERSION_PREV (V293)
#undef  MZX_VERSION
#define MZX_VERSION      (VERSION_SOURCE)
#endif

enum val_result
{
  VAL_SUCCESS,
  VAL_VERSION,    // Version issue
  VAL_INVALID,    // Failed validation
  VAL_TRUNCATED,  // Passed validation until it hit EOF
  VAL_MISSING,    // file or ptr location in file does not exist
  VAL_PROTECTED,  // Legacy file is protected, needs decryption
  VAL_ABORTED,    // Load aborted by user
};

CORE_LIBSPEC int world_magic(const char magic_string[3]);
CORE_LIBSPEC int save_magic(const char magic_string[5]);
CORE_LIBSPEC int get_version_string(char buffer[16], enum mzx_version version);

CORE_LIBSPEC int save_world(struct world *mzx_world, const char *file,
 boolean savegame, int file_version);
CORE_LIBSPEC boolean reload_world(struct world *mzx_world, const char *file,
 boolean *faded);
CORE_LIBSPEC void clear_world(struct world *mzx_world);
CORE_LIBSPEC void clear_global_data(struct world *mzx_world);
CORE_LIBSPEC void default_scroll_values(struct world *mzx_world);

CORE_LIBSPEC void change_board(struct world *mzx_world, int board_id);
CORE_LIBSPEC void change_board_set_values(struct world *mzx_world);
CORE_LIBSPEC void change_board_load_assets(struct world *mzx_world);

CORE_LIBSPEC void remap_vlayer(struct world *mzx_world,
 int new_width, int new_height);

boolean reload_savegame(struct world *mzx_world, const char *file,
 boolean *faded);
boolean reload_swap(struct world *mzx_world, const char *file, boolean *faded);

void save_counters_file(struct world *mzx_world, const char *file);
int load_counters_file(struct world *mzx_world, const char *file);

#ifdef CONFIG_LOADSAVE_METER
void meter_update_screen(int *curr, int target);
void meter_restore_screen(void);
void meter_initial_draw(int curr, int target, const char *title);
#endif

#ifdef CONFIG_EDITOR

#include <stdio.h>
struct zip_archive;

CORE_LIBSPEC void try_load_world(struct world *mzx_world,
 struct zip_archive **zp, vfile **vf, const char *file, boolean savegame,
 int *file_version, char *name);

CORE_LIBSPEC void default_vlayer(struct world *mzx_world);
CORE_LIBSPEC void default_global_data(struct world *mzx_world);

CORE_LIBSPEC void set_update_done(struct world *mzx_world);
CORE_LIBSPEC void refactor_board_list(struct world *mzx_world,
 struct board **new_board_list, int new_list_size,
 int *board_id_translation_list);
CORE_LIBSPEC void optimize_null_boards(struct world *mzx_world);

#endif // CONFIG_EDITOR

MEGAZEUX_END_DECLS

#endif /* MEGAZEUX_WORLD_H */
