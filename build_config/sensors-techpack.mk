include vendor/qcom/opensource/sensing-hub/build_config/sensinghub_vendor_product.mk

-include $(QCPATH)/techpack/artifacts/sensors/$(TARGET_BOARD_PLATFORM)/prebuilt.mk

ifneq (,$(wildcard $(QCPATH)/sensors-ship))
include $(QCPATH)/sensors-ship/build_config/sns_vendor_product.mk
endif

ifneq (,$(wildcard $(QCPATH)/sensors-internal))
include $(QCPATH)/sensors-internal/build_config/sns_vendor_noship_product.mk
endif

ifneq (,$(wildcard $(QCPATH)/sensors-android))
include $(QCPATH)/sensors-android/build_config/sns_la_vendor_product.mk
endif

ifneq (,$(wildcard $(QCPATH)/sensors-android-internal))
include $(QCPATH)/sensors-android-internal/build_config/sns_la_vendor_noship_product.mk
endif


.PHONY: sensors_tp  sensors_tp_sensinghub sensors_tp_common sensors_tp_hal

sensors_tp: sensors_tp_sensinghub sensors_tp_common sensors_tp_hal

sensors_tp_sensinghub: $(SENSING_HUB_PRODUCTS)

sensors_tp_common: $(SNS_VENDOR_PRODUCTS) $(SNS_VENDOR_PRODUCTS_DBG) $(SNS_VENDOR_INTERNAL_PRODUCTS_DBG)

sensors_tp_hal: $(SNS_LA_VENDOR_PRODUCTS) $(SNS_LA_VENDOR_PRODUCTS_DBG) $(SNS_LA_VENDOR_INTERNAL_PRODUCTS_DBG)


