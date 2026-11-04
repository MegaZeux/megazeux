/* MegaZeux
 *
 * Copyright (C) 2004 Gilead Kutnick <exophase@adelphia.net>
 * Copyright (C) 2017-2025 Alice Rowan <petrifiedrowan@gmail.com>
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
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "counter.h"
#include "data.h"
#include "rasm.h"
#include "str.h"
#include "util.h"
#include "algorithm/string_compare.h"
#include "io/fsafeopen.h"
#include "io/memfile.h"
#include "io/vio.h"

#ifdef CONFIG_DEBYTECODE

#define ARG_TYPE_FRAGMENT_NOT            ARG_TYPE_FRAGMENT | 0
#define ARG_TYPE_FRAGMENT_ANY            ARG_TYPE_FRAGMENT | 1
#define ARG_TYPE_FRAGMENT_PLAYER         ARG_TYPE_FRAGMENT | 2
#define ARG_TYPE_FRAGMENT_NS             ARG_TYPE_FRAGMENT | 3
#define ARG_TYPE_FRAGMENT_EW             ARG_TYPE_FRAGMENT | 4
#define ARG_TYPE_FRAGMENT_ATTACK         ARG_TYPE_FRAGMENT | 5
#define ARG_TYPE_FRAGMENT_ITEM           ARG_TYPE_FRAGMENT | 6
#define ARG_TYPE_FRAGMENT_SELF           ARG_TYPE_FRAGMENT | 7
#define ARG_TYPE_FRAGMENT_RANDOM         ARG_TYPE_FRAGMENT | 8
#define ARG_TYPE_FRAGMENT_STRING         ARG_TYPE_FRAGMENT | 9
#define ARG_TYPE_FRAGMENT_CHAR           ARG_TYPE_FRAGMENT | 10
#define ARG_TYPE_FRAGMENT_ALL            ARG_TYPE_FRAGMENT | 11
#define ARG_TYPE_FRAGMENT_EDIT           ARG_TYPE_FRAGMENT | 12
#define ARG_TYPE_FRAGMENT_PUSHABLE       ARG_TYPE_FRAGMENT | 13
#define ARG_TYPE_FRAGMENT_NONPUSHABLE    ARG_TYPE_FRAGMENT | 14
#define ARG_TYPE_FRAGMENT_LAVAWALKER     ARG_TYPE_FRAGMENT | 15
#define ARG_TYPE_FRAGMENT_NONLAVAWALKER  ARG_TYPE_FRAGMENT | 16
#define ARG_TYPE_FRAGMENT_ROW            ARG_TYPE_FRAGMENT | 17
#define ARG_TYPE_FRAGMENT_COUNTERS       ARG_TYPE_FRAGMENT | 18
#define ARG_TYPE_FRAGMENT_ID             ARG_TYPE_FRAGMENT | 19
#define ARG_TYPE_FRAGMENT_MOD            ARG_TYPE_FRAGMENT | 20
#define ARG_TYPE_FRAGMENT_ORDER          ARG_TYPE_FRAGMENT | 21
#define ARG_TYPE_FRAGMENT_THICK          ARG_TYPE_FRAGMENT | 22
#define ARG_TYPE_FRAGMENT_ARROW          ARG_TYPE_FRAGMENT | 23
#define ARG_TYPE_FRAGMENT_THIN           ARG_TYPE_FRAGMENT | 24
#define ARG_TYPE_FRAGMENT_MAXHEALTH      ARG_TYPE_FRAGMENT | 25
#define ARG_TYPE_FRAGMENT_POSITION       ARG_TYPE_FRAGMENT | 26
#define ARG_TYPE_FRAGMENT_MESG           ARG_TYPE_FRAGMENT | 27
#define ARG_TYPE_FRAGMENT_COLUMN         ARG_TYPE_FRAGMENT | 28
#define ARG_TYPE_FRAGMENT_COLOR          ARG_TYPE_FRAGMENT | 29
#define ARG_TYPE_FRAGMENT_SIZE           ARG_TYPE_FRAGMENT | 30
#define ARG_TYPE_FRAGMENT_BULLETN        ARG_TYPE_FRAGMENT | 31
#define ARG_TYPE_FRAGMENT_BULLETS        ARG_TYPE_FRAGMENT | 32
#define ARG_TYPE_FRAGMENT_BULLETE        ARG_TYPE_FRAGMENT | 33
#define ARG_TYPE_FRAGMENT_BULLETW        ARG_TYPE_FRAGMENT | 34
#define ARG_TYPE_FRAGMENT_BULLETCOLOR    ARG_TYPE_FRAGMENT | 35
#define ARG_TYPE_FRAGMENT_FIRST          ARG_TYPE_FRAGMENT | 36
#define ARG_TYPE_FRAGMENT_LAST           ARG_TYPE_FRAGMENT | 37
#define ARG_TYPE_FRAGMENT_FADE           ARG_TYPE_FRAGMENT | 38
#define ARG_TYPE_FRAGMENT_OUT            ARG_TYPE_FRAGMENT | 39
#define ARG_TYPE_FRAGMENT_IN             ARG_TYPE_FRAGMENT | 40
#define ARG_TYPE_FRAGMENT_BLOCK          ARG_TYPE_FRAGMENT | 41
#define ARG_TYPE_FRAGMENT_SFX            ARG_TYPE_FRAGMENT | 42
#define ARG_TYPE_FRAGMENT_INTENSITY      ARG_TYPE_FRAGMENT | 43
#define ARG_TYPE_FRAGMENT_SET            ARG_TYPE_FRAGMENT | 44
#define ARG_TYPE_FRAGMENT_PALETTE        ARG_TYPE_FRAGMENT | 45
#define ARG_TYPE_FRAGMENT_WORLD          ARG_TYPE_FRAGMENT | 46
#define ARG_TYPE_FRAGMENT_ALIGNEDROBOT   ARG_TYPE_FRAGMENT | 47
#define ARG_TYPE_FRAGMENT_GO             ARG_TYPE_FRAGMENT | 48
#define ARG_TYPE_FRAGMENT_SAVING         ARG_TYPE_FRAGMENT | 49
#define ARG_TYPE_FRAGMENT_SENSORONLY     ARG_TYPE_FRAGMENT | 50
#define ARG_TYPE_FRAGMENT_ON             ARG_TYPE_FRAGMENT | 51
#define ARG_TYPE_FRAGMENT_STATIC         ARG_TYPE_FRAGMENT | 52
#define ARG_TYPE_FRAGMENT_TRANSPARENT    ARG_TYPE_FRAGMENT | 53
#define ARG_TYPE_FRAGMENT_OVERLAY        ARG_TYPE_FRAGMENT | 54
#define ARG_TYPE_FRAGMENT_START          ARG_TYPE_FRAGMENT | 55
#define ARG_TYPE_FRAGMENT_LOOP           ARG_TYPE_FRAGMENT | 56
#define ARG_TYPE_FRAGMENT_EDGE           ARG_TYPE_FRAGMENT | 57
#define ARG_TYPE_FRAGMENT_SAM            ARG_TYPE_FRAGMENT | 58
#define ARG_TYPE_FRAGMENT_PLAY           ARG_TYPE_FRAGMENT | 59
#define ARG_TYPE_FRAGMENT_PERCENT        ARG_TYPE_FRAGMENT | 60
#define ARG_TYPE_FRAGMENT_HIGH           ARG_TYPE_FRAGMENT | 61
#define ARG_TYPE_FRAGMENT_MATCHES        ARG_TYPE_FRAGMENT | 62
#define ARG_TYPE_FRAGMENT_NONE           ARG_TYPE_FRAGMENT | 63
#define ARG_TYPE_FRAGMENT_INPUT          ARG_TYPE_FRAGMENT | 64
#define ARG_TYPE_FRAGMENT_DIR            ARG_TYPE_FRAGMENT | 65
#define ARG_TYPE_FRAGMENT_COUNTER        ARG_TYPE_FRAGMENT | 66
#define ARG_TYPE_FRAGMENT_DUPLICATE      ARG_TYPE_FRAGMENT | 67
#define ARG_TYPE_FRAGMENT_NO             ARG_TYPE_FRAGMENT | 68

#define ARG_TYPE_IGNORE_A                ARG_TYPE_IGNORE | 0
#define ARG_TYPE_IGNORE_AN               ARG_TYPE_IGNORE | 1
#define ARG_TYPE_IGNORE_AND              ARG_TYPE_IGNORE | 2
#define ARG_TYPE_IGNORE_AS               ARG_TYPE_IGNORE | 3
#define ARG_TYPE_IGNORE_AT               ARG_TYPE_IGNORE | 4
#define ARG_TYPE_IGNORE_BY               ARG_TYPE_IGNORE | 5
#define ARG_TYPE_IGNORE_ELSE             ARG_TYPE_IGNORE | 6
#define ARG_TYPE_IGNORE_FOR              ARG_TYPE_IGNORE | 7
#define ARG_TYPE_IGNORE_FROM             ARG_TYPE_IGNORE | 8
#define ARG_TYPE_IGNORE_IS               ARG_TYPE_IGNORE | 9
#define ARG_TYPE_IGNORE_OF               ARG_TYPE_IGNORE | 10
#define ARG_TYPE_IGNORE_THE              ARG_TYPE_IGNORE | 11
#define ARG_TYPE_IGNORE_THEN             ARG_TYPE_IGNORE | 12
#define ARG_TYPE_IGNORE_TO               ARG_TYPE_IGNORE | 13
#define ARG_TYPE_IGNORE_WITH             ARG_TYPE_IGNORE | 14

#define ARG_TYPE_NUMERIC_INDIRECT                                              \
 (ARG_TYPE_IMMEDIATE | ARG_TYPE_COUNTER_LOAD_NAME)                             \

#define ARG_TYPE_COLOR_INDIRECT                                                \
 (ARG_TYPE_COLOR | ARG_TYPE_COUNTER_LOAD_NAME)                                 \

#define ARG_TYPE_CHARACTER_INDIRECT                                            \
 (ARG_TYPE_CHARACTER | ARG_TYPE_IMMEDIATE | ARG_TYPE_COUNTER_LOAD_NAME)        \

#define ARG_TYPE_PARAM_INDIRECT                                                \
 (ARG_TYPE_PARAM | ARG_TYPE_COUNTER_LOAD_NAME)                                 \

// wait for N
static const enum arg_type cm2[] =
{
  ARG_TYPE_IGNORE_FOR,
  ARG_TYPE_NUMERIC_INDIRECT
};

// cycle N
static const enum arg_type cm3[] =
{
  ARG_TYPE_NUMERIC_INDIRECT
};

// go DIR for N
static const enum arg_type cm4[] =
{
  ARG_TYPE_DIRECTION,
  ARG_TYPE_IGNORE_FOR,
  ARG_TYPE_NUMERIC_INDIRECT
};

// walk N
static const enum arg_type cm5[] =
{
  ARG_TYPE_DIRECTION
};

// become COLOR ARG_TYPE_THING PARAM
static const enum arg_type cm6[] =
{
  ARG_TYPE_COLOR_INDIRECT,
  ARG_TYPE_THING,
  ARG_TYPE_PARAM_INDIRECT
};

// char CHAR
static const enum arg_type cm7[] =
{
  ARG_TYPE_CHARACTER_INDIRECT
};

// color COLOR
static const enum arg_type cm8[] =
{
  ARG_TYPE_COLOR_INDIRECT
};

// gotoxy N N
static const enum arg_type cm9[] =
{
  ARG_TYPE_NUMERIC_INDIRECT,
  ARG_TYPE_NUMERIC_INDIRECT
};

// set COUNTER to N
static const enum arg_type cm10[] =
{
  ARG_TYPE_COUNTER_STORE_NAME,
  ARG_TYPE_IGNORE_TO,
  ARG_TYPE_NUMERIC_INDIRECT
};

// inc COUNTER by N
static const enum arg_type cm11[] =
{
  ARG_TYPE_COUNTER_STORE_NAME,
  ARG_TYPE_IGNORE_BY,
  ARG_TYPE_NUMERIC_INDIRECT
};

// dec COUNTER by N
static const enum arg_type cm12[] =
{
  ARG_TYPE_COUNTER_STORE_NAME,
  ARG_TYPE_IGNORE_BY,
  ARG_TYPE_NUMERIC_INDIRECT
};

// if COUNTER EQUALITY N then LABEL
static const enum arg_type cm16[] =
{
  ARG_TYPE_COUNTER_LOAD_NAME,
  ARG_TYPE_EQUALITY,
  ARG_TYPE_NUMERIC_INDIRECT,
  ARG_TYPE_IGNORE_THEN,
  ARG_TYPE_LABEL_NAME
};

// if CONDITION then LABEL
static const enum arg_type cm18[] =
{
  ARG_TYPE_CONDITION,
  ARG_TYPE_IGNORE_THEN,
  ARG_TYPE_LABEL_NAME
};

// if not CONDITION then LABEL
static const enum arg_type cm19[] =
{
  ARG_TYPE_FRAGMENT_NOT,
  ARG_TYPE_CONDITION,
  ARG_TYPE_IGNORE_THEN,
  ARG_TYPE_LABEL_NAME
};

// if any COLOR THING PARAM then LABEL
static const enum arg_type cm20[] =
{
  ARG_TYPE_FRAGMENT_ANY,
  ARG_TYPE_COLOR_INDIRECT,
  ARG_TYPE_THING,
  ARG_TYPE_PARAM_INDIRECT,
  ARG_TYPE_IGNORE_THEN,
  ARG_TYPE_LABEL_NAME
};

// if no COLOR THING PARAM then LABEL
static const enum arg_type cm21[] =
{
  ARG_TYPE_FRAGMENT_NO,
  ARG_TYPE_COLOR_INDIRECT,
  ARG_TYPE_THING,
  ARG_TYPE_PARAM_INDIRECT,
  ARG_TYPE_IGNORE_THEN,
  ARG_TYPE_LABEL_NAME
};

// if COLOR THING PARAM at DIR then LABEL
static const enum arg_type cm22[] =
{
  ARG_TYPE_COLOR_INDIRECT,
  ARG_TYPE_THING,
  ARG_TYPE_PARAM_INDIRECT,
  ARG_TYPE_IGNORE_AT,
  ARG_TYPE_DIRECTION,
  ARG_TYPE_IGNORE_THEN,
  ARG_TYPE_LABEL_NAME
};

// if not COLOR THING PARAM at DIR then LABEL
static const enum arg_type cm23[] =
{
  ARG_TYPE_FRAGMENT_NOT,
  ARG_TYPE_COLOR_INDIRECT,
  ARG_TYPE_THING,
  ARG_TYPE_PARAM_INDIRECT,
  ARG_TYPE_IGNORE_AT,
  ARG_TYPE_DIRECTION,
  ARG_TYPE_IGNORE_THEN,
  ARG_TYPE_LABEL_NAME
};

// if COLOR THING PARAM at N N then LABEL
static const enum arg_type cm24[] =
{
  ARG_TYPE_COLOR_INDIRECT,
  ARG_TYPE_THING,
  ARG_TYPE_PARAM_INDIRECT,
  ARG_TYPE_IGNORE_AT,
  ARG_TYPE_NUMERIC_INDIRECT,
  ARG_TYPE_NUMERIC_INDIRECT,
  ARG_TYPE_IGNORE_THEN,
  ARG_TYPE_LABEL_NAME
};

// ifat N N then LABEL
static const enum arg_type cm25[] =
{
  ARG_TYPE_IGNORE_AT,
  ARG_TYPE_NUMERIC_INDIRECT,
  ARG_TYPE_NUMERIC_INDIRECT,
  ARG_TYPE_IGNORE_THEN,
  ARG_TYPE_LABEL_NAME
};

// if at DIR of player is COLOR THING PARAM then LABEL
static const enum arg_type cm26[] =
{
  ARG_TYPE_IGNORE_AT,
  ARG_TYPE_DIRECTION,
  ARG_TYPE_IGNORE_OF,
  ARG_TYPE_FRAGMENT_PLAYER,
  ARG_TYPE_IGNORE_IS,
  ARG_TYPE_COLOR_INDIRECT,
  ARG_TYPE_THING,
  ARG_TYPE_PARAM_INDIRECT,
  ARG_TYPE_IGNORE_THEN,
  ARG_TYPE_LABEL_NAME
};

// double COUNTER
static const enum arg_type cm27[] =
{
  ARG_TYPE_COUNTER_STORE_NAME
};

// half COUNTER
static const enum arg_type cm28[] =
{
  ARG_TYPE_COUNTER_STORE_NAME
};

// goto LABEL
static const enum arg_type cm29[] =
{
  ARG_TYPE_LABEL_NAME
};

// send ROBOT to EXT_LABEL
static const enum arg_type cm30[] =
{
  ARG_TYPE_ROBOT_NAME,
  ARG_TYPE_IGNORE_TO,
  ARG_TYPE_EXT_LABEL_NAME
};

// explode N
static const enum arg_type cm31[] =
{
  ARG_TYPE_NUMERIC_INDIRECT
};

// put COLOR THING PARAM to DIR
static const enum arg_type cm32[] =
{
  ARG_TYPE_COLOR_INDIRECT,
  ARG_TYPE_THING,
  ARG_TYPE_PARAM_INDIRECT,
  ARG_TYPE_IGNORE_TO,
  ARG_TYPE_DIRECTION
};

// give N ITEM
static const enum arg_type cm33[] =
{
  ARG_TYPE_NUMERIC_INDIRECT,
  ARG_TYPE_ITEM
};

// take N ITEM
static const enum arg_type cm34[] =
{
  ARG_TYPE_NUMERIC_INDIRECT,
  ARG_TYPE_ITEM
};

// take N ITEM else LABEL
static const enum arg_type cm35[] =
{
  ARG_TYPE_NUMERIC_INDIRECT,
  ARG_TYPE_ITEM,
  ARG_TYPE_IGNORE_ELSE,
  ARG_TYPE_LABEL_NAME
};

// mod STRING
static const enum arg_type cm38[] =
{
  ARG_TYPE_STRING
};

// sam N STRING
static const enum arg_type cm39[] =
{
  ARG_TYPE_NUMERIC_INDIRECT,
  ARG_TYPE_STRING
};

// volume N
static const enum arg_type cm40[] =
{
  ARG_TYPE_NUMERIC_INDIRECT
};

// end mod
static const enum arg_type cm41[] =
{
  ARG_TYPE_FRAGMENT_MOD
};

// end sam
static const enum arg_type cm42[] =
{
  ARG_TYPE_FRAGMENT_SAM
};

// play STRING
static const enum arg_type cm43[] =
{
  ARG_TYPE_STRING
};

// end play
static const enum arg_type cm44[] =
{
  ARG_TYPE_FRAGMENT_PLAY
};

// end play STRING
static const enum arg_type cm45[] =
{
  ARG_TYPE_FRAGMENT_PLAY,
  ARG_TYPE_STRING
};

// wait play
static const enum arg_type cm46[] =
{
  ARG_TYPE_FRAGMENT_PLAY
};

// sfx N
static const enum arg_type cm48[] =
{
  ARG_TYPE_NUMERIC_INDIRECT
};

// play sfx STRING
static const enum arg_type cm49[] =
{
  ARG_TYPE_FRAGMENT_SFX,
  ARG_TYPE_STRING
};

// open at DIR
static const enum arg_type cm50[] =
{
  ARG_TYPE_IGNORE_AT,
  ARG_TYPE_DIRECTION
};

// send at DIR to EXT_LABEL
static const enum arg_type cm53[] =
{
  ARG_TYPE_IGNORE_AT,
  ARG_TYPE_DIRECTION,
  ARG_TYPE_IGNORE_TO,
  ARG_TYPE_EXT_LABEL_NAME
};

// zap LABEL N
static const enum arg_type cm54[] =
{
  ARG_TYPE_LABEL_NAME,
  ARG_TYPE_NUMERIC_INDIRECT
};

// restore LABEL N
static const enum arg_type cm55[] =
{
  ARG_TYPE_LABEL_NAME,
  ARG_TYPE_NUMERIC_INDIRECT
};

// lockplayer ns
static const enum arg_type cm58[] =
{
  ARG_TYPE_FRAGMENT_NS
};

// lockplayer ew
static const enum arg_type cm59[] =
{
  ARG_TYPE_FRAGMENT_EW
};

// lockplayer attack
static const enum arg_type cm60[] =
{
  ARG_TYPE_FRAGMENT_ATTACK
};

// move player to DIR
static const enum arg_type cm61[] =
{
  ARG_TYPE_FRAGMENT_PLAYER,
  ARG_TYPE_IGNORE_TO,
  ARG_TYPE_DIRECTION
};

// move player to DIR else LABEL
static const enum arg_type cm62[] =
{
  ARG_TYPE_FRAGMENT_PLAYER,
  ARG_TYPE_IGNORE_TO,
  ARG_TYPE_DIRECTION,
  ARG_TYPE_IGNORE_ELSE,
  ARG_TYPE_LABEL_NAME
};

// put player at N N
static const enum arg_type cm63[] =
{
  ARG_TYPE_FRAGMENT_PLAYER,
  ARG_TYPE_IGNORE_AT,
  ARG_TYPE_NUMERIC_INDIRECT,
  ARG_TYPE_NUMERIC_INDIRECT
};

// if player at N N then LABEL
static const enum arg_type cm66[] =
{
  ARG_TYPE_FRAGMENT_PLAYER,
  ARG_TYPE_IGNORE_AT,
  ARG_TYPE_NUMERIC_INDIRECT,
  ARG_TYPE_NUMERIC_INDIRECT,
  ARG_TYPE_IGNORE_THEN,
  ARG_TYPE_LABEL_NAME
};

// put player DIR
static const enum arg_type cm67[] =
{
  ARG_TYPE_FRAGMENT_PLAYER,
  ARG_TYPE_DIRECTION
};

// try DIR else LABEL
static const enum arg_type cm68[] =
{
  ARG_TYPE_DIRECTION,
  ARG_TYPE_IGNORE_ELSE,
  ARG_TYPE_LABEL_NAME
};

// switch DIR with DIR
static const enum arg_type cm71[] =
{
  ARG_TYPE_DIRECTION,
  ARG_TYPE_IGNORE_WITH,
  ARG_TYPE_DIRECTION
};

// shoot to DIR
static const enum arg_type cm72[] =
{
  ARG_TYPE_IGNORE_TO,
  ARG_TYPE_DIRECTION
};

// laybomb DIR
static const enum arg_type cm73[] =
{
  ARG_TYPE_DIRECTION
};

// laybomb high DIR
static const enum arg_type cm74[] =
{
  ARG_TYPE_FRAGMENT_HIGH,
  ARG_TYPE_DIRECTION
};

// shootmissile DIR
static const enum arg_type cm75[] =
{
  ARG_TYPE_DIRECTION
};

// shootseeker DIR
static const enum arg_type cm76[] =
{
  ARG_TYPE_DIRECTION
};

// spitfire DIR
static const enum arg_type cm77[] =
{
  ARG_TYPE_DIRECTION
};

// lazerwall DIR for N
static const enum arg_type cm78[] =
{
  ARG_TYPE_DIRECTION,
  ARG_TYPE_IGNORE_FOR,
  ARG_TYPE_NUMERIC_INDIRECT
};

// put COLOR THING PARAM at N N
static const enum arg_type cm79[] =
{
  ARG_TYPE_COLOR_INDIRECT,
  ARG_TYPE_THING,
  ARG_TYPE_PARAM_INDIRECT,
  ARG_TYPE_IGNORE_AT,
  ARG_TYPE_NUMERIC_INDIRECT,
  ARG_TYPE_NUMERIC_INDIRECT
};

// die as an item
static const enum arg_type cm80[] =
{
  ARG_TYPE_IGNORE_AS,
  ARG_TYPE_IGNORE_AN,
  ARG_TYPE_FRAGMENT_ITEM
};

// send at N N to LABEL
static const enum arg_type cm81[] =
{
  ARG_TYPE_IGNORE_AT,
  ARG_TYPE_NUMERIC_INDIRECT,
  ARG_TYPE_NUMERIC_INDIRECT,
  ARG_TYPE_IGNORE_TO,
  ARG_TYPE_EXT_LABEL_NAME
};

// copyrobot ROBOT
static const enum arg_type cm82[] =
{
  ARG_TYPE_ROBOT_NAME
};

// copyrobot at N N
static const enum arg_type cm83[] =
{
  ARG_TYPE_IGNORE_AT,
  ARG_TYPE_NUMERIC_INDIRECT,
  ARG_TYPE_NUMERIC_INDIRECT
};

// copyrobot from DIR
static const enum arg_type cm84[] =
{
  ARG_TYPE_IGNORE_FROM,
  ARG_TYPE_DIRECTION
};

// duplicate self DIR
static const enum arg_type cm85[] =
{
  ARG_TYPE_FRAGMENT_SELF,
  ARG_TYPE_DIRECTION
};

// duplicate self to N N
static const enum arg_type cm86[] =
{
  ARG_TYPE_FRAGMENT_SELF,
  ARG_TYPE_IGNORE_AT,
  ARG_TYPE_NUMERIC_INDIRECT,
  ARG_TYPE_NUMERIC_INDIRECT
};

// bulletn is CHAR
static const enum arg_type cm87[] =
{
  ARG_TYPE_IGNORE_IS,
  ARG_TYPE_CHARACTER_INDIRECT
};

// bullets is CHAR
static const enum arg_type cm88[] =
{
  ARG_TYPE_IGNORE_IS,
  ARG_TYPE_CHARACTER_INDIRECT
};

// bullete is CHAR
static const enum arg_type cm89[] =
{
  ARG_TYPE_IGNORE_IS,
  ARG_TYPE_CHARACTER_INDIRECT
};

// bullete is CHAR
static const enum arg_type cm90[] =
{
  ARG_TYPE_IGNORE_IS,
  ARG_TYPE_CHARACTER_INDIRECT
};

// givekey COLOR
static const enum arg_type cm91[] =
{
  ARG_TYPE_COLOR_INDIRECT
};

// givekey COLOR else LABEL
static const enum arg_type cm92[] =
{
  ARG_TYPE_COLOR_INDIRECT,
  ARG_TYPE_IGNORE_ELSE,
  ARG_TYPE_LABEL_NAME
};

// takekey COLOR
static const enum arg_type cm93[] =
{
  ARG_TYPE_COLOR_INDIRECT
};

// takekey COLOR else LABEL
static const enum arg_type cm94[] =
{
  ARG_TYPE_COLOR_INDIRECT,
  ARG_TYPE_IGNORE_ELSE,
  ARG_TYPE_LABEL_NAME
};

// inc COUNTER by random N to N
static const enum arg_type cm95[] =
{
  ARG_TYPE_COUNTER_STORE_NAME,
  ARG_TYPE_IGNORE_BY,
  ARG_TYPE_FRAGMENT_RANDOM,
  ARG_TYPE_NUMERIC_INDIRECT,
  ARG_TYPE_IGNORE_TO,
  ARG_TYPE_NUMERIC_INDIRECT
};

// dec COUNTER by random N to N
static const enum arg_type cm96[] =
{
  ARG_TYPE_COUNTER_STORE_NAME,
  ARG_TYPE_IGNORE_BY,
  ARG_TYPE_FRAGMENT_RANDOM,
  ARG_TYPE_NUMERIC_INDIRECT,
  ARG_TYPE_IGNORE_TO,
  ARG_TYPE_NUMERIC_INDIRECT
};

// set COUNTER by random N to N
static const enum arg_type cm97[] =
{
  ARG_TYPE_COUNTER_STORE_NAME,
  ARG_TYPE_IGNORE_TO,
  ARG_TYPE_FRAGMENT_RANDOM,
  ARG_TYPE_NUMERIC_INDIRECT,
  ARG_TYPE_IGNORE_TO,
  ARG_TYPE_NUMERIC_INDIRECT
};

// trade N item for N item else LABEL
static const enum arg_type cm98[] =
{
  ARG_TYPE_NUMERIC_INDIRECT,
  ARG_TYPE_ITEM,
  ARG_TYPE_IGNORE_FOR,
  ARG_TYPE_NUMERIC_INDIRECT,
  ARG_TYPE_ITEM,
  ARG_TYPE_IGNORE_ELSE,
  ARG_TYPE_LABEL_NAME
};

// send at DIR of PLAYER to EXT_LABEL
static const enum arg_type cm99[] =
{
  ARG_TYPE_IGNORE_AT,
  ARG_TYPE_DIRECTION,
  ARG_TYPE_IGNORE_OF,
  ARG_TYPE_FRAGMENT_PLAYER,
  ARG_TYPE_IGNORE_TO,
  ARG_TYPE_EXT_LABEL_NAME
};

// put COLOR THING PARAM to DIR of player
static const enum arg_type cm100[] =
{
  ARG_TYPE_COLOR_INDIRECT,
  ARG_TYPE_THING,
  ARG_TYPE_PARAM_INDIRECT,
  ARG_TYPE_IGNORE_TO,
  ARG_TYPE_DIRECTION,
  ARG_TYPE_IGNORE_OF,
  ARG_TYPE_FRAGMENT_PLAYER
};

// / STRING
static const enum arg_type cm101[] =
{
  ARG_TYPE_STRING
};

// * STRING
static const enum arg_type cm102[] =
{
  ARG_TYPE_STRING
};

// [ STRING
static const enum arg_type cm103[] =
{
  ARG_TYPE_STRING
};

// ? LABEL STRING
static const enum arg_type cm104[] =
{
  ARG_TYPE_LABEL_NAME,
  ARG_TYPE_STRING
};

// ? COUNTER LABEL STRING
static const enum arg_type cm105[] =
{
  ARG_TYPE_COUNTER_LOAD_NAME,
  ARG_TYPE_LABEL_NAME,
  ARG_TYPE_STRING
};

// : LABEL
static const enum arg_type cm106[] =
{
  ARG_TYPE_LABEL_NAME
};

// . STRING
static const enum arg_type cm107[] =
{
  ARG_TYPE_STRING
};

// | LABEL
static const enum arg_type cm108[] =
{
  ARG_TYPE_LABEL_NAME
};

// teleport player to BOARD at N N
static const enum arg_type cm109[] =
{
  ARG_TYPE_FRAGMENT_PLAYER,
  ARG_TYPE_IGNORE_TO,
  ARG_TYPE_BOARD_NAME,
  ARG_TYPE_IGNORE_AT,
  ARG_TYPE_NUMERIC_INDIRECT,
  ARG_TYPE_NUMERIC_INDIRECT
};

// scrollview DIR for N
static const enum arg_type cm110[] =
{
  ARG_TYPE_DIRECTION,
  ARG_TYPE_IGNORE_FOR,
  ARG_TYPE_NUMERIC_INDIRECT
};

// input name STRING
static const enum arg_type cm111[] =
{
  ARG_TYPE_FRAGMENT_STRING,
  ARG_TYPE_STRING
};

// if string is STRING then LABEL
static const enum arg_type cm112[] =
{
  ARG_TYPE_FRAGMENT_STRING,
  ARG_TYPE_IGNORE_IS,
  ARG_TYPE_STRING,
  ARG_TYPE_IGNORE_THEN,
  ARG_TYPE_LABEL_NAME
};

// if string IS not STRING then LABEL
static const enum arg_type cm113[] =
{
  ARG_TYPE_FRAGMENT_STRING,
  ARG_TYPE_IGNORE_IS,
  ARG_TYPE_FRAGMENT_NOT,
  ARG_TYPE_STRING,
  ARG_TYPE_IGNORE_THEN,
  ARG_TYPE_LABEL_NAME
};

// if string matches STRING then LABEL
static const enum arg_type cm114[] =
{
  ARG_TYPE_FRAGMENT_STRING,
  ARG_TYPE_FRAGMENT_MATCHES,
  ARG_TYPE_STRING,
  ARG_TYPE_IGNORE_THEN,
  ARG_TYPE_LABEL_NAME
};

// player char is CHAR
static const enum arg_type cm115[] =
{
  ARG_TYPE_FRAGMENT_CHAR,
  ARG_TYPE_IGNORE_IS,
  ARG_TYPE_CHARACTER_INDIRECT
};

// % STRING
static const enum arg_type cm116[] =
{
  ARG_TYPE_STRING
};

// & STRING
static const enum arg_type cm117[] =
{
  ARG_TYPE_STRING
};

// move all COLOR THING PARAM to DIR
static const enum arg_type cm118[] =
{
  ARG_TYPE_FRAGMENT_ALL,
  ARG_TYPE_COLOR_INDIRECT,
  ARG_TYPE_THING,
  ARG_TYPE_PARAM_INDIRECT,
  ARG_TYPE_IGNORE_TO,
  ARG_TYPE_DIRECTION
};

// copy at N N to N N
static const enum arg_type cm119[] =
{
  ARG_TYPE_IGNORE_AT,
  ARG_TYPE_NUMERIC_INDIRECT,
  ARG_TYPE_NUMERIC_INDIRECT,
  ARG_TYPE_IGNORE_TO,
  ARG_TYPE_NUMERIC_INDIRECT,
  ARG_TYPE_NUMERIC_INDIRECT
};

// set edge color to COLOR
static const enum arg_type cm120[] =
{
  ARG_TYPE_FRAGMENT_EDGE,
  ARG_TYPE_FRAGMENT_COLOR,
  ARG_TYPE_IGNORE_TO,
  ARG_TYPE_COLOR_INDIRECT
};

// board to the DIR is BOARD
static const enum arg_type cm121[] =
{
  ARG_TYPE_IGNORE_TO,
  ARG_TYPE_IGNORE_THE,
  ARG_TYPE_DIRECTION,
  ARG_TYPE_IGNORE_IS,
  ARG_TYPE_BOARD_NAME
};

// board to the DIR is none
static const enum arg_type cm122[] =
{
  ARG_TYPE_IGNORE_TO,
  ARG_TYPE_IGNORE_THE,
  ARG_TYPE_DIRECTION,
  ARG_TYPE_FRAGMENT_NONE
};

// char edit CHAR to N N N N N N N N N N N N N N
static const enum arg_type cm123[] =
{
  ARG_TYPE_FRAGMENT_EDIT,
  ARG_TYPE_CHARACTER_INDIRECT,
  ARG_TYPE_IGNORE_TO,
  ARG_TYPE_NUMERIC_INDIRECT,
  ARG_TYPE_NUMERIC_INDIRECT,
  ARG_TYPE_NUMERIC_INDIRECT,
  ARG_TYPE_NUMERIC_INDIRECT,
  ARG_TYPE_NUMERIC_INDIRECT,
  ARG_TYPE_NUMERIC_INDIRECT,
  ARG_TYPE_NUMERIC_INDIRECT,
  ARG_TYPE_NUMERIC_INDIRECT,
  ARG_TYPE_NUMERIC_INDIRECT,
  ARG_TYPE_NUMERIC_INDIRECT,
  ARG_TYPE_NUMERIC_INDIRECT,
  ARG_TYPE_NUMERIC_INDIRECT,
  ARG_TYPE_NUMERIC_INDIRECT,
  ARG_TYPE_NUMERIC_INDIRECT
};

// become pushable
static const enum arg_type cm124[] =
{
  ARG_TYPE_FRAGMENT_PUSHABLE
};

// become nonpushable
static const enum arg_type cm125[] =
{
  ARG_TYPE_FRAGMENT_NONPUSHABLE
};

// blind for N
static const enum arg_type cm126[] =
{
  ARG_TYPE_IGNORE_FOR,
  ARG_TYPE_NUMERIC_INDIRECT
};

// firewalker for N
static const enum arg_type cm127[] =
{
  ARG_TYPE_IGNORE_FOR,
  ARG_TYPE_NUMERIC_INDIRECT
};

// freezetime for N
static const enum arg_type cm128[] =
{
  ARG_TYPE_IGNORE_FOR,
  ARG_TYPE_NUMERIC_INDIRECT
};

// slowtime for N
static const enum arg_type cm129[] =
{
  ARG_TYPE_IGNORE_FOR,
  ARG_TYPE_NUMERIC_INDIRECT
};

// wind for N
static const enum arg_type cm130[] =
{
  ARG_TYPE_IGNORE_FOR,
  ARG_TYPE_NUMERIC_INDIRECT
};

// copy from DIR to DIR
static const enum arg_type cm132[] =
{
  ARG_TYPE_IGNORE_FROM,
  ARG_TYPE_DIRECTION,
  ARG_TYPE_IGNORE_TO,
  ARG_TYPE_DIRECTION
};

// become a lavawalker
static const enum arg_type cm133[] =
{
  ARG_TYPE_IGNORE_A,
  ARG_TYPE_FRAGMENT_LAVAWALKER
};

// become a nonlavawalker
static const enum arg_type cm134[] =
{
  ARG_TYPE_IGNORE_A,
  ARG_TYPE_FRAGMENT_NONLAVAWALKER
};

// change from COLOR THING PARAM to COLOR THING PARAM
static const enum arg_type cm135[] =
{
  ARG_TYPE_IGNORE_FROM,
  ARG_TYPE_COLOR_INDIRECT,
  ARG_TYPE_THING,
  ARG_TYPE_PARAM_INDIRECT,
  ARG_TYPE_IGNORE_TO,
  ARG_TYPE_COLOR_INDIRECT,
  ARG_TYPE_THING,
  ARG_TYPE_PARAM_INDIRECT
};

// playercolor is COLOR
static const enum arg_type cm136[] =
{
  ARG_TYPE_IGNORE_IS,
  ARG_TYPE_COLOR_INDIRECT
};

// bulletcolor is COLOR
static const enum arg_type cm137[] =
{
  ARG_TYPE_IGNORE_IS,
  ARG_TYPE_COLOR_INDIRECT
};

// missiliecolor is COLOR
static const enum arg_type cm138[] =
{
  ARG_TYPE_IGNORE_IS,
  ARG_TYPE_COLOR_INDIRECT
};

// message row is N
static const enum arg_type cm139[] =
{
  ARG_TYPE_FRAGMENT_ROW,
  ARG_TYPE_IGNORE_IS,
  ARG_TYPE_NUMERIC_INDIRECT
};

// rel to self
static const enum arg_type cm140[] =
{
  ARG_TYPE_IGNORE_TO,
  ARG_TYPE_FRAGMENT_SELF
};

// rel to player
static const enum arg_type cm141[] =
{
  ARG_TYPE_IGNORE_TO,
  ARG_TYPE_FRAGMENT_PLAYER
};

// rel to counters
static const enum arg_type cm142[] =
{
  ARG_TYPE_IGNORE_TO,
  ARG_TYPE_FRAGMENT_COUNTERS
};

// change char id N to CHAR
static const enum arg_type cm143[] =
{
  ARG_TYPE_FRAGMENT_CHAR,
  ARG_TYPE_FRAGMENT_ID,
  ARG_TYPE_NUMERIC_INDIRECT,
  ARG_TYPE_IGNORE_TO,
  ARG_TYPE_CHARACTER_INDIRECT
};

// jump to mod order N
static const enum arg_type cm144[] =
{
  ARG_TYPE_IGNORE_TO,
  ARG_TYPE_FRAGMENT_MOD,
  ARG_TYPE_FRAGMENT_ORDER,
  ARG_TYPE_NUMERIC_INDIRECT
};

// ask STRING
static const enum arg_type cm145[] =
{
  ARG_TYPE_STRING
};

// change thick arrow char DIR to CHAR
static const enum arg_type cm147[] =
{
  ARG_TYPE_FRAGMENT_THICK,
  ARG_TYPE_FRAGMENT_ARROW,
  ARG_TYPE_FRAGMENT_CHAR,
  ARG_TYPE_DIRECTION,
  ARG_TYPE_IGNORE_TO,
  ARG_TYPE_CHARACTER_INDIRECT
};

// change thin arrow char DIR to CHAR
static const enum arg_type cm148[] =
{
  ARG_TYPE_FRAGMENT_THIN,
  ARG_TYPE_FRAGMENT_ARROW,
  ARG_TYPE_FRAGMENT_CHAR,
  ARG_TYPE_DIRECTION,
  ARG_TYPE_IGNORE_TO,
  ARG_TYPE_CHARACTER_INDIRECT
};

// set maxhealth to N
static const enum arg_type cm149[] =
{
  ARG_TYPE_FRAGMENT_MAXHEALTH,
  ARG_TYPE_IGNORE_TO,
  ARG_TYPE_NUMERIC_INDIRECT
};

// save player position
static const enum arg_type cm150[] =
{
  ARG_TYPE_FRAGMENT_PLAYER,
  ARG_TYPE_FRAGMENT_POSITION
};

// restore player position
static const enum arg_type cm151[] =
{
  ARG_TYPE_FRAGMENT_PLAYER,
  ARG_TYPE_FRAGMENT_POSITION
};

// exchange player position
static const enum arg_type cm152[] =
{
  ARG_TYPE_FRAGMENT_PLAYER,
  ARG_TYPE_FRAGMENT_POSITION
};

// set mesg column to N
static const enum arg_type cm153[] =
{
  ARG_TYPE_FRAGMENT_MESG,
  ARG_TYPE_FRAGMENT_COLUMN,
  ARG_TYPE_IGNORE_TO,
  ARG_TYPE_NUMERIC_INDIRECT
};
// clear mesg
static const enum arg_type cm154[] =
{
  ARG_TYPE_FRAGMENT_MESG
};

// center mesg
static const enum arg_type cm155[] =
{
  ARG_TYPE_FRAGMENT_MESG
};

// mod sam N N
static const enum arg_type cm157[] =
{
  ARG_TYPE_FRAGMENT_SAM,
  ARG_TYPE_NUMERIC_INDIRECT,
  ARG_TYPE_NUMERIC_INDIRECT
};

// volume N
static const enum arg_type cm158[] =
{
  ARG_TYPE_NUMERIC_INDIRECT
};

// scrollbase color is COLOR
static const enum arg_type cm159[] =
{
  ARG_TYPE_FRAGMENT_COLOR,
  ARG_TYPE_IGNORE_IS,
  ARG_TYPE_COLOR_INDIRECT
};

// scrollcorner color is COLOR
static const enum arg_type cm160[] =
{
  ARG_TYPE_FRAGMENT_COLOR,
  ARG_TYPE_IGNORE_IS,
  ARG_TYPE_COLOR_INDIRECT
};

// scrolltitle color is COLOR
static const enum arg_type cm161[] =
{
  ARG_TYPE_FRAGMENT_COLOR,
  ARG_TYPE_IGNORE_IS,
  ARG_TYPE_COLOR_INDIRECT
};

// scrollpointer color is COLOR
static const enum arg_type cm162[] =
{
  ARG_TYPE_FRAGMENT_COLOR,
  ARG_TYPE_IGNORE_IS,
  ARG_TYPE_COLOR_INDIRECT
};

// scrollarrow color is COLOR
static const enum arg_type cm163[] =
{
  ARG_TYPE_FRAGMENT_COLOR,
  ARG_TYPE_IGNORE_IS,
  ARG_TYPE_COLOR_INDIRECT
};

// viewport is at N N
static const enum arg_type cm164[] =
{
  ARG_TYPE_IGNORE_IS,
  ARG_TYPE_IGNORE_AT,
  ARG_TYPE_NUMERIC_INDIRECT,
  ARG_TYPE_NUMERIC_INDIRECT
};

// viewport size is N by N
static const enum arg_type cm165[] =
{
  ARG_TYPE_FRAGMENT_SIZE,
  ARG_TYPE_IGNORE_IS,
  ARG_TYPE_NUMERIC_INDIRECT,
  ARG_TYPE_IGNORE_BY,
  ARG_TYPE_NUMERIC_INDIRECT
};

// save player position to N
static const enum arg_type cm168[] =
{
  ARG_TYPE_FRAGMENT_PLAYER,
  ARG_TYPE_FRAGMENT_POSITION,
  ARG_TYPE_IGNORE_TO,
  ARG_TYPE_NUMERIC_INDIRECT
};

// restore player position from N
static const enum arg_type cm169[] =
{
  ARG_TYPE_FRAGMENT_PLAYER,
  ARG_TYPE_FRAGMENT_POSITION,
  ARG_TYPE_IGNORE_FROM,
  ARG_TYPE_NUMERIC_INDIRECT
};

// exchange player position with N
static const enum arg_type cm170[] =
{
  ARG_TYPE_FRAGMENT_PLAYER,
  ARG_TYPE_FRAGMENT_POSITION,
  ARG_TYPE_IGNORE_WITH,
  ARG_TYPE_NUMERIC_INDIRECT
};

// restore player position from N and duplicate self
static const enum arg_type cm171[] =
{
  ARG_TYPE_FRAGMENT_PLAYER,
  ARG_TYPE_FRAGMENT_POSITION,
  ARG_TYPE_IGNORE_FROM,
  ARG_TYPE_NUMERIC_INDIRECT,
  ARG_TYPE_IGNORE_AND,
  ARG_TYPE_FRAGMENT_DUPLICATE,
  ARG_TYPE_FRAGMENT_SELF
};

// exchange player position with N and duplicate self
static const enum arg_type cm172[] =
{
  ARG_TYPE_FRAGMENT_PLAYER,
  ARG_TYPE_FRAGMENT_POSITION,
  ARG_TYPE_IGNORE_WITH,
  ARG_TYPE_NUMERIC_INDIRECT,
  ARG_TYPE_IGNORE_AND,
  ARG_TYPE_FRAGMENT_DUPLICATE,
  ARG_TYPE_FRAGMENT_SELF
};

// player bulletn is CHAR
static const enum arg_type cm173[] =
{
  ARG_TYPE_FRAGMENT_BULLETN,
  ARG_TYPE_IGNORE_IS,
  ARG_TYPE_CHARACTER_INDIRECT
};

// player bullets is CHAR
static const enum arg_type cm174[] =
{
  ARG_TYPE_FRAGMENT_BULLETS,
  ARG_TYPE_IGNORE_IS,
  ARG_TYPE_CHARACTER_INDIRECT
};

// player bullete is CHAR
static const enum arg_type cm175[] =
{
  ARG_TYPE_FRAGMENT_BULLETE,
  ARG_TYPE_IGNORE_IS,
  ARG_TYPE_CHARACTER_INDIRECT
};

// player bulletw is CHAR
static const enum arg_type cm176[] =
{
  ARG_TYPE_FRAGMENT_BULLETW,
  ARG_TYPE_IGNORE_IS,
  ARG_TYPE_CHARACTER_INDIRECT
};

// neutral bulletn is CHAR
static const enum arg_type cm177[] =
{
  ARG_TYPE_FRAGMENT_BULLETN,
  ARG_TYPE_IGNORE_IS,
  ARG_TYPE_CHARACTER_INDIRECT
};

// neutral bullets is CHAR
static const enum arg_type cm178[] =
{
  ARG_TYPE_FRAGMENT_BULLETS,
  ARG_TYPE_IGNORE_IS,
  ARG_TYPE_CHARACTER_INDIRECT
};

// neutral bullete is CHAR
static const enum arg_type cm179[] =
{
  ARG_TYPE_FRAGMENT_BULLETE,
  ARG_TYPE_IGNORE_IS,
  ARG_TYPE_CHARACTER_INDIRECT
};

// neutral bulletw is CHAR
static const enum arg_type cm180[] =
{
  ARG_TYPE_FRAGMENT_BULLETW,
  ARG_TYPE_IGNORE_IS,
  ARG_TYPE_CHARACTER_INDIRECT
};

// enemy bulletn is CHAR
static const enum arg_type cm181[] =
{
  ARG_TYPE_FRAGMENT_BULLETN,
  ARG_TYPE_IGNORE_IS,
  ARG_TYPE_CHARACTER_INDIRECT
};

// enemy bullets is CHAR
static const enum arg_type cm182[] =
{
  ARG_TYPE_FRAGMENT_BULLETS,
  ARG_TYPE_IGNORE_IS,
  ARG_TYPE_CHARACTER_INDIRECT
};

// enemy bullete is CHAR
static const enum arg_type cm183[] =
{
  ARG_TYPE_FRAGMENT_BULLETE,
  ARG_TYPE_IGNORE_IS,
  ARG_TYPE_CHARACTER_INDIRECT
};

// enemy bulletw is CHAR
static const enum arg_type cm184[] =
{
  ARG_TYPE_FRAGMENT_BULLETW,
  ARG_TYPE_IGNORE_IS,
  ARG_TYPE_CHARACTER_INDIRECT
};

// player bulletcolor is COLOR
static const enum arg_type cm185[] =
{
  ARG_TYPE_FRAGMENT_BULLETCOLOR,
  ARG_TYPE_IGNORE_IS,
  ARG_TYPE_COLOR_INDIRECT
};

// neutral bulletcolor is COLOR
static const enum arg_type cm186[] =
{
  ARG_TYPE_FRAGMENT_BULLETCOLOR,
  ARG_TYPE_IGNORE_IS,
  ARG_TYPE_COLOR_INDIRECT
};

// enemy bulletcolor is COLOR
static const enum arg_type cm187[] =
{
  ARG_TYPE_FRAGMENT_BULLETCOLOR,
  ARG_TYPE_IGNORE_IS,
  ARG_TYPE_COLOR_INDIRECT
};

// rel self first
static const enum arg_type cm193[] =
{
  ARG_TYPE_FRAGMENT_SELF,
  ARG_TYPE_FRAGMENT_FIRST
};

// rel self last
static const enum arg_type cm194[] =
{
  ARG_TYPE_FRAGMENT_SELF,
  ARG_TYPE_FRAGMENT_LAST
};

// rel player first
static const enum arg_type cm195[] =
{
  ARG_TYPE_FRAGMENT_PLAYER,
  ARG_TYPE_FRAGMENT_FIRST
};

// rel player last
static const enum arg_type cm196[] =
{
  ARG_TYPE_FRAGMENT_PLAYER,
  ARG_TYPE_FRAGMENT_LAST
};

// rel counters first
static const enum arg_type cm197[] =
{
  ARG_TYPE_FRAGMENT_COUNTERS,
  ARG_TYPE_FRAGMENT_FIRST
};

// rel counters last
static const enum arg_type cm198[] =
{
  ARG_TYPE_FRAGMENT_COUNTERS,
  ARG_TYPE_FRAGMENT_LAST
};

// mod fade out
static const enum arg_type cm199[] =
{
  ARG_TYPE_FRAGMENT_FADE,
  ARG_TYPE_FRAGMENT_OUT
};

// mod fade in STRING
static const enum arg_type cm200[] =
{
  ARG_TYPE_FRAGMENT_FADE,
  ARG_TYPE_FRAGMENT_IN,
  ARG_TYPE_STRING
};

// copy block at N N for N by N to N N
static const enum arg_type cm201[] =
{
  ARG_TYPE_FRAGMENT_BLOCK,
  ARG_TYPE_IGNORE_AT,
  ARG_TYPE_NUMERIC_INDIRECT,
  ARG_TYPE_NUMERIC_INDIRECT,
  ARG_TYPE_IGNORE_FOR,
  ARG_TYPE_NUMERIC_INDIRECT,
  ARG_TYPE_IGNORE_BY,
  ARG_TYPE_NUMERIC_INDIRECT,
  ARG_TYPE_IGNORE_TO,
  ARG_TYPE_NUMERIC_INDIRECT,
  ARG_TYPE_NUMERIC_INDIRECT
};

// clip input
static const enum arg_type cm202[] =
{
  ARG_TYPE_FRAGMENT_INPUT
};

// push DIR
static const enum arg_type cm203[] =
{
  ARG_TYPE_DIRECTION
};

// scroll char CHAR DIR
static const enum arg_type cm204[] =
{
  ARG_TYPE_FRAGMENT_CHAR,
  ARG_TYPE_CHARACTER_INDIRECT,
  ARG_TYPE_DIRECTION
};

// flip char CHAR DIR
static const enum arg_type cm205[] =
{
  ARG_TYPE_FRAGMENT_CHAR,
  ARG_TYPE_CHARACTER_INDIRECT,
  ARG_TYPE_DIRECTION
};

// copy char CHAR to CHAR
static const enum arg_type cm206[] =
{
  ARG_TYPE_FRAGMENT_CHAR,
  ARG_TYPE_CHARACTER_INDIRECT,
  ARG_TYPE_IGNORE_TO,
  ARG_TYPE_CHARACTER_INDIRECT
};

// change SFX N to STRING
static const enum arg_type cm210[] =
{
  ARG_TYPE_FRAGMENT_SFX,
  ARG_TYPE_NUMERIC_INDIRECT,
  ARG_TYPE_IGNORE_TO,
  ARG_TYPE_STRING
};

// color intensity is at N percent
static const enum arg_type cm211[] =
{
  ARG_TYPE_FRAGMENT_INTENSITY,
  ARG_TYPE_IGNORE_IS,
  ARG_TYPE_IGNORE_AT,
  ARG_TYPE_NUMERIC_INDIRECT,
  ARG_TYPE_FRAGMENT_PERCENT
};

// color intensity N is at N percent
static const enum arg_type cm212[] =
{
  ARG_TYPE_FRAGMENT_INTENSITY,
  ARG_TYPE_NUMERIC_INDIRECT,
  ARG_TYPE_IGNORE_IS,
  ARG_TYPE_IGNORE_AT,
  ARG_TYPE_NUMERIC_INDIRECT,
  ARG_TYPE_FRAGMENT_PERCENT
};

// color fade out
static const enum arg_type cm213[] =
{
  ARG_TYPE_FRAGMENT_FADE,
  ARG_TYPE_FRAGMENT_OUT
};

// color fade in
static const enum arg_type cm214[] =
{
  ARG_TYPE_FRAGMENT_FADE,
  ARG_TYPE_FRAGMENT_IN
};

// set color N to N N N
static const enum arg_type cm215[] =
{
  ARG_TYPE_FRAGMENT_COLOR,
  ARG_TYPE_NUMERIC_INDIRECT,
  ARG_TYPE_IGNORE_TO,
  ARG_TYPE_NUMERIC_INDIRECT,
  ARG_TYPE_NUMERIC_INDIRECT,
  ARG_TYPE_NUMERIC_INDIRECT
};

// load char set STRING
static const enum arg_type cm216[] =
{
  ARG_TYPE_FRAGMENT_CHAR,
  ARG_TYPE_FRAGMENT_SET,
  ARG_TYPE_STRING
};

// multiply COUNTER by N
static const enum arg_type cm217[] =
{
  ARG_TYPE_COUNTER_STORE_NAME,
  ARG_TYPE_IGNORE_BY,
  ARG_TYPE_NUMERIC_INDIRECT
};

// divide counter by N
static const enum arg_type cm218[] =
{
  ARG_TYPE_COUNTER_STORE_NAME,
  ARG_TYPE_IGNORE_BY,
  ARG_TYPE_NUMERIC_INDIRECT
};

// modulo counter by N
static const enum arg_type cm219[] =
{
  ARG_TYPE_COUNTER_STORE_NAME,
  ARG_TYPE_IGNORE_BY,
  ARG_TYPE_NUMERIC_INDIRECT
};

// player char DIR is CHAR
static const enum arg_type cm220[] =
{
  ARG_TYPE_FRAGMENT_CHAR,
  ARG_TYPE_DIRECTION,
  ARG_TYPE_IGNORE_IS,
  ARG_TYPE_CHARACTER_INDIRECT
};

// load palette STRING
static const enum arg_type cm222[] =
{
  ARG_TYPE_FRAGMENT_PALETTE,
  ARG_TYPE_STRING
};

// mod fade to N by N
static const enum arg_type cm224[] =
{
  ARG_TYPE_FRAGMENT_FADE,
  ARG_TYPE_IGNORE_TO,
  ARG_TYPE_NUMERIC_INDIRECT,
  ARG_TYPE_IGNORE_BY,
  ARG_TYPE_NUMERIC_INDIRECT
};

// scrollview position N N
static const enum arg_type cm225[] =
{
  ARG_TYPE_FRAGMENT_POSITION,
  ARG_TYPE_NUMERIC_INDIRECT,
  ARG_TYPE_NUMERIC_INDIRECT
};

// swap world STRING
static const enum arg_type cm226[] =
{
  ARG_TYPE_FRAGMENT_WORLD,
  ARG_TYPE_STRING
};

// if alignedrobot with ROBOT then LABEL
static const enum arg_type cm227[] =
{
  ARG_TYPE_FRAGMENT_ALIGNEDROBOT,
  ARG_TYPE_IGNORE_WITH,
  ARG_TYPE_ROBOT_NAME,
  ARG_TYPE_IGNORE_THEN,
  ARG_TYPE_LABEL_NAME
};

// if first string is STRING then LABEL
static const enum arg_type cm231[] =
{
  ARG_TYPE_FRAGMENT_FIRST,
  ARG_TYPE_FRAGMENT_STRING,
  ARG_TYPE_IGNORE_IS,
  ARG_TYPE_STRING,
  ARG_TYPE_IGNORE_THEN,
  ARG_TYPE_LABEL_NAME
};

// persistent go STRING
static const enum arg_type cm232[] =
{
  ARG_TYPE_FRAGMENT_GO,
  ARG_TYPE_STRING
};

// wait for mod fade
static const enum arg_type cm233[] ={
  ARG_TYPE_IGNORE_FOR,
  ARG_TYPE_FRAGMENT_MOD,
  ARG_TYPE_FRAGMENT_FADE
};

// enable saving
static const enum arg_type cm235[] =
{
  ARG_TYPE_FRAGMENT_SAVING
};

// disable saving
static const enum arg_type cm236[] =
{
  ARG_TYPE_FRAGMENT_SAVING
};

// enable sensoronly saving
static const enum arg_type cm237[] ={
  ARG_TYPE_FRAGMENT_SENSORONLY,
  ARG_TYPE_FRAGMENT_SAVING
};

// status counter N is COUNTER
static const enum arg_type cm238[] =
{
  ARG_TYPE_FRAGMENT_COUNTER,
  ARG_TYPE_NUMERIC_INDIRECT,
  ARG_TYPE_IGNORE_IS,
  ARG_TYPE_COUNTER_STORE_NAME
};

// overlay is on
static const enum arg_type cm239[] =
{
  ARG_TYPE_IGNORE_IS,
  ARG_TYPE_FRAGMENT_ON
};

// overlay is static
static const enum arg_type cm240[] =
{
  ARG_TYPE_IGNORE_IS,
  ARG_TYPE_FRAGMENT_STATIC
};

// overlay is transparent
static const enum arg_type cm241[] =
{
  ARG_TYPE_IGNORE_IS,
  ARG_TYPE_FRAGMENT_TRANSPARENT
};

// put COLOR CHAR overlay to N N
static const enum arg_type cm242[] =
{
  ARG_TYPE_COLOR_INDIRECT,
  ARG_TYPE_CHARACTER_INDIRECT,
  ARG_TYPE_FRAGMENT_OVERLAY,
  ARG_TYPE_IGNORE_TO,
  ARG_TYPE_NUMERIC_INDIRECT,
  ARG_TYPE_NUMERIC_INDIRECT
};

// copy overlay block at N N for N by N to N N
static const enum arg_type cm243[] =
{
  ARG_TYPE_FRAGMENT_OVERLAY,
  ARG_TYPE_FRAGMENT_BLOCK,
  ARG_TYPE_IGNORE_AT,
  ARG_TYPE_NUMERIC_INDIRECT,
  ARG_TYPE_NUMERIC_INDIRECT,
  ARG_TYPE_IGNORE_FOR,
  ARG_TYPE_NUMERIC_INDIRECT,
  ARG_TYPE_IGNORE_BY,
  ARG_TYPE_NUMERIC_INDIRECT,
  ARG_TYPE_IGNORE_TO,
  ARG_TYPE_NUMERIC_INDIRECT,
  ARG_TYPE_NUMERIC_INDIRECT
};

// change overlay COLOR CHAR to COLOR CHAR
static const enum arg_type cm245[] =
{
  ARG_TYPE_FRAGMENT_OVERLAY,
  ARG_TYPE_COLOR_INDIRECT,
  ARG_TYPE_CHARACTER_INDIRECT,
  ARG_TYPE_IGNORE_TO,
  ARG_TYPE_COLOR_INDIRECT,
  ARG_TYPE_CHARACTER_INDIRECT
};

// change overlay COLOR to COLOR
static const enum arg_type cm246[] =
{
  ARG_TYPE_FRAGMENT_OVERLAY,
  ARG_TYPE_COLOR_INDIRECT,
  ARG_TYPE_IGNORE_TO,
  ARG_TYPE_COLOR_INDIRECT
};

// write overlay COLOR STRING at N N
static const enum arg_type cm247[] =
{
  ARG_TYPE_FRAGMENT_OVERLAY,
  ARG_TYPE_COLOR_INDIRECT,
  ARG_TYPE_STRING,
  ARG_TYPE_IGNORE_AT,
  ARG_TYPE_NUMERIC_INDIRECT,
  ARG_TYPE_NUMERIC_INDIRECT
};

// loop start
static const enum arg_type cm251[] =
{
  ARG_TYPE_FRAGMENT_START
};

// loop for N
static const enum arg_type cm252[] =
{
  ARG_TYPE_IGNORE_FOR,
  ARG_TYPE_NUMERIC_INDIRECT
};

// abort loop
static const enum arg_type cm253[] =
{
  ARG_TYPE_FRAGMENT_LOOP
};

// disable mesg edge
static const enum arg_type cm254[] =
{
  ARG_TYPE_FRAGMENT_MESG,
  ARG_TYPE_FRAGMENT_EDGE
};

// enable mesg edge
static const enum arg_type cm255[] =
{
  ARG_TYPE_FRAGMENT_MESG,
  ARG_TYPE_FRAGMENT_EDGE
};

struct mzx_command
{
  const char *name;
  int parameters;
  const enum arg_type *param_types;
};

static const struct mzx_command command_list[] =
{
  { "end",            0,  NULL  },
  { "die",            0,  NULL  },
  { "wait",           1,  cm2   },
  { "cycle",          1,  cm3   },
  { "go",             2,  cm4   },
  { "walk",           1,  cm5   },
  { "become",         3,  cm6   },
  { "char",           1,  cm7   },
  { "color",          1,  cm8   },
  { "gotoxy",         2,  cm9   },
  { "set",            2,  cm10  },
  { "inc",            2,  cm11  },
  { "dec",            2,  cm12  },
  { "__unused",       0,  NULL  },
  { "__unused",       0,  NULL  },
  { "__unused",       0,  NULL  },
  { "if",             4,  cm16  },
  { "__unused",       0,  NULL  },
  { "if",             2,  cm18  },
  { "if",             3,  cm19  },
  { "if",             5,  cm20  },
  { "if",             5,  cm21  },
  { "if",             5,  cm22  },
  { "if",             6,  cm23  },
  { "if",             6,  cm24  },
  { "if",             3,  cm25  },
  { "if",             6,  cm26  },
  { "double",         1,  cm27  },
  { "half",           1,  cm28  },
  { "goto",           1,  cm29  },
  { "send",           2,  cm30  },
  { "explode",        1,  cm31  },
  { "put",            4,  cm32  },
  { "give",           2,  cm33  },
  { "take",           2,  cm34  },
  { "take",           3,  cm35  },
  { "endgame",        0,  NULL  },
  { "endlife",        0,  NULL  },
  { "mod",            1,  cm38  },
  { "sam",            2,  cm39  },
  { "volume",         1,  cm40  },
  { "end",            1,  cm41  },
  { "end",            1,  cm42  },
  { "play",           1,  cm43  },
  { "end",            1,  cm44  },
  { "wait",           2,  cm45  },
  { "wait",           1,  cm46  },
  { "_blank_line",    0,  NULL  },
  { "sfx",            1,  cm48  },
  { "play",           2,  cm49  },
  { "open",           1,  cm50  },
  { "lockself",       0,  NULL  },
  { "unlockself",     0,  NULL  },
  { "send",           2,  cm53  },
  { "zap",            2,  cm54  },
  { "restore",        2,  cm55  },
  { "lockplayer",     0,  NULL  },
  { "unlockplayer",   0,  NULL  },
  { "lockplayer",     1,  cm58  },
  { "lockplayer",     1,  cm59  },
  { "lockplayer",     1,  cm60  },
  { "move",           2,  cm61  },
  { "move",           3,  cm62  },
  { "put",            3,  cm63  },
  { "__unused",       0,  NULL  },
  { "__unused",       0,  NULL  },
  { "if",             4,  cm66  },
  { "put",            2,  cm67  },
  { "try",            2,  cm68  },
  { "rotatecw",       0,  NULL  },
  { "rotateccw",      0,  NULL  },
  { "switch",         2,  cm71  },
  { "shoot",          1,  cm72  },
  { "laybomb",        1,  cm73  },
  { "laybomb",        2,  cm74  },
  { "shootmissile",   1,  cm75  },
  { "shootseeker",    1,  cm76  },
  { "spitfire",       1,  cm77  },
  { "lazerwall",      2,  cm78  },
  { "put",            5,  cm79  },
  { "die",            1,  cm80  },
  { "send",           3,  cm81  },
  { "copyrobot",      1,  cm82  },
  { "copyrobot",      2,  cm83  },
  { "copyrobot",      1,  cm84  },
  { "duplicate",      2,  cm85  },
  { "duplicate",      3,  cm86  },
  { "bulletn",        1,  cm87  },
  { "bullets",        1,  cm88  },
  { "bullete",        1,  cm89  },
  { "bulletw",        1,  cm90  },
  { "givekey",        1,  cm91  },
  { "givekey",        2,  cm92  },
  { "takekey",        1,  cm93  },
  { "takekey",        2,  cm94  },
  { "inc",            4,  cm95  },
  { "dec",            4,  cm96  },
  { "set",            4,  cm97  },
  { "trade",          5,  cm98  },
  { "send",           3,  cm99  },
  { "put",            5,  cm100 },
  { "/",              1,  cm101 },
  { "*",              1,  cm102 },
  { "[",              1,  cm103 },
  { "?",              2,  cm104 },
  { "?",              3,  cm105 },
  { ":",              1,  cm106 },
  { ".",              1,  cm107 },
  { "|",              1,  cm108 },
  { "teleport",       4,  cm109 },
  { "scrollview",     2,  cm110 },
  { "input",          2,  cm111 },
  { "if",             3,  cm112 },
  { "if",             4,  cm113 },
  { "if",             4,  cm114 },
  { "player",         2,  cm115 },
  { "%",              1,  cm116 },
  { "&",              1,  cm117 },
  { "move",           5,  cm118 },
  { "copy",           4,  cm119 },
  { "set",            3,  cm120 },
  { "board",          2,  cm121 },
  { "board",          2,  cm122 },
  { "char",           16, cm123 },
  { "become",         1,  cm124 },
  { "become",         1,  cm125 },
  { "blind",          1,  cm126 },
  { "firewalker",     1,  cm127 },
  { "freezetime",     1,  cm128 },
  { "slowtime",       1,  cm129 },
  { "wind",           1,  cm130 },
  { "avalanche",      0,  NULL  },
  { "copy",           2,  cm132 },
  { "become",         1,  cm133 },
  { "become",         1,  cm134 },
  { "change",         6,  cm135 },
  { "playercolor",    1,  cm136 },
  { "bulletcolor",    1,  cm137 },
  { "missilecolor",   1,  cm138 },
  { "message",        2,  cm139 },
  { "rel",            1,  cm140 },
  { "rel",            1,  cm141 },
  { "rel",            1,  cm142 },
  { "change",         4,  cm143 },
  { "jump",           3,  cm144 },
  { "ask",            1,  cm145 },
  { "fillhealth",     0,  NULL  },
  { "change",         5,  cm147 },
  { "change",         5,  cm148 },
  { "set",            2,  cm149 },
  { "save",           2,  cm150 },
  { "restore",        2,  cm151 },
  { "exchange",       2,  cm152 },
  { "set",            3,  cm153 },
  { "center",         1,  cm154 },
  { "clear",          1,  cm155 },
  { "resetview",      0,  NULL  },
  { "mod",            3,  cm157 },
  { "volume",         1,  cm158 },
  { "scrollbase",     2,  cm159 },
  { "scrollcorner",   2,  cm160 },
  { "scrolltitle",    2,  cm161 },
  { "scrollpointer",  2,  cm162 },
  { "scrollarrow",    2,  cm163 },
  { "viewport",       2,  cm164 },
  { "viewport",       3,  cm165 },
  { "__unused",       0,  NULL  },
  { "__unused",       0,  NULL  },
  { "save",           3,  cm168 },
  { "restore",        3,  cm169 },
  { "exchange",       3,  cm170 },
  { "restore",        5,  cm171 },
  { "exchange",       5,  cm172 },
  { "player",         2,  cm173 },
  { "player",         2,  cm174 },
  { "player",         2,  cm175 },
  { "player",         2,  cm176 },
  { "neutral",        2,  cm177 },
  { "neutral",        2,  cm178 },
  { "neutral",        2,  cm179 },
  { "neutral",        2,  cm180 },
  { "enemy",          2,  cm181 },
  { "enemy",          2,  cm182 },
  { "enemy",          2,  cm183 },
  { "enemy",          2,  cm184 },
  { "player",         2,  cm185 },
  { "neutral",        2,  cm186 },
  { "enemy",          2,  cm187 },
  { "__unused",       0,  NULL  },
  { "__unused",       0,  NULL  },
  { "__unused",       0,  NULL  },
  { "__unused",       0,  NULL  },
  { "__unused",       0,  NULL  },
  { "rel",            2,  cm193 },
  { "rel",            2,  cm194 },
  { "rel",            2,  cm195 },
  { "rel",            2,  cm196 },
  { "rel",            2,  cm197 },
  { "rel",            2,  cm198 },
  { "mod",            2,  cm199 },
  { "mod",            3,  cm200 },
  { "copy",           7,  cm201 },
  { "clip",           1,  cm202 },
  { "push",           1,  cm203 },
  { "scroll",         3,  cm204 },
  { "flip",           3,  cm205 },
  { "copy",           3,  cm206 },
  { "__unused",       0,  NULL  },
  { "__unused",       0,  NULL  },
  { "__unused",       0,  NULL  },
  { "change",         3,  cm210 },
  { "color",          3,  cm211 },
  { "color",          4,  cm212 },
  { "color",          2,  cm213 },
  { "color",          2,  cm214 },
  { "set",            5,  cm215 },
  { "load",           3,  cm216 },
  { "multiply",       2,  cm217 },
  { "divide",         2,  cm218 },
  { "modulo",         2,  cm219 },
  { "player",         3,  cm220 },
  { "__unused",       0,  NULL  },
  { "load",           2,  cm222 },
  { "__unused",       0,  NULL  },
  { "mod",            3,  cm224 },
  { "scrollview",     3,  cm225 },
  { "swap",           2,  cm226 },
  { "if",             3,  cm227 },
  { "__unused",       0,  NULL  },
  { "lockscroll",     0,  NULL  },
  { "unlockscroll",   0,  NULL  },
  { "if",             4,  cm231 },
  { "persistent",     2,  cm232 },
  { "wait",           2,  cm233 },
  { "__unused",       0,  NULL  },
  { "enable",         1,  cm235 },
  { "disable",        1,  cm236 },
  { "enable",         2,  cm237 },
  { "status",         3,  cm238 },
  { "overlay",        1,  cm239 },
  { "overlay",        1,  cm240 },
  { "overlay",        1,  cm241 },
  { "put",            5,  cm242 },
  { "copy",           8,  cm243 },
  { "__unused",       0,  NULL  },
  { "change",         5,  cm245 },
  { "change",         3,  cm246 },
  { "write",          5,  cm247 },
  { "__unused",       0,  NULL  },
  { "__unused",       0,  NULL  },
  { "__unused",       0,  NULL  },
  { "loop",           1,  cm251 },
  { "loop",           1,  cm252 },
  { "abort",          1,  cm253 },
  { "disable",        2,  cm254 },
  { "enable",         2,  cm255 }
};

static const struct mzx_command empty_command = { "", 0, NULL };

static const char *const dir_names[20] =
{
  "IDLE",
  "NORTH",
  "SOUTH",
  "EAST",
  "WEST",
  "RANDNS",
  "RANDEW",
  "RANDNE",
  "RANDNB",
  "SEEK",
  "RANDANY",
  "BENEATH",
  "ANYDIR",
  "FLOW",
  "NODIR",
  "RANDB",
  "RANDP",
  "CW",
  "OPP",
  "RANDNOT"
};

static const char *const equality_names[] =
{
  "=",
  "<",
  ">",
  ">=",
  "<=",
  "!=",
  "===",
  "?=",
  "?=="
};

static const char *const condition_names[18] =
{
  "walking",
  "swimming",
  "firewalking",
  "touching",
  "blocked",
  "aligned",
  "alignedns",
  "alignedew",
  "lastshot",
  "lasttouch",
  "rightpressed",
  "leftpressed",
  "uppressed",
  "downpressed",
  "spacepressed",
  "delpressed",
  "musicon",
  "pcsfxon"
};

static const char *const item_names[9] =
{
  "GEMS",
  "AMMOS",
  "TIME",
  "SCORE",
  "HEALTHS",
  "LIVES",
  "LOBOMBS",
  "HIBOMBS",
  "COINS"
};


static const char *const ignore_type_names[21] =
{
  "a",
  "an",
  "and",
  "as",
  "at",
  "by",
  "else",
  "for",
  "from",
  "is",
  "of",
  "the",
  "then",
  "to",
  "with"
};

static const char *const command_fragment_type_names[69] =
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

static const struct special_word sorted_special_word_list[] =
{
  { "a",                ARG_TYPE_IGNORE_A,               ARG_TYPE_IGNORE    },
  { "aligned",          ALIGNED,                         ARG_TYPE_CONDITION },
  { "alignedew",        ALIGNED_EW,                      ARG_TYPE_CONDITION },
  { "alignedns",        ALIGNED_NS,                      ARG_TYPE_CONDITION },
  { "alignedrobot",     ARG_TYPE_FRAGMENT_ALIGNEDROBOT,  ARG_TYPE_FRAGMENT  },
  { "all",              ARG_TYPE_FRAGMENT_ALL,           ARG_TYPE_FRAGMENT  },
  { "ammo",             AMMO,                            ARG_TYPE_THING     },
  { "ammos",            GIVE_AMMO,                       ARG_TYPE_ITEM      },
  { "an",               ARG_TYPE_IGNORE_AN,              ARG_TYPE_IGNORE    },
  { "and",              ARG_TYPE_IGNORE_AND,             ARG_TYPE_IGNORE    },
  { "any",              ARG_TYPE_FRAGMENT_ANY,           ARG_TYPE_FRAGMENT  },
  { "anydir",           ANYDIR,                          ARG_TYPE_DIRECTION },
  { "arrow",            ARG_TYPE_FRAGMENT_ARROW,         ARG_TYPE_FRAGMENT  },
  { "as",               ARG_TYPE_IGNORE_AS,              ARG_TYPE_IGNORE    },
  { "at",               ARG_TYPE_IGNORE_AT,              ARG_TYPE_IGNORE    },
  { "attack",           ARG_TYPE_FRAGMENT_ATTACK,        ARG_TYPE_FRAGMENT  },
  { "bear",             BEAR,                            ARG_TYPE_THING     },
  { "bearcub",          BEAR_CUB,                        ARG_TYPE_THING     },
  { "beneath",          BENEATH,                         ARG_TYPE_DIRECTION },
  { "block",            ARG_TYPE_FRAGMENT_BLOCK,         ARG_TYPE_FRAGMENT  },
  { "blocked",          BLOCKED,                         ARG_TYPE_CONDITION },
  { "bomb",             BOMB,                            ARG_TYPE_THING     },
  { "boulder",          BOULDER,                         ARG_TYPE_THING     },
  { "box",              BOX,                             ARG_TYPE_THING     },
  { "breakaway",        BREAKAWAY,                       ARG_TYPE_THING     },
  { "bullet",           BULLET,                          ARG_TYPE_THING     },
  { "bulletcolor",      ARG_TYPE_FRAGMENT_BULLETCOLOR,   ARG_TYPE_FRAGMENT  },
  { "bullete",          ARG_TYPE_FRAGMENT_BULLETE,       ARG_TYPE_FRAGMENT  },
  { "bulletgun",        BULLET_GUN,                      ARG_TYPE_THING     },
  { "bulletn",          ARG_TYPE_FRAGMENT_BULLETN,       ARG_TYPE_FRAGMENT  },
  { "bullets",          ARG_TYPE_FRAGMENT_BULLETS,       ARG_TYPE_FRAGMENT  },
  { "bulletw",          ARG_TYPE_FRAGMENT_BULLETW,       ARG_TYPE_FRAGMENT  },
  { "by",               ARG_TYPE_IGNORE_BY,              ARG_TYPE_IGNORE    },
  { "carpet",           CARPET,                          ARG_TYPE_THING     },
  { "cave",             CAVE,                            ARG_TYPE_THING     },
  { "ccwrotate",        CCW_ROTATE,                      ARG_TYPE_THING     },
  { "char",             ARG_TYPE_FRAGMENT_CHAR,          ARG_TYPE_FRAGMENT  },
  { "chest",            CHEST,                           ARG_TYPE_THING     },
  { "coin",             COIN,                            ARG_TYPE_THING     },
  { "coins",            GIVE_COINS,                      ARG_TYPE_ITEM      },
  { "color",            ARG_TYPE_FRAGMENT_COLOR,         ARG_TYPE_FRAGMENT  },
  { "column",           ARG_TYPE_FRAGMENT_COLUMN,        ARG_TYPE_FRAGMENT  },
  { "counter",          ARG_TYPE_FRAGMENT_COUNTER,       ARG_TYPE_FRAGMENT  },
  { "counters",         ARG_TYPE_FRAGMENT_COUNTERS,      ARG_TYPE_FRAGMENT  },
  { "crate",            CRATE,                           ARG_TYPE_THING     },
  { "customblock",      CUSTOM_BLOCK,                    ARG_TYPE_THING     },
  { "custombox",        CUSTOM_BOX,                      ARG_TYPE_THING     },
  { "custombreak",      CUSTOM_BREAK,                    ARG_TYPE_THING     },
  { "customfloor",      CUSTOM_FLOOR,                    ARG_TYPE_THING     },
  { "customhurt",       CUSTOM_HURT,                     ARG_TYPE_THING     },
  { "custompush",       CUSTOM_PUSH,                     ARG_TYPE_THING     },
  { "cw",               CW,                              ARG_TYPE_DIRECTION },
  { "cwrotate",         CW_ROTATE,                       ARG_TYPE_THING     },
  { "delpressed",       DELPRESSED,                      ARG_TYPE_CONDITION },
  { "dir",              ARG_TYPE_FRAGMENT_DIR,           ARG_TYPE_FRAGMENT  },
  { "door",             DOOR,                            ARG_TYPE_THING     },
  { "down",             SOUTH,                           ARG_TYPE_DIRECTION },
  { "downpressed",      DOWNPRESSED,                     ARG_TYPE_CONDITION },
  { "dragon",           DRAGON,                          ARG_TYPE_THING     },
  { "duplicate",        ARG_TYPE_FRAGMENT_DUPLICATE,     ARG_TYPE_FRAGMENT  },
  { "e",                EAST,                            ARG_TYPE_DIRECTION },
  { "east",             EAST,                            ARG_TYPE_DIRECTION },
  { "edge",             ARG_TYPE_FRAGMENT_EDGE,          ARG_TYPE_FRAGMENT  },
  { "edit",             ARG_TYPE_FRAGMENT_EDIT,          ARG_TYPE_FRAGMENT  },
  { "else",             ARG_TYPE_IGNORE_ELSE,            ARG_TYPE_IGNORE    },
  { "emovingwall",      E_MOVING_WALL,                   ARG_TYPE_THING     },
  { "energizer",        ENERGIZER,                       ARG_TYPE_THING     },
  { "ew",               ARG_TYPE_FRAGMENT_EW,            ARG_TYPE_FRAGMENT  },
  { "ewater",           E_WATER,                         ARG_TYPE_THING     },
  { "explosion",        EXPLOSION,                       ARG_TYPE_THING     },
  { "eye",              EYE,                             ARG_TYPE_THING     },
  { "fade",             ARG_TYPE_FRAGMENT_FADE,          ARG_TYPE_FRAGMENT  },
  { "fake",             FAKE,                            ARG_TYPE_THING     },
  { "fire",             FIRE,                            ARG_TYPE_THING     },
  { "firewalking",      FIRE_WALKING,                    ARG_TYPE_CONDITION },
  { "first",            ARG_TYPE_FRAGMENT_FIRST,         ARG_TYPE_FRAGMENT  },
  { "fish",             FISH,                            ARG_TYPE_THING     },
  { "floor",            FLOOR,                           ARG_TYPE_THING     },
  { "flow",             FLOW,                            ARG_TYPE_DIRECTION },
  { "for",              ARG_TYPE_IGNORE_FOR,             ARG_TYPE_IGNORE    },
  { "forest",           FOREST,                          ARG_TYPE_THING     },
  { "from",             ARG_TYPE_IGNORE_FROM,            ARG_TYPE_IGNORE    },
  { "gate",             GATE,                            ARG_TYPE_THING     },
  { "gem",              GEM,                             ARG_TYPE_THING     },
  { "gems",             GIVE_GEMS,                       ARG_TYPE_ITEM      },
  { "ghost",            GHOST,                           ARG_TYPE_THING     },
  { "go",               ARG_TYPE_FRAGMENT_GO,            ARG_TYPE_FRAGMENT  },
  { "goblin",           GOBLIN,                          ARG_TYPE_THING     },
  { "goop",             GOOP,                            ARG_TYPE_THING     },
  { "health",           HEALTH,                          ARG_TYPE_THING     },
  { "healths",          GIVE_HEALTH,                     ARG_TYPE_ITEM      },
  { "hibombs",          GIVE_HIBOMBS,                    ARG_TYPE_ITEM      },
  { "high",             ARG_TYPE_FRAGMENT_HIGH,          ARG_TYPE_FRAGMENT  },
  { "ice",              ICE,                             ARG_TYPE_THING     },
  { "id",               ARG_TYPE_FRAGMENT_ID,            ARG_TYPE_FRAGMENT  },
  { "idle",             IDLE,                            ARG_TYPE_DIRECTION },
  { "image_file",       IMAGE_FILE,                      ARG_TYPE_THING     },
  { "in",               ARG_TYPE_FRAGMENT_IN,            ARG_TYPE_FRAGMENT  },
  { "input",            ARG_TYPE_FRAGMENT_INPUT,         ARG_TYPE_FRAGMENT  },
  { "intensity",        ARG_TYPE_FRAGMENT_INTENSITY,     ARG_TYPE_FRAGMENT  },
  { "inviswall",        INVIS_WALL,                      ARG_TYPE_THING     },
  { "is",               ARG_TYPE_IGNORE_IS,              ARG_TYPE_IGNORE    },
  { "item",             ARG_TYPE_FRAGMENT_ITEM,          ARG_TYPE_FRAGMENT  },
  { "key",              KEY,                             ARG_TYPE_THING     },
  { "last",             ARG_TYPE_FRAGMENT_LAST,          ARG_TYPE_FRAGMENT  },
  { "lastshot",         LASTSHOT,                        ARG_TYPE_CONDITION },
  { "lasttouch",        LASTTOUCH,                       ARG_TYPE_CONDITION },
  { "lava",             LAVA,                            ARG_TYPE_THING     },
  { "lavawalker",       ARG_TYPE_FRAGMENT_LAVAWALKER,    ARG_TYPE_FRAGMENT  },
  { "lazer",            LAZER,                           ARG_TYPE_THING     },
  { "lazergun",         LAZER_GUN,                       ARG_TYPE_THING     },
  { "left",             WEST,                            ARG_TYPE_DIRECTION },
  { "leftpressed",      LEFTPRESSED,                     ARG_TYPE_CONDITION },
  { "life",             LIFE,                            ARG_TYPE_THING     },
  { "line",             LINE,                            ARG_TYPE_THING     },
  { "litbomb",          LIT_BOMB,                        ARG_TYPE_THING     },
  { "lives",            GIVE_LIVES,                      ARG_TYPE_ITEM      },
  { "lobombs",          GIVE_LOBOMBS,                    ARG_TYPE_ITEM      },
  { "lock",             LOCK,                            ARG_TYPE_THING     },
  { "loop",             ARG_TYPE_FRAGMENT_LOOP,          ARG_TYPE_FRAGMENT  },
  { "magicgem",         MAGIC_GEM,                       ARG_TYPE_THING     },
  { "matches",          ARG_TYPE_FRAGMENT_MATCHES,       ARG_TYPE_FRAGMENT  },
  { "maxhealth",        ARG_TYPE_FRAGMENT_MAXHEALTH,     ARG_TYPE_FRAGMENT  },
  { "mesg",             ARG_TYPE_FRAGMENT_MESG,          ARG_TYPE_FRAGMENT  },
  { "mine",             MINE,                            ARG_TYPE_THING     },
  { "missile",          MISSILE,                         ARG_TYPE_THING     },
  { "missilegun",       MISSILE_GUN,                     ARG_TYPE_THING     },
  { "mod",              ARG_TYPE_FRAGMENT_MOD,           ARG_TYPE_FRAGMENT  },
  { "musicon",          MUSICON,                         ARG_TYPE_CONDITION },
  { "n",                NORTH,                           ARG_TYPE_DIRECTION },
  { "nmovingwall",      N_MOVING_WALL,                   ARG_TYPE_THING     },
  { "no",               ARG_TYPE_FRAGMENT_NO,            ARG_TYPE_FRAGMENT  },
  { "nodir",            NODIR,                           ARG_TYPE_DIRECTION },
  { "none",             ARG_TYPE_FRAGMENT_NONE,          ARG_TYPE_FRAGMENT  },
  { "nonlavawalker",    ARG_TYPE_FRAGMENT_NONLAVAWALKER, ARG_TYPE_FRAGMENT  },
  { "nonpushable",      ARG_TYPE_FRAGMENT_NONPUSHABLE,   ARG_TYPE_FRAGMENT  },
  { "normal",           NORMAL,                          ARG_TYPE_THING     },
  { "north",            NORTH,                           ARG_TYPE_DIRECTION },
  { "not",              ARG_TYPE_FRAGMENT_NOT,           ARG_TYPE_FRAGMENT  },
  { "ns",               ARG_TYPE_FRAGMENT_NS,            ARG_TYPE_FRAGMENT  },
  { "nwater",           N_WATER,                         ARG_TYPE_THING     },
  { "of",               ARG_TYPE_IGNORE_OF,              ARG_TYPE_IGNORE    },
  { "on",               ARG_TYPE_FRAGMENT_ON,            ARG_TYPE_FRAGMENT  },
  { "opendoor",         OPEN_DOOR,                       ARG_TYPE_THING     },
  { "opengate",         OPEN_GATE,                       ARG_TYPE_THING     },
  { "opp",              OPP,                             ARG_TYPE_DIRECTION },
  { "order",            ARG_TYPE_FRAGMENT_ORDER,         ARG_TYPE_FRAGMENT  },
  { "out",              ARG_TYPE_FRAGMENT_OUT,           ARG_TYPE_FRAGMENT  },
  { "overlay",          ARG_TYPE_FRAGMENT_OVERLAY,       ARG_TYPE_FRAGMENT  },
  { "palette",          ARG_TYPE_FRAGMENT_PALETTE,       ARG_TYPE_FRAGMENT  },
  { "pcsfxon",          SOUNDON,                         ARG_TYPE_CONDITION },
  { "percent",          ARG_TYPE_FRAGMENT_PERCENT,       ARG_TYPE_FRAGMENT  },
  { "play",             ARG_TYPE_FRAGMENT_PLAY,          ARG_TYPE_FRAGMENT  },
  { "player",           ARG_TYPE_FRAGMENT_PLAYER,        ARG_TYPE_FRAGMENT  },
  { "position",         ARG_TYPE_FRAGMENT_POSITION,      ARG_TYPE_FRAGMENT  },
  { "potion",           POTION,                          ARG_TYPE_THING     },
  { "pouch",            POUCH,                           ARG_TYPE_THING     },
  { "pushable",         ARG_TYPE_FRAGMENT_PUSHABLE,      ARG_TYPE_FRAGMENT  },
  { "pushablerobot",    ROBOT_PUSHABLE,                  ARG_TYPE_THING     },
  { "pusher",           PUSHER,                          ARG_TYPE_THING     },
  { "randany",          RANDANY,                         ARG_TYPE_DIRECTION },
  { "randb",            RANDB,                           ARG_TYPE_DIRECTION },
  { "randew",           RANDEW,                          ARG_TYPE_DIRECTION },
  { "randnb",           RANDNB,                          ARG_TYPE_DIRECTION },
  { "randne",           RANDNE,                          ARG_TYPE_DIRECTION },
  { "randnot",          RANDNOT,                         ARG_TYPE_DIRECTION },
  { "randns",           RANDNS,                          ARG_TYPE_DIRECTION },
  { "random",           ARG_TYPE_FRAGMENT_RANDOM,        ARG_TYPE_FRAGMENT  },
  { "randp",            RANDP,                           ARG_TYPE_DIRECTION },
  { "ricochet",         RICOCHET,                        ARG_TYPE_THING     },
  { "ricochetpanel",    RICOCHET_PANEL,                  ARG_TYPE_THING     },
  { "right",            EAST,                            ARG_TYPE_DIRECTION },
  { "rightpressed",     RIGHTPRESSED,                    ARG_TYPE_CONDITION },
  { "ring",             RING,                            ARG_TYPE_THING     },
  { "robot",            ROBOT,                           ARG_TYPE_THING     },
  { "row",              ARG_TYPE_FRAGMENT_ROW,           ARG_TYPE_FRAGMENT  },
  { "runner",           RUNNER,                          ARG_TYPE_THING     },
  { "s",                SOUTH,                           ARG_TYPE_DIRECTION },
  { "sam",              ARG_TYPE_FRAGMENT_SAM,           ARG_TYPE_FRAGMENT  },
  { "saving",           ARG_TYPE_FRAGMENT_SAVING,        ARG_TYPE_FRAGMENT  },
  { "score",            GIVE_SCORE,                      ARG_TYPE_ITEM      },
  { "scroll",           SCROLL,                          ARG_TYPE_THING     },
  { "seek",             SEEK,                            ARG_TYPE_DIRECTION },
  { "seeker",           SEEKER,                          ARG_TYPE_THING     },
  { "self",             ARG_TYPE_FRAGMENT_SELF,          ARG_TYPE_FRAGMENT  },
  { "sensor",           SENSOR,                          ARG_TYPE_THING     },
  { "sensoronly",       ARG_TYPE_FRAGMENT_SENSORONLY,    ARG_TYPE_FRAGMENT  },
  { "set",              ARG_TYPE_FRAGMENT_SET,           ARG_TYPE_FRAGMENT  },
  { "sfx",              ARG_TYPE_FRAGMENT_SFX,           ARG_TYPE_FRAGMENT  },
  { "shark",            SHARK,                           ARG_TYPE_THING     },
  { "shootingfire",     SHOOTING_FIRE,                   ARG_TYPE_THING     },
  { "sign",             SIGN,                            ARG_TYPE_THING     },
  { "size",             ARG_TYPE_FRAGMENT_SIZE,          ARG_TYPE_FRAGMENT  },
  { "sliderew",         SLIDER_EW,                       ARG_TYPE_THING     },
  { "sliderns",         SLIDER_NS,                       ARG_TYPE_THING     },
  { "slimeblob",        SLIMEBLOB,                       ARG_TYPE_THING     },
  { "smovingwall",      S_MOVING_WALL,                   ARG_TYPE_THING     },
  { "snake",            SNAKE,                           ARG_TYPE_THING     },
  { "solid",            SOLID,                           ARG_TYPE_THING     },
  { "south",            SOUTH,                           ARG_TYPE_DIRECTION },
  { "space",            SPACE,                           ARG_TYPE_THING     },
  { "spacepressed",     SPACEPRESSED,                    ARG_TYPE_CONDITION },
  { "spider",           SPIDER,                          ARG_TYPE_THING     },
  { "spike",            SPIKE,                           ARG_TYPE_THING     },
  { "spinninggun",      SPINNING_GUN,                    ARG_TYPE_THING     },
  { "spittingtiger",    SPITTING_TIGER,                  ARG_TYPE_THING     },
  { "sprite",           SPRITE,                          ARG_TYPE_THING     },
  { "sprite_colliding", SPR_COLLISION,                   ARG_TYPE_THING     },
  { "stairs",           STAIRS,                          ARG_TYPE_THING     },
  { "start",            ARG_TYPE_FRAGMENT_START,         ARG_TYPE_FRAGMENT  },
  { "static",           ARG_TYPE_FRAGMENT_STATIC,        ARG_TYPE_FRAGMENT  },
  { "stillwater",       STILL_WATER,                     ARG_TYPE_THING     },
  { "string",           ARG_TYPE_FRAGMENT_STRING,        ARG_TYPE_FRAGMENT  },
  { "swater",           S_WATER,                         ARG_TYPE_THING     },
  { "swimming",         SWIMMING,                        ARG_TYPE_CONDITION },
  { "text",             TEXT_ID,                         ARG_TYPE_THING     },
  { "the",              ARG_TYPE_IGNORE_THE,             ARG_TYPE_IGNORE    },
  { "then",             ARG_TYPE_IGNORE_THEN,            ARG_TYPE_IGNORE    },
  { "thick",            ARG_TYPE_FRAGMENT_THICK,         ARG_TYPE_FRAGMENT  },
  { "thickweb",         THICK_WEB,                       ARG_TYPE_THING     },
  { "thief",            THIEF,                           ARG_TYPE_THING     },
  { "thin",             ARG_TYPE_FRAGMENT_THIN,          ARG_TYPE_FRAGMENT  },
  { "tiles",            TILES,                           ARG_TYPE_THING     },
  { "time",             GIVE_TIME,                       ARG_TYPE_ITEM      },
  { "to",               ARG_TYPE_IGNORE_TO,              ARG_TYPE_IGNORE    },
  { "touching",         TOUCHING,                        ARG_TYPE_CONDITION },
  { "transparent",      ARG_TYPE_FRAGMENT_TRANSPARENT,   ARG_TYPE_FRAGMENT  },
  { "transport",        TRANSPORT,                       ARG_TYPE_THING     },
  { "tree",             TREE,                            ARG_TYPE_THING     },
  { "under",            BENEATH,                         ARG_TYPE_DIRECTION },
  { "up",               NORTH,                           ARG_TYPE_DIRECTION },
  { "uppressed",        UPPRESSED,                       ARG_TYPE_CONDITION },
  { "w",                WEST,                            ARG_TYPE_DIRECTION },
  { "walking",          WALKING,                         ARG_TYPE_CONDITION },
  { "web",              WEB,                             ARG_TYPE_THING     },
  { "west",             WEST,                            ARG_TYPE_DIRECTION },
  { "whirlpool",        WHIRLPOOL_1,                     ARG_TYPE_THING     },
  { "whirlpool2",       WHIRLPOOL_2,                     ARG_TYPE_THING     },
  { "whirlpool3",       WHIRLPOOL_3,                     ARG_TYPE_THING     },
  { "whirlpool4",       WHIRLPOOL_4,                     ARG_TYPE_THING     },
  { "with",             ARG_TYPE_IGNORE_WITH,            ARG_TYPE_IGNORE    },
  { "wmovingwall",      W_MOVING_WALL,                   ARG_TYPE_THING     },
  { "world",            ARG_TYPE_FRAGMENT_WORLD,         ARG_TYPE_FRAGMENT  },
  { "wwater",           W_WATER,                         ARG_TYPE_THING     }
};

static const int num_special_words =
 sizeof(sorted_special_word_list) / sizeof(struct special_word);

static const struct special_word *find_special_word(const char *name,
 int name_length)
{
  char name_copy[33];
  int bottom = 0, top = num_special_words - 1, middle;
  const struct special_word *base = sorted_special_word_list;
  int cmpval;

  if(name_length > 32)
    name_length = 32;

  memcpy(name_copy, name, name_length);
  name_copy[name_length] = 0;

  while(bottom <= top)
  {
    middle = (top + bottom) / 2;
    cmpval = strcasecmp(name_copy, (sorted_special_word_list[middle]).name);

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

#define skip_sequence(condition)                                               \
  while(condition)                                                             \
  {                                                                            \
    str++;                                                                     \
  }                                                                            \
                                                                               \
  return str;                                                                  \

static inline boolean is_identifier_char(char c)
{
  return isalnum((int)c) || (c == '$') || (c == '_') ||
                            (c == '#') || (c == '.');
}

static char *skip_whitespace(char *str)
{
  skip_sequence(isspace((int)*str));
}

static char *skip_decimal(char *str)
{
  skip_sequence(isdigit((int)*str));
}

static char *skip_hex(char *str)
{
  skip_sequence(isxdigit((int)*str));
}

static char *find_whitespace(char *str)
{
  skip_sequence(*str && !isspace((int)*str));
}

static char *find_newline(char *str)
{
  skip_sequence(*str && (*str != '\n'));
}

char *find_non_identifier_char(char *str)
{
  skip_sequence(is_identifier_char(*str));
}

static char *find_comment(char *src)
{
  char *next;

  if(src[1] == '/')
  {
    // End of line, C++ style comment
    next = find_newline(src + 2);
    return next;
  }

  if(src[1] == '*')
  {
    // C style comment until '*/'
    int nest_level = 0;
    next = src + 2;

    while(*next)
    {
      if((next[0] == '/') && (next[1] == '*'))
      {
        next++;
        nest_level++;
      }

      if((next[0] == '*') && (next[1] == '/'))
      {
        next++;
        if(nest_level == 0)
          return next + 1;

        nest_level--;
      }

      next++;
    }
  }

  return src;
}

