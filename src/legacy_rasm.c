/* MegaZeux
 *
 * Copyright (C) 2004 Gilead Kutnick <exophase@adelphia.net>
 * Copyright (C) 2012-2025 Alice Rowan <petrifiedrowan@gmail.com>
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

#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "rasm.h"
#include "data.h"
#include "util.h"
#include "io/fsafeopen.h"
#include "io/memfile.h"
#include "io/vio.h"
#include "platform/log.h"

#define IMM_U16            (1 << 0)
#define IMM_S16            (1 << 0)
#define CHARACTER          (1 << 2)
#define COLOR              (1 << 3)
#define DIRECTION          (1 << 4)
#define THING              (1 << 5)
#define PARAM              (1 << 6)
#define STRING             (1 << 7)
#define EQUALITY           (1 << 8)
#define CONDITION          (1 << 9)
#define ITEM               (1 << 10)
#define EXTRA              (1 << 11)
#define UNDEFINED          (1 << 13)
#define EXTENDED           (1 << 14) /* Allow disassembly of char -> imm */

#define ERR_BADCHARACTER   2
#define ERR_INVALID        3

// 1 << 31 generates compiler warnings.

#define CMD                (1 << 30)
#define CMD_NOT            CMD | 0
#define CMD_ANY            CMD | 1
#define CMD_PLAYER         CMD | 2
#define CMD_NS             CMD | 3
#define CMD_EW             CMD | 4
#define CMD_ATTACK         CMD | 5
#define CMD_ITEM           CMD | 6
#define CMD_SELF           CMD | 7
#define CMD_RANDOM         CMD | 8
#define CMD_STRING         CMD | 9
#define CMD_CHAR           CMD | 10
#define CMD_ALL            CMD | 11
#define CMD_EDIT           CMD | 12
#define CMD_PUSHABLE       CMD | 13
#define CMD_NONPUSHABLE    CMD | 14
#define CMD_LAVAWALKER     CMD | 15
#define CMD_NONLAVAWALKER  CMD | 16
#define CMD_ROW            CMD | 17
#define CMD_COUNTERS       CMD | 18
#define CMD_ID             CMD | 19
#define CMD_MOD            CMD | 20
#define CMD_ORDER          CMD | 21
#define CMD_THICK          CMD | 22
#define CMD_ARROW          CMD | 23
#define CMD_THIN           CMD | 24
#define CMD_MAXHEALTH      CMD | 25
#define CMD_POSITION       CMD | 26
#define CMD_MESG           CMD | 27
#define CMD_COLUMN         CMD | 28
#define CMD_COLOR          CMD | 29
#define CMD_SIZE           CMD | 30
#define CMD_BULLETN        CMD | 31
#define CMD_BULLETS        CMD | 32
#define CMD_BULLETE        CMD | 33
#define CMD_BULLETW        CMD | 34
#define CMD_BULLETCOLOR    CMD | 35
#define CMD_FIRST          CMD | 36
#define CMD_LAST           CMD | 37
#define CMD_FADE           CMD | 38
#define CMD_OUT            CMD | 39
#define CMD_IN             CMD | 40
#define CMD_BLOCK          CMD | 41
#define CMD_SFX            CMD | 42
#define CMD_INTENSITY      CMD | 43
#define CMD_SET            CMD | 44
#define CMD_PALETTE        CMD | 45
#define CMD_WORLD          CMD | 46
#define CMD_ALIGNEDROBOT   CMD | 47
#define CMD_GO             CMD | 48
#define CMD_SAVING         CMD | 49
#define CMD_SENSORONLY     CMD | 50
#define CMD_ON             CMD | 51
#define CMD_STATIC         CMD | 52
#define CMD_TRANSPARENT    CMD | 53
#define CMD_OVERLAY        CMD | 54
#define CMD_START          CMD | 55
#define CMD_LOOP           CMD | 56
#define CMD_EDGE           CMD | 57
#define CMD_SAM            CMD | 58
#define CMD_PLAY           CMD | 59
#define CMD_PERCENT        CMD | 60
#define CMD_HIGH           CMD | 61
#define CMD_MATCHES        CMD | 62
#define CMD_NONE           CMD | 63
#define CMD_INPUT          CMD | 64
#define CMD_COUNTER        CMD | 66
#define CMD_DUPLICATE      CMD | 67
#define CMD_NO             CMD | 68

#define IGNORE_TYPE             (1 << 29)
#define IGNORE_TYPE_A           IGNORE_TYPE | 2
#define IGNORE_TYPE_AN          IGNORE_TYPE | 3
#define IGNORE_TYPE_AND         IGNORE_TYPE | 4
#define IGNORE_TYPE_AS          IGNORE_TYPE | 5
#define IGNORE_TYPE_AT          IGNORE_TYPE | 6
#define IGNORE_TYPE_BY          IGNORE_TYPE | 7
#define IGNORE_TYPE_ELSE        IGNORE_TYPE | 8
#define IGNORE_TYPE_FOR         IGNORE_TYPE | 9
#define IGNORE_TYPE_FROM        IGNORE_TYPE | 10
#define IGNORE_TYPE_IS          IGNORE_TYPE | 12
#define IGNORE_TYPE_OF          IGNORE_TYPE | 13
#define IGNORE_TYPE_THE         IGNORE_TYPE | 14
#define IGNORE_TYPE_THEN        IGNORE_TYPE | 15
#define IGNORE_TYPE_TO          IGNORE_TYPE | 19
#define IGNORE_TYPE_WITH        IGNORE_TYPE | 20

struct mzx_command
{
  const char *const name;
  int parameters;
  const int *const param_types;
};

struct mzx_command_rw
{
  char *name;
  int parameters;
  int *param_types;
};

static const int cm2[]   = { IGNORE_TYPE_FOR, IMM_U16 | STRING };
static const int cm3[]   = { IMM_U16 | STRING };
static const int cm4[]   = { DIRECTION, IGNORE_TYPE_FOR, IMM_U16 | STRING };
static const int cm5[]   = { DIRECTION };
static const int cm6[]   = { COLOR | STRING, THING, PARAM | STRING };
static const int cm7[]   = { CHARACTER | STRING | IMM_U16 };
static const int cm8[]   = { COLOR | STRING };
static const int cm9[]   = { IMM_S16 | STRING, IMM_S16 | STRING };
static const int cm10[]  = { STRING, IGNORE_TYPE_TO, IMM_U16 | STRING };
static const int cm11[]  = { STRING, IGNORE_TYPE_BY, IMM_U16 | STRING };
static const int cm12[]  = { STRING, IGNORE_TYPE_BY, IMM_U16 | STRING };
static const int cm13[]  = { STRING, IGNORE_TYPE_TO, STRING };
static const int cm14[]  = { STRING, IGNORE_TYPE_BY, STRING };
static const int cm15[]  = { STRING, IGNORE_TYPE_BY, STRING };
static const int cm16[]  = { STRING, EQUALITY, IMM_U16 | STRING,
 IGNORE_TYPE_THEN, STRING };
static const int cm17[]  = { STRING, EQUALITY, STRING, IGNORE_TYPE_THEN,
 STRING };
static const int cm18[]  = { CONDITION, IGNORE_TYPE_THEN, STRING };
static const int cm19[]  = { CMD_NOT, CONDITION, IGNORE_TYPE_THEN, STRING };
static const int cm20[]  = { CMD_ANY, COLOR | STRING, THING, PARAM | STRING,
 IGNORE_TYPE_THEN, STRING };
static const int cm21[]  = { CMD_NO, COLOR | STRING, THING, PARAM | STRING,
 IGNORE_TYPE_THEN, STRING };
static const int cm22[]  = { COLOR | STRING, THING, PARAM | STRING,
 IGNORE_TYPE_AT, DIRECTION, IGNORE_TYPE_THEN, STRING };
static const int cm23[]  = { CMD_NOT, COLOR | STRING, THING, PARAM | STRING,
 IGNORE_TYPE_AT, DIRECTION, IGNORE_TYPE_THEN, STRING };
static const int cm24[]  = { COLOR | STRING, THING, PARAM | STRING,
 IGNORE_TYPE_AT, IMM_S16 | STRING, IMM_S16 | STRING, IGNORE_TYPE_THEN, STRING };
static const int cm25[]  = { IGNORE_TYPE_AT, IMM_S16 | STRING, IMM_S16 | STRING,
 IGNORE_TYPE_THEN, STRING };
static const int cm26[]  = { IGNORE_TYPE_AT, DIRECTION, IGNORE_TYPE_OF,
 CMD_PLAYER, IGNORE_TYPE_IS, COLOR | STRING, THING, PARAM | STRING,
 IGNORE_TYPE_THEN, STRING };
static const int cm27[]  = { STRING };
static const int cm28[]  = { STRING };
static const int cm29[]  = { STRING };
static const int cm30[]  = { STRING, IGNORE_TYPE_TO, STRING };
static const int cm31[]  = { IMM_U16 | STRING };
static const int cm32[]  = { COLOR | STRING, THING, PARAM | STRING,
 IGNORE_TYPE_TO, DIRECTION };
static const int cm33[]  = { IMM_U16 | STRING, ITEM };
static const int cm34[]  = { IMM_U16 | STRING, ITEM };
static const int cm35[]  = { IMM_U16 | STRING, ITEM, IGNORE_TYPE_ELSE, STRING };
static const int cm38[]  = { STRING };
static const int cm39[]  = { IMM_U16 | STRING, STRING };
static const int cm40[]  = { IMM_U16 | STRING };
static const int cm41[]  = { CMD_MOD };
static const int cm42[]  = { CMD_SAM };
static const int cm43[]  = { STRING };
static const int cm44[]  = { CMD_PLAY };
static const int cm45[]  = { CMD_PLAY, STRING };
static const int cm46[]  = { CMD_PLAY };
static const int cm48[]  = { IMM_U16 | STRING };
static const int cm49[]  = { CMD_SFX, STRING };
static const int cm50[]  = { IGNORE_TYPE_AT, DIRECTION };
static const int cm53[]  = { IGNORE_TYPE_AT, DIRECTION, IGNORE_TYPE_TO,
 STRING };
static const int cm54[]  = { STRING, IMM_U16 | STRING };
static const int cm55[]  = { STRING, IMM_U16 | STRING };
static const int cm58[]  = { CMD_NS };
static const int cm59[]  = { CMD_EW };
static const int cm60[]  = { CMD_ATTACK };
static const int cm61[]  = { CMD_PLAYER, IGNORE_TYPE_TO, DIRECTION };
static const int cm62[]  = { CMD_PLAYER, IGNORE_TYPE_TO, DIRECTION,
 IGNORE_TYPE_ELSE, STRING };
static const int cm63[]  = { CMD_PLAYER, IGNORE_TYPE_AT, IMM_S16 | STRING,
 IMM_S16 | STRING };
static const int cm66[]  = { CMD_PLAYER, IGNORE_TYPE_AT, IMM_S16 | STRING,
 IMM_S16 | STRING, STRING };
static const int cm67[]  = { CMD_PLAYER, DIRECTION };
static const int cm68[]  = { DIRECTION, IGNORE_TYPE_ELSE, STRING };
static const int cm71[]  = { DIRECTION, IGNORE_TYPE_WITH, DIRECTION };
static const int cm72[]  = { IGNORE_TYPE_TO, DIRECTION };
static const int cm73[]  = { IGNORE_TYPE_TO, DIRECTION };
static const int cm74[]  = { CMD_HIGH, IGNORE_TYPE_TO, DIRECTION };
static const int cm75[]  = { IGNORE_TYPE_TO, DIRECTION };
static const int cm76[]  = { IGNORE_TYPE_TO, DIRECTION };
static const int cm77[]  = { IGNORE_TYPE_TO, DIRECTION };
static const int cm78[]  = { IGNORE_TYPE_TO, DIRECTION, IGNORE_TYPE_FOR,
 IMM_U16 | STRING };
static const int cm79[]  = { COLOR | STRING, THING, PARAM | STRING,
 IGNORE_TYPE_AT, IMM_S16 | STRING, IMM_S16 | STRING };
static const int cm80[]  = { IGNORE_TYPE_AS, IGNORE_TYPE_AN, CMD_ITEM };
static const int cm81[]  = { IGNORE_TYPE_AT, IMM_S16 | STRING, IMM_S16 | STRING,
 IGNORE_TYPE_TO, STRING };
static const int cm82[]  = { STRING };
static const int cm83[]  = { IGNORE_TYPE_AT, IMM_S16 | STRING,
 IMM_S16 | STRING };
static const int cm84[]  = { IGNORE_TYPE_FROM, DIRECTION };
static const int cm85[]  = { CMD_SELF, IGNORE_TYPE_TO, DIRECTION };
static const int cm86[]  = { CMD_SELF, IGNORE_TYPE_AT, IMM_S16 | STRING,
 IMM_S16 | STRING };
static const int cm87[]  = { IGNORE_TYPE_IS, CHARACTER | STRING | IMM_U16 };
static const int cm88[]  = { IGNORE_TYPE_IS, CHARACTER | STRING | IMM_U16 };
static const int cm89[]  = { IGNORE_TYPE_IS, CHARACTER | STRING | IMM_U16 };
static const int cm90[]  = { IGNORE_TYPE_IS, CHARACTER | STRING | IMM_U16 };
static const int cm91[]  = { COLOR | STRING };
static const int cm92[]  = { COLOR | STRING, IGNORE_TYPE_ELSE, STRING };
static const int cm93[]  = { COLOR | STRING };
static const int cm94[]  = { COLOR | STRING, IGNORE_TYPE_ELSE, STRING };
static const int cm95[]  = { STRING, IGNORE_TYPE_BY, CMD_RANDOM,
 IMM_U16 | STRING, IGNORE_TYPE_TO, IMM_U16 | STRING };
static const int cm96[]  = { STRING, IGNORE_TYPE_BY, CMD_RANDOM,
 IMM_U16 | STRING, IGNORE_TYPE_TO, IMM_U16 | STRING };
static const int cm97[]  = { STRING, IGNORE_TYPE_TO, CMD_RANDOM,
 IMM_U16 | STRING, IGNORE_TYPE_TO, IMM_U16 | STRING };
static const int cm98[]  = { IMM_U16 | STRING, ITEM, IGNORE_TYPE_FOR,
 IMM_U16 | STRING, ITEM, IGNORE_TYPE_ELSE, STRING };
static const int cm99[]  = { IGNORE_TYPE_AT, DIRECTION, IGNORE_TYPE_OF,
 CMD_PLAYER, IGNORE_TYPE_TO, STRING };
static const int cm100[] = { COLOR | STRING, THING, PARAM | STRING,
 IGNORE_TYPE_TO, DIRECTION, IGNORE_TYPE_OF, CMD_PLAYER };
static const int cm101[] = { STRING };
static const int cm102[] = { STRING };
static const int cm103[] = { STRING };
static const int cm104[] = { STRING, STRING };
static const int cm105[] = { STRING, STRING, STRING };
static const int cm106[] = { STRING };
static const int cm107[] = { STRING };
static const int cm108[] = { STRING };
static const int cm109[] = { CMD_PLAYER, IGNORE_TYPE_TO, STRING,
 IGNORE_TYPE_AT, IMM_S16 | STRING, IMM_S16 | STRING };
static const int cm110[] = { IGNORE_TYPE_TO, DIRECTION, IGNORE_TYPE_FOR,
 IMM_U16 | STRING };
static const int cm111[] = { CMD_STRING, STRING };
static const int cm112[] = { CMD_STRING, IGNORE_TYPE_IS, STRING,
 IGNORE_TYPE_THEN, STRING };
static const int cm113[] = { CMD_STRING, IGNORE_TYPE_IS, CMD_NOT, STRING,
 IGNORE_TYPE_THEN, STRING };
static const int cm114[] = { CMD_STRING, CMD_MATCHES, STRING, IGNORE_TYPE_THEN,
 STRING };
static const int cm115[] = { CMD_CHAR, IGNORE_TYPE_IS,
 CHARACTER | STRING | IMM_U16 };
static const int cm116[] = { STRING };
static const int cm117[] = { STRING };
static const int cm118[] = { CMD_ALL, COLOR | STRING, THING, PARAM | STRING,
 IGNORE_TYPE_TO, DIRECTION };
