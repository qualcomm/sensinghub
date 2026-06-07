AM_CPPFLAGS = \
              -Wall                     \
              -Wno-unused-parameter     \
              -Wno-unused-variable      \
              -Wno-unused-result        \
              -Wno-sign-compare        \
              -fexceptions              \
              -I$(srcdir)/inc          \
              -I$(top_srcdir)/apis/proto/nanopb_gen \
              -I$(top_srcdir)/session/1.0/inc \
               -I$(top_srcdir)/utils/inc \
              -I$(top_srcdir)/common/inc  \
              -I./

libsensinghublogger_la_SOURCES =  ./src/LoggerFactory.cpp    \
                                                         ./src/SensorsDiagLogger.cpp
    
include_HEADERS = $(srcdir)/inc/*.h

requiredlibs = $(top_builddir)/apis/proto/libsensinghubapi-c.la    \
                       $(top_builddir)/session/1.0/libsensinghubsession.la

lib_LTLIBRARIES = libsensinghublogger.la
libsensinghublogger_la_CC = @CC@
libsensinghublogger_la_CPPFLAGS = $(AM_CPPFLAGS)
libsensinghublogger_la_LDFLAGS = -shared @LDFLAGS@ -version-number @LT_VERSION_NUMBER@
if USE_GLIB
 libsensinghublogger_la_CPPFLAGS += -DUSE_GLIB @GLIB_CFLAGS@
 libsensinghublogger_la_LDFLAGS += @GLIB_LIBS@
endif
libsensinghublogger_la_LIBADD = $(requiredlibs)