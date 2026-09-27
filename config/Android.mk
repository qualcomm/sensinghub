LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

# List of sensors pd config files
sns_sensorspd_hub1_config_files := $(wildcard $(LOCAL_PATH)/registry/hub1/sensors/common/*.json)
sns_sensorspd_hub1_config_files += $(wildcard $(LOCAL_PATH)/registry/hub1/sensors/$(TARGET_BOARD_PLATFORM)/*.json)
sns_sensorspd_hub1_config_files += $(wildcard $(LOCAL_PATH)/registry/hub1/sensors/$(TARGET_BOARD_PLATFORM)/json.lst)

$(info "Adding hub1 sensors pd config files to vendor.img  = $(sns_sensorspd_hub1_config_files)")
define _add-sensorspd-hub1-config-files
 include $$(CLEAR_VARS)
 LOCAL_MODULE := hub1sensorspd$(notdir $(1))
 LOCAL_MODULE_STEM := $(notdir $(1))
 sns_sensorspd_hub1_config_modules += $$(LOCAL_MODULE)
 LOCAL_SRC_FILES := $(1:$(LOCAL_PATH)/%=%)
 LOCAL_MODULE_TAGS := optional
 LOCAL_MODULE_CLASS := ETC
ifeq ($(SNS_SUPPORT_MULTIPLE_SENSING_HUB),true)
 LOCAL_MODULE_PATH := $$(TARGET_OUT_VENDOR)/etc/sensors/hub1/config
else
 LOCAL_MODULE_PATH := $$(TARGET_OUT_VENDOR)/etc/sensors/config
endif
include $$(BUILD_PREBUILT)
endef

sns_sensorspd_hub1_config_modules :=
sns_sensorspd_hub1_config :=
$(foreach _conf-file, $(sns_sensorspd_hub1_config_files), $(eval $(call _add-sensorspd-hub1-config-files,$(_conf-file))))

include $(CLEAR_VARS)
LOCAL_MODULE := sns_sensorspd_hub1_config_module
LOCAL_MODULE_TAGS := optional
LOCAL_REQUIRED_MODULES := $(sns_sensorspd_hub1_config_modules)
include $(BUILD_PHONY_PACKAGE)

sns_sensorpd_hub1_reg_config_file := $(wildcard $(LOCAL_PATH)/registry/hub1/sensors/$(TARGET_BOARD_PLATFORM)/sns_reg_config)
$(info "sensor pd reg_config file  = $(sns_sensorpd_hub1_reg_config_file)")
ifeq (,$(sns_sensorpd_hub1_reg_config_file))
$(info "sensor pd reg_config doesn't exist")
sns_sensorpd_hub1_reg_config_file := $(LOCAL_PATH)/registry/hub1/sensors/common/sns_reg_config
endif
$(info "sensor pd reg_config file  = $(sns_sensorpd_hub1_reg_config_file)")
include $(CLEAR_VARS)
LOCAL_MODULE := sns_sensorspd_hub1_reg_config
LOCAL_MODULE_STEM := sns_reg_config
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_OWNER := qti
LOCAL_MODULE_CLASS := ETC
ifeq ($(SNS_SUPPORT_MULTIPLE_SENSING_HUB),true)
LOCAL_MODULE_PATH := $(TARGET_OUT_VENDOR_ETC)/sensors/hub1
else
LOCAL_MODULE_PATH := $(TARGET_OUT_VENDOR_ETC)/sensors
endif
LOCAL_SRC_FILES := $(sns_sensorpd_hub1_reg_config_file:$(LOCAL_PATH)/%=%)
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := sensors_hub1_registry
LOCAL_MODULE_STEM := sensors_registry
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_OWNER := qti
LOCAL_MODULE_CLASS := ETC
ifeq ($(SNS_SUPPORT_MULTIPLE_SENSING_HUB),true)
LOCAL_MODULE_PATH := $(PRODUCT_OUT)/persist/sensors/hub1/sensors/registry/registry
else
LOCAL_MODULE_PATH := $(PRODUCT_OUT)/persist/sensors/registry/registry
endif
LOCAL_SRC_FILES := registry/hub1/sensors_registry
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := sns_hub1_reg_version
LOCAL_MODULE_STEM := sns_reg_version
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_OWNER := qti
LOCAL_MODULE_CLASS := ETC
ifeq ($(SNS_SUPPORT_MULTIPLE_SENSING_HUB),true)
LOCAL_MODULE_PATH := $(PRODUCT_OUT)/persist/sensors/hub1/sensors/registry
else
LOCAL_MODULE_PATH := $(PRODUCT_OUT)/persist/sensors/registry
endif
LOCAL_SRC_FILES := registry/hub1/sns_reg_version
include $(BUILD_PREBUILT)

ifeq ($(TARGET_BOARD_PLATFORM),seraph) # TARGET_BOARD_PLATFORM==seraph

sns_sensorspd_config_files_seraph := $(wildcard $(LOCAL_PATH)/registry/hub1/sensors/common/*.json)
sns_sensorspd_config_files_seraph += $(wildcard $(LOCAL_PATH)/registry/hub1/sensors/$(TARGET_BOARD_PLATFORM)/*.json)
sns_sensorspd_config_files_seraph += $(wildcard $(LOCAL_PATH)/registry/hub1/sensors/$(TARGET_BOARD_PLATFORM)/json.lst)

$(info "Adding sensors pd config files to vendor.img for seraph = $(sns_sensorspd_config_files_seraph)")
define _add-sensorspd-config-files-seraph
 include $$(CLEAR_VARS)
 LOCAL_MODULE := sensorspdSeraph$(notdir $(1))
 LOCAL_MODULE_STEM := $(notdir $(1))
 sns_sensorspd_config_modules_seraph += $$(LOCAL_MODULE)
 LOCAL_SRC_FILES := $(1:$(LOCAL_PATH)/%=%)
 LOCAL_MODULE_TAGS := optional
 LOCAL_MODULE_CLASS := ETC
 LOCAL_MODULE_PATH := $$(TARGET_OUT_VENDOR)/etc/sensors/config
include $$(BUILD_PREBUILT)
endef

sns_sensorspd_config_modules_seraph :=
$(foreach _conf-file, $(sns_sensorspd_config_files_seraph), $(eval $(call _add-sensorspd-config-files-seraph,$(_conf-file))))

include $(CLEAR_VARS)
LOCAL_MODULE := sns_sensorspd_config_module_seraph
LOCAL_MODULE_TAGS := optional
LOCAL_REQUIRED_MODULES := $(sns_sensorspd_config_modules_seraph)
include $(BUILD_PHONY_PACKAGE)

sns_sensorpd_reg_config_file_seraph := $(wildcard $(LOCAL_PATH)/registry/hub1/sensors/$(TARGET_BOARD_PLATFORM)/sns_reg_config)
$(info "sensor pd reg_config file  = $(sns_sensorpd_reg_config_file_seraph)")
ifeq (,$(sns_sensorpd_reg_config_file_seraph))
$(info "sensor pd reg_config doesn't exist")
sns_sensorpd_reg_config_file_seraph := $(LOCAL_PATH)/registry/hub1/sensors/common/sns_reg_config
endif
$(info "sensor pd reg_config file  = $(sns_sensorpd_reg_config_file_seraph)")
include $(CLEAR_VARS)
LOCAL_MODULE := sns_sensorspd_hub1_reg_config_seraph
LOCAL_MODULE_STEM := sns_reg_config
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_OWNER := qti
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_PATH := $(TARGET_OUT_VENDOR_ETC)/sensors
LOCAL_SRC_FILES := $(sns_sensorpd_reg_config_file_seraph:$(LOCAL_PATH)/%=%)
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := sensors_registry_seraph
LOCAL_MODULE_STEM := sensors_registry
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_OWNER := qti
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_PATH := $(PRODUCT_OUT)/persist/sensors/registry/registry
LOCAL_SRC_FILES := registry/hub1/sensors_registry
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := sns_reg_version_seraph
LOCAL_MODULE_STEM := sns_reg_version
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_OWNER := qti
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_PATH := $(PRODUCT_OUT)/persist/sensors/registry
LOCAL_SRC_FILES := registry/hub1/sns_reg_version
include $(BUILD_PREBUILT)

endif  # TARGET_BOARD_PLATFORM==seraph

ifeq ($(SNS_SUPPORT_MULTIPLE_SENSING_HUB),true)
# List of hub2 sensors pd config files
sns_sensorspd_hub2_config_files := $(wildcard $(LOCAL_PATH)/registry/hub2/sensors/common/*.json)
sns_sensorspd_hub2_config_files += $(wildcard $(LOCAL_PATH)/registry/hub2/sensors/$(TARGET_BOARD_PLATFORM)/*.json)
sns_sensorspd_hub2_config_files += $(wildcard $(LOCAL_PATH)/registry/hub2/sensors/$(TARGET_BOARD_PLATFORM)/json.lst)

$(info "Adding hub2 sensors pd config files to vendor.img  = $(sns_sensorspd_hub2_config_files)")
define _add-sensorspd-hub2-config-files
 include $$(CLEAR_VARS)
 LOCAL_MODULE := hub2sensorspd$(notdir $(1))
 LOCAL_MODULE_STEM := $(notdir $(1))
 sns_sensorspd_hub2_config_modules += $$(LOCAL_MODULE)
 LOCAL_SRC_FILES := $(1:$(LOCAL_PATH)/%=%)
 LOCAL_MODULE_TAGS := optional
 LOCAL_MODULE_CLASS := ETC
 LOCAL_MODULE_PATH := $$(TARGET_OUT_VENDOR)/etc/sensors/hub2/config
include $$(BUILD_PREBUILT)
endef

sns_sensorspd_hub2_config_modules :=
sns_sensorspd_hub2_config :=
$(foreach _conf-file, $(sns_sensorspd_hub2_config_files), $(eval $(call _add-sensorspd-hub2-config-files,$(_conf-file))))

include $(CLEAR_VARS)
LOCAL_MODULE := sns_sensorspd_hub2_config_module
LOCAL_MODULE_TAGS := optional
LOCAL_REQUIRED_MODULES := $(sns_sensorspd_hub2_config_modules)
include $(BUILD_PHONY_PACKAGE)

include $(CLEAR_VARS)
LOCAL_MODULE := sensors_hub2_registry
LOCAL_MODULE_STEM := sensors_registry
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_OWNER := qti
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_PATH := $(PRODUCT_OUT)/persist/sensors/hub2/sensors/registry/registry
LOCAL_SRC_FILES := registry/hub2/sensors_registry
include $(BUILD_PREBUILT)

sns_sensorpd_hub2_reg_config_file := $(wildcard $(LOCAL_PATH)/registry/hub2/sensors/$(TARGET_BOARD_PLATFORM)/sns_reg_config)
$(info "sensor pd reg_config file  = $(sns_sensorpd_hub2_reg_config_file)")
ifeq (,$(sns_sensorpd_hub2_reg_config_file))
$(info "sensor pd reg_config doesn't exist")
sns_sensorpd_hub2_reg_config_file := $(LOCAL_PATH)/registry/hub2/sensors/common/sns_reg_config
endif
$(info "sensor pd reg_config file  = $(sns_sensorpd_hub2_reg_config_file)")
include $(CLEAR_VARS)
LOCAL_MODULE := sns_sensorspd_hub2_reg_config
LOCAL_MODULE_STEM := sns_reg_config
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_OWNER := qti
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_PATH := $(TARGET_OUT_VENDOR_ETC)/sensors/hub2
LOCAL_SRC_FILES := $(sns_sensorpd_hub2_reg_config_file:$(LOCAL_PATH)/%=%)
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := sns_hub2_reg_version
LOCAL_MODULE_STEM := sns_reg_version
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_OWNER := qti
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_PATH := $(PRODUCT_OUT)/persist/sensors/hub2/sensors/registry
LOCAL_SRC_FILES := registry/hub2/sns_reg_version
include $(BUILD_PREBUILT)
endif

# List of ois sensors pd config files
sns_oispd_config_files := $(wildcard $(LOCAL_PATH)/registry/hub1/ois/common/*.json)
sns_oispd_config_files += $(wildcard $(LOCAL_PATH)/registry/hub1/ois/$(TARGET_BOARD_PLATFORM)/*.json)
sns_oispd_config_files += $(wildcard $(LOCAL_PATH)/registry/hub1/ois/$(TARGET_BOARD_PLATFORM)/json.lst)

$(info "Adding ois pd config files to vendor.img  = $(sns_oispd_config_files)")
define _add-oispd-config-files
 include $$(CLEAR_VARS)
 LOCAL_MODULE := oispd$(notdir $(1))
 LOCAL_MODULE_STEM := $(notdir $(1))
 sns_oispd_config_modules += $$(LOCAL_MODULE)
 LOCAL_SRC_FILES := $(1:$(LOCAL_PATH)/%=%)
 LOCAL_MODULE_TAGS := optional
 LOCAL_MODULE_CLASS := ETC
 LOCAL_MODULE_PATH := $$(PRODUCT_OUT)/persist/hlos_rfs/shared/sensors/config
include $$(BUILD_PREBUILT)
endef

sns_oispd_config_modules :=
sns_oispd_config :=
$(foreach _conf-file, $(sns_oispd_config_files), $(eval $(call _add-oispd-config-files,$(_conf-file))))

include $(CLEAR_VARS)
LOCAL_MODULE := sns_oispd_config_module
LOCAL_MODULE_TAGS := optional
LOCAL_REQUIRED_MODULES := $(sns_oispd_config_modules)
include $(BUILD_PHONY_PACKAGE)

include $(CLEAR_VARS)
LOCAL_MODULE := sns_oispd_reg_config
LOCAL_MODULE_STEM := sns_reg_config
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_OWNER := qti
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_PATH := $(PRODUCT_OUT)/persist/hlos_rfs/shared/sensors
LOCAL_SRC_FILES := registry/hub1/ois/$(TARGET_BOARD_PLATFORM)/sns_reg_config
include $(BUILD_PREBUILT)

# Sensors PD ODP Config File
sns_sensorpd_odp_config_file := $(wildcard $(LOCAL_PATH)/registry/hub1/odp/$(TARGET_BOARD_PLATFORM)/sns_odp_config)
$(info "sensor pd odp_config file  = $(sns_sensorpd_odp_config_file)")
ifeq (,$(sns_sensorpd_odp_config_file))
$(info "sensor pd odp_config doesn't exist")
sns_sensorpd_odp_config_file := $(LOCAL_PATH)/registry/hub1/odp/common/sns_odp_config
endif
$(info "sensor pd odp_config file  = $(sns_sensorpd_odp_config_file)")

include $(CLEAR_VARS)
LOCAL_MODULE := sns_sensorspd_odp_config
LOCAL_MODULE_STEM := sns_odp_config
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_OWNER := qti
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_PATH := $(TARGET_OUT_VENDOR_ETC)/sensors
LOCAL_SRC_FILES := $(sns_sensorpd_odp_config_file:$(LOCAL_PATH)/%=%)
include $(BUILD_PREBUILT)

# Sensors PD ODP Version File
include $(CLEAR_VARS)
LOCAL_MODULE := sns_odp_version
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_OWNER := qti
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_PATH := $(PRODUCT_OUT)/persist/sensors/odp
LOCAL_SRC_FILES := registry/hub1/sns_odp_version
include $(BUILD_PREBUILT)
