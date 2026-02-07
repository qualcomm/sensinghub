AM_CPPFLAGS = -Werror                   \
              -Wall                     \
              -Wno-unused-parameter     \
              -Wno-unused-variable     \
              -Wno-unused-result        \
              -Wno-sign-compare        \
              -fexceptions              \
              -I$(srcdir)/inc   \
              -I$(top_srcdir)/apis/proto  \
              -I$(top_srcdir)/session/1.0/inc \
              -I$(top_srcdir)/common/inc  \
              -DUSE_SYS_LOG               \
              -I./

cpp_sources_qshutils = ./src/qshLog.cpp \
                       ./src/qshWakelock.cpp \
                       ./src/suidLookUp.cpp  \
                       ./src/qshSSR.cpp      \
                       ./src/qshJsonParser.cpp

include_HEADERS = $(srcdir)/inc/qshJsonParser.h \
                  $(srcdir)/inc/qshLog.h        \
                  $(srcdir)/inc/qshSSR.h        \
                  $(srcdir)/inc/qshTarget.h     \
                  $(srcdir)/inc/qshTimeUtil.h   \
                  $(srcdir)/inc/qshTrace.h      \
                  $(srcdir)/inc/qshWakelock.h   \
                  $(srcdir)/inc/qshWorker.h     \
                  $(srcdir)/inc/suidLookUp.h

requiredlibs = $(top_builddir)/apis/proto/libsensinghubapi.la  \
               $(top_builddir)/session/1.0/libsensinghubsession.la

lib_LTLIBRARIES = libqshUtil.la
libqshUtil_la_CC = @CC@
libqshUtil_la_SOURCES = $(cpp_sources_qshutils)
libqshUtil_la_CPPFLAGS = $(AM_CPPFLAGS)
libqshUtil_la_LDFLAGS = -shared @LDFLAGS@ -version-number @LT_VERSION_NUMBER@
if USE_GLIB
 libqshUtil_la_CPPFLAGS += -DUSE_GLIB @GLIB_CFLAGS@
 libqshUtil_la_LDFLAGS += @GLIB_LIBS@
endif
libqshUtil_la_LIBADD = $(requiredlibs)