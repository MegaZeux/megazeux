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

#ifndef MEGAZEUX_EDITOR_GRAPHICS_H
#define MEGAZEUX_EDITOR_GRAPHICS_H

#include "../compat.h"

MEGAZEUX_BEGIN_DECLS

#include "../graphics.h"

void store_backup_palette(char dest[SMZX_PAL_SIZE * 3]);
void load_backup_palette(const char src[SMZX_PAL_SIZE * 3]);
void store_backup_indices(char dest[SMZX_PAL_SIZE * 4]);
void load_backup_indices(const char src[SMZX_PAL_SIZE * 4]);

void save_palette(char *fname);
void save_index_file(char *fname);

void ec_save_set_var(const char *name, uint16_t first_chr, unsigned int num);
void ec_change_block(uint8_t offset, uint8_t charset,
 uint8_t width, uint8_t height, const char *matrix);
void ec_read_block(uint8_t offset, uint8_t charset,
 uint8_t width, uint8_t height, char *matrix);
void load_editor_charsets(void);
void ec_load_smzx(void);
void ec_load_smzx2(void);
void ec_load_blank(void);
void ec_load_ascii(void);
void ec_load_char_ascii(uint16_t char_number);
void ec_load_char_mzx(uint16_t char_number);
unsigned int compare_char(uint16_t chr_a, uint16_t chr_b);

MEGAZEUX_END_DECLS

#endif /* MEGAZEUX_EDITOR_GRAPHICS_H */
