# Below macro needs to be removed once SENSORS TP is fully enabled
# Same can controled by sensors_techpack_env.sh from techpack-sensors
BUILD_SENSORS_TECHPACK_SOURCE := true

$(call add_soong_config_namespace,qshconfig)

ifneq (,$(findstring $(PLATFORM_VERSION), 15 14 13 12 11))
$(call add_soong_config_var_value,qshconfig,isISessionAvailable,unset)
else
$(call add_soong_config_var_value,qshconfig,isISessionAvailable,set)
endif

#default values for the macros
$(call soong_config_set,qtisensors,hy00,false)
$(call soong_config_set,qtisensors,hy11,false)
$(call soong_config_set,qtisensors,hy22,false)
$(call soong_config_set,qtisensors,hwasan,false)

ifneq ($(BUILD_SENSORS_TECHPACK_SOURCE), true)
$(call soong_config_set,qtisensors,hy00,true)
$(call soong_config_set,qtisensors,hy11,true)
$(call soong_config_set,qtisensors,hy22,true)
$(call soong_config_set,qtisensors,hwasan,true)
endif

ifeq (,$(wildcard $(QCPATH)/sensors-internal))
$(call soong_config_set,qtisensors,hy11,true)
$(call soong_config_set,qtisensors,hwasan,true)
endif

ifeq (,$(wildcard $(QCPATH)/sensors-ship))
$(call soong_config_set,qtisensors,hy11,true)
$(call soong_config_set,qtisensors,hy22,true)
$(call soong_config_set,qtisensors,hwasan,true)
endif