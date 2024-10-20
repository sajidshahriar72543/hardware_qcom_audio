LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := audio_hal_includes

LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/audiod \
    $(LOCAL_PATH)/hal \
    $(LOCAL_PATH)/hal/audio_extn \
    $(LOCAL_PATH)/hal/msm8916 \
    $(LOCAL_PATH)/hal/msm8960 \
    $(LOCAL_PATH)/hal/msm8974 \
    $(LOCAL_PATH)/hal/voice_extn \
    $(LOCAL_PATH)/hdmi_in_test/src \
    $(LOCAL_PATH)/mm-audio/aenc-aac/qdsp6/inc \
    $(LOCAL_PATH)/mm-audio/aenc-amrnb/qdsp6/inc \
    $(LOCAL_PATH)/mm-audio/aenc-evrc/qdsp6/inc \
    $(LOCAL_PATH)/mm-audio/aenc-g711/qdsp6/inc \
    $(LOCAL_PATH)/mm-audio/aenc-qcelp13/qdsp6/inc \
    $(LOCAL_PATH)/post_proc \
    $(LOCAL_PATH)/qahw_api/inc \
    $(LOCAL_PATH)/qahw_api/test \
    $(LOCAL_PATH)/qahw/inc \
    $(LOCAL_PATH)/stt_meta

include $(BUILD_SHARED_LIBRARY)
