# Makefile.am - Automake script for sensors.qti

AM_CPPFLAGS = -Werror                   \
              -Wall                     \
              -Wno-unused-parameter     \
              -Wno-unused-result        \
              -fexceptions              \
              -I./

AM_CPPFLAGS += -DSNS_EXCLUDE_FASTRPC_INTERNAL_HEADERS

if ENABLE_VERSIONED_LIB
  AM_CPPFLAGS += -DSNS_VERSIONED_LIB_ENABLED
endif

AM_CPPFLAGS += @FASTRPC_CFLAGS@

cpp_sources = ./src/sensorsdaemon.cpp

bin_PROGRAMS = sensors.qti
sensors_qti_SOURCES = $(cpp_sources)
sensors_qti_CC = @CC@
sensors_qti_CPPFLAGS = $(AM_CFLAGS) $(AM_CPPFLAGS)
sensors_qti_LDFLAGS = -ldl
if USE_GLIB
 sensors_qti_CPPFLAGS += -DUSE_GLIB @GLIB_CFLAGS@
 sensors_qti_LDFLAGS += @GLIB_LIBS@
endif

if USE_SYSTEMD
systemdunitdir = @SYSTEMD_UNITDIR@
systemdunit_DATA = sensors.qti.service
endif