static const int cm119[] = { IGNORE_TYPE_AT, IMM_S16 | STRING, IMM_S16 | STRING,
 IGNORE_TYPE_TO, IMM_S16 | STRING, IMM_S16 | STRING };
static const int cm120[] = { CMD_EDGE, CMD_COLOR, IGNORE_TYPE_TO, COLOR | STRING };
static const int cm121[] = { IGNORE_TYPE_TO, IGNORE_TYPE_THE, DIRECTION,
 IGNORE_TYPE_IS, STRING };
static const int cm122[] = { IGNORE_TYPE_TO, IGNORE_TYPE_THE, DIRECTION,
 CMD_NONE };
static const int cm123[] = { CMD_EDIT, CHARACTER | STRING | IMM_U16 | EXTENDED,
 IGNORE_TYPE_TO,
 IMM_U16 | STRING, IMM_U16 | STRING, IMM_U16 | STRING, IMM_U16 | STRING,
 IMM_U16 | STRING, IMM_U16 | STRING, IMM_U16 | STRING, IMM_U16 | STRING,
 IMM_U16 | STRING, IMM_U16 | STRING, IMM_U16 | STRING, IMM_U16 | STRING,
 IMM_U16 | STRING, IMM_U16 | STRING };
static const int cm124[] = { CMD_PUSHABLE };
static const int cm125[] = { CMD_NONPUSHABLE };
static const int cm126[] = { IGNORE_TYPE_FOR, IMM_U16 | STRING };
static const int cm127[] = { IGNORE_TYPE_FOR, IMM_U16 | STRING };
static const int cm128[] = { IGNORE_TYPE_FOR, IMM_U16 | STRING };
static const int cm129[] = { IGNORE_TYPE_FOR, IMM_U16 | STRING };
static const int cm130[] = { IGNORE_TYPE_FOR, IMM_U16 | STRING };
static const int cm132[] = { IGNORE_TYPE_FROM, DIRECTION, IGNORE_TYPE_TO,
 DIRECTION };
static const int cm133[] = { IGNORE_TYPE_A, CMD_LAVAWALKER };
static const int cm134[] = { IGNORE_TYPE_A, CMD_NONLAVAWALKER };
static const int cm135[] = { IGNORE_TYPE_FROM, COLOR | STRING, THING,
 PARAM | STRING, IGNORE_TYPE_TO, COLOR | STRING, THING, PARAM | STRING};
static const int cm136[] = { IGNORE_TYPE_IS, COLOR | STRING};
static const int cm137[] = { IGNORE_TYPE_IS, COLOR | STRING };
static const int cm138[] = { IGNORE_TYPE_IS, COLOR | STRING };
static const int cm139[] = { CMD_ROW, IGNORE_TYPE_IS, IMM_U16 | STRING };
static const int cm140[] = { IGNORE_TYPE_TO, CMD_SELF };
static const int cm141[] = { IGNORE_TYPE_TO, CMD_PLAYER };
static const int cm142[] = { IGNORE_TYPE_TO, CMD_COUNTERS };
static const int cm143[] = { CMD_CHAR, CMD_ID, IMM_U16 | STRING,
 IGNORE_TYPE_TO, CHARACTER | STRING | IMM_U16 };
static const int cm144[] = { IGNORE_TYPE_TO, CMD_MOD, CMD_ORDER,
 IMM_U16 | STRING };
static const int cm145[] = { STRING };
static const int cm147[] = { CMD_THICK, CMD_ARROW, CMD_CHAR, DIRECTION,
 IGNORE_TYPE_TO, CHARACTER | STRING | IMM_U16 };
static const int cm148[] = { CMD_THIN, CMD_ARROW, CMD_CHAR, DIRECTION,
 IGNORE_TYPE_TO, CHARACTER | STRING | IMM_U16 };
static const int cm149[] = { CMD_MAXHEALTH, IMM_U16 | STRING };
static const int cm150[] = { CMD_PLAYER, CMD_POSITION };
static const int cm151[] = { CMD_PLAYER, CMD_POSITION };
static const int cm152[] = { CMD_PLAYER, CMD_POSITION };
static const int cm153[] = { CMD_MESG, CMD_COLUMN, IGNORE_TYPE_TO,
 IMM_U16 | STRING };
static const int cm154[] = { CMD_MESG };
static const int cm155[] = { CMD_MESG };
static const int cm157[] = { CMD_SAM, IMM_U16 | STRING, IMM_U16 | STRING };
static const int cm158[] = { STRING };
static const int cm159[] = { CMD_COLOR, IGNORE_TYPE_IS, COLOR | STRING };
static const int cm160[] = { CMD_COLOR, IGNORE_TYPE_IS, COLOR | STRING };
static const int cm161[] = { CMD_COLOR, IGNORE_TYPE_IS, COLOR | STRING };
static const int cm162[] = { CMD_COLOR, IGNORE_TYPE_IS, COLOR | STRING };
static const int cm163[] = { CMD_COLOR, IGNORE_TYPE_IS, COLOR | STRING };
static const int cm164[] = { IGNORE_TYPE_IS, IGNORE_TYPE_AT, IMM_U16 | STRING,
 IMM_U16 | STRING };
static const int cm165[] = { CMD_SIZE, IGNORE_TYPE_IS, IMM_U16 | STRING,
 IGNORE_TYPE_BY, IMM_U16 | STRING };
static const int cm166[] = { CMD_MESG, CMD_COLUMN, IGNORE_TYPE_TO, STRING };
static const int cm167[] = { CMD_ROW, IGNORE_TYPE_IS, STRING };
static const int cm168[] = { CMD_PLAYER, CMD_POSITION, IGNORE_TYPE_TO,
 IMM_U16 | STRING };
static const int cm169[] = { CMD_PLAYER, CMD_POSITION, IGNORE_TYPE_FROM,
 IMM_U16 | STRING };
static const int cm170[] = { CMD_PLAYER, CMD_POSITION, IGNORE_TYPE_WITH,
 IMM_U16 | STRING };
static const int cm171[] = { CMD_PLAYER, CMD_POSITION, IGNORE_TYPE_FROM,
 IMM_U16 | STRING, IGNORE_TYPE_AND, CMD_DUPLICATE, CMD_SELF };
static const int cm172[] = { CMD_PLAYER, CMD_POSITION, IGNORE_TYPE_WITH,
 IMM_U16 | STRING, IGNORE_TYPE_AND, CMD_DUPLICATE, CMD_SELF };
static const int cm173[] = { CMD_BULLETN, IGNORE_TYPE_IS,
 CHARACTER | STRING | IMM_U16 };
static const int cm174[] = { CMD_BULLETS, IGNORE_TYPE_IS,
 CHARACTER | STRING | IMM_U16 };
static const int cm175[] = { CMD_BULLETE, IGNORE_TYPE_IS,
 CHARACTER | STRING | IMM_U16 };
static const int cm176[] = { CMD_BULLETW, IGNORE_TYPE_IS,
 CHARACTER | STRING | IMM_U16 };
static const int cm177[] = { CMD_BULLETN, IGNORE_TYPE_IS,
 CHARACTER | STRING | IMM_U16 };
static const int cm178[] = { CMD_BULLETS, IGNORE_TYPE_IS,
 CHARACTER | STRING | IMM_U16 };
static const int cm179[] = { CMD_BULLETE, IGNORE_TYPE_IS,
 CHARACTER | STRING | IMM_U16 };
static const int cm180[] = { CMD_BULLETW, IGNORE_TYPE_IS,
 CHARACTER | STRING | IMM_U16 };
static const int cm181[] = { CMD_BULLETN, IGNORE_TYPE_IS,
 CHARACTER | STRING | IMM_U16 };
static const int cm182[] = { CMD_BULLETS, IGNORE_TYPE_IS,
 CHARACTER | STRING | IMM_U16 };
static const int cm183[] = { CMD_BULLETE, IGNORE_TYPE_IS,
 CHARACTER | STRING | IMM_U16 };
static const int cm184[] = { CMD_BULLETW, IGNORE_TYPE_IS,
 CHARACTER | STRING | IMM_U16 };
static const int cm185[] = { CMD_BULLETCOLOR, IGNORE_TYPE_IS,
 COLOR | STRING };
static const int cm186[] = { CMD_BULLETCOLOR, IGNORE_TYPE_IS,
 COLOR | STRING };
static const int cm187[] = { CMD_BULLETCOLOR, IGNORE_TYPE_IS,
 COLOR | STRING };
static const int cm193[] = { CMD_SELF, CMD_FIRST };
static const int cm194[] = { CMD_SELF, CMD_LAST };
static const int cm195[] = { CMD_PLAYER, CMD_FIRST };
static const int cm196[] = { CMD_PLAYER, CMD_LAST };
static const int cm197[] = { CMD_COUNTERS, CMD_FIRST };
static const int cm198[] = { CMD_COUNTERS, CMD_LAST };
static const int cm199[] = { CMD_FADE, CMD_OUT };
static const int cm200[] = { CMD_FADE, CMD_IN, STRING };
static const int cm201[] = { CMD_BLOCK, IGNORE_TYPE_AT, IMM_S16 | STRING,
 IMM_S16 | STRING, IGNORE_TYPE_FOR, IMM_U16 | STRING, IGNORE_TYPE_BY,
 IMM_U16 | STRING, IGNORE_TYPE_TO, IMM_S16 | STRING, IMM_S16 | STRING };
static const int cm202[] = { CMD_INPUT };
static const int cm203[] = { IGNORE_TYPE_TO, DIRECTION };
static const int cm204[] = { CMD_CHAR, CHARACTER | STRING | IMM_U16 | EXTENDED,
 DIRECTION };
static const int cm205[] = { CMD_CHAR, CHARACTER | STRING | IMM_U16 | EXTENDED,
 DIRECTION };
static const int cm206[] = { CMD_CHAR, CHARACTER | STRING | IMM_U16 | EXTENDED,
 IGNORE_TYPE_TO, CHARACTER | STRING | IMM_U16 | EXTENDED };
static const int cm210[] = { CMD_SFX, IMM_U16 | STRING, IGNORE_TYPE_TO,
 STRING };
static const int cm211[] = { CMD_INTENSITY, IGNORE_TYPE_IS, IGNORE_TYPE_AT,
 IMM_U16 | STRING, CMD_PERCENT };
static const int cm212[] = { CMD_INTENSITY, IMM_U16 | STRING, IGNORE_TYPE_IS,
 IGNORE_TYPE_AT, IMM_U16 | STRING, CMD_PERCENT };
static const int cm213[] = { CMD_FADE, CMD_OUT };
static const int cm214[] = { CMD_FADE, CMD_IN };
static const int cm215[] = { CMD_COLOR, IMM_U16 | STRING, IGNORE_TYPE_TO,
 IMM_U16 | STRING, IMM_U16 | STRING, IMM_U16 | STRING };
static const int cm216[] = { CMD_CHAR, CMD_SET, STRING };
static const int cm217[] = { STRING, IGNORE_TYPE_BY, IMM_U16 | STRING };
static const int cm218[] = { STRING, IGNORE_TYPE_BY, IMM_U16 | STRING };
static const int cm219[] = { STRING, IGNORE_TYPE_BY, IMM_U16 | STRING };
static const int cm220[] = { CMD_CHAR, DIRECTION, IGNORE_TYPE_IS,
 CHARACTER | STRING | IMM_U16 };
static const int cm222[] = { CMD_PALETTE, STRING };
static const int cm224[] = { CMD_FADE, IGNORE_TYPE_TO, IMM_U16 | STRING,
 IGNORE_TYPE_BY, IMM_U16 | STRING };
static const int cm225[] = { CMD_POSITION, IMM_S16 | STRING,
 IMM_S16 | STRING };
static const int cm226[] = { CMD_WORLD, STRING };
static const int cm227[] = { CMD_ALIGNEDROBOT, IGNORE_TYPE_WITH, STRING,
 IGNORE_TYPE_THEN, STRING };
static const int cm231[] = { CMD_FIRST, CMD_STRING, IGNORE_TYPE_IS, STRING,
 IGNORE_TYPE_THEN, STRING };
static const int cm232[] = { CMD_GO, STRING };
static const int cm233[] = { IGNORE_TYPE_FOR, CMD_MOD, CMD_FADE };
static const int cm235[] = { CMD_SAVING };
static const int cm236[] = { CMD_SAVING };
static const int cm237[] = { CMD_SENSORONLY, CMD_SAVING };
static const int cm238[] = { CMD_COUNTER, IMM_U16 | STRING, IGNORE_TYPE_IS,
 STRING };
static const int cm239[] = { IGNORE_TYPE_IS, CMD_ON };
static const int cm240[] = { IGNORE_TYPE_IS, CMD_STATIC };
static const int cm241[] = { IGNORE_TYPE_IS, CMD_TRANSPARENT };
static const int cm242[] = { COLOR | STRING, CHARACTER | STRING | IMM_U16,
 CMD_OVERLAY, IGNORE_TYPE_TO, IMM_S16 | STRING, IMM_S16 | STRING };
static const int cm243[] = { CMD_OVERLAY, CMD_BLOCK, IGNORE_TYPE_AT,
 IMM_S16 | STRING, IMM_S16 | STRING, IGNORE_TYPE_FOR, IMM_U16 | STRING,
 IGNORE_TYPE_BY, IMM_U16 | STRING, IGNORE_TYPE_TO, IMM_S16 | STRING,
 IMM_S16 | STRING };
static const int cm245[] = { CMD_OVERLAY, COLOR | STRING,
 CHARACTER | STRING | IMM_U16, IGNORE_TYPE_TO,
 COLOR | STRING, CHARACTER | STRING | IMM_U16 };
static const int cm246[] = { CMD_OVERLAY, COLOR | STRING, IGNORE_TYPE_TO,
 COLOR | STRING };
static const int cm247[] = { CMD_OVERLAY, COLOR | STRING, STRING,
 IGNORE_TYPE_AT, IMM_S16 | STRING, IMM_S16 | STRING };
static const int cm251[] = { CMD_START };
static const int cm252[] = { IGNORE_TYPE_FOR, IMM_U16 | STRING };
static const int cm253[] = { CMD_LOOP };
static const int cm254[] = { CMD_MESG, CMD_EDGE };
static const int cm255[] = { CMD_MESG, CMD_EDGE };

