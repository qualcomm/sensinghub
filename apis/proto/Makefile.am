# Automake script for protobuf/nanopb generation and libraries.

PROTOC ?= protoc
NANOPB_PLUGIN ?= @NANOPB_PLUGIN@

PROTOGEN_DIR = $(builddir)/proto_gen


$(PROTOGEN_DIR)/%.pb.cc $(PROTOGEN_DIR)/%.pb.h: %.proto
	@mkdir -p $(PROTOGEN_DIR)
	$(PROTOC) --proto_path=$(srcdir) --cpp_out=$(PROTOGEN_DIR) $<

nanopb.pb.cc nanopb.pb.h: nanopb.proto google/protobuf/descriptor.proto
	$(PROTOC) --proto_path=$(srcdir) --cpp_out=$(srcdir) nanopb.proto google/protobuf/descriptor.proto

# Generate nanopb C sources/headers.
# Keep nanopb outputs in the source tree so downstream projects can include
# them directly, matching the Android-style generated header layout.
NANOPB_GEN_DIR = $(srcdir)/nanopb_gen

# Nanopb outputs for the sns_*.proto files used across sensors modules.
$(NANOPB_GEN_DIR)/sns_%.pb.c $(NANOPB_GEN_DIR)/sns_%.pb.h: sns_%.proto
	@mkdir -p $(dir $(NANOPB_GEN_DIR)/$*)
	$(PROTOC) --proto_path=$(srcdir) \
		--plugin=protoc-gen-nanopb=$(NANOPB_PLUGIN) \
		--nanopb_out=$(NANOPB_GEN_DIR) $^

AM_CPPFLAGS = -I./  \
              -I$(PROTOGEN_DIR)   \
              -I$(NANOPB_GEN_DIR)

proto_list = $(shell find $(srcdir)/ -name "*.proto")
proto_filesdir = /etc/sensors/proto
proto_files_DATA = $(proto_list)

nodist_proto_SOURCES =  $(PROTOGEN_DIR)/google/protobuf/descriptor.pb.cc     \
                       $(PROTOGEN_DIR)/nanopb.pb.cc                         \
                       $(PROTOGEN_DIR)/sns_std_sensor.pb.cc                 \
                       $(PROTOGEN_DIR)/sns_physical_sensor_test.pb.cc       \
                       $(PROTOGEN_DIR)/sns_std_event_gated_sensor.pb.cc     \
                       $(PROTOGEN_DIR)/sns_cal.pb.cc                        \
                       $(PROTOGEN_DIR)/sns_std_type.pb.cc                   \
                       $(PROTOGEN_DIR)/sns_std.pb.cc                        \
                       $(PROTOGEN_DIR)/sns_accel.pb.cc                      \
                       $(PROTOGEN_DIR)/sns_ambient_light.pb.cc              \
                       $(PROTOGEN_DIR)/sns_ambient_temperature.pb.cc        \
                       $(PROTOGEN_DIR)/sns_amd.pb.cc                        \
                       $(PROTOGEN_DIR)/sns_client.pb.cc                     \
                       $(PROTOGEN_DIR)/sns_device_mode.pb.cc                \
                       $(PROTOGEN_DIR)/sns_device_orient.pb.cc              \
                       $(PROTOGEN_DIR)/sns_direct_channel.pb.cc             \
                       $(PROTOGEN_DIR)/sns_game_rv.pb.cc                    \
                       $(PROTOGEN_DIR)/sns_geomag_rv.pb.cc                  \
                       $(PROTOGEN_DIR)/sns_gyro_cal.pb.cc                   \
                       $(PROTOGEN_DIR)/sns_gyro.pb.cc                       \
                       $(PROTOGEN_DIR)/sns_hall.pb.cc                       \
                       $(PROTOGEN_DIR)/sns_humidity.pb.cc                   \
                       $(PROTOGEN_DIR)/sns_mag_cal.pb.cc                    \
                       $(PROTOGEN_DIR)/sns_mag.pb.cc                        \
                       $(PROTOGEN_DIR)/sns_pedometer.pb.cc                  \
                       $(PROTOGEN_DIR)/sns_pressure.pb.cc                   \
                       $(PROTOGEN_DIR)/sns_proximity.pb.cc                  \
                       $(PROTOGEN_DIR)/sns_rotv.pb.cc                       \
                       $(PROTOGEN_DIR)/sns_sar.pb.cc                        \
                       $(PROTOGEN_DIR)/sns_sensor_temperature.pb.cc         \
                       $(PROTOGEN_DIR)/sns_sig_motion.pb.cc                 \
                       $(PROTOGEN_DIR)/sns_suid.pb.cc                       \
                       $(PROTOGEN_DIR)/sns_tilt.pb.cc                       \
                       $(PROTOGEN_DIR)/sns_transport_ppe.pb.cc              \
                       $(PROTOGEN_DIR)/sns_persist_motion_detect.pb.cc      \
                       $(PROTOGEN_DIR)/sns_persist_stationary_detect.pb.cc  \
                       $(PROTOGEN_DIR)/sns_hinge_angle.pb.cc                \
                       $(PROTOGEN_DIR)/sns_gravity.pb.cc

