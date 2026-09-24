# Makefile.am - Automake script for libQshSession

# --------------------------------------------------------------------
# Compiler flags - single consolidated block, then conditionally
# extended. Use += for the second block to avoid overwrite.
# --------------------------------------------------------------------

AM_CPPFLAGS = \
    -Werror                                 \
    -Wno-return-type                        \
    -Wno-unused-result                      \
    -fpermissive                            \
    -include config.h                       \
    -I$(srcdir)                             \
    -I$(srcdir)/inc                         \
    -I$(top_srcdir)/session/1.0/inc         \
    -I$(top_srcdir)/session/1.0             \
    -I$(top_srcdir)/utils/inc               \
    -I$(top_srcdir)/utils                   \
    -I$(top_srcdir)/common/inc              \
    -I$(top_srcdir)/sessionlogger/inc       \
    -I$(top_srcdir)/apis/proto/nanopb_gen   \
    -I$(top_srcdir)/apis/proto              \
    -I$(top_srcdir)/core/qshQmiIDL          \
    $(GLIB_CFLAGS)

# --------------------------------------------------------------------
# QMI path selection
# --------------------------------------------------------------------
if USE_QMI_PATH
AM_CPPFLAGS += -I@QMI_PATH@
qmi_libs = -lqmi_common -lqencdec -lqcci -lqcsi
else
AM_CPPFLAGS += $(QMIFRAMEWORK_CFLAGS)
endif

# --------------------------------------------------------------------
# Source files
# --------------------------------------------------------------------
cpp_sources = \
    ./src/qmiSession.cpp        \
    ./src/glinkSession.cpp      \
    ./src/sessionImpl.cpp       \
    ./src/sessionLoggerUtil.cpp

# --------------------------------------------------------------------
# Required libraries
# --------------------------------------------------------------------
requiredlibs = \
    $(top_builddir)/core/qshQmiIDL/libQshQmiIDL.la \
    $(top_builddir)/utils/libqshUtil.la                      \
    $(top_builddir)/sessionlogger/libsensinghublogger.la     \
    $(top_builddir)/apis/proto/libsensinghubapi-c.la         \
    -ldl                        \
    $(SENSINGHUB_LIBS)          \
    $(GLIB_LIBS)                \
    $(NANOPB_LIBS)

if USE_QMI_PATH
requiredlibs += $(qmi_libs)
else
requiredlibs += $(QMIFRAMEWORK_LIBS)
endif

# --------------------------------------------------------------------
# Library target
# --------------------------------------------------------------------
library_includedir = $(pkgincludedir)

lib_LTLIBRARIES = libQshSession.la

libQshSession_la_SOURCES  = $(cpp_sources)
libQshSession_la_CPPFLAGS = $(AM_CPPFLAGS)
libQshSession_la_LDFLAGS  = -shared @LDFLAGS@ -version-number @LT_VERSION_NUMBER@
libQshSession_la_LIBADD   = $(requiredlibs)

if USE_GLIB
libQshSession_la_CPPFLAGS += -DUSE_GLIB
libQshSession_la_LDFLAGS  += $(GLIB_LIBS)
endif