static const struct mzx_command command_list[] =
{
  { "end",            0, NULL },
  { "die",            0, NULL },
  { "wait",           1, cm2 },
  { "cycle",          1, cm3 },
  { "go",             2, cm4 },
  { "walk",           1, cm5 },
  { "become",         3, cm6 },
  { "char",           1, cm7 },
  { "color",          1, cm8 },
  { "gotoxy",         2, cm9 },
  { "set",            2, cm10 },
  { "inc",            2, cm11 },
  { "dec",            2, cm12 },
  { "set",            2, cm13 },
  { "inc",            2, cm14 },
  { "dec",            2, cm15 },
  { "if",             4, cm16 },
  { "if",             4, cm17 },
  { "if",             2, cm18 },
  { "if",             3, cm19 },
  { "if",             5, cm20 },
  { "if",             5, cm21 },
  { "if",             5, cm22 },
  { "if",             6, cm23 },
  { "if",             6, cm24 },
  { "if",             3, cm25 },
  { "if",             6, cm26 },
  { "double",         1, cm27 },
  { "half",           1, cm28 },
  { "goto",           1, cm29 },
  { "send",           2, cm30 },
  { "explode",        1, cm31 },
  { "put",            4, cm32 },
  { "give",           2, cm33 },
  { "take",           2, cm34 },
  { "take",           3, cm35 },
  { "endgame",        0, NULL },
  { "endlife",        0, NULL },
  { "mod",            1, cm38 },
  { "sam",            2, cm39 },
  { "volume",         1, cm40 },
  { "end",            1, cm41 },
  { "end",            1, cm42 },
  { "play",           1, cm43 },
  { "end",            1, cm44 },
  { "wait",           2, cm45 },
  { "wait",           1, cm46 },
  { "_blank_line",    0, NULL },
  { "sfx",            1, cm48 },
  { "play",           2, cm49 },
  { "open",           1, cm50 },
  { "lockself",       0, NULL },
  { "unlockself",     0, NULL },
  { "send",           2, cm53 },
  { "zap",            2, cm54 },
  { "restore",        2, cm55 },
  { "lockplayer",     0, NULL },
  { "unlockplayer",   0, NULL },
  { "lockplayer",     1, cm58 },
  { "lockplayer",     1, cm59 },
  { "lockplayer",     1, cm60 },
  { "move",           2, cm61 },
  { "move",           3, cm62 },
  { "put",            3, cm63 },
  { "__unused",       0, NULL },
  { "__unused",       0, NULL },
  { "if",             4, cm66 },
  { "put",            2, cm67 },
  { "try",            2, cm68 },
  { "rotatecw",       0, NULL },
  { "rotateccw",      0, NULL },
  { "switch",         2, cm71 },
  { "shoot",          1, cm72 },
  { "laybomb",        1, cm73 },
  { "laybomb",        2, cm74 },
  { "shootmissile",   1, cm75 },
  { "shootseeker",    1, cm76 },
  { "spitfire",       1, cm77 },
  { "lazerwall",      2, cm78 },
  { "put",            5, cm79 },
  { "die",            1, cm80 },
  { "send",           3, cm81 },
  { "copyrobot",      1, cm82 },
  { "copyrobot",      2, cm83 },
  { "copyrobot",      1, cm84 },
  { "duplicate",      2, cm85 },
  { "duplicate",      3, cm86 },
  { "bulletn",        1, cm87 },
  { "bullets",        1, cm88 },
  { "bullete",        1, cm89 },
  { "bulletw",        1, cm90 },
  { "givekey",        1, cm91 },
  { "givekey",        2, cm92 },
  { "takekey",        1, cm93 },
  { "takekey",        2, cm94 },
  { "inc",            4, cm95 },
  { "dec",            4, cm96 },
  { "set",            4, cm97 },
  { "trade",          5, cm98 },
  { "send",           3, cm99 },
  { "put",            5, cm100 },
  { "/",              1, cm101 },
  { "*",              1, cm102 },
  { "[",              1, cm103 },
  { "?",              2, cm104 },
  { "?",              3, cm105 },
  { ":",              1, cm106 },
  { ".",              1, cm107 },
  { "|",              1, cm108 },
  { "teleport",       4, cm109 },
  { "scrollview",     2, cm110 },
  { "input",          2, cm111 },
  { "if",             3, cm112 },
  { "if",             4, cm113 },
  { "if",             4, cm114 },
  { "player",         2, cm115 },
  { "%",              1, cm116 },
  { "&",              1, cm117 },
  { "move",           5, cm118 },
  { "copy",           4, cm119 },
  { "set",            3, cm120 },
  { "board",          2, cm121 },
  { "board",          2, cm122 },
  { "char",           16, cm123 },
  { "become",         1, cm124 },
  { "become",         1, cm125 },
  { "blind",          1, cm126 },
  { "firewalker",     1, cm127 },
  { "freezetime",     1, cm128 },
  { "slowtime",       1, cm129 },
  { "wind",           1, cm130 },
  { "avalanche",      0, NULL },
  { "copy",           2, cm132 },
  { "become",         1, cm133 },
  { "become",         1, cm134 },
  { "change",         6, cm135 },
  { "playercolor",    1, cm136 },
  { "bulletcolor",    1, cm137 },
  { "missilecolor",   1, cm138 },
  { "message",        2, cm139 },
  { "rel",            1, cm140 },
  { "rel",            1, cm141 },
  { "rel",            1, cm142 },
  { "change",         4, cm143 },
  { "jump",           3, cm144 },
  { "ask",            1, cm145 },
  { "fillhealth",     0, NULL },
  { "change",         5, cm147 },
  { "change",         5, cm148 },
  { "set",            2, cm149 },
  { "save",           2, cm150 },
  { "restore",        2, cm151 },
  { "exchange",       2, cm152 },
  { "set",            3, cm153 },
  { "center",         1, cm154 },
  { "clear",          1, cm155 },
  { "resetview",      0, NULL },
  { "mod",            3, cm157 },
  { "volume",         1, cm158 },
  { "scrollbase",     2, cm159 },
  { "scrollcorner",   2, cm160 },
  { "scrolltitle",    2, cm161 },
  { "scrollpointer",  2, cm162 },
  { "scrollarrow",    2, cm163 },
  { "viewport",       2, cm164 },
  { "viewport",       3, cm165 },
  { "set",            3, cm166 },
  { "message",        2, cm167 },
  { "save",           3, cm168 },
  { "restore",        3, cm169 },
  { "exchange",       3, cm170 },
  { "restore",        5, cm171 },
  { "exchange",       5, cm172 },
  { "player",         2, cm173 },
  { "player",         2, cm174 },
  { "player",         2, cm175 },
  { "player",         2, cm176 },
  { "neutral",        2, cm177 },
  { "neutral",        2, cm178 },
  { "neutral",        2, cm179 },
  { "neutral",        2, cm180 },
  { "enemy",          2, cm181 },
  { "enemy",          2, cm182 },
  { "enemy",          2, cm183 },
  { "enemy",          2, cm184 },
  { "player",         2, cm185 },
  { "neutral",        2, cm186 },
  { "enemy",          2, cm187 },
  { "__unused",       0, NULL },
  { "__unused",       0, NULL },
  { "__unused",       0, NULL },
  { "__unused",       0, NULL },
  { "__unused",       0, NULL },
  { "rel",            2, cm193 },
  { "rel",            2, cm194 },
  { "rel",            2, cm195 },
  { "rel",            2, cm196 },
  { "rel",            2, cm197 },
  { "rel",            2, cm198 },
  { "mod",            2, cm199 },
  { "mod",            3, cm200 },
  { "copy",           7, cm201 },
  { "clip",           1, cm202 },
  { "push",           1, cm203 },
  { "scroll",         3, cm204 },
  { "flip",           3, cm205 },
  { "copy",           3, cm206 },
  { "__unused",       0, NULL },
  { "__unused",       0, NULL },
  { "__unused",       0, NULL },
  { "change",         3, cm210 },
  { "color",          3, cm211 },
  { "color",          4, cm212 },
  { "color",          2, cm213 },
  { "color",          2, cm214 },
  { "set",            5, cm215 },
  { "load",           3, cm216 },
  { "multiply",       2, cm217 },
  { "divide",         2, cm218 },
  { "modulo",         2, cm219 },
  { "player",         3, cm220 },
  { "__unused",       0, NULL },
  { "load",           2, cm222 },
  { "__unused",       0, NULL },
  { "mod",            3, cm224 },
  { "scrollview",     3, cm225 },
  { "swap",           2, cm226 },
  { "if",             3, cm227 },
  { "__unused",       0, NULL },
  { "lockscroll",     0, NULL },
  { "unlockscroll",   0, NULL },
  { "if",             4, cm231 },
  { "persistent",     2, cm232 },
  { "wait",           2, cm233 },
  { "__unused",       0, NULL },
  { "enable",         1, cm235 },
  { "disable",        1, cm236 },
  { "enable",         2, cm237 },
  { "status",         3, cm238 },
  { "overlay",        1, cm239 },
  { "overlay",        1, cm240 },
  { "overlay",        1, cm241 },
  { "put",            5, cm242 },
  { "copy",           8, cm243 },
  { "__unused",       0, NULL },
  { "change",         5, cm245 },
  { "change",         3, cm246 },
  { "write",          5, cm247 },
  { "__unused",       0, NULL },
  { "__unused",       0, NULL },
  { "__unused",       0, NULL },
  { "loop",           1, cm251 },
  { "loop",           1, cm252 },
  { "abort",          1, cm253 },
  { "disable",        2, cm254 },
  { "enable",         2, cm255 }
};

static const char *const command_fragments[69] =
{
  "not",
  "any",
  "player",
  "ns",
  "ew",
  "attack",
  "item",
  "self",
  "random",
  "string",
  "char",
  "all",
  "edit",
  "pushable",
  "nonpushable",
  "lavawalker",
  "nonlavawalker",
  "row",
  "counters",
  "id",
  "mod",
  "order",
  "thick",
  "arrow",
  "thin",
  "maxhealth",
  "position",
  "mesg",
  "column",
  "color",
  "size",
  "bulletn",
  "bullets",
  "bullete",
  "bulletw",
  "bulletcolor",
  "first",
  "last",
  "fade",
  "out",
  "in",
  "block",
  "sfx",
  "intensity",
  "set",
  "palette",
  "world",
  "alignedrobot",
  "go",
  "saving",
  "sensoronly",
  "on",
  "static",
  "transparent",
  "overlay",
  "start",
  "loop",
  "edge",
  "sam",
  "play",
  "percent",
  "high",
  "matches",
  "none",
  "input",
  "dir",
  "counter",
  "duplicate",
  "no"
};

/*
  0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F
*/
const char special_first_char[256] =
{
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,   // 0x00-0x0F
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,   // 0x10-0x1F
  0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 1, 0, 0, 0, 1, 1,   // 0x20-0x2F
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0,   // 0x30-0x3F
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,   // 0x40-0x4F
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0,   // 0x50-0x5F
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,   // 0x60-0x6F
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0,   // 0x70-0x7F
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,   // 0x80-0x8F
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,   // 0x90-0x9F
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,   // 0xA0-0xAF
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,   // 0xB0-0xBF
  0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0,   // 0xC0-0xCF
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,   // 0xD0-0xDF
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,   // 0xE0-0xEF
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0    // 0xF0-0xFF
};

static const struct search_entry_short sorted_argument_list[] =
{
  { "!=",               5,   S_EQUALITY  },
  { ",",                0,   S_EXTRA     },
  { ";",                1,   S_EXTRA     },
  { "<",                1,   S_EQUALITY  },
  { "<=",               4,   S_EQUALITY  },
  { "<>",               5,   S_EQUALITY  },
  { "=",                0,   S_EQUALITY  },
  { "=<",               4,   S_EQUALITY  },
  { "==",               0,   S_EQUALITY  },
  { "===",              6,   S_EQUALITY  },
  { "=>",               3,   S_EQUALITY  },
  { ">",                2,   S_EQUALITY  },
  { "><",               5,   S_EQUALITY  },
  { ">=",               3,   S_EQUALITY  },
  { "?=",               7,   S_EQUALITY  },
  { "?==",              8,   S_EQUALITY  },
  { "a",                2,   S_EXTRA     },
  { "aligned",          5,   S_CONDITION },
  { "alignedew",        7,   S_CONDITION },
  { "alignedns",        6,   S_CONDITION },
  { "alignedrobot",     47,  S_CMD       },
  { "all",              11,  S_CMD       },
  { "ammo",             35,  S_THING     },
  { "ammos",            1,   S_ITEM      },
  { "an",               3,   S_EXTRA     },
  { "and",              4,   S_EXTRA     },
  { "any",              1,   S_CMD       },
  { "anydir",           12,  S_DIR       },
  { "arrow",            23,  S_CMD       },
  { "as",               5,   S_EXTRA     },
  { "at",               6,   S_EXTRA     },
  { "attack",           5,   S_CMD       },
  { "bear",             94,  S_THING     },
  { "bearcub",          95,  S_THING     },
  { "beneath",          11,  S_DIR       },
  { "block",            41,  S_CMD       },
  { "blocked",          4,   S_CONDITION },
  { "bomb",             36,  S_THING     },
  { "boulder",          8,   S_THING     },
  { "box",              11,  S_THING     },
  { "breakaway",        6,   S_THING     },
  { "bullet",           61,  S_THING     },
  { "bulletcolor",      35,  S_CMD       },
  { "bullete",          33,  S_CMD       },
  { "bulletgun",        92,  S_THING     },
  { "bulletn",          31,  S_CMD       },
  { "bullets",          32,  S_CMD       },
  { "bulletw",          34,  S_CMD       },
  { "by",               7,   S_EXTRA     },
  { "carpet",           14,  S_THING     },
  { "cave",             44,  S_THING     },
  { "ccwrotate",        46,  S_THING     },
  { "char",             10,  S_CMD       },
  { "chest",            27,  S_THING     },
  { "coin",             50,  S_THING     },
  { "coins",            8,   S_ITEM      },
  { "color",            29,  S_CMD       },
  { "column",           28,  S_CMD       },
  { "counter",          66,  S_CMD       },
  { "counters",         18,  S_CMD       },
  { "crate",            9,   S_THING     },
  { "customblock",      5,   S_THING     },
  { "custombox",        12,  S_THING     },
  { "custombreak",      7,   S_THING     },
  { "customfloor",      17,  S_THING     },
  { "customhurt",       76,  S_THING     },
  { "custompush",       10,  S_THING     },
  { "cw",               17,  S_DIR       },
  { "cwrotate",         45,  S_THING     },
  { "delpressed",       15,  S_CONDITION },
  { "dir",              65,  S_CMD       },
  { "door",             41,  S_THING     },
  { "down",             2,   S_DIR       },
  { "downpressed",      13,  S_CONDITION },
  { "dragon",           86,  S_THING     },
  { "duplicate",        67,  S_CMD       },
  { "e",                3,   S_DIR       },
  { "east",             3,   S_DIR       },
  { "edge",             57,  S_CMD       },
  { "edit",             12,  S_CMD       },
  { "else",             8,   S_EXTRA     },
  { "emovingwall",      53,  S_THING     },
  { "energizer",        33,  S_THING     },
  { "ew",               4,   S_CMD       },
  { "ewater",           23,  S_THING     },
  { "explosion",        38,  S_THING     },
  { "eye",              81,  S_THING     },
  { "fade",             38,  S_CMD       },
  { "fake",             13,  S_THING     },
  { "fire",             63,  S_THING     },
  { "firewalking",      2,   S_CONDITION },
  { "first",            36,  S_CMD       },
  { "fish",             87,  S_THING     },
  { "floor",            15,  S_THING     },
  { "flow",             13,  S_DIR       },
  { "for",              9,   S_EXTRA     },
  { "forest",           65,  S_THING     },
  { "from",             10,  S_EXTRA     },
  { "gate",             47,  S_THING     },
  { "gem",              28,  S_THING     },
  { "gems",             0,   S_ITEM      },
  { "ghost",            85,  S_THING     },
  { "go",               48,  S_CMD       },
  { "goblin",           90,  S_THING     },
  { "goop",             34,  S_THING     },
  { "health",           30,  S_THING     },
  { "healths",          4,   S_ITEM      },
  { "hibombs",          7,   S_ITEM      },
  { "high",             61,  S_CMD       },
  { "ice",              25,  S_THING     },
  { "id",               19,  S_CMD       },
  { "idle",             0,   S_DIR       },
  { "image_file",       100, S_THING     },
  { "in",               40,  S_CMD       },
  { "input",            64,  S_CMD       },
  { "intensity",        43,  S_CMD       },
  { "into",             11,  S_EXTRA     },
  { "inviswall",        71,  S_THING     },
  { "is",               12,  S_EXTRA     },
  { "item",             6,   S_CMD       },
  { "key",              39,  S_THING     },
  { "last",             37,  S_CMD       },
  { "lastshot",         8,   S_CONDITION },
  { "lasttouch",        9,   S_CONDITION },
  { "lava",             26,  S_THING     },
  { "lavawalker",       15,  S_CMD       },
  { "lazer",            59,  S_THING     },
  { "lazergun",         60,  S_THING     },
  { "left",             4,   S_DIR       },
  { "leftpressed",      11,  S_CONDITION },
  { "life",             66,  S_THING     },
  { "line",             4,   S_THING     },
  { "litbomb",          37,  S_THING     },
  { "lives",            5,   S_ITEM      },
  { "lobombs",          6,   S_ITEM      },
  { "lock",             40,  S_THING     },
  { "loop",             56,  S_CMD       },
  { "magicgem",         29,  S_THING     },
  { "matches",          62,  S_CMD       },
  { "maxhealth",        25,  S_CMD       },
  { "mesg",             27,  S_CMD       },
  { "mine",             74,  S_THING     },
  { "missile",          62,  S_THING     },
  { "missilegun",       97,  S_THING     },
  { "mod",              20,  S_CMD       },
  { "musicon",          16,  S_CONDITION },
  { "n",                1,   S_DIR       },
  { "nmovingwall",      51,  S_THING     },
  { "no",               68,  S_CMD       },
  { "nodir",            14,  S_DIR       },
  { "none",             63,  S_CMD       },
  { "nonlavawalker",    16,  S_CMD       },
  { "nonpushable",      14,  S_CMD       },
  { "normal",           1,   S_THING     },
  { "north",            1,   S_DIR       },
  { "not",              0,   S_CMD       },
  { "ns",               3,   S_CMD       },
  { "nwater",           21,  S_THING     },
  { "of",               13,  S_EXTRA     },
  { "on",               51,  S_CMD       },
  { "opendoor",         42,  S_THING     },
  { "opengate",         48,  S_THING     },
  { "opp",              18,  S_DIR       },
  { "order",            21,  S_CMD       },
  { "out",              39,  S_CMD       },
  { "overlay",          54,  S_CMD       },
  { "palette",          45,  S_CMD       },
  { "pcsfxon",          17,  S_CONDITION },
  { "percent",          60,  S_CMD       },
  { "play",             59,  S_CMD       },
  { "player",           2,   S_CMD       },
  { "position",         26,  S_CMD       },
  { "potion",           32,  S_THING     },
  { "pouch",            55,  S_THING     },
  { "pushable",         13,  S_CMD       },
  { "pushablerobot",    123, S_THING     },
  { "pusher",           56,  S_THING     },
  { "randany",          10,  S_DIR       },
  { "randb",            15,  S_DIR       },
  { "randew",           6,   S_DIR       },
  { "randnb",           8,   S_DIR       },
  { "randne",           7,   S_DIR       },
  { "randnot",          19,  S_DIR       },
  { "randns",           5,   S_DIR       },
  { "random",           8,   S_CMD       },
  { "randp",            16,  S_DIR       },
  { "ricochet",         73,  S_THING     },
  { "ricochetpanel",    72,  S_THING     },
  { "right",            3,   S_DIR       },
  { "rightpressed",     10,  S_CONDITION },
  { "ring",             31,  S_THING     },
  { "robot",            124, S_THING     },
  { "row",              17,  S_CMD       },
  { "runner",           84,  S_THING     },
  { "s",                2,   S_DIR       },
  { "sam",              58,  S_CMD       },
  { "saving",           49,  S_CMD       },
  { "score",            3,   S_ITEM      },
  { "scroll",           126, S_THING     },
  { "seek",             9,   S_DIR       },
  { "seeker",           79,  S_THING     },
  { "self",             7,   S_CMD       },
  { "sensor",           122, S_THING     },
  { "sensoronly",       50,  S_CMD       },
  { "set",              44,  S_CMD       },
  { "sfx",              42,  S_CMD       },
  { "shark",            88,  S_THING     },
  { "shootingfire",     78,  S_THING     },
  { "sign",             125, S_THING     },
  { "size",             30,  S_CMD       },
  { "sliderew",         58,  S_THING     },
  { "sliderns",         57,  S_THING     },
  { "slimeblob",        83,  S_THING     },
  { "smovingwall",      52,  S_THING     },
  { "snake",            80,  S_THING     },
  { "solid",            2,   S_THING     },
  { "south",            2,   S_DIR       },
  { "space",            0,   S_THING     },
  { "spacepressed",     14,  S_CONDITION },
  { "spider",           89,  S_THING     },
  { "spike",            75,  S_THING     },
  { "spinninggun",      93,  S_THING     },
  { "spittingtiger",    91,  S_THING     },
  { "sprite",           98,  S_THING     },
  { "sprite_colliding", 99,  S_THING     },
  { "stairs",           43,  S_THING     },
  { "start",            55,  S_CMD       },
  { "static",           52,  S_CMD       },
  { "stillwater",       20,  S_THING     },
  { "string",           9,   S_CMD       },
  { "swater",           22,  S_THING     },
  { "swimming",         1,   S_CONDITION },
  { "text",             77,  S_THING     },
  { "the",              14,  S_EXTRA     },
  { "then",             15,  S_EXTRA     },
  { "there",            16,  S_EXTRA     },
  { "thick",            22,  S_CMD       },
  { "thickweb",         19,  S_THING     },
  { "thief",            82,  S_THING     },
  { "thin",             24,  S_CMD       },
  { "through",          17,  S_EXTRA     },
  { "thru",             18,  S_EXTRA     },
  { "tiles",            16,  S_THING     },
  { "time",             2,   S_ITEM      },
  { "to",               19,  S_EXTRA     },
  { "touching",         3,   S_CONDITION },
  { "transparent",      53,  S_CMD       },
  { "transport",        49,  S_THING     },
  { "tree",             3,   S_THING     },
  { "under",            11,  S_DIR       },
  { "up",               1,   S_DIR       },
  { "uppressed",        12,  S_CONDITION },
  { "w",                4,   S_DIR       },
  { "walking",          0,   S_CONDITION },
  { "web",              18,  S_THING     },
  { "west",             4,   S_DIR       },
  { "whirlpool",        67,  S_THING     },
  { "whirlpool2",       68,  S_THING     },
  { "whirlpool3",       69,  S_THING     },
  { "whirlpool4",       70,  S_THING     },
  { "with",             20,  S_EXTRA     },
  { "wmovingwall",      54,  S_THING     },
  { "world",            46,  S_CMD       },
  { "wwater",           24,  S_THING     }
};

