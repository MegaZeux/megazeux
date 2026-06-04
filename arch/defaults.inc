#
# Default toolchain variables.
# Also applies CROSS_COMPILE.
#

CC      ?= gcc
CXX     ?= g++
AR      ?= ar
AS      ?= as
STRIP   ?= strip
OBJCOPY ?= objcopy
WINDRES ?= windres
PEFIX   ?= true

CHMOD   ?= chmod
CP      ?= cp
HOST_CC ?= gcc
LN      ?= ln
MKDIR   ?= mkdir
MV      ?= mv
RM      ?= rm

ifneq (${CROSS_COMPILE},)
ifeq (${CC},cc)
CC      = gcc
endif
CC      := ${CROSS_COMPILE}${CC}
CXX     := ${CROSS_COMPILE}${CXX}
AR      := ${CROSS_COMPILE}${AR}
AS      := ${CROSS_COMPILE}${AS}
STRIP   := ${CROSS_COMPILE}${STRIP}
OBJCOPY := ${CROSS_COMPILE}${OBJCOPY}
WINDRES := ${CROSS_COMPILE}${WINDRES}
endif