static boolean is_color(char *value)
{
  if((value[0] == 'c') && (value[1] != 0) &&
   ((isxdigit((int)value[1]) || (value[1] == '?')) &&
   (isxdigit((int)value[2]) || (value[2] == '?')) &&
   (isspace(value[3]) || (value[3] == '\0'))))
  {
    return true;
  }
  else
  {
    return false;
  }
}

static boolean is_basic_color(char *value)
{
  if((value[0] == 'c') && (value[1] != 0) &&
   isxdigit((int)value[1]) &&
   isxdigit((int)value[2]) &&
   (isspace(value[3]) || (value[3] == '\0')))
  {
    return true;
  }
  else
  {
    return false;
  }
}

static boolean is_param(char *value)
{
  if((value[0] == 'p') && (value[1] != 0) &&
   ((isxdigit((int)value[1]) && isxdigit((int)value[2])) ||
   ((value[1] == '?') && (value[2] == '?'))) &&
   (isspace(value[3]) || (value[3] == '\0')))
  {
    return true;
  }
  else
  {
    return false;
  }
}

static boolean is_basic_param(char *value)
{
  if((value[0] == 'p') && (value[1] != 0) &&
   isxdigit((int)value[1]) &&
   isxdigit((int)value[2]) &&
   (isspace(value[3]) || (value[3] == '\0')))
  {
    return true;
  }
  else
  {
    return false;
  }
}