static const int num_argument_names =
 sizeof(sorted_argument_list) / sizeof(struct search_entry_short);

static int escape_chars(char *dest, char *src)
{
  if(src[0] == '\\')
  {
    switch(src[1])
    {
      case '"':
        *dest = '"';
        return 2;

      case '0':
        *dest = 0;
        return 2;

      case 'n':
        *dest = '\n';
        return 2;

      case 'r':
        *dest = '\r';
        return 2;

      case 't':
        *dest = '\t';
        return 2;

      case '\\':
        *dest = '\\';
        return 2;
    }
  }

  *dest = *src;
  return 1;
}

static int get_param(char *cmd_line)
{
  if((cmd_line[1] == '?') && (cmd_line[2] == '?'))
  {
    return 256;
  }

  return strtol(cmd_line + 1, NULL, 16);
}

static int is_color(char *cmd_line)
{
  if( (cmd_line[0] == 'c') &&
  ( (isxdigit((int)cmd_line[1]) && (cmd_line[2] == '?'))  ||
    (isxdigit((int)cmd_line[2]) && (cmd_line[1] == '?'))  ||
    (isxdigit((int)cmd_line[1]) && isxdigit((int)cmd_line[2])) ||
    ((cmd_line[1] == '?') && (cmd_line[2] == '?')) ) )
  {
    return 1;
  }
  else
  {
    return 0;
  }
}

static int is_param(char *cmd_line)
{
  if((cmd_line[0] == 'p') &&
   ((isxdigit((int)cmd_line[1]) && isxdigit((int)cmd_line[2])) ||
   ((cmd_line[1] == '?') && (cmd_line[2] == '?'))))
  {
    return 1;
  }
  else
  {
    return 0;
  }
}

#ifdef CONFIG_DEBYTECODE
static
#else
__editor_maybe_static
#endif
const struct search_entry_short *find_argument(char *name)
{
  const struct search_entry_short *base = sorted_argument_list;
  int bottom = 0, top = num_argument_names - 1, middle;
  int cmpval;

  while(bottom <= top)
  {
    middle = (top + bottom) / 2;
    cmpval = strcasecmp(name, (sorted_argument_list[middle]).name);

    if(cmpval > 0)
      bottom = middle + 1;
    else

    if(cmpval < 0)
      top = middle - 1;
    else
      return base + middle;
  }

  return NULL;
}

int get_color(char *cmd_line)
{
  if(cmd_line[1] == '?')
  {
    if(cmd_line[2] == '?')
    {
      return 0x100 + 0x10 + 0x10;
    }
    else
    {
      return strtol(cmd_line + 2, NULL, 16) | 0x100;
    }
  }

  if(cmd_line[2] == '?')
  {
    char temp[2];
    temp[0] = cmd_line[1];
    temp[1] = 0;
    return (strtol(temp, NULL, 16) + 0x10) | 0x100;
  }

  return strtol(cmd_line + 1, NULL, 16);
}

static int rasm_parse_argument(char *cmd_line, char **next,
 int *arg_translated, int *error, int *arg_short)
{
  char current = *cmd_line;
  char *space_position;
  const struct search_entry_short *matched_argument;

  *error = 0;

  switch(current)
  {
    case '"':
    {
      // If the first character is a quote it'll be a string.
      // It either has to be terminated with a " or a null terminator.
      // \'s can escape quotes.
      do
      {
        cmd_line++;
        current = *cmd_line;
        if((current == '\\') && ((cmd_line[1] == '"') ||
         (cmd_line[1] == '\\')))
        {
          cmd_line++;
        }

      } while((current != '"') && current);

      *arg_translated = 0;
      if(current)
        *next = cmd_line + 1;
      else
        *next = cmd_line;

      *arg_short = S_STRING;
      return STRING;
    }

    case '-':
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
    {
      // If the first character is a negative sign or a numerical constant,
      // it's an immediate number. I can't figure out the difference between
      // IMM_U16 and IMM_S16 to Robotic so I'm giving IMM_U16 for now.

      *arg_translated = strtol(cmd_line, next, 10);
      *arg_short = IMM_U16;
      return IMM_U16;
    }

    case '\'':
    {
      // If the first letter is a single quote then it's a character;
      // make sure there's only one constant there and a close quote.
      char c;
      int n = escape_chars(&c, cmd_line + 1);

      if(cmd_line[n + 1] != '\'')
      {
        *error = ERR_BADCHARACTER;
        return -1;
      }

      *arg_translated = (int)c;
      *next = cmd_line + n + 2;
      *arg_short = S_CHARACTER;
      return CHARACTER;
    }

    // Is it a color?
    case 'c':
    {
      if(is_color(cmd_line))
      {
        *arg_translated = (int)get_color(cmd_line);
        *next = cmd_line + 3;
        *arg_short = S_COLOR;
        return COLOR;
      }
      break;
    }

    case 'p':
    {
      // Is it a parameter?
      if(is_param(cmd_line))
      {
        *arg_translated = (int)get_param(cmd_line);
        *next = cmd_line + 3;
        *arg_short = S_PARAM;
        return PARAM;
      }
      break;
    }


    case '$':
    {
      // This is a hex immediate
      *arg_translated = strtol(cmd_line + 1, next, 16);
      *arg_short = IMM_U16;
      return IMM_U16;
    }
  }

  // It's most likely an unquoted character
  if(!isprint((int)current))
  {
    //Seriously, who would name a counter a smiley face, anyways?
    if(isspace((int)cmd_line[1]) || !cmd_line[1])
    {
      *arg_translated = current;
      *next = cmd_line + 1;
      *arg_short = S_CHARACTER;
      return CHARACTER;
    }
  }

  // Otherwise, it's a word matched parameter

  space_position = cmd_line;

  do
  {
    space_position++;
    current = *space_position;
  } while((current != ' ') && (current) && (current != '\n'));

  *space_position = 0;
  matched_argument = find_argument(cmd_line);
  *space_position = current;

  *next = space_position;

  if(matched_argument)
  {
    int arg_type = matched_argument->type;
    *arg_short = arg_type;
    *arg_translated = matched_argument->offset;

    if(arg_type == S_CMD)
    {
      return (CMD | matched_argument->offset);
    }
    else
    {
      return (1 << matched_argument->type);
    }
  }

  *arg_short = S_UNDEFINED;
  *arg_translated = 0;
  *error = ERR_INVALID;
  return UNDEFINED;
}

static int get_word(char *dest, size_t left, char *source, char t)
{
  size_t i = 0;
  char current;

  current = *source;

  while((current != t) && (current != 0) && (current != '\n') && (i + 1 < left))
  {
    source += escape_chars(dest + i, source);
    current = *source;

    i++;
  }

  dest[i] = 0;

  return i;
}

static const struct search_entry sorted_command_list[] =
{
  { "%",              1, { 116 } },
  { "&",              1, { 117 } },
  { "*",              1, { 102 } },
  { ".",              1, { 107 } },
  { "/",              1, { 101 } },
  { ":",              1, { 106 } },
  { "?",              2, { 104, 105 } },
  { "[",              1, { 103 } },
  { "abort",          1, { 253 } },
  { "ask",            1, { 145 } },
  { "avalanche",      1, { 131 } },
  { "become",         5, { 124, 125, 133, 134, 6 } },
  { "blind",          1, { 126 } },
  { "board",          2, { 121, 122 } },
  { "bulletcolor",    1, { 137 } },
  { "bullete",        1, { 89 } },
  { "bulletn",        1, { 87 } },
  { "bullets",        1, { 88 } },
  { "bulletw",        1, { 90 } },
  { "center",         1, { 154 } },
  { "change",         7, { 135, 143, 147, 148, 210, 245, 246 } },
  { "char",           2, { 123, 7 } },
  { "clear",          1, { 155 } },
  { "clip",           1, { 202 } },
  { "color",          5, { 211, 212, 213, 214, 8 } },
  { "copy",           5, { 119, 132, 201, 206, 243 } },
  { "copyrobot",      3, { 82, 83, 84 } },
  { "cycle",          1, { 3 } },
  { "dec",            3, { 12, 15, 96 } },
  { "die",            2, { 1, 80 } },
  { "disable",        2, { 236, 254 } },
  { "divide",         1, { 218 } },
  { "double",         1, { 27 } },
  { "duplicate",      2, { 85, 86 } },
  { "enable",         3, { 235, 237, 255 } },
  { "end",            4, { 0, 41, 42, 44 } },
  { "endgame",        1, { 36 } },
  { "endlife",        1, { 37 } },
  { "enemy",          5, { 181, 182, 183, 184, 187 } },
  { "exchange",       3, { 152, 170, 172 } },
  { "explode",        1, { 31 } },
  { "fillhealth",     1, { 146 } },
  { "firewalker",     1, { 127 } },
  { "flip",           1, { 205 } },
  { "freezetime",     1, { 128 } },
  { "give",           1, { 33 } },
  { "givekey",        2, { 91, 92 } },
  { "go",             1, { 4 } },
  { "goto",           1, { 29 } },
  { "gotoxy",         1, { 9 } },
  { "half",           1, { 28 } },
  { "if",             19, { 112, 113, 114, 16, 17, 18, 19, 20, 21, 22,
   227, 23, 231, 24, 25, 26, 64, 65, 66 } },
  { "inc",            3, { 11, 14, 95 } },
  { "input",          1, { 111 } },
  { "jump",           1, { 144 } },
  { "laybomb",        2, { 73, 74 } },
  { "lazerwall",      1, { 78 } },
  { "load",           2, { 216, 222 } },
  { "lockplayer",     4, { 56, 58, 59, 60 } },
  { "lockscroll",     1, { 229 } },
  { "lockself",       1, { 51 } },
  { "loop",           2, { 251, 252 } },
  { "message",        2, { 139, 167 } },
  { "missilecolor",   1, { 138 } },
  { "mod",            5, { 199, 200, 224, 38, 157 } },
  { "modulo",         1, { 219 } },
  { "move",           3, { 118, 61, 62 } },
  { "multiply",       1, { 217 } },
  { "neutral",        5, { 177, 178, 179, 180, 186 } },
  { "open",           1, { 50 } },
  { "overlay",        3, { 239, 240, 241 } },
  { "persistent",     1, { 232 } },
  { "play",           2, { 43, 49 } },
  { "player",         7, { 115, 173, 174, 175, 176, 185, 220 } },
  { "playercolor",    1, { 136 } },
  { "push",           1, { 203 } },
  { "put",            6, { 100, 242, 32, 63, 67, 79 } },
  { "rel",            9, { 140, 141, 142, 193, 194, 195, 196, 197, 198 } },
  { "resetview",      1, { 156 } },
  { "restore",        4, { 151, 169, 171, 55 } },
  { "rotateccw",      1, { 70 } },
  { "rotatecw",       1, { 69 } },
  { "sam",            1, { 39 } },
  { "save",           2, { 150, 168 } },
  { "scroll",         1, { 204 } },
  { "scrollarrow",    1, { 163 } },
  { "scrollbase",     1, { 159 } },
  { "scrollcorner",   1, { 160 } },
  { "scrollpointer",  1, { 162 } },
  { "scrolltitle",    1, { 161 } },
  { "scrollview",     2, { 110, 225 } },
  { "send",           4, { 30, 53, 81, 99 } },
  { "set",            8, { 10, 120, 13, 149, 153, 166, 215, 97 } },
  { "sfx",            1, { 48 } },
  { "shoot",          1, { 72 } },
  { "shootmissile",   1, { 75 } },
  { "shootseeker",    1, { 76 } },
  { "slowtime",       1, { 129 } },
  { "spitfire",       1, { 77 } },
  { "status",         1, { 238 } },
  { "swap",           1, { 226 } },
  { "switch",         1, { 71 } },
  { "take",           2, { 34, 35 } },
  { "takekey",        2, { 93, 94 } },
  { "teleport",       1, { 109 } },
  { "trade",          1, { 98 } },
  { "try",            1, { 68 } },
  { "unlockplayer",   1, { 57 } },
  { "unlockscroll",   1, { 230 } },
  { "unlockself",     1, { 52 } },
  { "viewport",       2, { 164, 165 } },
  { "volume",         2, { 158, 40 } },
  { "wait",           4, { 2, 233, 45, 46 } },
  { "walk",           1, { 5 } },
  { "wind",           1, { 130 } },
  { "write",          1, { 247 } },
  { "zap",            1, { 54 } },
  { "|",              1, { 108 } },
};

