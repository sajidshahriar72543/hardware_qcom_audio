/* Copyright (c) 2021, The Linux Foundation. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above
      copyright notice, this list of conditions and the following
      disclaimer in the documentation and/or other materials provided
      with the distribution.
    * Neither the name of The Linux Foundation nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.*/
/*
* Changes from Qualcomm Innovation Center are provided under the following license:
*
* Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted (subject to the limitations in the
* disclaimer below) provided that the following conditions are met:
*
*    * Redistributions of source code must retain the above copyright
*      notice, this list of conditions and the following disclaimer.
*
*    * Redistributions in binary form must reproduce the above
*      copyright notice, this list of conditions and the following
*      disclaimer in the documentation and/or other materials provided
*      with the distribution.
*
*    * Neither the name of Qualcomm Innovation Center, Inc. nor the names of its
*      contributors may be used to endorse or promote products derived
*      from this software without specific prior written permission.
*
* NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE
* GRANTED BY THIS LICENSE. THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT
* HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
* WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
* MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
* IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
* ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
* DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
* GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
* INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
* IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
* OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
* IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#include "PowerPolicyClient.h"

#include <android-base/logging.h>
#include <dlfcn.h>
#include <cutils/properties.h>

#ifdef DAEMON_SUPPORT_AUTO
#define LIB_AUDIO_HAL_PLUGIN "libaudiohalpluginclient.so"
#else
#define LIB_AUDIO_HAL_PLUGIN "libaudiohalplugin.so"
#endif

namespace aafap = aidl::android::frameworks::automotive::powerpolicy;

using aafap::CarPowerPolicy;
using aafap::CarPowerPolicyFilter;
using aafap::PowerComponent;
using ::android::frameworks::automotive::powerpolicy::hasComponent;
using ::ndk::ScopedAStatus;

namespace {

constexpr PowerComponent kAudioComponent = PowerComponent::AUDIO;
constexpr PowerComponent kMicComponent = PowerComponent::MICROPHONE;

}  // namespace

PowerPolicyClient::PowerPolicyClient(power_policy_init_config_t init_config) {

    fp_in_set_power_policy = init_config.fp_in_set_power_policy;
    fp_out_set_power_policy = init_config.fp_out_set_power_policy;
    fp_snd_card_status = init_config.fp_snd_card_status;

    plugin_handle = dlopen(LIB_AUDIO_HAL_PLUGIN, RTLD_NOW);
    if (plugin_handle == NULL) {
        LOG(ERROR) << "Failed to open plugin library";
        return;
    }

    hal_plugin_send_msg = (hal_plugin_send_msg_t) dlsym(plugin_handle,
                                         "audio_hal_plugin_send_msg");
    if (hal_plugin_send_msg == NULL) {
        LOG(ERROR) << "dlsym failed for audio_hal_plugin_send_msg";
        dlclose(plugin_handle);
        plugin_handle = NULL;
    }

    LOG(ERROR) << "PowerPolicyClient Initialzed";
}

PowerPolicyClient::~PowerPolicyClient() {
    if (plugin_handle != NULL)
        dlclose(plugin_handle);
}

void PowerPolicyClient::onInitFailed() {
    LOG(ERROR) << "Initializing power policy client failed";
}

std::vector<PowerComponent> PowerPolicyClient::getComponentsOfInterest() {
    std::vector<PowerComponent> components{kAudioComponent, kMicComponent};
    return components;
}

ScopedAStatus PowerPolicyClient::onPolicyChanged(const CarPowerPolicy& powerPolicy) {
    uint8_t disable = 0;
    char lpm_state[PROPERTY_VALUE_MAX] = LPM_DEFAULT_STATE,
        hiber_state[PROPERTY_VALUE_MAX] = {0};

#if defined (PLATFORM_MSMSTEPPE)
    property_get("persist.vendor.vehicle.lpm", lpm_state, LPM_DEFAULT_STATE);
    property_get("vendor.car.hiber", hiber_state, HIBER_DEFAULT_STATE);
    LOG(ERROR) << "hiber state "<<hiber_state<< " lpm state "<<lpm_state;
#endif

    if (hasComponent(powerPolicy.enabledComponents, kAudioComponent)) {
        LOG(ERROR) << "Power policy: Audio component is enabled";
        disable = 0;

        if (!strcmp(lpm_state, "disk") && !strcmp(hiber_state, "hiberExit")) {
            while(!fp_snd_card_status()) {
                //sleep for 500msec
                usleep(500000);
            }
        }
        fp_out_set_power_policy(!disable);
        if (hal_plugin_send_msg != NULL)
            hal_plugin_send_msg(AUDIO_HAL_PLUGIN_MSG_SILENT_MODE,
                                &disable, sizeof(disable));
    } else if (hasComponent(powerPolicy.disabledComponents, kAudioComponent)) {
        LOG(ERROR) << "Power policy: Audio component is disabled";
        disable = 1;

        fp_out_set_power_policy(!disable);

        if (!strcmp(lpm_state, "disk") && !strcmp(hiber_state, "hiberEntry")) {
            LOG(ERROR) << "avoid mercury output path disable during S2D";
            return ScopedAStatus::ok();
        }

        if (hal_plugin_send_msg != NULL)
            hal_plugin_send_msg(AUDIO_HAL_PLUGIN_MSG_SILENT_MODE,
                                &disable, sizeof(disable));
    }

    if (hasComponent(powerPolicy.enabledComponents, kMicComponent)) {
        LOG(ERROR) << "Power policy: Microphone component is enabled";
        disable = 0;

        if (!strcmp(lpm_state, "disk") && !strcmp(hiber_state, "hiberExit")) {
            while(!fp_snd_card_status()) {
                 //sleep for 500msec
                usleep(500000);
            }
        }
        fp_in_set_power_policy(!disable);
        if (hal_plugin_send_msg != NULL)
            hal_plugin_send_msg(AUDIO_HAL_PLUGIN_MSG_MIC_STATE,
                                &disable, sizeof(disable));
    } else if (hasComponent(powerPolicy.disabledComponents, kMicComponent)) {
        disable = 1;

        fp_in_set_power_policy(!disable);
        if (!strcmp(lpm_state, "disk") && !strcmp(hiber_state, "hiberEntry")) {
            LOG(ERROR) << "avoid mercury Mic path disable during S2D";
            return ScopedAStatus::ok();
        }
        if (hal_plugin_send_msg != NULL)
            hal_plugin_send_msg(AUDIO_HAL_PLUGIN_MSG_MIC_STATE,
                                &disable, sizeof(disable));
        LOG(ERROR) << "Power policy: Microphone component is disabled";
    }
    return ScopedAStatus::ok();
}
