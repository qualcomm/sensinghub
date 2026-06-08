# Makefile.am - Automake script for SessionClient

AM_CPPFLAGS = -Wall                             \
              -fexceptions                      \
              -I$(top_srcdir)/common/inc/             \
              -I$(top_srcdir)/session/1.0/inc/        \
              -I$(top_builddir)/apis/proto/nanopb_gen \
              -I$(top_srcdir)/apis/proto/nanopb_gen

cpp_sources = SessionClient.cpp

requiredlibs = ../../session/1.0/libsensinghubsession.la \
               ../../apis/proto/libsensinghubapi-c.la \
               ../../utils/libqshUtil.la


bin_PROGRAMS = SessionClient
SessionClient_SOURCES = $(cpp_sources)
SessionClient_CC = @CC@
SessionClient_CPPFLAGS = $(AM_CPPFLAGS)
SessionClient_LDADD = $(requiredlibs)