static const int num_command_names =
 sizeof(sorted_command_list) / sizeof(struct search_entry);

static const struct search_entry *find_command(const char *name)
{
  int bottom = 0, top = num_command_names - 1, middle;
  const struct search_entry *base = sorted_command_list;
  int cmpval;

  while(bottom <= top)
  {
    middle = (top + bottom) / 2;
    cmpval = strcasecmp(name, (sorted_command_list[middle]).name);

    if(cmpval > 0)
      bottom = middle + 1;
    else

    if(cmpval < 0)
      top = middle - 1;
    else
      return base + middle;
  }

  return NULL;
}

static const char *const type_names[] =
{
  "imm",
  "imm",
  "char",
  "color",
  "dir",
  "thing",
  "param",
  "str",
  "equ",
  "cond",
  "item",
  "extra",
  "undef",
  "undef"
};

static void get_wanted_arg(char *buffer, int arg)
{
  char *str_position = buffer;
  int multi = 0;
  int i;

  if(arg & CMD)
  {
    sprintf(buffer, "\'%s\'", command_fragments[arg & ~CMD]);
  }
  else

  for(i = 0; i < 14; i++)
  {
    if(arg & (1 << i))
    {
      if(multi)
      {
        *str_position = '/';
        str_position++;
      }
      else
      {
        multi = 1;
      }

      strcpy(str_position, type_names[i]);
      str_position += strlen(type_names[i]);
    }
  }
}

static void print_error(int arg_number, char *error_buffer, int bad_arg,
 int correct_arg)
{
  char bad_arg_err[64];
  char correct_arg_err[64];

  get_wanted_arg(bad_arg_err, bad_arg);
  get_wanted_arg(correct_arg_err, correct_arg);

  sprintf(error_buffer, "a%d: expected %s, got %s", arg_number + 1,
   correct_arg_err, bad_arg_err);
}

static int match_command(struct mzx_command_rw *cmd, char *error_buffer)
{
  int i, i2, i3;
  int num_params;
  const struct search_entry *matched_command;
  const struct mzx_command *current_command;

  error_buffer[0] = 0;

  matched_command = find_command(cmd->name);

  if(matched_command)
  {
    for(i = 0; i < matched_command->count; i++)
    {
      current_command = command_list + matched_command->offsets[i];

      if(strcasecmp(cmd->name, current_command->name))
        continue;

      if(cmd->parameters != current_command->parameters)
      {
        if(!error_buffer[0])
        {
          switch(current_command->parameters)
          {
            case 0:
            {
              sprintf(error_buffer, "expected no arguments, got %d",
               cmd->parameters);
              break;
            }

            case 1:
            {
              sprintf(error_buffer, "expected 1 argument, got %d",
               cmd->parameters);
              break;
            }

            default:
            {
              sprintf(error_buffer, "expected %d arguments, got %d",
               current_command->parameters, cmd->parameters);
              break;
            }
          }
        }
        continue;
      }

      num_params = cmd->parameters;

      for(i2 = 0, i3 = 0; i2 < num_params; i2++)
      {
        if(!(current_command->param_types[i2] & IGNORE_TYPE))
        {
          if(cmd->param_types[i3] & UNDEFINED)
          {
            // Dest must be string
            if(!(current_command->param_types[i2] & STRING))
            {
              print_error(i3, error_buffer, cmd->param_types[i3],
               current_command->param_types[i2]);
              break;
            }
          }
          else

          if(current_command->param_types[i2] & CMD)
          {
            if(cmd->param_types[i3] != current_command->param_types[i2])
            {
              if(!error_buffer[0])
              {
                print_error(i3, error_buffer, cmd->param_types[i3],
                 current_command->param_types[i2]);
              }

              break;
            }
          }
          else

          if((cmd->param_types[i3] & current_command->param_types[i2]) !=
           cmd->param_types[i3])
          {
            // Can allow char if imm is allowed, and can allow imm if
            // param is allowed.
            // We must also ignore CMD and IGNORE_TYPE because these
            // cause the meanings of the bits to become overloaded.
            if(!(((cmd->param_types[i3] & CHARACTER) &&
             (current_command->param_types[i2] & (IMM_S16 | IMM_U16))) ||
             (((cmd->param_types[i3] & (IMM_S16 | IMM_U16)) &&
             (current_command->param_types[i2] & PARAM)))) ||
             (cmd->param_types[i3] & (CMD | IGNORE_TYPE)))
            {
              print_error(i3, error_buffer, cmd->param_types[i3],
               current_command->param_types[i2]);
              break;
            }
          }

          i3++;
        }
        else
        {
          // Don't count this one
          num_params++;
        }
      }

      if(i2 == num_params)
        return matched_command->offsets[i];
    }
  }
  else
  {
    sprintf(error_buffer, "command \'%s\' does not exist", cmd->name);
  }

  return -1;
}

static void rasm_skip_whitespace(char *cpos, char **next)
{
  while(*cpos == ' ')
  {
    cpos++;
  }
  *next = cpos;
}

static int assemble_command(int command_number, struct mzx_command_rw *cmd,
 void *params[32], char *obj_pos, char **next_obj_pos)
{
  int i;
  int size;
  char *c_obj_pos = obj_pos + 2;

  obj_pos[1] = (char)command_number;

  for(i = 0; i < cmd->parameters; i++)
  {
    if(cmd->param_types[i] & (STRING | UNDEFINED))
    {
      size_t str_size = strlen((char *)params[i]);
      *(c_obj_pos) = (char)str_size + 1;
      strcpy(c_obj_pos + 1, (char *)params[i]);
      c_obj_pos += str_size + 2;
    }
    else
    {
      if(!(cmd->param_types[i] & CMD))
      {
        // It's not a command fragment
        *c_obj_pos = 0;
        memcpy(c_obj_pos + 1, (char *)params[i], 2);
        c_obj_pos += 3;
      }
    }
  }

  size = (int)(c_obj_pos - obj_pos - 1);
  *obj_pos = size;
  *c_obj_pos = size;

  *next_obj_pos = c_obj_pos + 1;

  return size + 2;
}

#ifndef CONFIG_DEBYTECODE
__editor_maybe_static
#endif
int legacy_assemble_line(char *cpos, char *output_buffer, char *error_buffer,
 char *param_listing, int *arg_count_ext)
{
  char *current_line_position, *next_line_position;
  char *next;
  int translated_command = 0;
  int arg_count = 0;
  int current_arg_short = 0;
  int current_arg_type, last_arg_type;
  int current_arg_translation = 0, last_arg_translation = -1;
  int error;
  char command_name[256];
  int command_params[32];
  char temp[ROBOT_MAX_TR];
  char *first_non_space = NULL;
  void *param_list[32];
  int advance;
  int dir_modifier_buffer = 0;
  int words = 0;
  char *temp_pos = temp;
  size_t temp_left = sizeof(temp);

  struct mzx_command_rw current_command;

  current_command.name = command_name;
  current_command.param_types = command_params;

  if(cpos[0])
  {
    rasm_skip_whitespace(cpos + 1, &first_non_space);
    if(cpos[0] == ' ')
    {
      cpos = first_non_space;
      rasm_skip_whitespace(cpos + 1, &first_non_space);
    }
  }

  if(!cpos[0] || (cpos[0] == '\n'))
  {
    translated_command = 47;
    strcpy(command_name, command_list[47].name);
    current_command.parameters = 0;
    arg_count = 0;
  }
  else

  if(special_first_char[(int)cpos[0]] && (first_non_space[0] != '\"'))
  {
    size_t str_size;

    command_name[0] = cpos[0];
    command_name[1] = 0;
    current_command.parameters = 1;

    str_size = strlen(first_non_space) + 1;
    if(temp_left < str_size)
      goto err_buffer;

    memcpy(temp_pos, first_non_space, str_size);
    current_command.param_types[0] = STRING;

    param_list[0] = temp_pos;
    temp_pos += str_size;
    temp_left -= str_size;
    arg_count = 1;

    if(param_listing)
    {
      param_listing[0] = S_STRING;
      words = 1;
    }

    translated_command = match_command(&current_command, error_buffer);
  }
  else
  {
    current_line_position = cpos;

    current_line_position +=
     get_word(command_name, 256, current_line_position, ' ');
    words++;
    last_arg_type = 0;

    while((*current_line_position != 0) && (*current_line_position != '\n'))
    {
      rasm_skip_whitespace(current_line_position, &current_line_position);

      if(*current_line_position == 0)
        break;

      current_arg_type = rasm_parse_argument(current_line_position,
       &next_line_position, &current_arg_translation, &error,
       &current_arg_short);

      if(param_listing)
        param_listing[words] = current_arg_short;

      current_command.param_types[arg_count] = current_arg_type;

      if(error == ERR_BADCHARACTER)
      {
        sprintf(error_buffer, "Unterminated char constant.");
        return -1;
      }

      if(current_arg_type != EXTRA)
      {
        if(current_arg_type == UNDEFINED)
        {
          // Grab the string off the command list.
          size_t str_size =
           get_word(temp_pos, temp_left, current_line_position, ' ');

          if(temp_left < str_size + 1)
            goto err_buffer;

          param_list[arg_count] = temp_pos;
          temp_pos += str_size + 1;
          temp_left -= str_size + 1;
          advance = 1;
          dir_modifier_buffer = 0;
        }
        else

        if(current_arg_type == STRING)
        {
          // Grab the string off the command list.
          size_t str_size =
           get_word(temp_pos, temp_left, current_line_position + 1, '"');

          if(temp_left < str_size + 1)
            goto err_buffer;

          param_list[arg_count] = temp_pos;
          temp_pos += str_size + 1;
          temp_left -= str_size + 1;
          advance = 1;
          dir_modifier_buffer = 0;
        }
        else

        if(current_arg_type == DIRECTION)
        {
          if(current_arg_translation >= 16)
          {
            dir_modifier_buffer |= 1 << (current_arg_translation - 12);
            advance = 0;
          }
          else

          if((last_arg_type == CONDITION) &&
           ((last_arg_translation == 0) || (last_arg_translation == 3) ||
           (last_arg_translation == 4) || (last_arg_translation == 8) ||
           (last_arg_translation == 9)))
          {
            ((char *)param_list[arg_count - 1])[1] |=
             current_arg_translation | dir_modifier_buffer;
            advance = 0;
          }
          else
          {
            if(temp_left < 2)
              goto err_buffer;

            // Store the translation into the command list.
            temp_pos[0] = current_arg_translation | dir_modifier_buffer;
            temp_pos[1] = 0;

            param_list[arg_count] = temp_pos;
            temp_pos += 2;
            temp_left -= 2;
            advance = 1;
            dir_modifier_buffer = 0;
          }
        }
        else
        {
          if(temp_left < 2)
            goto err_buffer;

          // Store the translation into the command list.
          temp_pos[0] = current_arg_translation;
          temp_pos[1] = current_arg_translation >> 8;

          param_list[arg_count] = temp_pos;
          temp_pos += 2;
          temp_left -= 2;
          advance = 1;
          dir_modifier_buffer = 0;
        }

        if(advance)
        {
          last_arg_type = current_arg_type;
          last_arg_translation = current_arg_translation;
          arg_count++;
        }

        if(arg_count == 32)
        {
          sprintf(error_buffer, "Too many arguments to parse.");
          return -1;
        }
      }
      else
      {
        dir_modifier_buffer = 0;
      }

      current_line_position = next_line_position;
    }

    current_command.parameters = arg_count;
    translated_command = match_command(&current_command, error_buffer);

    if(translated_command == -1)
      return -1;
  }

  if(param_listing)
    *arg_count_ext = words;

  return assemble_command(translated_command,
   &current_command, param_list, output_buffer, &next);

err_buffer:
  sprintf(error_buffer, "Token %d exceeded command buffer.", arg_count);
  return -1;
}

#ifndef CONFIG_DEBYTECODE

char *assemble_file(char *name, int *size)
{
  vfile *input_file = fsafeopen(name, "rb");
  char line_buffer[256];
  char bytecode_buffer[256];
  char error_buffer[256];
  int line_bytecode_length;
  int allocated_size = 1024;
  char *buffer;
  int output_position = 1;
  int current_size = 1;

  if(!input_file)
    return NULL;

  buffer = cmalloc(1024);
  buffer[0] = 0xFF;

  // fsafegets ensures no line terminators are present
  while(vfsafegets(line_buffer, 255, input_file))
  {
    line_bytecode_length =
     legacy_assemble_line(line_buffer, bytecode_buffer, error_buffer, NULL, NULL);

    if((line_bytecode_length != -1) &&
     ((current_size + line_bytecode_length) < MAX_OBJ_SIZE))
    {
      if((current_size + line_bytecode_length) > allocated_size)
      {
        allocated_size *= 2;
        buffer = crealloc(buffer, allocated_size);
      }
      memcpy(buffer + output_position, bytecode_buffer, line_bytecode_length);
      output_position += line_bytecode_length;
      current_size += line_bytecode_length;
    }
    else
    {
      free(buffer);
      buffer = NULL;
      goto exit_out;
    }
  }

  // trim the buffer to match the output size
  buffer = crealloc(buffer, current_size + 1);
  buffer[current_size] = 0;
  *size = current_size + 1;

exit_out:
  vfclose(input_file);
  return buffer;
}

char *assemble_program(char *src, int len, int *size)
{
  char line_buffer[256];
  char bytecode_buffer[256];
  char error_buffer[256];
  int line_bytecode_length;
  int allocated_size = 1024;
  char *buffer;
  struct memfile mf;

  int output_position = 1;
  int current_size = 1;

  buffer = cmalloc(1024);
  buffer[0] = 0xFF;

  mfopen(src, len, &mf);

  // It's wasteful to copy each line, but the alternative is worse...
  while(mfsafegets(line_buffer, 256, &mf))
  {
    line_bytecode_length =
     legacy_assemble_line(line_buffer, bytecode_buffer, error_buffer, NULL, NULL);

    if((line_bytecode_length != -1) &&
     ((current_size + line_bytecode_length) < MAX_OBJ_SIZE))
    {
      if((current_size + line_bytecode_length) > allocated_size)
      {
        allocated_size *= 2;
        buffer = crealloc(buffer, allocated_size);
      }
      memcpy(buffer + output_position, bytecode_buffer, line_bytecode_length);
      output_position += line_bytecode_length;
      current_size += line_bytecode_length;
    }
    else
    {
      free(buffer);
      return NULL;
    }
  }

  // trim the buffer to match the output size
  buffer = crealloc(buffer, current_size + 1);
  buffer[current_size] = 0;
  *size = current_size + 1;

  return buffer;
}