BUILT_SOURCES = $(PROTOGEN_DIR)/google/protobuf/descriptor.pb.h     \
                $(PROTOGEN_DIR)/nanopb.pb.h                         \
                $(PROTOGEN_DIR)/sns_std_sensor.pb.h                 \
                $(PROTOGEN_DIR)/sns_physical_sensor_test.pb.h       \
                $(PROTOGEN_DIR)/sns_std_event_gated_sensor.pb.h     \
                $(PROTOGEN_DIR)/sns_cal.pb.h                        \
                $(PROTOGEN_DIR)/sns_std_type.pb.h                   \
                $(PROTOGEN_DIR)/sns_std.pb.h                        \
                $(PROTOGEN_DIR)/sns_accel.pb.h                      \
                $(PROTOGEN_DIR)/sns_ambient_light.pb.h              \
                $(PROTOGEN_DIR)/sns_ambient_temperature.pb.h        \
                $(PROTOGEN_DIR)/sns_amd.pb.h                        \
                $(PROTOGEN_DIR)/sns_client.pb.h                     \
                $(PROTOGEN_DIR)/sns_device_mode.pb.h                \
                $(PROTOGEN_DIR)/sns_device_orient.pb.h              \
                $(PROTOGEN_DIR)/sns_direct_channel.pb.h             \
                $(PROTOGEN_DIR)/sns_game_rv.pb.h                    \
                $(PROTOGEN_DIR)/sns_geomag_rv.pb.h                  \
                $(PROTOGEN_DIR)/sns_gyro_cal.pb.h                   \
                $(PROTOGEN_DIR)/sns_gyro.pb.h                       \
                $(PROTOGEN_DIR)/sns_hall.pb.h                       \
                $(PROTOGEN_DIR)/sns_humidity.pb.h                   \
                $(PROTOGEN_DIR)/sns_mag_cal.pb.h                    \
                $(PROTOGEN_DIR)/sns_mag.pb.h                        \
                $(PROTOGEN_DIR)/sns_pedometer.pb.h                  \
                $(PROTOGEN_DIR)/sns_pressure.pb.h                   \
                $(PROTOGEN_DIR)/sns_proximity.pb.h                  \
                $(PROTOGEN_DIR)/sns_rotv.pb.h                       \
                $(PROTOGEN_DIR)/sns_sar.pb.h                        \
                $(PROTOGEN_DIR)/sns_sensor_temperature.pb.h         \
                $(PROTOGEN_DIR)/sns_sig_motion.pb.h                 \
                $(PROTOGEN_DIR)/sns_suid.pb.h                       \
                $(PROTOGEN_DIR)/sns_tilt.pb.h                       \
                $(PROTOGEN_DIR)/sns_transport_ppe.pb.h              \
                $(PROTOGEN_DIR)/sns_persist_motion_detect.pb.h      \
                $(PROTOGEN_DIR)/sns_persist_stationary_detect.pb.h  \
                $(PROTOGEN_DIR)/sns_hinge_angle.pb.h                \
                $(PROTOGEN_DIR)/sns_gravity.pb.h

sources_libsensinghub = $(nodist_proto_SOURCES)
include_HEADERS = $(proto_list)

