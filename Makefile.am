ACLOCAL_AMFLAGS = -I m4

dirs = apis/proto \
       session/1.0  \
       utils \
       sessionlogger  \
       examples/SessionClient

SUBDIRS = $(dirs)

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = sensinghub.pc sensinghub-cpp.pc

EXTRA_DIST = $(pkgconfig_DATA) README.md LICENSE.txt