static boolean is_string_valued_ident(char *value, size_t len, boolean is_interpolation)
{
  boolean ret;
  char tmp;

  // TODO: make this $INPUT?
  if(is_interpolation && len == 5 && !memcasecmp(value, "INPUT", len))
    return true;

  // TODO: this may need to account for interpolation too...
  tmp = value[len];
  value[len] = '\0';
  ret = is_string(value);
  value[len] = tmp;
  return ret;
}

static int get_param(char *cmd_line)
{
  if((cmd_line[1] == '?') && (cmd_line[2] == '?'))
    return 0x100;

  return strtol(cmd_line + 1, NULL, 16);
}

#ifdef CONFIG_EDITOR

int get_thing(char *name, int name_length)
{
  const struct special_word *special_word =
   find_special_word(name, name_length);

  if((special_word == NULL) || (special_word->arg_type != ARG_TYPE_THING))
    return -1;

  return special_word->instance_type;
}

#endif /* CONFIG_EDITOR */

__editor_maybe_static int unescape_char(char *dest, char c)
{
  switch(c)
  {
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
  }

  dest[0] = c;
  return 1;
}

static int escape_chars(char *dest, char *src, int escape_char,
 boolean escape_interpolation)
{
  if(src[0] == 0)
    return 0;

  if(src[0] == '\\')
  {
    if(escape_interpolation && (src[1] == '{' || src[1] == '}'))
    {
      *dest = src[1];
      return 2;
    }

    if(src[1] == escape_char)
    {
      *dest = escape_char;
      return 2;
    }

    switch(src[1])
    {
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

static char *get_character_interpolated(char *src, char terminator,
 int *contains_expressions);
static char *get_expression(char *src, const char terminator1, const char terminator2);

enum is_op_type
{
  NOT_OPERATOR = 0,
  IS_OPERATOR_UNARY = 0, // ignore these; they'll get picked up as token pieces.

  IS_OPERATOR = 1,
  IS_OPERATOR_TERNARY_OPEN = 2,
  IS_OPERATOR_TERNARY_CLOSE = 3,
};

static char *get_expr_binary_operator_token(char *src, int *_is_operator)
{
  int is_operator = IS_OPERATOR;

  switch(*src)
  {
    // One char operators, not the start of a two char one.
    case '+':
    case '-': // Can be a regular operator or a unary operator.
    case '/':
    case '%':
    case '&':
    case '|':
    case '^':
    case '=':
      break;

    // Unary only.
    case '~':
      is_operator = IS_OPERATOR_UNARY;
      break;

    // Ternary operator special case - this needs to be counted
    case '?':
      is_operator = IS_OPERATOR_TERNARY_OPEN;
      break;

    // Ternary operator special case - this needs to be counted
    case ':':
      is_operator = IS_OPERATOR_TERNARY_CLOSE;
      break;

    case '*':
      if(src[1] == '*')
        src++;
      break;

    case '>':
      if(src[1] == '=')
      {
        src++;
      }
      else

      if(src[1] == '>')
      {
        if(src[2] == '>')
        {
          src++;
        }
        src++;
      }
      break;

    case '<':
      if((src[1] == '<') ||
       (src[1] == '='))
      {
        src++;
      }
      break;

    case '!':
      if(src[1] == '=')
      {
        src++;
      }
      else
      {
        is_operator = IS_OPERATOR_UNARY;
      }
      break;

    default:
      is_operator = NOT_OPERATOR;
      break;
  }

  if(is_operator)
    src++;

  *_is_operator = is_operator;
  return src;
}

static char *get_expr_value_token(char *src)
{
  char *next = src;

  // Unary operators are okay, just eats up spaces until next ones.
  while((*next == '-') || (*next == '~') || (*next == '!'))
    next = skip_whitespace(next + 1);

  switch(*next)
  {
    case '0':
      if(next[1] == 'x')
        next = skip_hex(next + 2);
      else
        next = skip_decimal(next);
      break;

    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
      next = skip_decimal(next);
      break;

    case '(':
      // Start of an expression, verify that it is one.
      next = get_expression(next + 1, ')', ')');
      if(*next != ')')
        return src;

      next++;
      break;

    case '`':
    {
      // This is exactly like a name in normal contexts.
      int contains_expressions;
      next = get_character_interpolated(next + 1, '`', &contains_expressions);

      if(*next != '`')
        return src;

      next++;
      break;
    }

    default:
    {
      char *start = next;

      // Grab a valid identifier out of this.
      next = find_non_identifier_char(next);

      // After skipping spaces/unaries, we're at a non-identifier char? Invalid
      if(next == start)
        return src;

      break;
    }
  }

  return next;
}

static char *get_expression(char *src, const char terminator1, const char terminator2)
{
  char *next, *next_next;
  int ternary_level = 0;
  int is_operator;

  // Skip initial whitespace.
  src = skip_whitespace(src);

  // Need a value to kick things off.
  next = get_expr_value_token(src);

  if(next == src)
    return src;

  // Now keep going until we hit the end - for every
  // time we don't we need an operator then a value.
  while(true)
  {
    next = skip_whitespace(next);
    if(*next == terminator1 || *next == terminator2)
      break;
    if(*next == '\0')
      return src;

    next = get_expr_binary_operator_token(next, &is_operator);
    if(!is_operator)
      return src;

    if(is_operator == IS_OPERATOR_TERNARY_OPEN)
      ternary_level++;

    else if(is_operator == IS_OPERATOR_TERNARY_CLOSE)
      ternary_level--;

    if(ternary_level < 0)
      return src;

    next = skip_whitespace(next);
    // Now get a value token.
    next_next = get_expr_value_token(next);
    if(next_next == next)
      return src;

    next = next_next;
  }

  if(ternary_level > 0)
    return src;

  return next;
}

static char *get_interpolation_expression(char *src)
{
  char *next = src;
  char *next_next;
  int contains_expressions;
  boolean numeric_only;

  // Now keep going until we hit the end.
  while(*next != '}')
  {
    next = skip_whitespace(next);

    // TODO: replace this with proper format syntax.
    numeric_only = false;
    if(*next == '#' || *next == '+')
    {
      numeric_only = true;
      next = skip_whitespace(next + 1);
    }

    switch(*next)
    {
      case 0:
        return src;

      case '(':
        /* Numeric expression. */
        next = get_expression(next + 1, ')', ')');
        if(*next != ')')
          return src;

        next++;
        break;

      case '"':
        // String literal.
        next = get_character_interpolated(next + 1, '"',
         &contains_expressions);
        if(*next != '"')
          return src;
        if(numeric_only)
          return src;

        next++;
        break;

      case '`':
        // String identifier?
        next_next = get_character_interpolated(next + 1, '`',
         &contains_expressions);
        if(next_next <= next + 1 || *next_next != '`')
          return src;

        // Note: this can't reliably detect all strings due to nested string
        // interpolation. This shouldn't hurt anything (currently).
        if(!is_string_valued_ident(next + 1, next_next - next - 1, false))
        {
          // Numeric expression without parentheses?
          next_next = get_expression(next, ',', '}');
          if(next_next <= next)
            return src;
        }
        else
        {
          if(numeric_only)
            return src;

          next_next++;
        }

        next = next_next;
        break;

      default:
        // String identifier?
        next_next = find_non_identifier_char(next);

        if(next_next <= next ||
         !is_string_valued_ident(next, next_next - next, true))
        {
          // Numeric expression without parentheses?
          next_next = get_expression(next, ',', '}');
          if(next_next <= next)
            return src;
        }
        else
        {
          if(numeric_only)
            return src;
        }

        next = next_next;
        break;
    }

    // Find comma or terminating >.
    next = skip_whitespace(next);
    if(*next != ',' && *next != '}')
      return src;
    if(*next == ',')
      next++;
  }

  return next;
}

static char *get_character_interpolated(char *src, char terminator,
 int *contains_expressions)
{
  int num_expressions = 0;
  char current_char = *src;
  char *next = src;

  while(current_char && (current_char != terminator))
  {
    if(current_char == '}')
    {
      // Unescaped } is a compile error.
      return src;
    }
    else

    if(current_char == '{')
    {
      next = get_interpolation_expression(next + 1);
      num_expressions++;
      if(*next != '}')
      {
        *contains_expressions = -num_expressions;
        return src;
      }
    }
    else

    if((current_char == '\\') && (next[1] != 0))
    {
      next++;
    }

    next++;
    current_char = *next;
  }

  *contains_expressions = num_expressions;

  return next;
}

static char *get_token(char *src, struct token *token)
{
  char *next;
  int contains_expression = 0;
  enum token_type token_type;

  src = skip_whitespace(src);

  switch(*src)
  {
    case '"':
      // String literal.
      next = get_character_interpolated(src + 1, '"', &contains_expression);

      token_type = TOKEN_TYPE_STRING_LITERAL;

      if(*next != '"')
        token_type |= TOKEN_TYPE_INVALID;
      else
        next++;

      break;

    case '`':
      // Name.
      next = get_character_interpolated(src + 1, '`', &contains_expression);

      token_type = TOKEN_TYPE_NAME;

      if(*next != '`')
        token_type |= TOKEN_TYPE_INVALID;
      else
        next++;

      break;


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
      // Immediate - maybe a hex immediate.
      if((src[0] == '0') && (src[1] == 'x'))
      {
        token->arg_value.numeric_literal = strtol(src, &next, 16);
        token_type = TOKEN_TYPE_NUMERIC_LITERAL_BASE16;
      }
      else
      {
        token->arg_value.numeric_literal = strtol(src, &next, 10);
        token_type = TOKEN_TYPE_NUMERIC_LITERAL_BASE10;
      }

      // TODO: remove when new bytecode
      if(token->arg_value.numeric_literal < -32768 ||
       token->arg_value.numeric_literal > 32767)
        token_type |= TOKEN_TYPE_INVALID;

      if(next == src)
        token_type |= TOKEN_TYPE_INVALID;

      break;


    case '\'':
      // If the first letter is a single quote then it's a character;
      token_type = TOKEN_TYPE_CHARACTER_LITERAL;

      next = src + 1 + escape_chars(&(token->arg_value.char_literal),
       src + 1, -1, false);

      if((next == src + 1) || (*next != '\''))
        token_type |= TOKEN_TYPE_INVALID;
      else
        next++;

      break;

    case '(':
      // Expression.
      next = get_expression(src + 1, ')', ')');

      token_type = TOKEN_TYPE_EXPRESSION;

      if(*next != ')')
        token_type |= TOKEN_TYPE_INVALID;
      else
        next++;

      break;

    case '<':
      // Match <, <=, or <>
      token_type = TOKEN_TYPE_EQUALITY;
      next = src + 1;
      if(src[1] == '=')
      {
        token->arg_value.equality_type = LESS_THAN_OR_EQUAL;
        next++;
      }
      else

      if(src[1] == '>')
      {
        token->arg_value.equality_type = NOT_EQUAL;
        next++;
      }
      else
      {
        token->arg_value.equality_type = LESS_THAN;
      }

      break;

    // Match >, >=, or ><
    case '>':
      token_type = TOKEN_TYPE_EQUALITY;
      next = src + 1;
      if(src[1] == '=')
      {
        token->arg_value.equality_type = GREATER_THAN_OR_EQUAL;
        next++;
      }
      else

      if(src[1] == '<')
      {
        token->arg_value.equality_type = NOT_EQUAL;
        next++;
      }
      else
      {
        token->arg_value.equality_type = GREATER_THAN;
      }

      break;

    // Match =, ==, ===, =<, or =>
    case '=':
      token_type = TOKEN_TYPE_EQUALITY;
      token->arg_value.equality_type = EQUAL;

      next = src + 1;
      if(src[1] == '=')
      {
        if(src[2] == '=')
        {
          token->arg_value.equality_type = EXACTLY_EQUAL;
          next++;
        }
        next++;
      }
      else

      if(src[1] == '<')
      {
        token->arg_value.equality_type = LESS_THAN_OR_EQUAL;
        next++;
      }
      else

      if(src[1] == '>')
      {
        token->arg_value.equality_type = GREATER_THAN_OR_EQUAL;
        next++;
      }

      break;

    // Match ! or !=
    case '!':
      token_type = TOKEN_TYPE_EQUALITY;
      next = src + 1;
      if(src[1] == '=')
      {
        token->arg_value.equality_type = NOT_EQUAL;
        next += 1;
      }
      else
      {
        token_type |= TOKEN_TYPE_INVALID;
      }
      break;

    // Match ?= or ?==
    case '?':
      token_type = TOKEN_TYPE_EQUALITY;
      next = src + 1;
      if(src[1] == '=')
      {
        token->arg_value.equality_type = WILD_EQUAL;
        next++;

        if(src[2] == '=')
        {
          token->arg_value.equality_type = WILD_EXACTLY_EQUAL;
          next++;
        }
      }
      else
      {
        token_type |= TOKEN_TYPE_INVALID;
      }
      break;

    // Comment, maybe
    case '/':
      token_type = TOKEN_TYPE_COMMENT;
      next = find_comment(src);
      if(next == src)
        token_type |= TOKEN_TYPE_INVALID;
      break;

    default:
      if(is_color(src))
      {
        // Color
        next = src + 3;
        // We need two different types, since basic colors can also be
        // identifiers. Wildcard colors can't, since they have ?s.
        if(is_basic_color(src))
          token_type = TOKEN_TYPE_BASIC_COLOR;
        else
          token_type = TOKEN_TYPE_WILDCARD_COLOR;
      }
      else

      if(is_param(src))
      {
        // Param
        next = src + 3;
        // We need two different types, since basic params can also be
        // identifiers. Wildcard params can't, since they have ?s.
        if(is_basic_param(src))
          token_type = TOKEN_TYPE_BASIC_PARAM;
        else
          token_type = TOKEN_TYPE_WILDCARD_PARAM;
      }

      else
      {
        // It's just a string - could be an ignore word or otherwise a basic
        // identifier.
        next = find_non_identifier_char(src);
        token_type = TOKEN_TYPE_BASIC_IDENTIFIER;
      }

      // Until we decide we care about what special word this is then this
      // stays NULL.
      token->value_is_cached = false;
      break;
  }

  // The next character must be the terminator or a space.
  if(!isspace(*next) && *next != '\0')
    token_type = TOKEN_TYPE_INVALID;

  token->length = next - src;
  token->value = src;
  token->type = token_type;

  return next;
}

struct command_set
{
  const char *const name;
  int name_length;
  int count;
  const int offsets[16];
};

#define command_set(name, count, ...) \
  { name, sizeof(name) - 1, count, { __VA_ARGS__ } }

static const struct command_set sorted_command_list[] =
{
  command_set("%",              1, 116                                 ),
  command_set("&",              1, 117                                 ),
  command_set("*",              1, 102                                 ),
  command_set(".",              1, 107                                 ),
  command_set("/",              1, 101                                 ),
  command_set(":",              1, 106                                 ),
  //                               3    2
  command_set("?",              2, 105, 104                            ),
  command_set("[",              1, 103                                 ),
  command_set("abort",          1, 253                                 ),
  command_set("ask",            1, 145                                 ),
  command_set("avalanche",      1, 131                                 ),
  //                               3  1    1    1    1
  command_set("become",         5, 6, 134, 124, 125, 133               ),
  command_set("blind",          1, 126                                 ),
  //                               2    2
  command_set("board",          2, 121, 122                            ),
  command_set("bulletcolor",    1, 137                                 ),
  command_set("bullete",        1, 89                                  ),
  command_set("bulletn",        1, 87                                  ),
  command_set("bullets",        1, 88                                  ),
  command_set("bulletw",        1, 90                                  ),
  command_set("center",         1, 154                                 ),
  //                               6    5    5    5    4    3    3
  command_set("change",         7, 135, 147, 148, 245, 143, 210, 246   ),
  //                               16 1
  command_set("char",           2, 7, 123                              ),
  command_set("clear",          1, 155                                 ),
  command_set("clip",           1, 202                                 ),
  //                               4    3    2    2    1
  command_set("color",          5, 212, 211, 213, 214, 8               ),
  //                               8    7    4    3    2
  command_set("copy",           5, 243, 201, 119, 206, 132             ),
  //                               2   1   1
  command_set("copyrobot",      3, 83, 82, 84                          ),
  command_set("cycle",          1, 3                                   ),
  //                               4   2
  command_set("dec",            2, 96, 12                              ),
  //                               1   0
  command_set("die",            2, 80, 1                               ),
  //                               2    1
  command_set("disable",        2, 254, 236                            ),
  command_set("divide",         1, 218                                 ),
  command_set("double",         1, 27                                  ),
  //                               3   2
  command_set("duplicate",      2, 86, 85                              ),
  //                               2    2    1
  command_set("enable",         3, 237, 255, 235                       ),
  //                               1   1   1   0
  command_set("end",            4, 41, 42, 44, 0                       ),
  command_set("endgame",        1, 36                                  ),
  command_set("endlife",        1, 37                                  ),
  //                               2    2    2    2    2
  command_set("enemy",          5, 181, 182, 183, 184, 187             ),
  //                               5    3    2
  command_set("exchange",       3, 172, 170, 152                       ),
  command_set("explode",        1, 31                                  ),
  command_set("fillhealth",     1, 146                                 ),
  command_set("firewalker",     1, 127                                 ),
  command_set("flip",           1, 205                                 ),
  command_set("freezetime",     1, 128                                 ),
  command_set("give",           1, 33                                  ),
  //                               2   1
  command_set("givekey",        2, 92, 91                              ),
  command_set("go",             1, 4                                   ),
  command_set("goto",           1, 29                                  ),
  command_set("gotoxy",         1, 9                                   ),
  command_set("half",           1, 28                                  ),
  command_set("if",             16,
  // 6   6   6   5   5   5   4   4   4    4    4    3   3   3    3    2
    23, 24, 26, 20, 21, 22, 16, 66, 113, 114, 231, 19, 25, 112, 227, 18),
  //                               4   2
  command_set("inc",            2, 95, 11                              ),
  command_set("input",          1, 111                                 ),
  command_set("jump",           1, 144                                 ),
  //                               2   1
  command_set("laybomb",        2, 74, 73                              ),
  command_set("lazerwall",      1, 78                                  ),
  //                               3    2
  command_set("load",           2, 216, 222                            ),
  //                               1   1   1   0
  command_set("lockplayer",     4, 58, 59, 60, 56                      ),
  command_set("lockscroll",     1, 229                                 ),
  command_set("lockself",       1, 51                                  ),
  //                               1    1
  command_set("loop",           2, 251, 252                            ),
  command_set("message",        1, 139                                 ),
  command_set("missilecolor",   1, 138                                 ),
  //                               3    3    3    2    1
  command_set("mod",            5, 157, 200, 224, 199, 38              ),
  command_set("modulo",         1, 219                                 ),
  //                               5    3   2
  command_set("move",           3, 118, 62, 61                         ),
  command_set("multiply",       1, 217                                 ),
  //                               2    2    2    2    2
  command_set("neutral",        5, 177, 178, 179, 180, 186             ),
  command_set("open",           1, 50                                  ),
  //                               1    1    1
  command_set("overlay",        3, 239, 240, 241                       ),
  command_set("persistent",     1, 232                                 ),
  //                               2   1
  command_set("play",           2, 49, 43                              ),
  //                               3    2    2    2    2    2    2
  command_set("player",         7, 220, 115, 173, 174, 175, 176, 185   ),
  command_set("playercolor",    1, 136                                 ),
  command_set("push",           1, 203                                 ),
  //                               5   5    5    4   3   2
  command_set("put",            6, 79, 100, 242, 32, 63, 67            ),
  command_set("rel",            9,
  // 2    2    2    2    2    2    1    1    1
     193, 194, 195, 196, 197, 198, 140, 141, 142                       ),
  command_set("resetview",      1, 156                                 ),
  //                               5    3    2   2
  command_set("restore",        4, 171, 169, 55, 151                   ),
  command_set("rotateccw",      1, 70                                  ),
  command_set("rotatecw",       1, 69                                  ),
  command_set("sam",            1, 39                                  ),
  //                               3    2
  command_set("save",           2, 168, 150                            ),
  command_set("scroll",         1, 204                                 ),
  command_set("scrollarrow",    1, 163                                 ),
  command_set("scrollbase",     1, 159                                 ),
  command_set("scrollcorner",   1, 160                                 ),
  command_set("scrollpointer",  1, 162                                 ),
  command_set("scrolltitle",    1, 161                                 ),
  //                               3    2
  command_set("scrollview",     2, 225, 110                            ),
  //                               3   3   3   2
  command_set("send",           4, 81, 99, 30, 53,                     ),
  //                               5    4   3    3    2   2
  command_set("set",            6, 215, 97, 120, 153, 10, 149          ),
  command_set("sfx",            1, 48                                  ),
  command_set("shoot",          1, 72                                  ),
  command_set("shootmissile",   1, 75                                  ),
  command_set("shootseeker",    1, 76                                  ),
  command_set("slowtime",       1, 129                                 ),
  command_set("spitfire",       1, 77                                  ),
  command_set("status",         1, 238                                 ),
  command_set("swap",           1, 226                                 ),
  command_set("switch",         1, 71                                  ),
  //                               3   2
  command_set("take",           2, 35, 34                              ),
  //                               2   1
  command_set("takekey",        2, 94, 93                              ),
  command_set("teleport",       1, 109                                 ),
  command_set("trade",          1, 98                                  ),
  command_set("try",            1, 68                                  ),
  command_set("unlockplayer",   1, 57                                  ),
  command_set("unlockscroll",   1, 230                                 ),
  command_set("unlockself",     1, 52                                  ),
  //                               3    2
  command_set("viewport",       2, 165, 164                            ),
  //                               1   1
  command_set("volume",         2, 40, 158                             ),
  //                               2    2   1  1
  command_set("wait",           4, 233, 45, 2, 46                      ),
  command_set("walk",           1, 5                                   ),
  command_set("wind",           1, 130                                 ),
  command_set("write",          1, 247                                 ),
  command_set("zap",            1, 54                                  ),
  command_set("|",              1, 108                                 )
};

static const int num_command_names =
 sizeof(sorted_command_list) / sizeof(struct command_set);

static struct command_set empty_command_set = { "", 0, 0, { 0 } };

static const struct command_set *find_command_set(const char *name,
 int name_length)
{
  char name_copy[33];
  int bottom = 0, top = num_command_names - 1, middle;
  const struct command_set *base = sorted_command_list;
  int cmpval;

  if(name_length > 32)
    name_length = 32;

  memcpy(name_copy, name, name_length);
  name_copy[name_length] = 0;

  while(bottom <= top)
  {
    middle = (top + bottom) / 2;
    cmpval = strcasecmp(name_copy, (sorted_command_list[middle]).name);

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

// This structure acts as a cache so we don't have to keep grabbing the same
// tokens we already got.

struct token_collection
{
  struct token *tokens;
  int token_count;
  int tokens_allocated;
  int token_position;
  enum arg_type_indexed *match_types;
  int match_types_allocated;
  char *next;
};

static void token_collection_initialize(struct token_collection
 *token_collection, char *src)
{
  token_collection->tokens = malloc(sizeof(struct token));
  token_collection->token_count = 1;
  token_collection->tokens_allocated = 1;
  token_collection->next = src;
}

static void token_collection_start_match_sequence(struct token_collection
 *token_collection)
{
  token_collection->token_position = 1;
  token_collection->match_types = malloc(sizeof(enum arg_type_indexed));
  token_collection->match_types_allocated = 1;
}

static struct token *token_collection_get_token(struct token_collection
 *token_collection, enum arg_type_indexed **_match_type)
{
  struct token new_token;
  struct token *tokens = token_collection->tokens;
  enum arg_type_indexed *match_types = token_collection->match_types;
  int token_count = token_collection->token_count;
  int tokens_allocated = token_collection->tokens_allocated;
  int token_position = token_collection->token_position;
  int match_types_allocated = token_collection->match_types_allocated;
  char *next;

  // See if we need to get a new token off the stream, or if we already
  // have one cached.
  if(token_position >= token_count)
  {
    next = get_token(token_collection->next, &new_token);

    // If the token received is not valid then we return NULL to
    // signify that we couldn't get a token.
    if((new_token.length == 0) || (new_token.type & TOKEN_TYPE_INVALID))
      return NULL;

    token_collection->next = next;

    // We may need to allocate more space for the incoming token.
    if(token_count >= tokens_allocated)
    {
      tokens_allocated *= 2;
      tokens = realloc(tokens, sizeof(struct token) * tokens_allocated);
      token_collection->tokens_allocated = tokens_allocated;
      token_collection->tokens = tokens;
    }

    memcpy(&(tokens[token_count]), &new_token, sizeof(struct token));

    token_collection->token_count = token_count + 1;
  }

  // We may need to allocate more in the match table.
  if(token_position >= match_types_allocated)
  {
    match_types_allocated *= 2;
    match_types = realloc(match_types,
     sizeof(enum arg_type_indexed) * match_types_allocated);

    token_collection->match_types_allocated = match_types_allocated;
    token_collection->match_types = match_types;
  }

  token_collection->token_position = token_position + 1;

  *_match_type = &(match_types[token_position]);
  return &(tokens[token_position]);
}

static boolean match_special_word_condition(struct token *current_token,
 enum token_type token_type)
{
  if(token_type != TOKEN_TYPE_BASIC_IDENTIFIER)
    return false;

  if(!current_token->value_is_cached)
  {
    current_token->arg_value.special_word =
     find_special_word(current_token->value, current_token->length);
    current_token->value_is_cached = true;
  }

  return current_token->arg_value.special_word != NULL;
}

static boolean match_special_word_type(struct token *current_token,
 enum token_type token_type, enum arg_type arg_type_instance)
{
  return match_special_word_condition(current_token, token_type) &&
   current_token->arg_value.special_word->arg_type == arg_type_instance;
}

static boolean match_arg_and_special_word_type(struct token *current_token,
 enum token_type token_type, enum arg_type arg_type,
 enum arg_type arg_type_instance)
{
  return (arg_type & arg_type_instance) &&
   match_special_word_type(current_token, token_type, arg_type_instance);
}

#define get_token_skip_comments(do_on_null)                                    \
  current_token = token_collection_get_token(token_collection, &match_type);   \
  if(current_token == NULL)                                                    \
    do_on_null;                                                                \
                                                                               \
  while(current_token->type == TOKEN_TYPE_COMMENT)                             \
  {                                                                            \
    *match_type = ARG_TYPE_INDEXED_COMMENT;                                    \
    current_token = token_collection_get_token(token_collection, &match_type); \
                                                                               \
    if(current_token == NULL)                                                  \
      do_on_null;                                                              \
  }                                                                            \
  token_type = current_token->type                                             \

static struct token *match_direction(struct token *current_token,
 struct token_collection *token_collection)
{
  int current_direction = current_token->arg_value.special_word->instance_type;

  if(current_direction > RANDB)
  {
    enum arg_type_indexed *match_type;
    enum token_type token_type;
    int modifiers_found = current_direction;

    do
    {
      // Must get another token, must be direction too.
      get_token_skip_comments(return NULL);

      // And it must be a root direction.
      token_type = current_token->type;

      if(match_special_word_type(current_token, token_type,
                                 ARG_TYPE_DIRECTION))
      {
        *match_type = ARG_TYPE_INDEXED_DIRECTION;
        current_direction =
         current_token->arg_value.special_word->instance_type;

        // If it's a terminal (non-modifier) direction we end here,
        // otherwise it accumulates.
        if(current_direction <= RANDB)
        {
          return current_token;
        }
        else
        {
          if(modifiers_found & current_direction)
            return NULL;

          modifiers_found |= current_direction;
        }
      }
      else
      {
        return NULL;
      }
    } while(1);
  }
  return current_token;
}

static boolean whitespace_until_newline(char *src, char **_next)
{
  char *next = src;

  while(1)
  {
    if(*next == '\n')
    {
      next++;
      break;
    }

    if(*next == 0)
      break;

    // Make sure there's only whitespace until the newline or null terminator.
    if(!isspace((int)*next))
    {
      *_next = next;
      return false;
    }

    next++;
  }

  *_next = next;
  return true;
}

// Returns -1 for no match, otherwise returns a strength. 0 is the strongest,
// higher numbers are weaker.

static int match_command(const struct mzx_command *command,
 struct token_collection *token_collection, char **_next)
{
  enum arg_type arg_type;
  enum arg_type_indexed *match_type;
  char *next;
  struct token *current_token = &(token_collection->tokens[0]);
  enum token_type token_type;
  int arg_position = 0;
  int parameters = command->parameters;
  int strength = 0;
  int match_next_arg = 0;

  // Step through each parameter in the command - it requires 0 to N
  // tokens (possibly depending on what tokens we have). If we need more
  // tokens then use get_token to get them to token_collection.

  token_collection_start_match_sequence(token_collection);

  while(arg_position < parameters)
  {
    arg_type = command->param_types[arg_position];
    arg_position++;

    // All args will take at least one token. If a token wants to try to match
    // the next argument in the list, don't get a new token.

    if(!match_next_arg)
    {
      // Alters current_token, token_type, and sometimes match_type
      get_token_skip_comments(goto no_match);
    }
    match_next_arg = 0;

    if(arg_type & ARG_TYPE_IGNORE)
    {
      parameters++;

      if(match_special_word_condition(current_token, token_type) &&
       (current_token->arg_value.special_word->arg_type == ARG_TYPE_IGNORE) &&
       (current_token->arg_value.special_word->instance_type == arg_type))
      {
        // We entered the optional ignore word - suck it up
        // and move on to the next argument.
        *match_type = ARG_TYPE_INDEXED_IGNORE;
        continue;
      }

      // Try to match the next argument in the list.
      match_next_arg = 1;
      continue;
    }

    if(arg_type & ARG_TYPE_FRAGMENT)
    {
      if(match_special_word_condition(current_token, token_type) &&
       (current_token->arg_value.special_word->arg_type == ARG_TYPE_FRAGMENT)
       && (current_token->arg_value.special_word->instance_type == arg_type))
      {
        *match_type = ARG_TYPE_INDEXED_FRAGMENT;
        continue;
      }
      else
      {
        // Must match ARG_TYPE_FRAGMENT of this instance - we can't escape
        // this one.
        goto no_match;
      }
    }
    else
    {
      // If the token can't be a command fragment then it can't be a command
      // word. So make this bail here if it is one.

      if(token_type == TOKEN_TYPE_BASIC_IDENTIFIER)
      {
        if(find_command_set(current_token->value, current_token->length))
          goto no_match;
      }
    }

    // Otherwise, check if it's any of these in order from lowest to greatest
    // precedence.
    if((arg_type & ARG_TYPE_IMMEDIATE) &&
     ((token_type == TOKEN_TYPE_NUMERIC_LITERAL_BASE10) ||
     (token_type == TOKEN_TYPE_NUMERIC_LITERAL_BASE16)))
    {
      *match_type = ARG_TYPE_INDEXED_IMMEDIATE;
      continue;
    }

    if((arg_type & ARG_TYPE_STRING) &&
     (token_type == TOKEN_TYPE_STRING_LITERAL))
    {
      *match_type = ARG_TYPE_INDEXED_STRING;
      continue;
    }

    if((arg_type & ARG_TYPE_CHARACTER) &&
     (token_type == TOKEN_TYPE_CHARACTER_LITERAL))
    {
      *match_type = ARG_TYPE_INDEXED_CHARACTER;
      continue;
    }

    if((arg_type & ARG_TYPE_COLOR) &&
     ((token_type == TOKEN_TYPE_BASIC_COLOR) ||
     (token_type == TOKEN_TYPE_WILDCARD_COLOR)))
    {
      *match_type = ARG_TYPE_INDEXED_COLOR;
      continue;
    }

    if((arg_type & ARG_TYPE_PARAM) &&
     ((token_type == TOKEN_TYPE_BASIC_PARAM) ||
     (token_type == TOKEN_TYPE_WILDCARD_PARAM)))
    {
      *match_type = ARG_TYPE_INDEXED_PARAM;
      continue;
    }

    if(match_arg_and_special_word_type(current_token, token_type, arg_type,
                                       ARG_TYPE_DIRECTION))
    {
      // Directions are peculiar things. They can actually contain multiple
      // direction words, so parse forward the extra ones.
      *match_type = ARG_TYPE_INDEXED_DIRECTION;
      current_token = match_direction(current_token, token_collection);

      if(current_token)
        continue;

      goto no_match;
    }

    if(match_arg_and_special_word_type(current_token, token_type, arg_type,
                                       ARG_TYPE_THING))
    {
      *match_type = ARG_TYPE_INDEXED_THING;
      continue;
    }

    if((arg_type & ARG_TYPE_EQUALITY) && (token_type == TOKEN_TYPE_EQUALITY))
    {
      *match_type = ARG_TYPE_INDEXED_EQUALITY;
      continue;
    }

    if(match_arg_and_special_word_type(current_token, token_type, arg_type,
                                       ARG_TYPE_CONDITION))
    {
      enum condition condition_type =
       current_token->arg_value.special_word->instance_type;
      *match_type = ARG_TYPE_INDEXED_CONDITION;

      switch(condition_type)
      {
        case WALKING:
        case TOUCHING:
        case BLOCKED:
        case LASTSHOT:
        case LASTTOUCH:
          // These condition types require a direction as well.
          get_token_skip_comments(goto no_match);

          if(match_special_word_type(current_token, token_type,
                                     ARG_TYPE_DIRECTION))
          {
            // Again directions can be two part.
            *match_type = ARG_TYPE_INDEXED_DIRECTION;
            current_token = match_direction(current_token, token_collection);

            if(current_token)
              continue;
          }
          goto no_match;

        default:
          continue;
      }
    }

    if(match_arg_and_special_word_type(current_token, token_type, arg_type,
                                       ARG_TYPE_ITEM))
    {
      *match_type = ARG_TYPE_INDEXED_ITEM;
      continue;
    }

    if((arg_type & (ARG_TYPE_COUNTER_LOAD_NAME | ARG_TYPE_COUNTER_STORE_NAME |
     ARG_TYPE_LABEL_NAME | ARG_TYPE_EXT_LABEL_NAME | ARG_TYPE_BOARD_NAME |
     ARG_TYPE_ROBOT_NAME)) &&
     ((token_type == TOKEN_TYPE_NAME) ||
     (token_type == TOKEN_TYPE_BASIC_IDENTIFIER) ||
     (token_type == TOKEN_TYPE_EXPRESSION) ||
     (token_type == TOKEN_TYPE_BASIC_COLOR) ||
     (token_type == TOKEN_TYPE_BASIC_PARAM)))
    {
      // A name match is a weak match, so take away from the strength by
      // decrementing it.
      strength++;
      if(arg_type & (ARG_TYPE_LABEL_NAME | ARG_TYPE_EXT_LABEL_NAME))
        *match_type  = ARG_TYPE_INDEXED_LABEL;
      else

      if(token_type == TOKEN_TYPE_EXPRESSION)
        *match_type = ARG_TYPE_INDEXED_EXPRESSION;
      else
        *match_type = ARG_TYPE_INDEXED_NAME;
      continue;
    }

    goto no_match;
  }

  next = current_token->value + current_token->length;

  while(1)
  {
    if(whitespace_until_newline(next, &next))
    {
      *_next = next;
      return strength;
    }
    else
    {
      // Pick off a comment.
      current_token = token_collection_get_token(token_collection,
       &match_type);

      if((current_token == NULL) || (current_token->type != TOKEN_TYPE_COMMENT))
        goto no_match;

      *match_type = ARG_TYPE_INDEXED_COMMENT;
      next = current_token->value + current_token->length;
    }
  }

 no_match:
  free(token_collection->match_types);
  return -1;
}

__editor_maybe_static struct token *parse_command(char *src, char **_next,
 int *_num_tokens)
{
  const struct command_set *command_set;
  const struct mzx_command *current_command = NULL;
  struct token *tokens;
  int command_number;
  int command_in_set;
  int command_name_length;
  int arg_in_match;
  char *command_base;
  char *next;
  int match_strength;
  int best_match_strength = 10000;
  int best_match_tokens = -1;
  int best_match_command_number = -1;

  enum arg_type_indexed *best_match_types = NULL;
  char *match_next;
  char *best_match_next = NULL;

  struct token_collection token_collection;

  next = skip_whitespace(src);

  // Could be a comment.
  if((next[0] == '/') && ((next[1] == '/') || (next[1] == '*')))
  {
    // Starting a command with a comment is like matching a command that has
    // zero length.
    command_set = &empty_command_set;
    current_command = &empty_command;
  }
  else
  {
    // What we need to do is get the first token, but this is a special one
    // that must just be an name.

    if(special_first_char[(int)(*next)])
      command_name_length = 1;
    else
      command_name_length = find_whitespace(next) - next;

    command_set = find_command_set(next, command_name_length);

    if(command_set == NULL)
      return NULL;
  }

  command_base = next;
  next += command_set->name_length;

  token_collection_initialize(&token_collection, next);

  tokens = token_collection.tokens;
  tokens[0].value = command_base;
  tokens[0].length = command_set->name_length;
  tokens[0].arg_type_indexed = ARG_TYPE_INDEXED_COMMAND;

  // Check to see if any of the commands in the set is a match.
  for(command_in_set = 0; command_in_set < command_set->count;
   command_in_set++)
  {
    command_number = command_set->offsets[command_in_set];
    current_command = &(command_list[command_number]);

    // See how well the current command matches.
    match_strength = match_command(current_command, &token_collection,
     &match_next);

    if(match_strength == -1)
    {
      // This means there was no match.
      continue;
    }

    // See if we got a stronger match than the strongest we have.
    if((match_strength < best_match_strength) &&
     (token_collection.token_position >= best_match_tokens))
    {
      // If so, throw out the best one and make this the best.
      if(best_match_types != NULL)
        free(best_match_types);

      best_match_command_number = command_number;
      best_match_tokens = token_collection.token_position;
      best_match_strength = match_strength;
      best_match_types = token_collection.match_types;
      best_match_next = match_next;
    }
    else
    {
      // Otherwise, throw out this one.
      free(token_collection.match_types);
    }

    // You can't get stronger than 0 - if that's what we got then we can
    // stop looking.
    if(best_match_strength == 0)
      break;
  }

  if(command_set->count == 0)
  {
    match_strength = match_command(current_command, &token_collection,
     &match_next);
    if(match_strength >= 0)
    {
      best_match_tokens = token_collection.token_position;
      best_match_types = token_collection.match_types;
      best_match_next = match_next;
      best_match_command_number = -1;
    }
  }

  if(best_match_tokens != -1)
  {
    // Size token array down to what we actually used.

    tokens = token_collection.tokens;
    tokens[0].arg_value.command_number = best_match_command_number;

    if(best_match_tokens > 1)
    {
      for(arg_in_match = 1; arg_in_match < best_match_tokens; arg_in_match++)
      {
        tokens[arg_in_match].arg_type_indexed = best_match_types[arg_in_match];
      }
    }
    free(best_match_types);

    if(_next)
      *_next = best_match_next;

    *_num_tokens = best_match_tokens;
    return tokens;
  }

  if(_next)
    *_next = src;

  *_num_tokens = -1;

  free(token_collection.tokens);
  return NULL;
}

#define assemble_get_token_skip_comments()                                     \
  token++;                                                                     \
                                                                               \
  while(token->type == TOKEN_TYPE_COMMENT)                                     \
  {                                                                            \
    token++;                                                                   \
  }                                                                            \

static int assemble_direction(struct token **_token)
{
  struct token *token = *_token;
  int combined_arg_value =
   token->arg_value.special_word->instance_type;
  int arg_value = combined_arg_value;

  while(arg_value > RANDB)
  {
    assemble_get_token_skip_comments();
    arg_value = token->arg_value.special_word->instance_type;
    combined_arg_value |= arg_value;
  }

  *_token = token;

  return combined_arg_value;
}

static int assemble_arg(struct token **_token, char *output,
 boolean escape_interpolation)
{
  boolean string_type = false;
  int arg_value = 0;
  char *value;
  int length;
  struct token *token = *_token;
  int escape_char = -1;

  switch(token->arg_type_indexed)
  {
    case ARG_TYPE_INDEXED_IMMEDIATE:
      arg_value = token->arg_value.numeric_literal;
      break;

    case ARG_TYPE_INDEXED_CHARACTER:
      arg_value = token->arg_value.char_literal;
      break;

    case ARG_TYPE_INDEXED_EQUALITY:
      arg_value = token->arg_value.equality_type;
      break;

    case ARG_TYPE_INDEXED_COLOR:
      arg_value = get_color(token->value);
      break;

    case ARG_TYPE_INDEXED_PARAM:
      arg_value = get_param(token->value);
      break;

    case ARG_TYPE_INDEXED_DIRECTION:
      arg_value = assemble_direction(&token);
      break;

    case ARG_TYPE_INDEXED_CONDITION:
      arg_value = token->arg_value.special_word->instance_type;

      switch(arg_value)
      {
        case WALKING:
        case TOUCHING:
        case BLOCKED:
        case LASTSHOT:
        case LASTTOUCH:
          assemble_get_token_skip_comments();
          arg_value |= (assemble_direction(&token) << 8);
          break;
      }
      break;

    case ARG_TYPE_INDEXED_ITEM:
    case ARG_TYPE_INDEXED_THING:
      arg_value = token->arg_value.special_word->instance_type;
      break;

    case ARG_TYPE_INDEXED_NAME:
    case ARG_TYPE_INDEXED_LABEL:
      string_type = true;
      value = token->value;
      length = token->length;

      if(*value == '`')
      {
        value++;
        length -= 2;
        escape_char = '`';
      }
      break;

    case ARG_TYPE_INDEXED_STRING:
      string_type = true;
      value = token->value;
      length = token->length;

      value++;
      length -= 2;
      escape_char = '"';
      break;

    case ARG_TYPE_INDEXED_EXPRESSION:
      string_type = true;
      value = token->value;
      length = token->length;
      break;

    case ARG_TYPE_INDEXED_COMMENT:
    case ARG_TYPE_INDEXED_IGNORE:
    case ARG_TYPE_INDEXED_FRAGMENT:
    case ARG_TYPE_INDEXED_COMMAND:
      *_token = token + 1;
      return 0;
  }

  *_token = token + 1;

  if(string_type)
  {
    int raw_string_length = 0;
    char *output_base = output;
    int string_offset = 0;

    output++;

    while(string_offset < length)
    {
      if((value[string_offset] == '\\') &&
       (value[string_offset + 1] == '\\') && (escape_interpolation == false))
      {
        // This is a strange case where the \\ is preserved as such so it can
        // be escaped at runtime, but still has to be parsed as \\ so as to avoid
        // escaping what comes next.
        string_offset += 2;
        output[0] = '\\';
        output[1] = '\\';
        output += 2;
        raw_string_length += 2;
      }
      else
      {
        string_offset +=
         escape_chars(output, value + string_offset, escape_char,
         escape_interpolation);

        output++;
        raw_string_length++;
      }
    }
    *output_base = raw_string_length + 1;
    *output = 0;

    return raw_string_length + 2;
  }
  else
  {
    output[0] = 0;
    output[1] = arg_value;
    output[2] = arg_value >> 8;
    return 3;
  }
}

static char *assemble_command(char *src, char **_output)
{
  char *next;
  int num_parse_tokens;
  struct token *parse_tokens = parse_command(src, &next, &num_parse_tokens);

  if(parse_tokens)
  {
    int command_number = parse_tokens[0].arg_value.command_number;

    if(command_number != -1)
    {
      char *output_base = *_output;
      char *output = output_base + 2;
      struct token *current_token = &(parse_tokens[1]);
      int command_length = 1;
      int arg_length;

      output_base[1] = command_number;

      while(current_token != &(parse_tokens[num_parse_tokens]))
      {
        arg_length = assemble_arg(&current_token, output,
         (command_number == ROBOTIC_CMD_LABEL));
        output += arg_length;
        command_length += arg_length;
      }

      output_base[0] = command_length;
      output_base[command_length + 1] = command_length;
      *_output = output + 1;
    }
    free(parse_tokens);
    return next;
  }

  {
    char short_output[512];
    int newline_pos = strcspn(src, "\n");
    if(newline_pos > 0)
    {
      snprintf(short_output, sizeof(short_output), "%.*s", newline_pos, src);
      short_output[sizeof(short_output) - 1] = 0;
      warn("Failed to assemble command: %s\n", short_output);
    }
  }
  return NULL;
}

void assemble_program(char *program_source, char **_bytecode,
 int *_bytecode_length, struct command_mapping **_command_map,
 int *_command_map_length)
{
  int bytecode_length = 1;
  int bytecode_offset = 1;
  int bytecode_length_allocated = 256;
  char *program_bytecode = malloc(bytecode_length_allocated);

  char command_buffer[512];
  char *output;

  int command_length;

  int cmd_map_length = 1;
  int cmd_map_allocated = 256;
  struct command_mapping *cmd_map = NULL;
  char *src_start = program_source;
  char *line_start = NULL;
  char *line_text = NULL;
  int real_line = 1;

  if(_command_map)
  {
    cmd_map = malloc(cmd_map_allocated * sizeof(struct command_mapping));
    cmd_map[0].real_line = 0;
    cmd_map[0].bc_pos = 0;
    cmd_map[0].src_pos = -1;
  }

  program_bytecode[0] = 0xFF;

  do
  {
    if(cmd_map)
    {
      line_start = program_source;
      line_text = program_source;

      // Count whitespace lines until start of token.
      // FIXME this is a hack
      while(whitespace_until_newline(line_text, &line_text))
      {
        line_start = line_text;
        real_line++;
        if(!*line_text)
          break;
      }

      if(cmd_map_length >= cmd_map_allocated)
      {
        cmd_map_allocated *= 2;
        cmd_map =
         realloc(cmd_map, cmd_map_allocated * sizeof(struct command_mapping));
      }

      cmd_map[cmd_map_length].real_line = real_line;
      cmd_map[cmd_map_length].bc_pos = bytecode_offset;
      cmd_map[cmd_map_length].src_pos = line_start - src_start;
      cmd_map_length++;

      // Disassemble command into command_buffer.
      output = command_buffer;
      program_source = assemble_command(program_source, &output);

      // Comment? backtrack
      if(output == command_buffer)
        cmd_map_length--;

      // Count the number of lines passed during assembly.
      // FIXME this is a hack
      while(line_text < program_source)
      {
        if(*line_text == '\n')
          real_line++;

        line_text++;
      }
    }

    else
    {
      // Disassemble command into command_buffer.
      output = command_buffer;
      program_source = assemble_command(program_source, &output);
    }

    // See if it's valid
    if(program_source)
    {
      // Increment the total size (plus size for new line)
      command_length = output - command_buffer;
      bytecode_length += command_length;

      // Reallocate buffer if necessary.
      while(bytecode_length > bytecode_length_allocated)
      {
        bytecode_length_allocated *= 2;
        program_bytecode =
         realloc(program_bytecode, bytecode_length_allocated);
      }

      // Copy new assembly into buffer.
      memcpy(program_bytecode + bytecode_offset, command_buffer,
       command_length);

      // Offset bytecode (source) position.
      bytecode_offset += command_length;
    }
  } while(program_source);

  // Terminate program with 0.
  bytecode_length++;
  program_bytecode = realloc(program_bytecode, bytecode_length);
  program_bytecode[bytecode_offset] = 0;

  *_bytecode = program_bytecode;
  *_bytecode_length = bytecode_length;

  // Shrink the command mapping.
  if(cmd_map)
  {
    cmd_map = realloc(cmd_map, cmd_map_length * sizeof(struct command_mapping));

    *_command_map = cmd_map;
    *_command_map_length = cmd_map_length;
  }
}


enum legacy_command_number
{
  ROBOTIC_CMD_UNUSED  = 0xFF00,
  ROBOTIC_CMD_REMOVED = 0xFF01
};

static const int legacy_command_to_current[256] =
{
  ROBOTIC_CMD_END,
  ROBOTIC_CMD_DIE,
  ROBOTIC_CMD_WAIT,
  ROBOTIC_CMD_CYCLE,
  ROBOTIC_CMD_GO,
  ROBOTIC_CMD_WALK,
  ROBOTIC_CMD_BECOME,
  ROBOTIC_CMD_CHAR,
  ROBOTIC_CMD_COLOR,
  ROBOTIC_CMD_GOTOXY,
  ROBOTIC_CMD_SET,
  ROBOTIC_CMD_INC,
  ROBOTIC_CMD_DEC,
  ROBOTIC_CMD_UNUSED,
  ROBOTIC_CMD_UNUSED,
  ROBOTIC_CMD_UNUSED,
  ROBOTIC_CMD_IF,
  ROBOTIC_CMD_UNUSED,
  ROBOTIC_CMD_IF_CONDITION,
  ROBOTIC_CMD_IF_NOT_CONDITION,
  ROBOTIC_CMD_IF_ANY,
  ROBOTIC_CMD_IF_NO,
  ROBOTIC_CMD_IF_THING_DIR,
  ROBOTIC_CMD_IF_NOT_THING_DIR,
  ROBOTIC_CMD_IF_THING_XY,
  ROBOTIC_CMD_IF_AT,
  ROBOTIC_CMD_IF_DIR_OF_PLAYER,
  ROBOTIC_CMD_DOUBLE,
  ROBOTIC_CMD_HALF,
  ROBOTIC_CMD_GOTO,
  ROBOTIC_CMD_SEND,
  ROBOTIC_CMD_EXPLODE,
  ROBOTIC_CMD_PUT_DIR,
  ROBOTIC_CMD_GIVE,
  ROBOTIC_CMD_TAKE,
  ROBOTIC_CMD_TAKE_OR,
  ROBOTIC_CMD_ENDGAME,
  ROBOTIC_CMD_ENDLIFE,
  ROBOTIC_CMD_MOD,
  ROBOTIC_CMD_SAM,
  ROBOTIC_CMD_VOLUME,
  ROBOTIC_CMD_END_MOD,
  ROBOTIC_CMD_END_SAM,
  ROBOTIC_CMD_PLAY,
  ROBOTIC_CMD_END_PLAY,
  ROBOTIC_CMD_WAIT_THEN_PLAY,
  ROBOTIC_CMD_WAIT_PLAY,
  ROBOTIC_CMD_BLANK_LINE,
  ROBOTIC_CMD_SFX,
  ROBOTIC_CMD_PLAY_IF_SILENT,
  ROBOTIC_CMD_OPEN,
  ROBOTIC_CMD_LOCKSELF,
  ROBOTIC_CMD_UNLOCKSELF,
  ROBOTIC_CMD_SEND_DIR,
  ROBOTIC_CMD_ZAP,
  ROBOTIC_CMD_RESTORE,
  ROBOTIC_CMD_LOCKPLAYER,
  ROBOTIC_CMD_UNLOCKPLAYER,
  ROBOTIC_CMD_LOCKPLAYER_NS,
  ROBOTIC_CMD_LOCKPLAYER_EW,
  ROBOTIC_CMD_LOCKPLAYER_ATTACK,
  ROBOTIC_CMD_MOVE_PLAYER_DIR,
  ROBOTIC_CMD_MOVE_PLAYER_DIR_OR,
  ROBOTIC_CMD_PUT_PLAYER_XY,
  ROBOTIC_CMD_REMOVED,
  ROBOTIC_CMD_REMOVED,
  ROBOTIC_CMD_IF_PLAYER_XY,
  ROBOTIC_CMD_PUT_PLAYER_DIR,
  ROBOTIC_CMD_TRY_DIR,
  ROBOTIC_CMD_ROTATECW,
  ROBOTIC_CMD_ROTATECCW,
  ROBOTIC_CMD_SWITCH,
  ROBOTIC_CMD_SHOOT,
  ROBOTIC_CMD_LAYBOMB,
  ROBOTIC_CMD_LAYBOMB_HIGH,
  ROBOTIC_CMD_SHOOTMISSILE,
  ROBOTIC_CMD_SHOOTSEEKER,
  ROBOTIC_CMD_SPITFIRE,
  ROBOTIC_CMD_LAZERWALL,
  ROBOTIC_CMD_PUT_XY,
  ROBOTIC_CMD_DIE_ITEM,
  ROBOTIC_CMD_SEND_XY,
  ROBOTIC_CMD_COPYROBOT_NAMED,
  ROBOTIC_CMD_COPYROBOT_XY,
  ROBOTIC_CMD_COPYROBOT_DIR,
  ROBOTIC_CMD_DUPLICATE_SELF_DIR,
  ROBOTIC_CMD_DUPLICATE_SELF_XY,
  ROBOTIC_CMD_BULLETN,
  ROBOTIC_CMD_BULLETS,
  ROBOTIC_CMD_BULLETE,
  ROBOTIC_CMD_BULLETW,
  ROBOTIC_CMD_GIVEKEY,
  ROBOTIC_CMD_GIVEKEY_OR,
  ROBOTIC_CMD_TAKEKEY,
  ROBOTIC_CMD_TAKEKEY_OR,
  ROBOTIC_CMD_INC_RANDOM,
  ROBOTIC_CMD_DEC_RANDOM,
  ROBOTIC_CMD_SET_RANDOM,
  ROBOTIC_CMD_TRADE,
  ROBOTIC_CMD_SEND_DIR_PLAYER,
  ROBOTIC_CMD_PUT_DIR_PLAYER,
  ROBOTIC_CMD_SLASH,
  ROBOTIC_CMD_MESSAGE_LINE,
  ROBOTIC_CMD_MESSAGE_BOX_LINE,
  ROBOTIC_CMD_MESSAGE_BOX_OPTION,
  ROBOTIC_CMD_MESSAGE_BOX_MAYBE_OPTION,
  ROBOTIC_CMD_LABEL,
  ROBOTIC_CMD_COMMENT,
  ROBOTIC_CMD_ZAPPED_LABEL,
  ROBOTIC_CMD_TELEPORT,
  ROBOTIC_CMD_SCROLLVIEW,
  ROBOTIC_CMD_INPUT,
  ROBOTIC_CMD_IF_INPUT,
  ROBOTIC_CMD_IF_INPUT_NOT,
  ROBOTIC_CMD_IF_INPUT_MATCHES,
  ROBOTIC_CMD_PLAYER_CHAR,
  ROBOTIC_CMD_MESSAGE_BOX_COLOR_LINE,
  ROBOTIC_CMD_MESSAGE_BOX_CENTER_LINE,
  ROBOTIC_CMD_MOVE_ALL,
  ROBOTIC_CMD_COPY,
  ROBOTIC_CMD_SET_EDGE_COLOR,
  ROBOTIC_CMD_BOARD,
  ROBOTIC_CMD_BOARD_IS_NONE,
  ROBOTIC_CMD_CHAR_EDIT,
  ROBOTIC_CMD_BECOME_PUSHABLE,
  ROBOTIC_CMD_BECOME_NONPUSHABLE,
  ROBOTIC_CMD_BLIND,
  ROBOTIC_CMD_FIREWALKER,
  ROBOTIC_CMD_FREEZETIME,
  ROBOTIC_CMD_SLOWTIME,
  ROBOTIC_CMD_WIND,
  ROBOTIC_CMD_AVALANCHE,
  ROBOTIC_CMD_COPY_DIR,
  ROBOTIC_CMD_BECOME_LAVAWALKER,
  ROBOTIC_CMD_BECOME_NONLAVAWALKER,
  ROBOTIC_CMD_CHANGE,
  ROBOTIC_CMD_PLAYERCOLOR,
  ROBOTIC_CMD_BULLETCOLOR,
  ROBOTIC_CMD_MISSILECOLOR,
  ROBOTIC_CMD_MESSAGE_ROW,
  ROBOTIC_CMD_REL_SELF,
  ROBOTIC_CMD_REL_PLAYER,
  ROBOTIC_CMD_REL_COUNTERS,
  ROBOTIC_CMD_SET_ID_CHAR,
  ROBOTIC_CMD_JUMP_MOD_ORDER,
  ROBOTIC_CMD_ASK,
  ROBOTIC_CMD_FILLHEALTH,
  ROBOTIC_CMD_THICK_ARROW,
  ROBOTIC_CMD_THIN_ARROW,
  ROBOTIC_CMD_SET_MAX_HEALTH,
  ROBOTIC_CMD_SAVE_PLAYER_POSITION,
  ROBOTIC_CMD_RESTORE_PLAYER_POSITION,
  ROBOTIC_CMD_EXCHANGE_PLAYER_POSITION,
  ROBOTIC_CMD_MESSAGE_COLUMN,
  ROBOTIC_CMD_CENTER_MESSAGE,
  ROBOTIC_CMD_CLEAR_MESSAGE,
  ROBOTIC_CMD_RESETVIEW,
  ROBOTIC_CMD_MOD_SAM,
  ROBOTIC_CMD_VOLUME2,
  ROBOTIC_CMD_SCROLLBASE,
  ROBOTIC_CMD_SCROLLCORNER,
  ROBOTIC_CMD_SCROLLTITLE,
  ROBOTIC_CMD_SCROLLPOINTER,
  ROBOTIC_CMD_SCROLLARROW,
  ROBOTIC_CMD_VIEWPORT,
  ROBOTIC_CMD_VIEWPORT_WIDTH,
  ROBOTIC_CMD_UNUSED,
  ROBOTIC_CMD_UNUSED,
  ROBOTIC_CMD_SAVE_PLAYER_POSITION_N,
  ROBOTIC_CMD_RESTORE_PLAYER_POSITION_N,
  ROBOTIC_CMD_EXCHANGE_PLAYER_POSITION_N,
  ROBOTIC_CMD_RESTORE_PLAYER_POSITION_N_DUPLICATE_SELF,
  ROBOTIC_CMD_EXCHANGE_PLAYER_POSITION_N_DUPLICATE_SELF,
  ROBOTIC_CMD_PLAYER_BULLETN,
  ROBOTIC_CMD_PLAYER_BULLETS,
  ROBOTIC_CMD_PLAYER_BULLETE,
  ROBOTIC_CMD_PLAYER_BULLETW,
  ROBOTIC_CMD_NEUTRAL_BULLETN,
  ROBOTIC_CMD_NEUTRAL_BULLETS,
  ROBOTIC_CMD_NEUTRAL_BULLETE,
  ROBOTIC_CMD_NEUTRAL_BULLETW,
  ROBOTIC_CMD_ENEMY_BULLETN,
  ROBOTIC_CMD_ENEMY_BULLETS,
  ROBOTIC_CMD_ENEMY_BULLETE,
  ROBOTIC_CMD_ENEMY_BULLETW,
  ROBOTIC_CMD_PLAYER_BULLET_COLOR,
  ROBOTIC_CMD_NEUTRAL_BULLET_COLOR,
  ROBOTIC_CMD_ENEMY_BULLET_COLOR,
  ROBOTIC_CMD_UNUSED,
  ROBOTIC_CMD_UNUSED,
  ROBOTIC_CMD_UNUSED,
  ROBOTIC_CMD_UNUSED,
  ROBOTIC_CMD_UNUSED,
  ROBOTIC_CMD_REL_SELF_FIRST,
  ROBOTIC_CMD_REL_SELF_LAST,
  ROBOTIC_CMD_REL_PLAYER_FIRST,
  ROBOTIC_CMD_REL_PLAYER_LAST,
  ROBOTIC_CMD_REL_COUNTERS_FIRST,
  ROBOTIC_CMD_REL_COUNTERS_LAST,
  ROBOTIC_CMD_MOD_FADE_OUT,
  ROBOTIC_CMD_MOD_FADE_IN,
  ROBOTIC_CMD_COPY_BLOCK,
  ROBOTIC_CMD_CLIP_INPUT,
  ROBOTIC_CMD_PUSH,
  ROBOTIC_CMD_SCROLL_CHAR,
  ROBOTIC_CMD_FLIP_CHAR,
  ROBOTIC_CMD_COPY_CHAR,
  ROBOTIC_CMD_UNUSED,
  ROBOTIC_CMD_UNUSED,
  ROBOTIC_CMD_UNUSED,
  ROBOTIC_CMD_CHANGE_SFX,
  ROBOTIC_CMD_COLOR_INTENSITY_ALL,
  ROBOTIC_CMD_COLOR_INTENSITY_N,
  ROBOTIC_CMD_COLOR_FADE_OUT,
  ROBOTIC_CMD_COLOR_FADE_IN,
  ROBOTIC_CMD_SET_COLOR,
  ROBOTIC_CMD_LOAD_CHAR_SET,
  ROBOTIC_CMD_MULTIPLY,
  ROBOTIC_CMD_DIVIDE,
  ROBOTIC_CMD_MODULO,
  ROBOTIC_CMD_PLAYER_CHAR_DIR,
  ROBOTIC_CMD_UNUSED,
  ROBOTIC_CMD_LOAD_PALETTE,
  ROBOTIC_CMD_UNUSED,
  ROBOTIC_CMD_MOD_FADE_TO,
  ROBOTIC_CMD_SCROLLVIEW_XY,
  ROBOTIC_CMD_SWAP_WORLD,
  ROBOTIC_CMD_IF_ALIGNEDROBOT,
  ROBOTIC_CMD_UNUSED,
  ROBOTIC_CMD_LOCKSCROLL,
  ROBOTIC_CMD_UNLOCKSCROLL,
  ROBOTIC_CMD_IF_FIRST_INPUT,
  ROBOTIC_CMD_PERSISTENT_GO,
  ROBOTIC_CMD_WAIT_MOD_FADE,
  ROBOTIC_CMD_UNUSED,
  ROBOTIC_CMD_ENABLE_SAVING,
  ROBOTIC_CMD_DISABLE_SAVING,
  ROBOTIC_CMD_ENABLE_SENSORONLY_SAVING,
  ROBOTIC_CMD_STATUS_COUNTER,
  ROBOTIC_CMD_OVERLAY_ON,
  ROBOTIC_CMD_OVERLAY_STATIC,
  ROBOTIC_CMD_OVERLAY_TRANSPARENT,
  ROBOTIC_CMD_OVERLAY_PUT_OVERLAY,
  ROBOTIC_CMD_COPY_OVERLAY_BLOCK,
  ROBOTIC_CMD_UNUSED_244,
  ROBOTIC_CMD_CHANGE_OVERLAY,
  ROBOTIC_CMD_CHANGE_OVERLAY_COLOR,
  ROBOTIC_CMD_WRITE_OVERLAY,
  ROBOTIC_CMD_UNUSED,
  ROBOTIC_CMD_UNUSED,
  ROBOTIC_CMD_UNUSED,
  ROBOTIC_CMD_LOOP_START,
  ROBOTIC_CMD_LOOP_FOR,
  ROBOTIC_CMD_ABORT_LOOP,
  ROBOTIC_CMD_DISABLE_MESG_EDGE,
  ROBOTIC_CMD_ENABLE_MESG_EDGE
};

static boolean is_simple_identifier_name(char *src, int length,
 boolean allow_special_words)
{
  // Length can't be zero (for ``)
  if(length == 0)
    return false;

  // It must not be a command name.
  if(allow_special_words == false)
  {
    if(find_command_set(src, length))
      return false;

    if(find_special_word(src, length))
      return false;
  }

  // Can't start with numeric
  if(isdigit((int)*src))
    return false;

  // And it must not contain any invalid chars.
  while(length)
  {
    if(!is_identifier_char(*src))
      return false;

    src++;
    length--;
  }

  return true;
}

#define legacy_disassemble_print_sequence(condition)                          \
  int string_length = *_string_length;                                        \
  char *output = *_output;                                                    \
                                                                              \
  while((condition) && string_length)                                         \
  {                                                                           \
    *output = *src;                                                           \
    output++;                                                                 \
    src++;                                                                    \
    string_length--;                                                          \
  }                                                                           \
                                                                              \
  *_output = output;                                                          \
  *_string_length = string_length;                                            \
  return src;                                                                 \


static char *legacy_disassemble_print_spaces(char *src, char **_output,
 int *_string_length)
{
  legacy_disassemble_print_sequence(*src == ' ');
}

static char *legacy_disassemble_print_decimal(char *src, char **_output,
 int *_string_length)
{
  legacy_disassemble_print_sequence(isdigit((int)*src));
}

static char *legacy_disassemble_print_hex(char *src, char **_output,
 int *_string_length)
{
  legacy_disassemble_print_sequence(isxdigit((int)*src));
}

static char *legacy_disassemble_print_expression(char *src, char **_output,
 int *_string_length, boolean is_interpolation);
static char *legacy_disassemble_print_string_expressions(char *src,
 char **_output, int *_string_length, int early_terminator,
 int escape_char, int is_expression_interpolation);

static char *legacy_disassemble_print_binary_operator(char *src,
 char **_output, int *_string_length)
{
  char *output = *_output;
  char operator_char = *src;
  int string_length = *_string_length - 1;

  switch(operator_char)
  {
    // One char operators, not the start of a two char one.
    case '+':
    case '-':
    case '/':
    case '%':
    case '~':
    case '*':
    case '=':
    case '?':
    case ':':
      *output = operator_char;
      output++;
      break;

    case 'x':
      *output = '^';
      output++;
      break;

    case 'o':
      *output = '|';
      output++;
      break;

    case 'a':
      *output = '&';
      output++;
      break;

    case '^':
      output[0] = '*';
      output[1] = '*';
      output += 2;
      break;

    case '>':
    case '<':
      *output = operator_char;
      output++;
      if(src[1] == operator_char)
      {
        *output = operator_char;
        output++;
        if(src[2] == operator_char)
        {
          *output = operator_char;
          output++;
          src++;
          string_length--;
        }
        src++;
        string_length--;
      }
      if(src[1] == '=')
      {
        *output = '=';
        output++;
        src++;
        string_length--;
      }
      break;

    case '!':
      *output = operator_char;
      output++;
      if(src[1] == '=')
      {
        *output = '=';
        output++;
        src++;
        string_length--;
      }
      break;


    default:
      return src;
      break;
  }

  *_output = output;
  *_string_length = string_length;
  return src + 1;
}


static char *legacy_disassemble_print_expr_value_token(char *src,
 char **_output, int *_string_length, boolean is_first, boolean is_interpolation)
{
  int string_length = *_string_length;
  char *output = *_output;
  char *next = src;
  char *next_next;

  // Unary operators are okay, just eats up spaces until next ones.
  while((*next == '-') || (*next == '~') || (*next == '!'))
  {
    *output = *next;
    output++;
    string_length--;
    next = legacy_disassemble_print_spaces(next + 1, &output, &string_length);

    if(string_length == 0)
      return src;
  }

  // If it's an operator then it's not an expression.
  switch(*next)
  {
    // Immediate or hex
    case '0':
      if(next[1] == 'x')
      {
        if(string_length < 3)
          return 0;
        string_length -= 2;
        output[0] = '0';
        output[1] = 'x';
        output += 2;
        next = legacy_disassemble_print_hex(next + 2, &output, &string_length);
      }
      else
      {
        next = legacy_disassemble_print_decimal(next, &output, &string_length);
      }

      if(string_length == 0)
        return src;

      break;

    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
      next = legacy_disassemble_print_decimal(next, &output, &string_length);

      if(string_length == 0)
        return src;

      break;

    case '(':
      // Start of an expression, verify that it is one.
      next_next =
       legacy_disassemble_print_expression(next, &output, &string_length, false);

      if((string_length == 0) || (next_next == next))
        return src;

      next = next_next;
      break;

    case '\'':
    case '&':
    {
      int name_length;
      char base_char = *next;
      char *base_output = output;
      boolean wrap_parentheses = false;
      boolean wrap_backticks = false;

      next_next = legacy_disassemble_print_string_expressions(next + 1,
       &output, &string_length, base_char, '`', false);

      name_length = output - base_output;

      if(*next_next != base_char)
        return src;

      // Take off the enclosing chars that weren't printed.
      string_length -= 2;

      if(is_first && is_interpolation &&
       is_string_valued_ident(next + 1, name_length, is_interpolation))
      {
        // Hack: wrap in parentheses so this doesn't get mistaken for a
        // string interpolation. It might be nice to have a more readable
        // way of supporting this someday e.g. int($string).
        wrap_parentheses = true;
      }

      if(!is_simple_identifier_name(next + 1, name_length, true))
      {
        wrap_backticks = true;
      }

      if(wrap_parentheses || wrap_backticks)
      {
        int num = wrap_parentheses + wrap_backticks;
        int pos = 0;
        memmove(base_output + num, base_output, name_length);

        output += num * 2;
        if(wrap_parentheses)
        {
          base_output[pos] = '(';
          output[-1 - pos] = ')';
          pos++;
        }
        if(wrap_backticks)
        {
          base_output[pos] = '`';
          output[-1 - pos] = '`';
          pos++;
        }
      }

      next = next_next + 1;
      break;
    }

    default:
      // Invalid.
      return src;
  }

  *_string_length = string_length;
  *_output = output;
  return next;
}

static char *legacy_disassemble_print_expression(char *src, char **_output,
 int *_string_length, boolean is_interpolation)
{
  char *next, *next_next;
  char *output = *_output;
  int string_length = *_string_length;

  // First character must be the opening paren, but if it weren't we
  // wouldn't be here. However, the string_length must be greater than
  // 1.
  // When interpolating, convert to interpolation brackets instead of ().

  *output = is_interpolation ? '{' : '(';
  output++;
  next = src + 1;
  string_length--;

  next = legacy_disassemble_print_spaces(next, &output, &string_length);
  if(string_length == 0)
    return src;

  // Need a value to kick things off.
  next_next = legacy_disassemble_print_expr_value_token(next, &output,
   &string_length, true, is_interpolation);

  if((string_length == 0) || (next_next == next))
    return src;

  next = next_next;

  // Now keep going until we hit the end - for every
  // time we don't we need an operator then a value.
  while(*next != ')')
  {
    if(string_length == 0)
      return src;

    next = legacy_disassemble_print_spaces(next, &output, &string_length);

    if(*next == ')')
      break;

    next_next = legacy_disassemble_print_binary_operator(next, &output,
     &string_length);

    if((string_length == 0) || (next_next == next))
      return src;

    next = legacy_disassemble_print_spaces(next_next, &output, &string_length);

    next_next = legacy_disassemble_print_expr_value_token(next, &output,
     &string_length, false, is_interpolation);

    if((string_length == 0) || (next_next == next))
      return src;

    next = legacy_disassemble_print_spaces(next_next, &output, &string_length);
  }

  *output = is_interpolation ? '}' : ')';
  output++;
  next++;
  string_length--;

  *_string_length = string_length;
  *_output = output;
  return next;
}

// If you don't want an early terminator then give this -1.
static char *legacy_disassemble_print_string_expressions(char *src,
 char **_output, int *_string_length, int early_terminator,
 int escape_char, int is_expression_interpolation)
{
  char current_char = *src;
  char *output = *_output;
  int string_length = *_string_length;

  while(string_length && (current_char != early_terminator))
  {
    // Hack: legacy Robotic allowed unclosed interpolation in expression
    // identifiers to work, so we need to as well.
    if(current_char == '\'' && is_expression_interpolation)
      break;

    if(current_char == '(')
    {
      char *next = legacy_disassemble_print_expression(src, &output,
       &string_length, true);

      if(next == src)
      {
        output[0] = '(';
        output++;
        src++;
        string_length--;
      }
      else
      {
        src = next;
      }
    }
    else

    if(current_char == '&')
    {
      // Convert old interpolation to new interpolation.
      if((string_length > 1) && (src[1] == '&'))
      {
        // Or just print an &.. no more of this && stuff.
        output[0] = '&';
        output++;
        src += 2;
        string_length -= 2;
      }
      else
      {
        int name_length;
        char *next;
        char *base_output;
        char *name_offset;

        src++;

        base_output = output;

        // Make room for the expression bracket.
        output++;

        string_length--;

        // Special formatting for + and #
        if((*src == '+') || (*src == '#'))
        {
          *output = *src;
          output++;
          src++;
          string_length--;
        }

        name_offset = output;

        next = legacy_disassemble_print_string_expressions(src, &output,
         &string_length, '&', -1, true);

        name_length = output - name_offset;

        // Consume the terminating char if it's an &. Interpolation can also
        // terminate with a null or a ' (due to a bug); in these cases, don't.
        if(*next == '&')
        {
          string_length--;
          next++;
        }

        if(!is_simple_identifier_name(name_offset, name_length, true))
        {
          memmove(name_offset + 1, name_offset, name_length);
          *name_offset = '`';
          output[1] = '`';
          output += 2;
        }

        *base_output = '{';
        *output = '}';
        output++;
        src = next;
      }
    }
    else
    {
      if((current_char == '{') || (current_char == '}') ||
       (current_char == escape_char))
      {
        output[0] = '\\';
        output[1] = current_char;
        output += 2;
      }
      else
      {
        output += unescape_char(output, current_char);
      }
      src++;
      string_length--;
    }
    current_char = *src;
  }

  *_string_length = string_length;
  *_output = output;
  return src;
}


#define write_arg_word(str)                                                    \
{                                                                              \
  const char *_str = str;                                                      \
  strcpy(output, _str);                                                        \
  *_output = output + strlen(_str);                                            \
  return src;                                                                  \
}                                                                              \

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

static char *print_dir(int dir, char *output)
{
  if(dir & 0x10)
  {
    strcpy(output, "RANDP ");
    output += 6;
  }

  if(dir & 0x20)
  {
    strcpy(output, "CW ");
    output += 3;
  }

  if(dir & 0x40)
  {
    strcpy(output, "OPP ");
    output += 4;
  }

  if(dir & 0x80)
  {
    strcpy(output, "RANDNOT ");
    output += 8;
  }

  strcpy(output, dir_names[dir & 0xF]);
  return output + strlen(dir_names[dir & 0xF]);
}

static char *legacy_disassemble_arg(enum arg_type arg_type, char *src,
 char **_output, boolean print_ignores, int base)
{
  char *output = *_output;
  int compiled_arg_type;

  // First two take nothing from the arg stream.
  if(arg_type & ARG_TYPE_FRAGMENT)
    write_arg_word(command_fragment_type_names[arg_type & ~ARG_TYPE_FRAGMENT]);

  if(arg_type & ARG_TYPE_IGNORE)
  {
    if(print_ignores)
      write_arg_word(ignore_type_names[arg_type & ~ARG_TYPE_IGNORE]);

    return src;
  }

  // 0 is int, non-zero is string of that length.
  compiled_arg_type = *src;

  if(compiled_arg_type == 0)
  {
    int compiled_arg_value = src[1] | (src[2] << 8);
    src += 3;

    if(arg_type & ARG_TYPE_ITEM)
    {
      if(compiled_arg_value < (int)ARRAY_SIZE(item_names))
      {
        write_arg_word(item_names[compiled_arg_value]);
      }
      else
      {
        write_arg_word("INVALID_ITEM");
      }
    }

    if(arg_type & ARG_TYPE_CONDITION)
    {
      int condition = compiled_arg_value & 0xFF;
      int direction = compiled_arg_value >> 8;
      const char *_str;

      if(condition < (int)ARRAY_SIZE(condition_names))
        _str = condition_names[condition];

      else
        _str = "invalid_condition";

      strcpy(output, _str);
      output += strlen(_str);

      // Some conditions have a direction after them too.
      switch(condition)
      {
        case WALKING:
        case TOUCHING:
        case BLOCKED:
        case LASTSHOT:
        case LASTTOUCH:
          // Get next arg
          *output = ' ';
          output++;
          output = print_dir(direction, output);
          break;
      }
      *_output = output;
      return src;
    }

    if(arg_type & ARG_TYPE_EQUALITY)
    {
      if(compiled_arg_value < (int)ARRAY_SIZE(equality_names))
      {
        write_arg_word(equality_names[compiled_arg_value]);
      }
      else
      {
        write_arg_word("??");
      }
    }

    if(arg_type & ARG_TYPE_THING)
    {
      if(compiled_arg_value < (int)ARRAY_SIZE(thing_names))
      {
        write_arg_word(thing_names[compiled_arg_value]);
      }
      else
      {
        write_arg_word("Invalid_thing");
      }
    }

    if(arg_type & ARG_TYPE_DIRECTION)
    {
      *_output = print_dir(compiled_arg_value, output);
      return src;
    }

    if(arg_type & ARG_TYPE_PARAM)
    {
      /* Note: disassembling invalid values modulo 256 to match how
       * they are interpreted. The DOS disassembler would disassemble
       * all invalid values to p?? and the port disassembler would
       * produce either p?? or junk prior to 2.93c.
       */
      output[0] = 'p';
      if(compiled_arg_value == 0x100)
      {
        output[1] = '?';
        output[2] = '?';
        output[3] = 0;
      }
      else
      {
        sprintf(output + 1, "%02x", compiled_arg_value & 0xff);
      }
      *_output = output + 3;
      return src;
    }

    if(arg_type & ARG_TYPE_COLOR)
    {
      print_color(compiled_arg_value, output);
      *_output = output + 3;
      return src;
    }

    /* Character: prefer immediate representation if not a normal char. */
    if((arg_type & ARG_TYPE_CHARACTER) && (compiled_arg_value & 0xff00))
      arg_type &= ~ARG_TYPE_CHARACTER;

    if(arg_type & ARG_TYPE_CHARACTER)
    {
      *output = '\'';
      output++;
      output += unescape_char(output, compiled_arg_value);
      *output = '\'';
      *_output = output + 1;

      return src;
    }

    if(arg_type & ARG_TYPE_IMMEDIATE)
    {
      char immediate_value[16];

      if(base == 10)
        sprintf(immediate_value, "%d", (short)compiled_arg_value);
      else
        sprintf(immediate_value, "0x%x", (short)compiled_arg_value);

      write_arg_word(immediate_value);
    }
  }
  else
  {
    int arg_length = compiled_arg_type - 1;
    src++;

    // An expression is an expression if it's allowed to be one.
    if((src[0] == '(') && (arg_type & ARG_TYPE_COUNTER_LOAD_NAME))
    {
      int expression_length = arg_length;
      char *base_output = output;
      char *next =
       legacy_disassemble_print_expression(src, &output, &expression_length, false);

      // For now, letting this fall through if we get something like:
      // (expr)trailing
      if(expression_length == 0)
      {
        // What happens here is that if the expression (just a normal one,
        // not interpolated in a name) is just an expression, then it passes
        // through to be one of these other things, as a name with its pieces
        // escaped.
        if(next != src)
        {
          *_output = output;
          return next + 1;
        }
      }
      output = base_output;
    }

    if(arg_type & (ARG_TYPE_COUNTER_LOAD_NAME | ARG_TYPE_COUNTER_STORE_NAME |
     ARG_TYPE_LABEL_NAME | ARG_TYPE_EXT_LABEL_NAME | ARG_TYPE_BOARD_NAME |
     ARG_TYPE_ROBOT_NAME))
    {
      if(is_simple_identifier_name(src, arg_length, false))
      {
        src = legacy_disassemble_print_string_expressions(src, &output,
         &arg_length, -1, -1, false);
      }
      else
      {
        *output = '`';
        output++;
        src = legacy_disassemble_print_string_expressions(src, &output,
         &arg_length, -1, '`', false);
        *output = '`';
        output++;
      }

      *_output = output;
      return src + 1;
    }

    if(arg_type & ARG_TYPE_STRING)
    {
      *output = '"';
      output++;
      src = legacy_disassemble_print_string_expressions(src, &output,
       &arg_length, -1, '"', false);
      *output = '"';

      *_output = output + 1;
      return src + 1;
    }
  }

  return src;
}

__editor_maybe_static
int legacy_disassemble_command(char *command_base, char *output_base,
 int *line_length, int bytecode_length, boolean print_ignores, int base)
{
  int command_length = *command_base;
  char *command_src = command_base;
  char *output = output_base;
  const struct mzx_command *command;
  enum arg_type arg_type;

  if(command_length > bytecode_length)
    return 0;

  if(command_length)
  {
    int legacy_command_number = command_src[1];
    int current_command_number;
    int parameters;
    int i;

    // Get the command number we're using now, if any.
    current_command_number = legacy_command_to_current[legacy_command_number];

    switch(current_command_number)
    {
      case ROBOTIC_CMD_REMOVED:
        switch(legacy_command_number)
        {
          case ROBOTIC_CMD_OBSOLETE_IF_PLAYER_DIR:
            current_command_number = ROBOTIC_CMD_IF_CONDITION;
            command_src[4] = command_src[3];
            command_src[3] = TOUCHING;
            break;

          case ROBOTIC_CMD_OBSOLETE_IF_NOT_PLAYER_DIR:
            current_command_number = ROBOTIC_CMD_IF_NOT_CONDITION;
            command_src[4] = command_src[3];
            command_src[3] = TOUCHING;
            break;
        }
        break;

      case ROBOTIC_CMD_BLANK_LINE:
        *line_length = 0;
        return 1;

      case ROBOTIC_CMD_COMMENT:
      {
        int comment_length = command_src[2];

        if(command_src[3] != '@')
        {
          output[0] = '/';
          output[1] = '/';
          output[2] = ' ';
          output += 3;

          command_src += 3;
          comment_length--;
          while(comment_length)
          {
            *output = *command_src;
            output++;
            command_src++;
            comment_length--;
          }
          *line_length = output - output_base;
          return command_length;
        }
      }
    }

    // If it's no longer supported we might have to do some magic.
    command = &(command_list[current_command_number]);

    strcpy(output, command->name);
    output += strlen(command->name);
    *output = 0;
    *line_length = output - output_base;

    command_src += 2;
    parameters = command->parameters;

    *output = ' ';
    output++;

    for(i = 0; i < parameters; i++)
    {
      arg_type = command->param_types[i];
      command_src = legacy_disassemble_arg(arg_type, command_src, &output,
       print_ignores, base);

      if(arg_type & ARG_TYPE_IGNORE)
      {
        parameters++;
        if(!print_ignores)
          break;
      }

      if(i < (parameters - 1))
      {
        *output = ' ';
        output++;
      }
    }

    *line_length = output - output_base;
  }

  return command_length;
}

char *legacy_disassemble_program(char *program_bytecode, int bytecode_length,
 int *_disasm_length, boolean print_ignores, int base)
{
  int disasm_length = 0;
  int disasm_offset = 0;
  int disasm_length_allocated = 256;
  char *program_disasm = malloc(disasm_length_allocated);

  // Needs enough room for any expansions - right now that's just ^ to **.
  char command_buffer[512];

  int command_length;
  int disasm_line_length;

  program_bytecode++;

  do
  {
    // Disassemble command into command_buffer.
    command_length = legacy_disassemble_command(program_bytecode,
     command_buffer, &disasm_line_length, bytecode_length,
     print_ignores, base);

    // See if it's not an ending command.
    if(command_length)
    {
      // Increment the total size (plus size for new line)
      disasm_length += disasm_line_length + 1;

      // Reallocate buffer if necessary.
      while(disasm_length > disasm_length_allocated)
      {
        disasm_length_allocated *= 2;
        program_disasm =  realloc(program_disasm, disasm_length_allocated);
      }

      // Copy new line into buffer.
      memcpy(program_disasm + disasm_offset, command_buffer,
       disasm_line_length);
      command_buffer[disasm_line_length] = 0;

      // Write newline char.
      disasm_offset += disasm_line_length;
      program_disasm[disasm_offset] = '\n';
      disasm_offset++;

      // Offset bytecode (source) position.
      program_bytecode += command_length + 2;
      bytecode_length -= command_length + 2;
    }
  } while(command_length);

  // Null terminate and return total size.
  disasm_length++;
  program_disasm = realloc(program_disasm, disasm_length);
  program_disasm[disasm_offset] = 0;
  *_disasm_length = disasm_length;

  return program_disasm;
}

char *legacy_convert_file(char *file_name, int *_disasm_length,
 boolean print_ignores, int base)
{
  vfile *legacy_source_file = fsafeopen(file_name, "rt");

  if(legacy_source_file)
  {
    int disasm_length = 0;
    int disasm_offset = 0;
    int disasm_length_allocated = 256;
    char *program_disasm = malloc(disasm_length_allocated);

    char source_buffer[256];
    char command_buffer[512];
    char bytecode_buffer[256];
    char errors[256];

    int disasm_line_length;

    while(vfsafegets(source_buffer, 256, legacy_source_file))
    {
      // Assemble line
      legacy_assemble_line(source_buffer, bytecode_buffer, errors,
       NULL, NULL);

      // Disassemble command into command_buffer.
      legacy_disassemble_command(bytecode_buffer, command_buffer,
       &disasm_line_length, 256, print_ignores, base);
      command_buffer[disasm_line_length] = 0;

      // Increment the total size (plus size for new line)
      disasm_length += disasm_line_length + 1;

      // Reallocate buffer if necessary.
      while(disasm_length > disasm_length_allocated)
      {
        disasm_length_allocated *= 2;
        program_disasm =  realloc(program_disasm, disasm_length_allocated);
      }

      // Copy new line into buffer.
      memcpy(program_disasm + disasm_offset, command_buffer,
       disasm_line_length);

      // Write newline char.
      disasm_offset += disasm_line_length;
      program_disasm[disasm_offset] = '\n';
      disasm_offset++;
    }

    // Null terminate and return total size.
    disasm_length++;
    program_disasm = realloc(program_disasm, disasm_length);
    program_disasm[disasm_offset] = 0;
    *_disasm_length = disasm_length;

    vfclose(legacy_source_file);
    return program_disasm;
  }

  return NULL;
}

char *legacy_convert_program(char *src, int len, int *_disasm_length,
 boolean print_ignores, int base)
{
  if(len)
  {
    struct memfile mf;

    int disasm_length = 0;
    int disasm_offset = 0;
    int disasm_length_allocated = 256;
    char *program_disasm = malloc(disasm_length_allocated);

    char source_buffer[256];
    char command_buffer[512];
    char bytecode_buffer[256];
    char errors[256];

    int disasm_line_length;

    mfopen(src, len, &mf);

    // Copying to a buffer isn't the quickest solution, but trying to
    // handle it differently turns into spaghetti.
    while(mfsafegets(source_buffer, 256, &mf))
    {
      // Assemble line
      legacy_assemble_line(source_buffer, bytecode_buffer, errors,
       NULL, NULL);

      // Disassemble command into command_buffer.
      legacy_disassemble_command(bytecode_buffer, command_buffer,
       &disasm_line_length, 256, print_ignores, base);
      command_buffer[disasm_line_length] = 0;

      // Increment the total size (plus size for new line)
      disasm_length += disasm_line_length + 1;

      // Reallocate buffer if necessary.
      while(disasm_length > disasm_length_allocated)
      {
        disasm_length_allocated *= 2;
        program_disasm =  realloc(program_disasm, disasm_length_allocated);
      }

      // Copy new line into buffer.
      memcpy(program_disasm + disasm_offset, command_buffer,
       disasm_line_length);

      // Write newline char.
      disasm_offset += disasm_line_length;
      program_disasm[disasm_offset] = '\n';
      disasm_offset++;
    }

    // Null terminate and return total size.
    disasm_length++;
    program_disasm = realloc(program_disasm, disasm_length);
    program_disasm[disasm_offset] = 0;
    *_disasm_length = disasm_length;

    return program_disasm;
  }

  return NULL;
}

#endif /* CONFIG_DEBYTECODE */
