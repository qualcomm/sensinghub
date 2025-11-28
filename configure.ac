#                                               -*- Autoconf -*-
# configure.ac -- Autoconf script for sensors
#
# Process this file with autoconf to produce a configure script
# Requires autoconf tool later than 2.61
AC_PREREQ(2.61)

# Initialize the sensors package version 1.0.0
AC_INIT([sensing-hub], 1.0.0)

# Does not strictly follow GNU Coding standards
AM_INIT_AUTOMAKE([foreign subdir-objects dist-bzip2 tar-pax])

# Disables auto rebuilding of configure, Makefile.ins
AM_MAINTAINER_MODE

# defines some macros variable to be included by source
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])

# Checks for programs.
AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_CXX
AC_PROG_LIBTOOL
AC_PROG_AWK
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
PKG_PROG_PKG_CONFIG

# Checks for libraries

PKG_CHECK_MODULES([PROTOBUF], [protobuf])
AC_SUBST([PROTOBUF_CFLAGS])
AC_SUBST([PROTOBUF_LIBS])
AC_SUBST(PROTOBUF_VERSION)

AC_SUBST([PROTOBUF_CFLAGS])
AC_SUBST([PROTOBUF_LIBS])
AC_SUBST(PROTOBUF_VERSION)

AC_CHECK_PROG([PROTOC], [protoc], [protoc])
AS_IF([test "x${PROTOC}" == "x"],
      [AC_MSG_ERROR([ProtoBuf compiler "protoc" not found.])])
AC_PATH_PROG([PROTOCPATH], [protoc])
AC_SUBST(PROTOC)

AC_ARG_WITH(sanitized-headers,
        AS_HELP_STRING([--with-sanitized-headers=DIR],
                [Specify the location of the sanitized Linux headers]),
        [CPPFLAGS="$CPPFLAGS -idirafter $withval"])

AC_ARG_WITH([common_includes],
      AS_HELP_STRING([--with-common-includes=@<:@dir@:>@],
         [Specify the location of the common headers]),
            [common_incdir=$withval],
               with_common_includes=no)

if test "x$with_common_includes" != "xno"; then
   CFLAGS="${CFLAGS} -I${common_incdir}"
fi

AC_ARG_WITH(systemd,
        AS_HELP_STRING([--with-systemd], [include services in systemd]),
        [SYSTEMD="yes"],
        [SYSTEMD="no"])
AM_CONDITIONAL([USE_SYSTEMD], [test "x${SYSTEMD}" != "xno"])

AC_ARG_ENABLE(versioned-lib,
        AS_HELP_STRING([--enable-versioned-lib], [enable versioned libs for debian packages]),
        [VERSIONED_LIB="yes"],
        [VERSIONED_LIB="no"])
AM_CONDITIONAL([ENABLE_VERSIONED_LIB], [test "x${VERSIONED_LIB}" != "xno"])

m4_define([LT_MAJOR], [1])
m4_define([LT_MINOR], [0])
m4_define([LT_PATCH], [0])

AC_SUBST([LT_VERSION], LT_MAJOR.LT_MINOR.LT_PATCH)
AC_SUBST([LT_VERSION_NUMBER], LT_MAJOR:LT_MINOR:LT_PATCH)

AC_CONFIG_FILES([             \
        Makefile              \
        apis/proto/Makefile   \
        session/1.0/Makefile  \
        examples/SessionClient/Makefile \
        sensing-hub.pc
               ])
AC_OUTPUT