protogendir = $(includedir)/proto_gen
protogen_HEADERS = $(filter %.pb.h,$(BUILT_SOURCES))

nanopb_includedir = $(includedir)/nanopb_gen
nanopb_include_HEADERS =  $(PROTOGEN_DIR)/nanopb.pb.h                         \
                         $(NANOPB_GEN_DIR)/sns_std_sensor.pb.h                 \
                         $(NANOPB_GEN_DIR)/sns_physical_sensor_test.pb.h       \
                         $(NANOPB_GEN_DIR)/sns_std_event_gated_sensor.pb.h     \
                         $(NANOPB_GEN_DIR)/sns_cal.pb.h                        \
                         $(NANOPB_GEN_DIR)/sns_std_type.pb.h                   \
                         $(NANOPB_GEN_DIR)/sns_std.pb.h                        \
                         $(NANOPB_GEN_DIR)/sns_accel.pb.h                      \
                         $(NANOPB_GEN_DIR)/sns_ambient_light.pb.h              \
                         $(NANOPB_GEN_DIR)/sns_ambient_temperature.pb.h        \
                         $(NANOPB_GEN_DIR)/sns_amd.pb.h                        \
                         $(NANOPB_GEN_DIR)/sns_client.pb.h                     \
                         $(NANOPB_GEN_DIR)/sns_device_mode.pb.h                \
                         $(NANOPB_GEN_DIR)/sns_device_orient.pb.h              \
                         $(NANOPB_GEN_DIR)/sns_direct_channel.pb.h             \
                         $(NANOPB_GEN_DIR)/sns_game_rv.pb.h                    \
                         $(NANOPB_GEN_DIR)/sns_geomag_rv.pb.h                  \
                         $(NANOPB_GEN_DIR)/sns_gyro_cal.pb.h                   \
                         $(NANOPB_GEN_DIR)/sns_gyro.pb.h                       \
                         $(NANOPB_GEN_DIR)/sns_hall.pb.h                       \
                         $(NANOPB_GEN_DIR)/sns_humidity.pb.h                   \
                         $(NANOPB_GEN_DIR)/sns_mag_cal.pb.h                    \
                         $(NANOPB_GEN_DIR)/sns_mag.pb.h                        \
                         $(NANOPB_GEN_DIR)/sns_pedometer.pb.h                  \
                         $(NANOPB_GEN_DIR)/sns_pressure.pb.h                   \
                         $(NANOPB_GEN_DIR)/sns_proximity.pb.h                  \
                         $(NANOPB_GEN_DIR)/sns_rotv.pb.h                       \
                         $(NANOPB_GEN_DIR)/sns_sar.pb.h                        \
                         $(NANOPB_GEN_DIR)/sns_sensor_temperature.pb.h         \
                         $(NANOPB_GEN_DIR)/sns_sig_motion.pb.h                 \
                         $(NANOPB_GEN_DIR)/sns_suid.pb.h                       \
                         $(NANOPB_GEN_DIR)/sns_tilt.pb.h                       \
                         $(NANOPB_GEN_DIR)/sns_transport_ppe.pb.h              \
                         $(NANOPB_GEN_DIR)/sns_persist_motion_detect.pb.h      \
                         $(NANOPB_GEN_DIR)/sns_persist_stationary_detect.pb.h  \
                         $(NANOPB_GEN_DIR)/sns_hinge_angle.pb.h                \
                         $(NANOPB_GEN_DIR)/sns_gravity.pb.h


CLEANFILES =

AUTOMAKE_OPTIONS = foreign

library_includedir = $(pkgincludedir)

lib_LTLIBRARIES = libsensinghubapi.la libsensinghubapi-c.la

requiredlibs_sensinghubapi = @PROTOBUF_LIBS@

libsensinghubapi_la_CC = @CC@
libsensinghubapi_la_SOURCES = $(sources_libsensinghub)
libsensinghubapi_la_CFLAGS = @PROTOBUF_CFLAGS@
libsensinghubapi_la_CXXFLAGS = $(AM_CXXFLAGS) -include $(PROTOGEN_DIR)/google/protobuf/descriptor.pb.h @PROTOBUF_CFLAGS@
libsensinghubapi_la_LDFLAGS = -shared @LDFLAGS@ -version-number @LT_VERSION_NUMBER@
libsensinghubapi_la_LIBADD = $(requiredlibs_sensinghubapi)