__editor_maybe_static void print_color(int color, char *color_buffer)
{
  /* Simulate the 2.x interpreter function fix_color here to guarantee
   * that invalid values are accurately disassembled as they are interpreted.
   * Invalid values below 0 (signed 16-bit) are ultimately handled mod 256.
   * Invalid values above 0x120 are treated identically to c??.
   *
   * This interpreter behavior dates back to 2.51 or older, but the DOS
   * disassembler would treat all invalid values as c??, and port versions
   * prior to 2.93c would print one of two forms of invalid junk instead.
   */
  if((int16_t)color < 0x100)
  {
    sprintf(color_buffer, "c%02x", color & 0xff);
  }
  else

  if(color < 0x110)
  {
    sprintf(color_buffer, "c?%1x", color & 0x0f);
  }
  else

  if(color < 0x120)
  {
    sprintf(color_buffer, "c%1x?", color & 0x0f);
  }
  else
  {
    sprintf(color_buffer, "c??");
  }
}

static int print_dir(int dir, char *dir_buffer, char *arg_types,
 int arg_place)
{
  static const char *const dir_types[20] =
  {
    "IDLE", "NORTH", "SOUTH", "EAST", "WEST", "RANDNS", "RANDEW", "RANDNE",
    "RANDNB", "SEEK", "RANDANY", "BENEATH", "ANYDIR", "FLOW", "NODIR",
    "RANDB", "RANDP", "CW", "OPP", "RANDNOT"
  };

  char *dir_write = dir_buffer;

  if(dir & 0x10)
  {
    strcpy(dir_write, "RANDP ");
    if(arg_types)
    {
      arg_types[arg_place] = S_DIR;
      arg_place++;
    }
    dir_write += 6;
  }

  if(dir & 0x20)
  {
    strcpy(dir_write, "CW ");
    if(arg_types)
    {
      arg_types[arg_place] = S_DIR;
      arg_place++;
    }
    dir_write += 3;
  }

  if(dir & 0x40)
  {
    strcpy(dir_write, "OPP ");
    if(arg_types)
    {
      arg_types[arg_place] = S_DIR;
      arg_place++;
    }
    dir_write += 4;
  }

  if(dir & 0x80)
  {
    strcpy(dir_write, "RANDNOT ");
    if(arg_types)
    {
      arg_types[arg_place] = S_DIR;
      arg_place++;
    }
    dir_write += 8;
  }

  if(arg_types)
    arg_types[arg_place] = S_DIR;

  sprintf(dir_write, "%s", dir_types[dir & 0x0F]);

  return arg_place;
}

__editor_maybe_static int unescape_char(char *dest, char c)
{
  switch(c)
  {
    case '"':
      dest[0] = '\\';
      dest[1] = '"';
      return 2;

    case 0:
      dest[0] = '\\';
      dest[1] = '0';
      return 2;

    case '\n':
      dest[0] = '\\';
      dest[1] = 'n';
      return 2;

    case '\r':
      dest[0] = '\\';
      dest[1] = 'r';
      return 2;

    case '\t':
      dest[0] = '\\';
      dest[1] = 't';
      return 2;

    case '\\':
      dest[0] = '\\';
      dest[1] = '\\';
      return 2;

    default:
      dest[0] = c;
      return 1;
  }
}

__editor_maybe_static int disassemble_line(char *cpos, char **next,
 char *output_buffer, char *error_buffer, int *total_bytes,
 int print_ignores, char *arg_types, int *arg_count, int base)
{
  static const char *const equality_types[9] =
  {
    "=", "<", ">", ">=", "<=", "!=", "===", "?=", "?=="
  };

  static const char *const condition_types[18] =
  {
    "walking", "swimming", "firewalking", "touching", "blocked", "aligned",
    "alignedns", "alignedew", "lastshot", "lasttouch", "rightpressed",
    "leftpressed", "uppressed", "downpressed", "spacepressed", "delpressed",
    "musicon", "pcsfxon"
  };

  static const char *const item_types[9] =
  {
    "GEMS", "AMMOS", "TIME", "SCORE", "HEALTHS", "LIVES",
    "LOBOMBS", "HIBOMBS", "COINS"
  };

  static const char *const ignore_list[21] =
  {
    ",", ";", "a", "an", "and", "as", "at", "by", "else", "for", "from",
    "into", "is", "of", "the", "then", "there", "through", "thru", "to",
    "with"
  };

  int length = *cpos;
  int i;
  char *input_position = cpos + 2;
  char *output_position = output_buffer;
  const struct mzx_command *current_command;

  if(length)
  {
    int command_number = *(cpos + 1);
    int current_param;
    int total_params;
    int words = 0;

    current_command = &command_list[command_number];

    if(command_number != ROBOTIC_CMD_BLANK_LINE)
    {
      strcpy(output_position, current_command->name);
      output_position += strlen(current_command->name);
    }

    total_params = current_command->parameters;

    for(i = 0; i < total_params; i++, words++)
    {
      *output_position = ' ';
      output_position++;

      current_param = current_command->param_types[i];

      if(current_param & CMD)
      {
        // Command fragment
        int fragment_type = current_param & ~CMD;
        strcpy(output_position, command_fragments[fragment_type]);
        output_position += strlen(command_fragments[fragment_type]);
        if(arg_types)
          arg_types[words] = S_CMD;
      }
      else

      if(current_param & IGNORE_TYPE)
      {
        if(print_ignores)
        {
          // Ignore fragment
          int ignore_type = current_param & ~IGNORE_TYPE;
          if(arg_types)
            arg_types[words] = S_EXTRA;
          strcpy(output_position, ignore_list[ignore_type]);
          output_position += strlen(ignore_list[ignore_type]);
        }
        else
        {
          words--;
          output_position--;
        }

        total_params++;
      }
      else
      {
        int param_type = current_param;

        // If it can be a string, if it's NOT a string check if it can
        // be anything else too.
        if(param_type & STRING)
        {
          if(*input_position)
          {
            // It is a string, so make it that
            param_type &= STRING;
          }
          else
          {
            // Otherwise use the non-string part
            param_type &= ~STRING;
          }
        }

        // Give char representation, not immediate
        if(param_type & CHARACTER)
        {
          // ...unless this is an extended char in a command that supports it.
          if(input_position[2] && (param_type & EXTENDED))
            param_type = IMM_U16;
          else
            param_type &= ~IMM_U16;
        }
        param_type &= ~EXTENDED;

        switch(param_type)
        {
          case IMM_U16:
          {
            int imm = (short)(*(input_position + 1) |
             (*(input_position + 2) << 8));
            char imm_buffer[16];

            if(arg_types)
              arg_types[words] = S_IMM_U16;

            input_position += 3;
            if(base == 10)
              sprintf(imm_buffer, "%d", imm);
            else
              sprintf(imm_buffer, "$%x", imm);

            strcpy(output_position, imm_buffer);
            output_position += strlen(imm_buffer);
            break;
          }

          case CHARACTER:
          {
            int character = *(input_position + 1);

            if(arg_types)
              arg_types[words] = S_CHARACTER;

            input_position += 3;
            output_position[0] = '\'';
            output_position++;

            output_position += unescape_char(output_position, character);

            output_position[0] = '\'';
            output_position++;

            break;
          }

          case COLOR:
          {
            int color = *(input_position + 1) | (*(input_position + 2) << 8);
            char color_buffer[16];

            if(arg_types)
              arg_types[words] = S_COLOR;

            input_position += 3;
            print_color(color, color_buffer);
            strcpy(output_position, color_buffer);
            output_position += strlen(color_buffer);
            break;
          }

          case DIRECTION:
          {
            int dir = *(input_position + 1);
            char dir_buffer[64];
            words = print_dir(dir, dir_buffer, arg_types, words);

            strcpy(output_position, dir_buffer);
            output_position += strlen(dir_buffer);
            input_position += 3;
            break;
          }

          case THING:
          {
            int thing = *(input_position + 1);
            input_position += 3;

            if(arg_types)
              arg_types[words] = S_THING;

            if(thing >= (int)ARRAY_SIZE(thing_names))
            {
              strcpy(output_position, "??");
              output_position += 2;
              break;
            }

            strcpy(output_position, thing_names[thing]);
            output_position += strlen(thing_names[thing]);
            break;
          }

          case PARAM:
          {
            int param = *(input_position + 1) | (*(input_position + 2) << 8);
            char param_buffer[16];

            if(arg_types)
              arg_types[words] = S_PARAM;

            /* Note: disassembling invalid values modulo 256 to match how
             * they are interpreted. The DOS disassembler would disassemble
             * all invalid values to p?? and the port disassembler would
             * produce either p?? or junk prior to 2.93c.
             */
            input_position += 3;
            if(param == 0x100)
            {
              sprintf(param_buffer, "p??");
            }
            else
            {
              sprintf(param_buffer, "p%02x", param & 0xff);
            }

            strcpy(output_position, param_buffer);
            output_position += strlen(param_buffer);
            break;
          }

          case STRING:
          {
            int num_chars = *input_position - 1;
            int i;

            if(arg_types)
              arg_types[words] = S_STRING;

            input_position++;

            *output_position = '"';
            output_position++;

            for(i = 0; i < num_chars; i++)
            {
              output_position +=
               unescape_char(output_position, *input_position);

              input_position++;
            }

            *output_position = '"';
            input_position++;
            output_position++;

            break;
          }

          case EQUALITY:
          {
            int equality = *(input_position + 1);
            input_position += 3;

            if(arg_types)
              arg_types[words] = S_EQUALITY;

            // Is this actually a valid operator?
            if(equality < (int)ARRAY_SIZE(equality_types))
            {
              strcpy(output_position, equality_types[equality]);
              output_position += strlen(equality_types[equality]);
            }
            else
            {
              strcpy(output_position, "??");
              output_position += 2;
            }
            break;
          }

          case CONDITION:
          {
            int condition = *(input_position + 1);
            int direction = *(input_position + 2);
            input_position += 3;

            if(arg_types)
              arg_types[words] = S_CONDITION;

            if(condition >= (int)ARRAY_SIZE(condition_types))
            {
              strcpy(output_position, "??");
              output_position += 2;
              break;
            }

            strcpy(output_position, condition_types[condition]);
            output_position += strlen(condition_types[condition]);

            if((condition == 0) || (condition == 3) || (condition == 4) ||
             (condition == 8) || (condition == 9))
            {
              char dir_buffer[64];
              dir_buffer[0] = ' ';
              words = print_dir(direction, dir_buffer + 1, arg_types,
               words + 1);
              strcpy(output_position, dir_buffer);
              output_position += strlen(dir_buffer);
            }

            break;
          }

          case ITEM:
          {
            int item = *(input_position + 1);
            input_position += 3;

            if(arg_types)
              arg_types[words] = S_ITEM;

            if(item >= (int)ARRAY_SIZE(item_types))
            {
              strcpy(output_position, "??");
              output_position += 2;
              break;
            }

            strcpy(output_position, item_types[item]);
            output_position += strlen(item_types[item]);
            break;
          }
        }
      }
    }

    *output_position = 0;

    *next = cpos + (*cpos) + 2;
    *total_bytes = (int)(output_position - output_buffer);

    if(arg_types)
      *arg_count = words;

    return 1;
  }

  return 0;
}

void disassemble_file(char *name, char *program, int program_length,
 int allow_ignores, int base)
{
  vfile *output_file = fsafeopen(name, "wb");
  char command_buffer[256];
  char error_buffer[256];
  char *current_robot_pos = program + 1;
  char *next;
  int new_line;
  int line_text_length;

  if(!output_file)
    return;

  do
  {
    new_line = disassemble_line(current_robot_pos,
     &next, command_buffer, error_buffer,
     &line_text_length, allow_ignores, NULL, NULL, base);

    if(new_line)
    {
      vfwrite(command_buffer, line_text_length, 1, output_file);
      vfputc('\n', output_file);
    }

    current_robot_pos = next;
  } while(new_line);

  vfclose(output_file);
}

static inline int get_program_line_count(char *program, int program_length)
{
  char *end = program + program_length;
  int line_num = 0;

  // Skip 0xFF
  program++;

  while(program < end)
  {
    program += *program + 2;
    line_num++;
  }

  return line_num;
}

void disassemble_program(char *program, int program_length,
 char **_source, int *_source_length, struct command_mapping **_command_map,
 int *_command_map_length)
{
  // Take a program and turn in into source code plus a mapping from the
  // bytecode to sourcecode. Required for the robot debugger.

  char line_buffer[256] = { 0 };
  int line_size = 0;

  // Better to overshoot than undershoot here.
  int source_length = program_length * 2;
  char *source = cmalloc(source_length);
  int offset = 0;

  struct command_mapping *cmd_map = NULL;
  int cmd_map_length = 0;
  int i = 1;

  char *start = program;
  char *next = program + 1;

  int ret;

  if(_command_map && _command_map_length)
  {
    cmd_map_length = get_program_line_count(program, program_length);
    cmd_map = cmalloc(cmd_map_length * sizeof(struct command_mapping));

    cmd_map[0].real_line = 0;
    cmd_map[0].bc_pos = 0;
    cmd_map[0].src_pos = 0;
  }

  while(*next)
  {
    line_size = 0;

    if(cmd_map && i < cmd_map_length)
    {
      cmd_map[i].real_line = i;
      cmd_map[i].bc_pos = next - start;
      cmd_map[i].src_pos = offset;
      i++;
    }

    ret = disassemble_line(next, &next,
     line_buffer,
     NULL,        // Error buffer
     &line_size,
     1,           // Print ignored words
     NULL,        // Arg types
     NULL,        // Arg count
     10           // Numeric base
    );

    // +2 for line break and potentially the terminator
    while(source_length - offset < line_size + 2)
    {
      source_length *= 2;
      source = crealloc(source, source_length);
    }

    if(ret)
    {
      memcpy(source + offset, line_buffer, line_size);
      offset += line_size;

      source[offset] = '\n';
      offset++;
    }
  }

  source[offset] = '\0';

  // Shrink program alloc
  source_length = offset;
  source = crealloc(source, source_length + 1);

  *_source = source;
  *_source_length = source_length;

  if(_command_map && _command_map_length)
  {
    *_command_map = cmd_map;
    *_command_map_length = cmd_map_length;
  }
}
#endif // !CONFIG_DEBYTECODE

enum v1_params
{
  V1_NO_PARAM,
  V1_16,              // unsigned
  V1_U8,              // unsigned
  V1_STRING,
  V1_CHAR,
  V1_COLOR_WILDCARD,  // bit 8 is stored as bit 7 of the next byte(!).
  V1_COLOR,           // unused in ver1to2; colors with no wildcard support.
  V1_DIRECTION,
  V1_ITEM,
  V1_EQUALITY,
  V1_CONDITION,
  V1_THING,
  V1_PARAM,
  V1_PARAM_WILDCARD,  // p0 -> p??
  V1_ADD_PARAM,       // param was not present in the original command, use 256.
  V1_8,               // signed

  V1_SPECIAL_CHAR_EDIT,     // translate 14 type-2 parameters
  V1_SPECIAL_TOUCHING_DIR,  // convert direction to condition: touching [dir]
};

