#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.
#
# periodic - a library for repeating periodic events
# Copyright (C) 2008, David Shaw <dshaw@jabberwocky.com>
#
# This library is free software; you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as
# published by the Free Software Foundation; either version 2.1 of the
# License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
# 02110-1301, USA

AC_PREREQ(2.61)
AC_REVISION($Id$)
AC_COPYRIGHT([Copyright (C) 2008, David Shaw <dshaw@jabberwocky.com>])
AC_INIT([periodic],[0.5],[dshaw@jabberwocky.com])
AC_CONFIG_SRCDIR([lib/periodic.c])
AC_CONFIG_HEADER([config.h])
AM_INIT_AUTOMAKE([std-options])

# Checks for programs.
AC_PROG_CC
AC_PROG_LIBTOOL

# Checks for libraries.
AC_MSG_CHECKING([for pthreads])
_save_libs=$LIBS
LIBS="$_save_libs -lpthread"
AC_LINK_IFELSE([AC_LANG_PROGRAM([#include <pthread.h>],[pthread_cleanup_push(0,0); pthread_cleanup_pop(0);])],found=yes,found=no)
AC_MSG_RESULT($found)
if test $found = no ; then
   AC_MSG_ERROR([periodic requires pthreads])
fi
LIBS=$_save_libs

# Checks for header files.

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST

# Checks for library functions.

AC_ARG_ENABLE(optimization,
   AS_HELP_STRING([--disable-optimization],[disable compiler optimization]),
   [if test $enableval = no ; then
      CFLAGS=`echo $CFLAGS | sed 's/-O[[0-9]]//'`
   fi])

if test "$GCC" = yes; then
   CFLAGS="-Wall -Wno-variadic-macros -pedantic $CFLAGS"
fi

AC_CONFIG_FILES([Makefile lib/Makefile examples/Makefile periodic.pc periodic.spec])
AC_OUTPUT