NANOPB_LIBS ?= @NANOPB_LIBS@
NANOPB_LIBS ?= -lprotobuf-nanopb

nodist_nanopb_SOURCES = \
                        $(NANOPB_GEN_DIR)/sns_std_sensor.pb.c                 \
                        $(NANOPB_GEN_DIR)/sns_physical_sensor_test.pb.c       \
                        $(NANOPB_GEN_DIR)/sns_std_event_gated_sensor.pb.c     \
                        $(NANOPB_GEN_DIR)/sns_cal.pb.c                        \
                        $(NANOPB_GEN_DIR)/sns_std_type.pb.c                   \
                        $(NANOPB_GEN_DIR)/sns_std.pb.c                        \
                        $(NANOPB_GEN_DIR)/sns_accel.pb.c                      \
                        $(NANOPB_GEN_DIR)/sns_ambient_light.pb.c              \
                        $(NANOPB_GEN_DIR)/sns_ambient_temperature.pb.c        \
                        $(NANOPB_GEN_DIR)/sns_amd.pb.c                        \
                        $(NANOPB_GEN_DIR)/sns_client.pb.c                     \
                        $(NANOPB_GEN_DIR)/sns_device_mode.pb.c                \
                        $(NANOPB_GEN_DIR)/sns_device_orient.pb.c              \
                        $(NANOPB_GEN_DIR)/sns_direct_channel.pb.c             \
                        $(NANOPB_GEN_DIR)/sns_game_rv.pb.c                    \
                        $(NANOPB_GEN_DIR)/sns_geomag_rv.pb.c                  \
                        $(NANOPB_GEN_DIR)/sns_gyro_cal.pb.c                   \
                        $(NANOPB_GEN_DIR)/sns_gyro.pb.c                       \
                        $(NANOPB_GEN_DIR)/sns_hall.pb.c                       \
                        $(NANOPB_GEN_DIR)/sns_humidity.pb.c                   \
                        $(NANOPB_GEN_DIR)/sns_mag_cal.pb.c                    \
                        $(NANOPB_GEN_DIR)/sns_mag.pb.c                        \
                        $(NANOPB_GEN_DIR)/sns_pedometer.pb.c                  \
                        $(NANOPB_GEN_DIR)/sns_pressure.pb.c                   \
                        $(NANOPB_GEN_DIR)/sns_proximity.pb.c                  \
                        $(NANOPB_GEN_DIR)/sns_rotv.pb.c                       \
                        $(NANOPB_GEN_DIR)/sns_sar.pb.c                        \
                        $(NANOPB_GEN_DIR)/sns_sensor_temperature.pb.c         \
                        $(NANOPB_GEN_DIR)/sns_sig_motion.pb.c                 \
                        $(NANOPB_GEN_DIR)/sns_suid.pb.c                       \
                        $(NANOPB_GEN_DIR)/sns_tilt.pb.c                       \
                        $(NANOPB_GEN_DIR)/sns_transport_ppe.pb.c              \
                        $(NANOPB_GEN_DIR)/sns_persist_motion_detect.pb.c      \
                        $(NANOPB_GEN_DIR)/sns_persist_stationary_detect.pb.c  \
                        $(NANOPB_GEN_DIR)/sns_hinge_angle.pb.c                \
                        $(NANOPB_GEN_DIR)/sns_gravity.pb.c

BUILT_SOURCES += $(nodist_nanopb_SOURCES)
CLEANFILES += $(nodist_nanopb_SOURCES)

libsensinghubapi_c_la_CC = @CC@
libsensinghubapi_c_la_SOURCES = $(nodist_nanopb_SOURCES)
libsensinghubapi_c_la_LDFLAGS = -shared @LDFLAGS@ -version-number @LT_VERSION_NUMBER@
libsensinghubapi_c_la_LIBADD = $(NANOPB_LIBS)

BUILT_SOURCES += $(nodist_proto_SOURCES)
CLEANFILES += $(nodist_proto_SOURCES)

