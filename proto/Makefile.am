# Makefile.am - Automake script for proto files to generate equivalent *.pb.cc, *.pb.h, snsproto.la & qsh.la

%.pb.cc %.pb.h: %.proto
	$(PROTOC) --proto_path=$(srcdir) --cpp_out=$(srcdir) $^

AM_CPPFLAGS = -I./

proto_list = $(shell find  $(srcdir)/ -name "*.proto")
proto_filesdir = /etc/sensors/proto
proto_files_DATA = $(proto_list)

nodist_proto_SOURCES = nanopb.pb.cc                         \
                       sns_std_sensor.pb.cc                 \
                       sns_physical_sensor_test.pb.cc       \
                       sns_std_event_gated_sensor.pb.cc     \
                       sns_cal.pb.cc                        \
                       sns_std_type.pb.cc                   \
                       sns_std.pb.cc                        \
                       sns_accel.pb.cc                      \
                       sns_ambient_light.pb.cc              \
                       sns_ambient_temperature.pb.cc        \
                       sns_amd.pb.cc                        \
                       sns_client.pb.cc                     \
                       sns_device_mode.pb.cc                \
                       sns_device_orient.pb.cc              \
                       sns_direct_channel.pb.cc             \
                       sns_game_rv.pb.cc                    \
                       sns_geomag_rv.pb.cc                  \
                       sns_gyro_cal.pb.cc                   \
                       sns_gyro.pb.cc                       \
                       sns_hall.pb.cc                       \
                       sns_humidity.pb.cc                   \
                       sns_mag_cal.pb.cc                    \
                       sns_mag.pb.cc                        \
                       sns_pedometer.pb.cc                  \
                       sns_pressure.pb.cc                   \
                       sns_proximity.pb.cc                  \
                       sns_rotv.pb.cc                       \
                       sns_sar.pb.cc                        \
                       sns_sensor_temperature.pb.cc         \
                       sns_sig_motion.pb.cc                 \
                       sns_suid.pb.cc                       \
                       sns_tilt.pb.cc                       \
                       sns_transport_ppe.pb.cc              \
                       sns_persist_motion_detect.pb.cc      \
                       sns_persist_stationary_detect.pb.cc  \
                       sns_hinge_angle.pb.cc                \
                       sns_gravity.pb.cc

BUILT_SOURCES = nanopb.pb.h                         \
                sns_std_sensor.pb.h                 \
                sns_physical_sensor_test.pb.h       \
                sns_std_event_gated_sensor.pb.h     \
                sns_cal.pb.h                        \
                sns_std_type.pb.h                   \
                sns_std.pb.h                        \
                sns_accel.pb.h                      \
                sns_ambient_light.pb.h              \
                sns_ambient_temperature.pb.h        \
                sns_amd.pb.h                        \
                sns_client.pb.h                     \
                sns_device_mode.pb.h                \
                sns_device_orient.pb.h              \
                sns_direct_channel.pb.h             \
                sns_game_rv.pb.h                    \
                sns_geomag_rv.pb.h                  \
                sns_gyro_cal.pb.h                   \
                sns_gyro.pb.h                       \
                sns_hall.pb.h                       \
                sns_humidity.pb.h                   \
                sns_mag_cal.pb.h                    \
                sns_mag.pb.h                        \
                sns_pedometer.pb.h                  \
                sns_pressure.pb.h                   \
                sns_proximity.pb.h                  \
                sns_rotv.pb.h                       \
                sns_sar.pb.h                        \
                sns_sensor_temperature.pb.h         \
                sns_sig_motion.pb.h                 \
                sns_suid.pb.h                       \
                sns_tilt.pb.h                       \
                sns_transport_ppe.pb.h              \
                sns_persist_motion_detect.pb.h      \
                sns_persist_stationary_detect.pb.h  \
                sns_hinge_angle.pb.h                \
                sns_gravity.pb.h

sources_libsensinghub = $(nodist_proto_SOURCES)

include_HEADERS = $(BUILT_SOURCES) $(proto_list)

library_includedir = $(pkgincludedir)

lib_LTLIBRARIES = libsensinghubapi.la

requiredlibs_sensinghubapi = @PROTOBUF_LIBS@

libsensinghubapi_la_CC = @CC@
libsensinghubapi_la_SOURCES = $(sources_libsensinghub)
libsensinghubapi_la_CFLAGS = $(AM_CFLAGS) @PROTOBUF_CFLAGS@
libsensinghubapi_la_LDFLAGS = -shared @LDFLAGS@ -version-number @LT_VERSION_NUMBER@
libsensinghubapi_la_LIBADD = $(requiredlibs_sensinghubapi)
