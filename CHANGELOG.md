# Changelog

All notable changes to this project will be documented in this file.

## [Unreleased]

## [2.3.4] - 2026-11-06
- Update the qli folder json.lst to accommodate qsh_camera_common.json and qsh_camera_ov08x_0.json

## [2.3.3] - 2026-10-28
- Fix qshJsonParser member initialization order
- Consolidate hub1/hub2 sensor registry configs into default
- Update SPDX license identifier to BSD-3-Clause

## [2.3.2] - 2026-10-12

- Add registry configs for kalama/qrb5165/qrbx210 and new sensor protos

## [2.3.1] - 2026-10-07

- Add rpmsgSession communication layer
- Fix shikra registry config path and add systemd SOC-based service gating
- ci: use sudo for QMI Framework/FastRPC installs into system paths

## [2.3.0] - 2026-09-29

- Add core QMI/GLink session implementation module
- Add sensors config registry subsystem
- sensors: add sensorsdaemon (sensors.qti) service to open-source
- Fix installed header paths for nanopb/proto
- build.sh: pick qmi-headers
- ci: install QMI Framework and enable OSS QMI build
- ci: fix qmi-framework pkg-config Cflags to include qmi_framework subdir

## [2.2.3] - 2026-08-07

- sensors: load versioned QshSession library
- Replace pull_request_target with pull_request in GitHub Actions workflow