static const uint8_t v1_command_translation[256][8] =
{
  // cvt=convert to a different command.
  { ROBOTIC_CMD_END },
  { ROBOTIC_CMD_DIE },
  { ROBOTIC_CMD_WAIT,               V1_U8 },
  { ROBOTIC_CMD_CYCLE,              V1_U8 },
  { ROBOTIC_CMD_GO,                 V1_DIRECTION, V1_U8 },
  { ROBOTIC_CMD_WALK,               V1_DIRECTION },
  { ROBOTIC_CMD_BECOME,             V1_COLOR_WILDCARD, V1_THING, V1_PARAM },
  { ROBOTIC_CMD_CHAR,               V1_CHAR },
  { ROBOTIC_CMD_COLOR,              V1_COLOR },
  { ROBOTIC_CMD_GOTOXY,             V1_8, V1_8 },
  { ROBOTIC_CMD_SET,                V1_STRING, V1_16 },
  { ROBOTIC_CMD_INC,                V1_STRING, V1_16 },
  { ROBOTIC_CMD_DEC,                V1_STRING, V1_16 },
  { ROBOTIC_CMD_SET,                V1_STRING, V1_STRING }, // cvt
  { ROBOTIC_CMD_INC,                V1_STRING, V1_STRING }, // cvt
  { ROBOTIC_CMD_DEC,                V1_STRING, V1_STRING }, // cvt
  { ROBOTIC_CMD_IF,                 V1_STRING, V1_EQUALITY, V1_16, V1_STRING },
  { ROBOTIC_CMD_IF,                 V1_STRING, V1_EQUALITY, V1_STRING, V1_STRING }, // cvt
  { ROBOTIC_CMD_IF_CONDITION,       V1_CONDITION, V1_STRING },
  { ROBOTIC_CMD_IF_NOT_CONDITION,   V1_CONDITION, V1_STRING },
  { ROBOTIC_CMD_IF_ANY,             V1_COLOR_WILDCARD, V1_THING, V1_ADD_PARAM, V1_STRING },
  { ROBOTIC_CMD_IF_NO,              V1_COLOR_WILDCARD, V1_THING, V1_ADD_PARAM, V1_STRING },
  { ROBOTIC_CMD_IF_THING_DIR,       V1_COLOR_WILDCARD, V1_THING, V1_ADD_PARAM, V1_DIRECTION, V1_STRING },
  { ROBOTIC_CMD_IF_NOT_THING_DIR,   V1_COLOR_WILDCARD, V1_THING, V1_ADD_PARAM, V1_DIRECTION, V1_STRING },
  { ROBOTIC_CMD_IF_THING_XY,        V1_COLOR_WILDCARD, V1_THING, V1_ADD_PARAM, V1_8, V1_8, V1_STRING },
  { ROBOTIC_CMD_IF_AT,              V1_8, V1_8, V1_STRING },
  { ROBOTIC_CMD_IF_DIR_OF_PLAYER,   V1_DIRECTION, V1_COLOR_WILDCARD, V1_THING, V1_ADD_PARAM, V1_STRING },
  { ROBOTIC_CMD_DOUBLE,             V1_STRING },
  { ROBOTIC_CMD_HALF,               V1_STRING },
  { ROBOTIC_CMD_GOTO,               V1_STRING },
  { ROBOTIC_CMD_SEND,               V1_STRING, V1_STRING },
  { ROBOTIC_CMD_EXPLODE,            V1_U8 },
  { ROBOTIC_CMD_PUT_DIR,            V1_COLOR, V1_THING, V1_PARAM, V1_DIRECTION },
  { ROBOTIC_CMD_GIVE,               V1_16, V1_ITEM },
  { ROBOTIC_CMD_TAKE,               V1_16, V1_ITEM },
  { ROBOTIC_CMD_TAKE_OR,            V1_16, V1_ITEM, V1_STRING },
  { ROBOTIC_CMD_ENDGAME },
  { ROBOTIC_CMD_ENDLIFE },
  { ROBOTIC_CMD_MOD,                V1_STRING },
  { ROBOTIC_CMD_SAM,                V1_16, V1_STRING },
  { ROBOTIC_CMD_VOLUME,             V1_U8 },
  { ROBOTIC_CMD_END_MOD },
  { ROBOTIC_CMD_END_SAM },
  { ROBOTIC_CMD_PLAY,               V1_STRING },
  { ROBOTIC_CMD_END_PLAY },
  { ROBOTIC_CMD_WAIT_THEN_PLAY,     V1_STRING },
  { ROBOTIC_CMD_WAIT_PLAY },
  { ROBOTIC_CMD_BLANK_LINE },
  { ROBOTIC_CMD_SFX,                V1_U8 },
  { ROBOTIC_CMD_PLAY_IF_SILENT,     V1_STRING },
  { ROBOTIC_CMD_OPEN,               V1_DIRECTION },
  { ROBOTIC_CMD_LOCKSELF },
  { ROBOTIC_CMD_UNLOCKSELF },
  { ROBOTIC_CMD_SEND_DIR,           V1_DIRECTION, V1_STRING },
  { ROBOTIC_CMD_ZAP,                V1_STRING, V1_U8 },
  { ROBOTIC_CMD_RESTORE,            V1_STRING, V1_U8 },
  { ROBOTIC_CMD_LOCKPLAYER },
  { ROBOTIC_CMD_UNLOCKPLAYER },
  { ROBOTIC_CMD_LOCKPLAYER_NS },
  { ROBOTIC_CMD_LOCKPLAYER_EW },
  { ROBOTIC_CMD_LOCKPLAYER_ATTACK },
  { ROBOTIC_CMD_MOVE_PLAYER_DIR,    V1_DIRECTION },
  { ROBOTIC_CMD_MOVE_PLAYER_DIR_OR, V1_DIRECTION, V1_STRING },
  { ROBOTIC_CMD_PUT_PLAYER_XY,      V1_8, V1_8, },
  { ROBOTIC_CMD_IF_CONDITION,       V1_SPECIAL_TOUCHING_DIR, V1_STRING }, // cvt
  { ROBOTIC_CMD_IF_NOT_CONDITION,   V1_SPECIAL_TOUCHING_DIR, V1_STRING }, // cvt
  { ROBOTIC_CMD_IF_PLAYER_XY,       V1_8, V1_8, V1_STRING },
  { ROBOTIC_CMD_PUT_PLAYER_DIR,     V1_DIRECTION },
  { ROBOTIC_CMD_TRY_DIR,            V1_DIRECTION, V1_STRING },
  { ROBOTIC_CMD_ROTATECW },
  { ROBOTIC_CMD_ROTATECCW },
  { ROBOTIC_CMD_SWITCH,             V1_DIRECTION, V1_DIRECTION },
  { ROBOTIC_CMD_SHOOT,              V1_DIRECTION },
  { ROBOTIC_CMD_LAYBOMB,            V1_DIRECTION },
  { ROBOTIC_CMD_LAYBOMB_HIGH,       V1_DIRECTION },
  { ROBOTIC_CMD_SHOOTMISSILE,       V1_DIRECTION },
  { ROBOTIC_CMD_SHOOTSEEKER,        V1_DIRECTION },
  { ROBOTIC_CMD_SPITFIRE,           V1_DIRECTION },
  { ROBOTIC_CMD_LAZERWALL,          V1_DIRECTION, V1_U8 },
  { ROBOTIC_CMD_PUT_XY,             V1_COLOR, V1_THING, V1_PARAM, V1_8, V1_8 },
  { ROBOTIC_CMD_DIE_ITEM },
  { ROBOTIC_CMD_SEND_XY,            V1_8, V1_8, V1_STRING },
  { ROBOTIC_CMD_COPYROBOT_NAMED,    V1_STRING },
  { ROBOTIC_CMD_COPYROBOT_XY,       V1_8, V1_8 },
  { ROBOTIC_CMD_COPYROBOT_DIR,      V1_DIRECTION },
  { ROBOTIC_CMD_DUPLICATE_SELF_DIR, V1_DIRECTION },
  { ROBOTIC_CMD_DUPLICATE_SELF_XY,  V1_8, V1_8 },
  { ROBOTIC_CMD_BULLETN,            V1_CHAR },
  { ROBOTIC_CMD_BULLETS,            V1_CHAR },
  { ROBOTIC_CMD_BULLETE,            V1_CHAR },
  { ROBOTIC_CMD_BULLETW,            V1_CHAR },
  { ROBOTIC_CMD_GIVEKEY,            V1_COLOR },
  { ROBOTIC_CMD_GIVEKEY_OR,         V1_COLOR, V1_STRING },
  { ROBOTIC_CMD_TAKEKEY,            V1_COLOR },
  { ROBOTIC_CMD_TAKEKEY_OR,         V1_COLOR, V1_STRING },
  { ROBOTIC_CMD_INC_RANDOM,         V1_STRING, V1_16, V1_16 },
  { ROBOTIC_CMD_DEC_RANDOM,         V1_STRING, V1_16, V1_16 },
  { ROBOTIC_CMD_SET_RANDOM,         V1_STRING, V1_16, V1_16 },
  { ROBOTIC_CMD_TRADE,              V1_16, V1_ITEM, V1_16, V1_ITEM, V1_STRING },
  { ROBOTIC_CMD_SEND_DIR_PLAYER,    V1_DIRECTION, V1_STRING },
  { ROBOTIC_CMD_PUT_DIR_PLAYER,     V1_COLOR, V1_THING, V1_PARAM, V1_DIRECTION },
  { ROBOTIC_CMD_SLASH,              V1_STRING },
  { ROBOTIC_CMD_MESSAGE_LINE,       V1_STRING },
  { ROBOTIC_CMD_MESSAGE_BOX_LINE,   V1_STRING },
  { ROBOTIC_CMD_MESSAGE_BOX_OPTION, V1_STRING, V1_STRING },
  { ROBOTIC_CMD_MESSAGE_BOX_MAYBE_OPTION, V1_STRING, V1_STRING, V1_STRING },
  { ROBOTIC_CMD_LABEL,              V1_STRING },
  { ROBOTIC_CMD_COMMENT,            V1_STRING },
  { ROBOTIC_CMD_ZAPPED_LABEL,       V1_STRING },
  { ROBOTIC_CMD_TELEPORT,           V1_STRING, V1_8, V1_8 },
  { ROBOTIC_CMD_SCROLLVIEW,         V1_DIRECTION, V1_U8 },
  { ROBOTIC_CMD_INPUT,              V1_STRING },
  { ROBOTIC_CMD_IF_INPUT,           V1_STRING, V1_STRING },
  { ROBOTIC_CMD_IF_INPUT_NOT,       V1_STRING, V1_STRING },
  { ROBOTIC_CMD_IF_INPUT_MATCHES,   V1_STRING, V1_STRING },
  { ROBOTIC_CMD_PLAYER_CHAR,        V1_CHAR },
  { ROBOTIC_CMD_MESSAGE_BOX_COLOR_LINE, V1_STRING },
  { ROBOTIC_CMD_MESSAGE_BOX_CENTER_LINE, V1_STRING },
  { ROBOTIC_CMD_MOVE_ALL,           V1_COLOR_WILDCARD, V1_THING, V1_ADD_PARAM, V1_DIRECTION },
  { ROBOTIC_CMD_COPY,               V1_8, V1_8, V1_8, V1_8 },
  { ROBOTIC_CMD_SET_EDGE_COLOR,     V1_COLOR },
  { ROBOTIC_CMD_BOARD,              V1_DIRECTION, V1_STRING },
  { ROBOTIC_CMD_BOARD_IS_NONE,      V1_DIRECTION },
  { ROBOTIC_CMD_CHAR_EDIT,          V1_CHAR, V1_SPECIAL_CHAR_EDIT },
  { ROBOTIC_CMD_BECOME_PUSHABLE },
  { ROBOTIC_CMD_BECOME_NONPUSHABLE },
  { ROBOTIC_CMD_BLIND,              V1_U8 },
  { ROBOTIC_CMD_FIREWALKER,         V1_U8 },
  { ROBOTIC_CMD_FREEZETIME,         V1_U8 },
  { ROBOTIC_CMD_SLOWTIME,           V1_U8 },
  { ROBOTIC_CMD_WIND,               V1_U8 },
  { ROBOTIC_CMD_AVALANCHE },
  { ROBOTIC_CMD_COPY_DIR,           V1_DIRECTION, V1_DIRECTION },
  { ROBOTIC_CMD_BECOME_LAVAWALKER },
  { ROBOTIC_CMD_BECOME_NONLAVAWALKER },
  { ROBOTIC_CMD_CHANGE,             V1_COLOR_WILDCARD, V1_THING, V1_ADD_PARAM,
                                    V1_COLOR_WILDCARD, V1_THING, V1_PARAM_WILDCARD },
  { ROBOTIC_CMD_PLAYERCOLOR,        V1_COLOR },
  { ROBOTIC_CMD_BULLETCOLOR,        V1_COLOR },
  { ROBOTIC_CMD_MISSILECOLOR,       V1_COLOR },
  { ROBOTIC_CMD_MESSAGE_ROW,        V1_U8 },
  { ROBOTIC_CMD_REL_SELF },
  { ROBOTIC_CMD_REL_PLAYER },
  { ROBOTIC_CMD_REL_COUNTERS },
  { ROBOTIC_CMD_SET_ID_CHAR,        V1_16, V1_CHAR },
  { ROBOTIC_CMD_JUMP_MOD_ORDER,     V1_U8 },
  { ROBOTIC_CMD_ASK,                V1_STRING },
  { ROBOTIC_CMD_FILLHEALTH },
  { ROBOTIC_CMD_THICK_ARROW,        V1_DIRECTION, V1_CHAR },
  { ROBOTIC_CMD_THIN_ARROW,         V1_DIRECTION, V1_CHAR },
  { ROBOTIC_CMD_SET_MAX_HEALTH,     V1_16 },
  { ROBOTIC_CMD_SAVE_PLAYER_POSITION },
  { ROBOTIC_CMD_RESTORE_PLAYER_POSITION },
  { ROBOTIC_CMD_EXCHANGE_PLAYER_POSITION },
  { ROBOTIC_CMD_MESSAGE_COLUMN,     V1_U8 },
  { ROBOTIC_CMD_CENTER_MESSAGE },
  { ROBOTIC_CMD_CLEAR_MESSAGE },
  { ROBOTIC_CMD_RESETVIEW },
  { ROBOTIC_CMD_MOD_SAM,            V1_16, V1_U8 },
  { ROBOTIC_CMD_VOLUME,             V1_STRING }, // cvt
  { ROBOTIC_CMD_SCROLLBASE,         V1_COLOR },
  { ROBOTIC_CMD_SCROLLCORNER,       V1_COLOR },
  { ROBOTIC_CMD_SCROLLTITLE,        V1_COLOR },
  { ROBOTIC_CMD_SCROLLPOINTER,      V1_COLOR },
  { ROBOTIC_CMD_SCROLLARROW,        V1_COLOR },
  { ROBOTIC_CMD_VIEWPORT,           V1_U8, V1_U8 },
  { ROBOTIC_CMD_VIEWPORT_WIDTH,     V1_U8, V1_U8 },
  { ROBOTIC_CMD_MESSAGE_COLUMN,     V1_STRING }, // cvt
  { ROBOTIC_CMD_MESSAGE_ROW,        V1_STRING }, // cvt

  /* The following future commands aren't supported by MegaZeux 1.x, but
   * are allowed to make automated testing of 1.x worlds possible. */
  [ROBOTIC_CMD_SWAP_WORLD] = { ROBOTIC_CMD_SWAP_WORLD, V1_STRING },
};

