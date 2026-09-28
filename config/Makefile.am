pkgconfigdir = $(libdir)/pkgconfig

EXTRA_DIST = $(pkgconfig_DATA)

version_dir = @sysconfdir@/sensors/registry

reg_config_dir = @sysconfdir@/sensors

shikra_dir = @base_libdir@/firmware/qcom/shikra/sensors
shikra_out_dir = @localstatedir@/lib/tqftpserv/sensors/registry
rules_out_dir = @sysconfdir@/udev/rules.d

common_list = $(wildcard $(srcdir)/registry/hub1/sensors/common/*.json)
common_jsondir   = $(reg_config_dir)/config
common_json_DATA = $(common_list)

qcm6490_list = $(wildcard $(srcdir)/registry/hub1/sensors/qcm6490/*.json)
qcm6490_jsondir = $(reg_config_dir)/config
qcm6490_json_DATA = $(qcm6490_list)

jsonlst_filesdir = $(reg_config_dir)/config
jsonlst_files_DATA = $(srcdir)/registry/hub1/sensors/qli/json.lst

regconfig_filesdir = $(reg_config_dir)
regconfig_files_DATA = $(srcdir)/registry/hub1/sensors/qcm6490/sns_reg_config

hamoa_list = $(wildcard $(srcdir)/registry/hub1/sensors/hamoa/*.json)
hamoa_jsondir = $(reg_config_dir)/config
hamoa_json_DATA = $(hamoa_list)

pakala_list = $(wildcard $(srcdir)/registry/hub1/sensors/sun/*.json)
pakala_jsondir = $(reg_config_dir)/config
pakala_json_DATA = $(pakala_list)

knp_list = $(wildcard $(srcdir)/registry/hub1/sensors/canoe/kaanapali*.json)
knp_jsondir = $(reg_config_dir)/config
knp_json_DATA = $(knp_list)

glymur_list = $(wildcard $(srcdir)/registry/hub1/sensors/glymur/*.json)
glymur_jsondir = $(reg_config_dir)/config
glymur_json_DATA = $(glymur_list)

bengal_list = $(wildcard $(srcdir)/registry/hub1/sensors/bengal/*.json)
bengal_jsondir = $(reg_config_dir)/config
bengal_json_DATA = $(bengal_list)

talos_list = $(shell find  $(srcdir)/registry/hub1/sensors/talos/*.json -type f)
talos_jsondir = $(reg_config_dir)/config
talos_json_DATA = $(talos_list)

shikra_list = $(wildcard $(srcdir)/registry/hub1/sensors/shikra/*.json) \
              $(wildcard $(srcdir)/registry/hub1/sensors/shikra/json.lst) \
              $(wildcard $(srcdir)/registry/hub1/sensors/common/*.json)
shikra_jsondir = $(shikra_dir)/config
shikra_json_DATA = $(shikra_list)

output_filesdir = $(shikra_out_dir)
output_files_DATA = $(srcdir)/registry/hub1/sns_reg_version \
                    $(srcdir)/registry/hub1/sensors_registry

shikra_regconfig_filesdir = $(shikra_dir)
shikra_regconfig_files_DATA = $(srcdir)/registry/hub1/sensors/shikra/sns_reg_config

sns_reg_verdir = $(version_dir)
sns_reg_ver_DATA = $(srcdir)/registry/hub1/sns_reg_version
