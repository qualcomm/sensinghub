# Makefile.am - Automake script for libQshQmiIDL

AM_CPPFLAGS = -Werror                   \
              -I./                      \
              -Wall                     \
              -Wno-unused-parameter     \
              -Wno-unused-variable      \
              -fexceptions

cpp_sources = ./sns_client_api_v01.c

requiredlibs = @GLIB_LIBS@

if USE_QMI_PATH
AM_CPPFLAGS += -I@QMI_PATH@
qmi_libs = -lqmi_common -lqencdec -lqcci -lqcsi
requiredlibs += $(qmi_libs)
else
AM_CPPFLAGS += @QMIFRAMEWORK_CFLAGS@
requiredlibs += @QMIFRAMEWORK_LIBS@
endif



lib_LTLIBRARIES = libQshQmiIDL.la
libQshQmiIDL_la_CC = @CC@
libQshQmiIDL_la_SOURCES = $(cpp_sources)
libQshQmiIDL_la_CPPFLAGS = $(AM_CPPFLAGS)
libQshQmiIDL_la_LDFLAGS = -shared  -version-number @LT_VERSION_NUMBER@
if USE_GLIB
libQshQmiIDL_la_CPPFLAGS += -DUSE_GLIB @GLIB_CFLAGS@
libQshQmiIDL_la_LDFLAGS += @GLIB_LIBS@
endif
libQshQmiIDL_la_LIBADD = $(requiredlibs)