pkgconfigdir = $(libdir)/pkgconfig

EXTRA_DIST = $(pkgconfig_DATA)

version_dir = @sysconfdir@/sensors/registry

reg_config_dir = @sysconfdir@/sensors

shikra_dir = @base_libdir@/firmware/qcom/shikra/sensors
shikra_out_dir = @localstatedir@/lib/tqftpserv/sensors/registry
rules_out_dir = @sysconfdir@/udev/rules.d

common_list = $(wildcard $(srcdir)/registry/default/sensors/common/*.json)
common_jsondir   = $(reg_config_dir)/config
common_json_DATA = $(common_list)

qcm6490_list = $(wildcard $(srcdir)/registry/default/sensors/qcm6490/*.json)
qcm6490_jsondir = $(reg_config_dir)/config
qcm6490_json_DATA = $(qcm6490_list)

jsonlst_filesdir = $(reg_config_dir)/config
jsonlst_files_DATA = $(srcdir)/registry/default/sensors/qli/json.lst

regconfig_filesdir = $(reg_config_dir)
regconfig_files_DATA = $(srcdir)/registry/default/sensors/qcm6490/sns_reg_config

hamoa_list = $(wildcard $(srcdir)/registry/default/sensors/hamoa/*.json)
hamoa_jsondir = $(reg_config_dir)/config
hamoa_json_DATA = $(hamoa_list)

pakala_list = $(wildcard $(srcdir)/registry/default/sensors/sun/*.json)
pakala_jsondir = $(reg_config_dir)/config
pakala_json_DATA = $(pakala_list)

knp_list = $(wildcard $(srcdir)/registry/default/sensors/canoe/kaanapali*.json)
knp_jsondir = $(reg_config_dir)/config
knp_json_DATA = $(knp_list)

glymur_list = $(wildcard $(srcdir)/registry/default/sensors/glymur/*.json)
glymur_jsondir = $(reg_config_dir)/config
glymur_json_DATA = $(glymur_list)

bengal_list = $(wildcard $(srcdir)/registry/default/sensors/bengal/*.json)
bengal_jsondir = $(reg_config_dir)/config
bengal_json_DATA = $(bengal_list)

kalama_list = $(wildcard $(srcdir)/registry/default/sensors/kalama/*.json)
kalama_jsondir = $(reg_config_dir)/config
kalama_json_DATA = $(kalama_list)

qrb5165_list = $(wildcard $(srcdir)/registry/default/sensors/qrb5165/*.json)
qrb5165_jsondir = $(reg_config_dir)/config
qrb5165_json_DATA = $(qrb5165_list)

qrbx210_list = $(wildcard $(srcdir)/registry/default/sensors/qrbx210/*.json)
qrbx210_jsondir = $(reg_config_dir)/config
qrbx210_json_DATA = $(qrbx210_list)

talos_list = $(shell find  $(srcdir)/registry/default/sensors/talos/*.json -type f)
talos_jsondir = $(reg_config_dir)/config
talos_json_DATA = $(talos_list)

shikra_list = $(wildcard $(srcdir)/registry/default/sensors/shikra/*.json)
shikra_jsondir = $(reg_config_dir)/config
shikra_json_DATA = $(shikra_list)

output_filesdir = $(shikra_out_dir)
output_files_DATA = $(srcdir)/registry/default/sns_reg_version \
                    $(srcdir)/registry/default/sensors_registry

sns_reg_verdir = $(version_dir)
sns_reg_ver_DATA = $(srcdir)/registry/default/sns_reg_version
