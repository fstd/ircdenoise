#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([ircdenoise], [0.0.0], [van.fisted@googlemail.com])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_SRCDIR([src/ircdenoise.c])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_AUX_DIR([build-aux])
AM_INIT_AUTOMAKE([-Wall -Werror foreign])

# Checks for programs.
AC_PROG_CC_C99
AM_PROG_CC_C_O
AC_PROG_INSTALL
AC_PROG_MAKE_SET


AC_CHECK_LIB([srsbsns], [ringbuf_init], [], [AC_MSG_ERROR([libsrsbsns not installed?])])
AC_CHECK_LIB([srsirc], [irc_init], [], [AC_MSG_ERROR([libsrsirc not installed?])])


AC_CHECK_HEADERS([stdlib.h limits.h stddef.h sys/time.h])
AC_CHECK_HEADER_STDBOOL


AC_TYPE_SIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT64_T


AC_CHECK_FUNCS([atexit gettimeofday select strchr strdup strerror strstr strtoull])
AC_FUNC_MALLOC



if test "$(uname)" = "Linux"; then
AC_DEFINE([DUMPSIG], [SIGUSR1], [SIGUSR1 on Linux, SIGINFO on the BSDs])
:
else
AC_DEFINE([DUMPSIG], [SIGINFO], [SIGUSR1 on Linux, SIGINFO on the BSDs])
:
fi


AC_CONFIG_FILES([Makefile
                 src/Makefile])
AC_OUTPUT

echo \
"-------------------------------------------------

 ${PACKAGE_NAME} Version ${PACKAGE_VERSION}

 Prefix: '${prefix}'.
 Compiler: '${CC} ${CFLAGS}'
 Libraries: '${LIBS}'

--------------------------------------------------"