boolean legacy_convert_v1_program(char **_dest, int *_dest_len,
 int *_cur_prog_line, const char *src, int src_len)
{
  uint8_t buf[257];
  size_t dest_len;
  size_t dest_alloc;
  int cur_prog_line = *_cur_prog_line;
  int i;
  char *dest;
  char *tmp;

  // Special case- replace very short programs with FF 00.
  if(src_len <= 2)
  {
    dest = (char *)cmalloc(2);
    if(!dest)
      return false;

    dest[0] = 0xff;
    dest[1] = 0;
    *_dest = dest;
    *_dest_len = 2;
    *_cur_prog_line = 0;
    return true;
  }

  // Fix out-of-bounds cur_prog_line.
  if(cur_prog_line < 0 || cur_prog_line >= src_len)
  {
    debug("Robot has out-of-bounds v1 cur_prog_line %d (len: %d)\n",
     cur_prog_line, src_len);
    cur_prog_line = 0;
  }

  dest_alloc = src_len * 2;
  dest = (char *)cmalloc(dest_alloc);
  if(!dest)
    return false;

  dest[0] = 0xff;
  dest_len = 1;

  for(i = 1; i + 3 < src_len && *src;)
  {
    int cmd_off = i;
    int cmd_len = src[i++];
    int cmd = src[i++];
    int next_cmd = i + cmd_len;
    int len = 1;
    int j;

    if(next_cmd >= src_len)
    {
      trace("invalid 1.x command: length extends past program, truncating\n");
      break;
    }

    // All Robo-P commands past MESSAGE ROW "str" (167) should be treated as
    // NOPs, unless some special forward compatibilty is needed (SWAP WORLD).
    if(cmd > ROBOTIC_CMD_UNUSED_167 && cmd != ROBOTIC_CMD_SWAP_WORLD)
      cmd = ROBOTIC_CMD_BLANK_LINE;

    buf[len++] = v1_command_translation[cmd][0];

    for(j = 1; v1_command_translation[cmd][j]; j++)
    {
      enum v1_params type = v1_command_translation[cmd][j];
      switch(type)
      {
        case V1_16:
        case V1_CONDITION:
        {
          if(i + 2 > src_len || len + 3 > 255)
            goto err;

          buf[len++] = 0;
          buf[len++] = src[i++];
          buf[len++] = src[i++];
          break;
        }

        case V1_U8:
        case V1_CHAR:
        case V1_COLOR: // no thing for wildcard, or wildcard is ignored
        case V1_DIRECTION:
        case V1_ITEM:
        case V1_EQUALITY:
        {
          if(i >= src_len || len + 3 > 255)
            goto err;

          buf[len++] = 0;
          buf[len++] = src[i++];
          buf[len++] = 0;
          break;
        }

        case V1_STRING:
        {
          int start = len;
          int str_len = 0;
          // Reserve length byte.
          len++;

          while(len < 255 && i < src_len && src[i])
          {
            if(src[i] == '&')
            {
              // Convert & to &&. ver1to2 did this conditionally for (mainly)
              // text displaying commands, but it really ought to be applied
              // unconditionally.
              buf[len++] = '&';
              str_len++;

              if(len >= 255)
                goto err;
            }
            buf[len++] = src[i++];
            str_len++;
          }
          if(len >= 255 || i >= src_len)
            goto err;

          buf[start] = str_len + 1;
          buf[len++] = '\0';
          i++;
          break;
        }

        case V1_COLOR_WILDCARD:
        {
          if(i >= src_len || len + 3 > 255)
            goto err;

          buf[len++] = 0;
          buf[len++] = src[i++];

          // bit 8 is stored as bit 7 of the thing
          if(i + 1 <= src_len && v1_command_translation[cmd][j + 1] == V1_THING)
            buf[len++] = src[i] >> 7;
          else
            buf[len++] = 0;
          break;
        }

        case V1_THING:
        {
          if(i >= src_len || len + 3 > 255)
            goto err;

          // Bit 7 is used to store color bit 8 in some cases.
          buf[len++] = 0;
          buf[len++] = src[i++] & 0x7f;
          buf[len++] = 0;
          break;
        }

        case V1_PARAM:
        case V1_PARAM_WILDCARD:
        {
          int param;
          if(i >= src_len || len + 3 > 255)
            goto err;
          param = src[i++];

          // p?? encodes to a value of 0(!), convert to 256.
          // VER1TO2 did this unconditionally, which only worked because
          // non-CHANGE replacement commands treated p?? like 0 until 2.80d.
          if(type == V1_PARAM_WILDCARD && param == 0)
            param = 256;

          buf[len++] = 0;
          buf[len++] = param & 0xff;
          buf[len++] = param >> 8;
          break;
        }

        case V1_ADD_PARAM:
        {
          if(len + 3 > 255)
            goto err;

          buf[len++] = 0;
          buf[len++] = 0;
          buf[len++] = (256 >> 8);
          break;
        }

        case V1_8:
        {
          int16_t param;
          if(i >= src_len || len + 3 > 255)
            goto err;

          param = (int8_t)src[i++];

          buf[len++] = 0;
          buf[len++] = param & 0xff;
          buf[len++] = param >> 8;
          break;
        }

        case V1_SPECIAL_CHAR_EDIT:
        {
          // Hack so the parameter type mapping table doesn't need to be huge.
          int k;
          for(k = 0; k < 14; k++)
          {
            if(i >= src_len || len + 3 > 255)
              goto err;

            buf[len++] = 0;
            buf[len++] = src[i++];
            buf[len++] = 0;
          }
          break;
        }

        case V1_SPECIAL_TOUCHING_DIR:
        {
          // Input is the DIR parameter of IF [NOT] PLAYER DIR "label".
          // This gets converted to IF [NOT] TOUCHING DIR "label".
          if(i >= src_len || len + 3 > 255)
            goto err;

          buf[len++] = 0;
          buf[len++] = TOUCHING;
          buf[len++] = src[i++];
          break;
        }

        default:
        {
          warn("unhandled 1.x parameter type! FIXME!!!\n");
          break;
        }
      }
    }

    if(i >= src_len || i >= next_cmd)
    {
      trace("invalid 1.x command: params too long, truncating.\n");
      break;
    }
    // Chronos Stasis 1.01 has some unusual corrupted robots with broken
    // trailing length bytes. This is safe to quietly fix.
    if(src[next_cmd - 1] != cmd_len)
      trace("invalid 1.x command: trailing length != starting length.\n");

    // Convert v1 cur_prog_line to v2 cur_prog_line.
    // Any value in the middle of a command is junk and should be filtered.
    if(cur_prog_line > cmd_off && cur_prog_line < next_cmd)
    {
      debug("Robot has invalid v1 cur_prog_line %d @ offset %d (len: %d)\n",
       cur_prog_line, cmd_off, src_len);
      cur_prog_line = 0;
    }
    if(cur_prog_line == cmd_off)
      cur_prog_line = dest_len;

    i = next_cmd;

    buf[0] = len - 1;
    buf[len] = len - 1;
    len++;

    // +1 for program terminating 0.
    if(dest_len + len + 1 > dest_alloc)
    {
      while(dest_len + len + 1 > dest_alloc)
      {
        if(dest_alloc > INT_MAX)
          goto err;

        dest_alloc *= 2;
      }
      tmp = (char *)crealloc(dest, dest_alloc);
      if(!tmp)
        goto err;

      dest = tmp;
    }
    memcpy(dest + dest_len, buf, len);
    dest_len += len;
  }

  dest[dest_len++] = '\0';

  tmp = (char *)crealloc(dest, dest_len);
  if(tmp)
    dest = tmp;

  *_dest = dest;
  *_dest_len = dest_len;
  *_cur_prog_line = cur_prog_line;
  return true;

err:
  free(dest);
  return false;
}

enum bytecode_fix_status
{
  BC_NO_ERROR,
  BC_COULDNT_FIX,
  BC_MISSING_TERMINATOR,
  BC_WRONG_TERMINATOR,
  BC_APPLIED_PATCH,
  BC_REPLACED_WITH_NOP
};

struct program_patch
{
  const char *bad_command;
  const int bad_command_length;
  const char *patch;
  const int patch_length;
  const boolean require_exact_command; // if false, the match can be a prefix.
  const boolean is_end_of_program;
};

static const struct program_patch program_patches[] =
{
  // Fix the global robot from worlds produced by Mikawo's zzt2mzx generator.
  // The correct end of the robot exists in the template board supplied with
  // the converter, so it's known exactly what should be there.
  {
    "\x04\x30\x03\x00\x01\x6E", 6,
    "\x04\x30\x00\x26\x00\x04"
    "\x04\x5B\x00\x0E\x00\x04"
    "\x01\x00\x01"
    "\x07\x6A\x05!^wk\x00\x07"
    "\x01\x2C\x01"
    "\x04\x30\x00\x26\x00\x04"
    "\x04\x5B\x00\x0F\x00\x04"
    "\x01\x00\x01\x00", 43,
    true,
    true
  },
  // Loco, board 2, 45 19
  {
    "\x04\x1D\x02\x61\x00\x7E", 6,
    "\x04\x1D\x02\x61\x00\x04"
    "\x04\x6A\x02\x62\x00\x04"
    "\x19\x4F\x00\x20\x01\x00\x05\x00", 20,
    true,
    false
  },
  // Manuel the Manx, board 6, 3 25
  // There's no recoverable data here, but this board is inaccessible anyway.
  {
    "\x0F\x6E\x00\x04\x00\x00Aloopcoun\x84\x84\x84", 17,
    "\x00", 1,
    true,
    true
  },
  // Slave Pit global robot (no recoverable data)
  {
    "\xFF\xFF\xFF\x00\x00\x00\xFF\xFF\xFF\x88\x01\x00", 12,
    "\x00", 1,
    false,
    true
  },
};

static void validate_legacy_bytecode_print(char *bc, int program_length,
 int offset)
{
#ifdef DEBUG
  char *err_mesg;
  char *pos;
  int command_length;
  int print_length;
  int i;

  if(offset > program_length)
  {
    fprintf(mzxerr, "\n");
    debug("Offset exceeded program length\n");
    debug("Prog len: %d    Offset: %d\n", program_length, offset);
    return;
  }
  else

  if(offset == program_length)
  {
    debug("Offset reached end of program (length %d)\n", program_length);
    return;
  }

  command_length = bc[offset];
  print_length = MIN(command_length + 2, program_length - offset);

  err_mesg = cmalloc(sizeof(char) * (print_length * 3 + 2));
  err_mesg[0] = 0;
  pos = err_mesg;

  for(i = 0; i < print_length; i++)
  {
    snprintf(pos, 4, "%X ", bc[offset + i]);
    pos += strlen(pos);
  }

  debug("Prog len: %i    Offset: %i\n", program_length, offset);
  debug("Bytecode: %s\n", err_mesg);
  free(err_mesg);
#endif
}

static enum bytecode_fix_status validate_legacy_bytecode_attempt_fix(char **_bc,
 int *_len, int offset)
{
  const struct program_patch *p;
  char *bc = *_bc;
  int program_length = *_len;
  int command_length;
  int left = program_length - offset;
  size_t i;

  // Can't do anything in this case.
  if(offset > program_length)
    return BC_COULDNT_FIX;

  if(left == 0)
  {
    // Might just be a truncated last byte, which is an easy fix.
    bc = crealloc(bc, program_length + 1);
    bc[program_length] = '\x00';
    *_len = program_length + 1;
    *_bc = bc;
    debug("Added missing terminator to truncated program\n\n");
    return BC_MISSING_TERMINATOR;
  }
  else

  if(left == 1)
  {
    // Could also be a wrong last byte (Sidewinder's Engines).
    debug("Corrected program terminator %X\n", bc[offset]);
    bc[offset] = '\x00';
    return BC_WRONG_TERMINATOR;
  }
  else

  if(left == 2 && bc[offset + 1] == '\x00')
  {
    // Due to Eternal Eclipse Taoyarin 1.0 consistently having these malformed
    // bytecode endings, this seems to have been a general MZX bug. Just zero
    // out the first byte and decrease the length by one.
    debug("Corrected program terminator %X %X\n", bc[offset], bc[offset+1]);
    bc[offset] = '\x00';
    *_len = program_length - 1;
    return BC_WRONG_TERMINATOR;
  }

  // The rest of the fixes are more technical, so print detailed debug output.
  validate_legacy_bytecode_print(bc, program_length, offset);

  command_length = MIN(bc[offset] + 2, left);

  // Check for specific patches. The bad command has to exactly match what's
  // in the list for a patch to be applied.
  for(i = 0; i < ARRAY_SIZE(program_patches); i++)
  {
    p = &program_patches[i];
    if(left < p->bad_command_length || command_length < p->bad_command_length)
      continue;

    // If this is set, the match string can't be a prefix of the command.
    if(p->require_exact_command && command_length > p->bad_command_length)
      continue;

    if(!memcmp(bc + offset, p->bad_command, p->bad_command_length))
    {
      int new_length = program_length;
      boolean add_terminator = false;

      if(p->is_end_of_program)
        new_length += p->patch_length - left;

      if(left <= p->patch_length && !p->is_end_of_program)
      {
        new_length++;
        add_terminator = true;
      }

      if(new_length != program_length)
        bc = crealloc(bc, new_length);

      memcpy(bc + offset, p->patch, p->patch_length);

      if(add_terminator)
        bc[offset + p->patch_length] = '\x00';

      *_len = new_length;
      *_bc = bc;
      debug("Applied command patch %zu\n\n", i);
      return BC_APPLIED_PATCH;
    }
  }

  // If the bad command is consistent with itself, NOP it.
  if(bc[offset] + offset + 2 <= program_length)
  {
    int cmd_len = bc[offset];
    int end_len = bc[cmd_len + offset + 1];

    if(cmd_len > 0 && cmd_len == end_len)
    {
      bc[offset + 1] = ROBOTIC_CMD_BLANK_LINE;
      debug("Replaced invalid command with NOP\n\n");
      return BC_REPLACED_WITH_NOP;
    }
  }

  // Truncate the program and display an error.
  // NOTE: returning this probably means this will get dummied out right now,
  // so don't bother with reallocation.
  //bc = crealloc(bc, offset + 1);
  bc[offset] = '\x00';
  //*_bc = bc;
  //*_len = offset + 1;
  debug("Truncated program at invalid command\n\n");
  return BC_COULDNT_FIX;
}

static inline int check_numeric_param(const char *bc, int min, int max)
{
  if(bc[0] == 0)
  {
    int val = bc[1] | ((signed char)bc[2] << 8);
    if(val >= min && val <= max)
      return val;
  }
  return INT_MIN;
}

boolean validate_legacy_bytecode(char **_bc, int *_program_length,
 int *_cur_prog_line)
{
  char *bc = *_bc;
  int program_length = *_program_length;
  int cur_prog_line = _cur_prog_line ? *_cur_prog_line : 0;
  int cur_command_start = 0, cur_command_length = 0, cur_param_length = 0;
  int cur_command, p;
  int i = 1;
  enum bytecode_fix_status status = BC_NO_ERROR;

  if(!bc)
    return false;

  // First -- fix the odd robots that appear in old MZX games,
  // such as Forest of Ruin and Catacombs of Zeux.
  if(program_length <= 2)
  {
    if(program_length < 2)
    {
      bc = crealloc(bc, 2);
      program_length = 2;
    }
    bc[0] = 0xFF;
    bc[1] = 0x0;
  }

  // Error out if the start of the program is invalid.
  if(bc[0] != 0xFF)
    status = BC_COULDNT_FIX;

  // One iteration should be a single command.
  while(status != BC_COULDNT_FIX)
  {
    cur_command_length = bc[i];
    i++;
    if(cur_command_length == 0)
      break;

    cur_command_start = i;

    if(i + cur_command_length >= program_length ||
     bc[i + cur_command_length] != cur_command_length)
    {
      i--;
      status = validate_legacy_bytecode_attempt_fix(&bc, &program_length, i);
      continue;
    }

    cur_command = bc[i];
    i++;

    for(p = 0; p < command_list[cur_command].parameters; p++)
    {
      int param_type = command_list[cur_command].param_types[p];

      if((param_type & IGNORE_TYPE) || (param_type & CMD))
        continue;

      cur_param_length = bc[i];
      if(cur_param_length == 0)
        cur_param_length = 2;

      // THINGs must be length 0 and smaller than 127
      if((param_type & THING) && check_numeric_param(bc + i, 0, 127) < 0)
      {
        i = cur_command_start - 1;
        status = validate_legacy_bytecode_attempt_fix(&bc, &program_length, i);
        break;
      }

      // ITEMs must be length 0 and smaller than the number of ITEM types.
      if((param_type & ITEM) && check_numeric_param(bc + i, 0, 8) < 0)
      {
        i = cur_command_start - 1;
        status = validate_legacy_bytecode_attempt_fix(&bc, &program_length, i);
        break;
      }

      i += cur_param_length + 1;
    }

    // Retry patched command...
    if(i < cur_command_start)
      continue;

    if((i > cur_command_start + cur_command_length + 1) || (i > program_length))
    {
      i = cur_command_start - 1;
      status = validate_legacy_bytecode_attempt_fix(&bc, &program_length, i);
      continue;
    }

    // Now that the command is known to be roughly valid: cur_prog_line anywhere
    // between the command byte and the second length byte is invalid.
    if(cur_prog_line >= cur_command_start &&
     cur_prog_line <= cur_command_start + cur_command_length)
    {
      debug("Robot has invalid cur_prog_line %d @ offset %d (len: %d)\n",
       cur_prog_line, cur_command_start - 1, *_program_length);
      cur_prog_line = 0;
    }

    i = cur_command_start + cur_command_length + 1;
  }

  if((status != BC_COULDNT_FIX) && (i < program_length))
  {
    debug("Robot checked for %i but program length is %i; extra removed\n",
     program_length, i);

    bc = crealloc(bc, i);
    program_length = i;
  }

  *_bc = bc;
  *_program_length = program_length;

  if((status == BC_COULDNT_FIX) || (i > program_length))
    return false;

  // Fix out-of-bounds cur_prog_line too.
  if(cur_prog_line < 0 || cur_prog_line >= program_length)
  {
    debug("Robot has out-of-bounds cur_prog_line %d (old len:%d new len:%d)\n",
     cur_prog_line, *_program_length, program_length);
    cur_prog_line = 0;
  }

  if(_cur_prog_line)
    *_cur_prog_line = cur_prog_line;

  return true;
}
