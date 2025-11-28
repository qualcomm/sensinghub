# Makefile.am - Automake script for libSensingHubSession

AM_CPPFLAGS = -I$(srcdir)/inc \
              -I$(top_srcdir)/common/inc/

if ENABLE_VERSIONED_LIB
  AM_CPPFLAGS += -DSNS_VERSIONED_LIB_ENABLED
endif

cpp_sources = src/SessionFactory.cpp

requiredlibs = -ldl

include_HEADERS = $(srcdir)/inc/ISession.h        \
                  $(srcdir)/inc/SessionFactory.h     \
                  $(top_srcdir)/common/inc/suid.h

library_includedir = $(pkgincludedir)

lib_LTLIBRARIES = libsensinghubsession.la
libsensinghubsession_la_CC = @CC@
libsensinghubsession_la_SOURCES = $(cpp_sources)
libsensinghubsession_la_CPPFLAGS = $(AM_CPPFLAGS)
libsensinghubsession_la_LDFLAGS = -shared @LDFLAGS@ -version-number @LT_VERSION_NUMBER@
libsensinghubsession_la_LIBADD = $(requiredlibs)