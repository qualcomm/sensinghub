# Makefile.am - Automake script for SessionClient

AM_CPPFLAGS = -Werror                           \
              -Wall                             \
              -fexceptions                      \
              -I./../../common/inc/             \
              -I./../../session/1.0/inc/        \
              -I./../../apis/proto/

cpp_sources = SessionClient.cpp

requiredlibs = ../../session/1.0/libsensinghubsession.la \
               ../../apis/proto/libsensinghubapi.la \
               @PROTOBUF_LIBS@

bin_PROGRAMS = SessionClient
SessionClient_SOURCES = $(cpp_sources)
SessionClient_CC = @CC@
SessionClient_CPPFLAGS = $(AM_CPPFLAGS)
SessionClient_LDADD = $(requiredlibs)
