/*
 * Copyright (C) 2011 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef AISPEECH_AUDIO_HAL_INTERFACE_H
#define AISPEECH_AUDIO_HAL_INTERFACE_H

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <dlfcn.h>
#include <stdbool.h>




#ifdef __cplusplus
extern "C" {
#endif

#define AISPEECH_AUDIO_HAL_LIBRARY_PATH     "./library/basic/libs/r311_32/libaispeechaudio.so"


#define LOGV(...)  printf(__VA_ARGS__)
#define LOGD(...)  printf(__VA_ARGS__)
#define LOGW(...)  printf(__VA_ARGS__)
#define LOGE(...)  fprintf(stderr, __VA_ARGS__)


/**
 * Do not modify below!
 */

/**
 * Name of the hal_module_info
 */
#define HAL_MODULE_INFO_SYM         HMI

/**
 * Name of the hal_module_info as a string
 */
#define HAL_MODULE_INFO_SYM_AS_STR  "HMI"

/*
 * Value for the hw_module_t.tag field
 */

#define MAKE_TAG_CONSTANT(A,B,C,D) (((A) << 24) | ((B) << 16) | ((C) << 8) | (D))

#define HARDWARE_MODULE_TAG MAKE_TAG_CONSTANT('H', 'W', 'M', 'T')
#define HARDWARE_DEVICE_TAG MAKE_TAG_CONSTANT('H', 'W', 'D', 'T')

#define HARDWARE_MAKE_API_VERSION(maj,min) \
            ((((maj) & 0xff) << 8) | ((min) & 0xff))

/*
 * The current HAL API version.
 *
 * All module implementations must set the hw_module_t.hal_api_version field
 * to this value when declaring the module with HAL_MODULE_INFO_SYM.
 *
 * Note that previous implementations have always set this field to 0.
 * Therefore, libhardware HAL API will always consider versions 0.0 and 1.0
 * to be 100% binary compatible.
 *
 */
#define HARDWARE_HAL_API_VERSION HARDWARE_MAKE_API_VERSION(1, 0)

/*
 * Helper macros for module implementors.
 *
 * The derived modules should provide convenience macros for supported
 * versions so that implementations can explicitly specify module/device
 * versions at definition time.
 *
 * Use this macro to set the hw_module_t.module_api_version field.
 */
#define HARDWARE_MODULE_API_VERSION(maj,min) HARDWARE_MAKE_API_VERSION(maj,min)

/*
 * Use this macro to set the hw_device_t.version field
 */
#define HARDWARE_DEVICE_API_VERSION(maj,min) HARDWARE_MAKE_API_VERSION(maj,min)

/**
 * The id of this module
 */
#define AUDIO_HARDWARE_MODULE_ID "audio"

/**
 * Name of the audio devices to open
 */
#define AUDIO_HARDWARE_INTERFACE "audio_hw_if"

/* Use version 0.1 to be compatible with first generation of audio hw module with version_major
 * hardcoded to 1. No audio module API change.
 */
#define AUDIO_MODULE_API_VERSION_0_1 HARDWARE_MODULE_API_VERSION(0, 1)
#define AUDIO_MODULE_API_VERSION_CURRENT AUDIO_MODULE_API_VERSION_0_1

/* First generation of audio devices had version hardcoded to 0. all devices with versions < 1.0
 * will be considered of first generation API.
 */
#define AUDIO_DEVICE_API_VERSION_0_0 HARDWARE_DEVICE_API_VERSION(0, 0)
#define AUDIO_DEVICE_API_VERSION_1_0 HARDWARE_DEVICE_API_VERSION(1, 0)
#define AUDIO_DEVICE_API_VERSION_2_0 HARDWARE_DEVICE_API_VERSION(2, 0)
#define AUDIO_DEVICE_API_VERSION_3_0 HARDWARE_DEVICE_API_VERSION(3, 0)
#define AUDIO_DEVICE_API_VERSION_CURRENT AUDIO_DEVICE_API_VERSION_3_0
/* Minimal audio HAL version supported by the audio framework */
#define AUDIO_DEVICE_API_VERSION_MIN AUDIO_DEVICE_API_VERSION_2_0

typedef enum {
    AUDIO_SOURCE_DEFAULT             = 0,
    AUDIO_SOURCE_MIC                 = 1,
    AUDIO_SOURCE_VOICE_UPLINK        = 2,
    AUDIO_SOURCE_VOICE_DOWNLINK      = 3,
    AUDIO_SOURCE_VOICE_CALL          = 4,
    AUDIO_SOURCE_CAMCORDER           = 5,
    AUDIO_SOURCE_VOICE_RECOGNITION   = 6,
    AUDIO_SOURCE_VOICE_COMMUNICATION = 7,
    AUDIO_SOURCE_REMOTE_SUBMIX       = 8, /* Source for the mix to be presented remotely.      */
                                          /* An example of remote presentation is Wifi Display */
                                          /*  where a dongle attached to a TV can be used to   */
                                          /*  play the mix captured by this audio source.      */
    AUDIO_SOURCE_CNT,
    AUDIO_SOURCE_MAX                 = AUDIO_SOURCE_CNT - 1,
    AUDIO_SOURCE_A2DP_SINK           = 1997,
    AUDIO_SOURCE_VOICE_DLINK_ULINK_MIX = 1998,
    AUDIO_SOURCE_HOTWORD             = 1999, /* A low-priority, preemptible audio source for
                                                for background software hotword detection.
                                                Same tuning as AUDIO_SOURCE_VOICE_RECOGNITION.
                                                Used only internally to the framework. Not exposed
                                                at the audio HAL. */
} audio_source_t;

typedef enum {
    AUDIO_DEVICE_OUT_SPEAKER        = 0x2,
    AUDIO_DEVICE_OUT_WIRED_HEADSET  = 0x4,
    AUDIO_DEVICE_OUT_BLUETOOTH_SCO  = 0x10,
    AUDIO_DEVICE_OUT_BLUETOOTH_A2DP = 0x80,
    AUDIO_DEVICE_OUT_DEFAULT        = 0x40000000
} audio_device_out_t;

static const char KEY_BT_StATE[] = "bt_state";
//set the value via the function audio_hw_device_t->get_parameters
static const char KEY_AUDIO_RES_DIR[] = "audio_res_dir";
//get the value via the function audio_hw_device_t->get_parameters
static const char KEY_AUDIO_HAL_VER[] = "audio_hal_ver"; 
//get the value via the function audio_hw_device_t->get_parameters_int
static const char KEY_AUDIO_DOA[] = "audio_doa"; 

/**************************************/

struct audio_stream_in {
    /**
     * Return size of input buffer in bytes for this stream - eg. 4800.
     * It should be a multiple of the frame size.
     */
    size_t (*get_buffer_size)(const struct audio_stream_in *stream);

    /** Read audio buffer in from audio driver. Returns number of bytes read, or a
     *  negative status_t. If at least one frame was read prior to the error,
     *  read should return that byte count and then return an error in the subsequent call.
     */
    ssize_t (*read)(struct audio_stream_in *stream, void* buffer,
                    size_t bytes);

    /**
     * Put the audio hardware input/output into standby mode.
     * Driver should exit from standby mode at the next I/O operation.
     * Returns 0 on success and <0 on failure.
     */
    int (*standby)(struct audio_stream_in *stream);
};
typedef struct audio_stream_in audio_stream_in_t;

struct audio_stream_out {
    /**
     *  Write audio buffer to audio driver. Returns number of bytes write, or a
     *  negative status_t. If at least one frame was written prior to the error,
     *  write should return that byte count and then return an error in the subsequent call.
     */
    ssize_t (*write)(struct audio_stream_out *stream, void* buffer,
                    size_t bytes);

    /**
     * Put the audio hardware input/output into standby mode.
     * Driver should exit from standby mode at the next I/O operation.
     * Returns 0 on success and <0 on failure.
     */
    int (*standby)(struct audio_stream_out *stream);
};
typedef struct audio_stream_out audio_stream_out_t;

/**********************************************************************/
struct hw_module_t;
struct hw_module_methods_t;
struct hw_device_t;

/**
 * Every hardware module must have a data structure named HAL_MODULE_INFO_SYM
 * and the fields of this data structure must begin with hw_module_t
 * followed by module specific information.
 */
typedef struct hw_module_t {
    /** tag must be initialized to HARDWARE_MODULE_TAG */
    uint32_t tag;

    /**
     * The API version of the implemented module. The module owner is
     * responsible for updating the version when a module interface has
     * changed.
     *
     * The derived modules such as gralloc and audio own and manage this field.
     * The module user must interpret the version field to decide whether or
     * not to inter-operate with the supplied module implementation.
     * For example, SurfaceFlinger is responsible for making sure that
     * it knows how to manage different versions of the gralloc-module API,
     * and AudioFlinger must know how to do the same for audio-module API.
     *
     * The module API version should include a major and a minor component.
     * For example, version 1.0 could be represented as 0x0100. This format
     * implies that versions 0x0100-0x01ff are all API-compatible.
     *
     * In the future, libhardware will expose a hw_get_module_version()
     * (or equivalent) function that will take minimum/maximum supported
     * versions as arguments and would be able to reject modules with
     * versions outside of the supplied range.
     */
    uint16_t module_api_version;
#define version_major module_api_version
    /**
     * version_major/version_minor defines are supplied here for temporary
     * source code compatibility. They will be removed in the next version.
     * ALL clients must convert to the new version format.
     */

    /**
     * The API version of the HAL module interface. This is meant to
     * version the hw_module_t, hw_module_methods_t, and hw_device_t
     * structures and definitions.
     *
     * The HAL interface owns this field. Module users/implementations
     * must NOT rely on this value for version information.
     *
     * Presently, 0 is the only valid value.
     */
    uint16_t hal_api_version;
#define version_minor hal_api_version

    /** Identifier of module */
    const char *id;

    /** Name of this module */
    const char *name;

    /** Author/owner/implementor of the module */
    const char *author;

    /** Modules methods */
    struct hw_module_methods_t* methods;

    /** module's dso */
    void* dso;

#ifdef __LP64__
    uint64_t reserved[32-7];
#else
    /** padding to 128 bytes, reserved for future use */
    uint32_t reserved[32-7];
#endif

} hw_module_t;

typedef struct hw_module_methods_t {
    /** Open a specific device */
    int (*open)(const struct hw_module_t* module, const char* id,
            struct hw_device_t** device);

} hw_module_methods_t;

/**
 * Every device data structure must begin with hw_device_t
 * followed by module specific public methods and attributes.
 */
typedef struct hw_device_t {
    /** tag must be initialized to HARDWARE_DEVICE_TAG */
    uint32_t tag;

    /**
     * Version of the module-specific device API. This value is used by
     * the derived-module user to manage different device implementations.
     *
     * The module user is responsible for checking the module_api_version
     * and device version fields to ensure that the user is capable of
     * communicating with the specific module implementation.
     *
     * One module can support multiple devices with different versions. This
     * can be useful when a device interface changes in an incompatible way
     * but it is still necessary to support older implementations at the same
     * time. One such example is the Camera 2.0 API.
     *
     * This field is interpreted by the module user and is ignored by the
     * HAL interface itself.
     */
    uint32_t version;

    /** reference to the module this device belongs to */
    struct hw_module_t* module;

    /** padding reserved for future use */
#ifdef __LP64__
    uint64_t reserved[12];
#else
    uint32_t reserved[12];
#endif

    /** Close this device */
    int (*close)(struct hw_device_t* device);

} hw_device_t;

struct audio_hw_device {
    /**
     * Common methods of the audio device.  This *must* be the first member of audio_hw_device
     * as users of this structure will cast a hw_device_t to audio_hw_device pointer in contexts
     * where it's known the hw_device_t references an audio_hw_device.
     */
    struct hw_device_t common;

    /**
     * check to see if the audio hardware interface has been initialized.
     * returns 0 on success, -ENODEV on failure.
     */
    int (*init_check)(const struct audio_hw_device *dev);

    /* mic mute */
    int (*set_mic_mute)(struct audio_hw_device *dev, bool state);
    int (*get_mic_mute)(const struct audio_hw_device *dev, bool *state);

    /**
     * set global audio parameters. The function accepts a list of parameter
     * key value pairs in the form: key1=value1;key2=value2;...
     */
    int (*set_parameters)(struct audio_hw_device *dev, const char *kv_pairs);

    char* (*get_parameters)(const struct audio_hw_device *dev,
                             const char *keys, char *out_values, int length_values);

    int (*get_parameters_int)(const struct audio_hw_device *dev,
                             const char *keys, int *out_value);

    /** This method creates and opens the audio hardware input stream */
    int (*open_input_stream)(struct audio_hw_device *dev,
                             audio_source_t source,
                             int sample_rate,
                             int channel_count,
                             struct audio_stream_in **stream_in);

    void (*close_input_stream)(struct audio_hw_device *dev,
                               struct audio_stream_in *stream_in);

    /** This method creates and opens the audio hardware output stream */
    int (*open_output_stream)(struct audio_hw_device *dev,
                             audio_device_out_t device,
                             int sample_rate,
                             int channel_count,
                             struct audio_stream_out **stream_out);

    void (*close_output_stream)(struct audio_hw_device *dev,
                               struct audio_stream_out *stream_out);
};
typedef struct audio_hw_device audio_hw_device_t;

/** convenience API for opening and closing a supported device */

static inline int load_audio_hw_device(audio_hw_device_t **dev)
{
    void *handle = NULL;
    struct hw_module_t *hmi = NULL;
    int rc;

    if (access(AISPEECH_AUDIO_HAL_LIBRARY_PATH, R_OK) != 0) {
        LOGW("load: module=%s not exist!", AISPEECH_AUDIO_HAL_LIBRARY_PATH);
        rc = -1;
        goto out;
    }

    /*
     * load the symbols resolving undefined symbols before
     * dlopen returns. Since RTLD_GLOBAL is not or'd in with
     * RTLD_NOW the external symbols will not be global
     */
    handle = dlopen(AISPEECH_AUDIO_HAL_LIBRARY_PATH, RTLD_NOW);
    if (handle == NULL) {
        char const *err_str = dlerror();
        LOGW("load: module=%s\n%s", AISPEECH_AUDIO_HAL_LIBRARY_PATH, err_str?err_str:"unknown");
        rc = -1;
        goto out;
    }

    /* Get the address of the struct hal_module_info. */
    hmi = (struct hw_module_t *)dlsym(handle, HAL_MODULE_INFO_SYM_AS_STR);
    if (hmi == NULL) {
        LOGW("load: couldn't find symbol %s", HAL_MODULE_INFO_SYM_AS_STR);
        rc = -1;
        goto out;
    }

    /* Check that the id matches */
    if (strcmp(AUDIO_HARDWARE_MODULE_ID, hmi->id) != 0) {
        LOGW("load: hmi->id(%s) != %s", hmi->id, AUDIO_HARDWARE_MODULE_ID);
        rc = -1;
        goto out;
    }
    hmi->dso = handle;

    rc = hmi->methods->open(hmi, AUDIO_HARDWARE_INTERFACE, (struct hw_device_t**)dev);
    if (rc) {
        LOGW("couldn't open audio hw device in %s.aispeech (%s)\n", AUDIO_HARDWARE_MODULE_ID, strerror(-rc));
        goto out;
    }

    if ((*dev)->common.version < AUDIO_DEVICE_API_VERSION_MIN) {
        LOGW("%s wrong audio hw device version %04x\n", __func__, (*dev)->common.version);
        rc = -1;
        goto out;
    }

    return 0;

out:
    if (handle != NULL) {
        dlclose(handle);
        handle = NULL;
    }
    *dev = NULL;
    return rc;
}

static inline int audio_hw_device_close(struct audio_hw_device* device)
{
    struct hw_module_t *hmi;
    int rc;

    hmi = (struct hw_module_t *)(device->common.module);
    rc = device->common.close(&device->common);

    if (hmi->dso != NULL) {
        dlclose(hmi->dso);
    }

    return rc;
}


#ifdef __cplusplus
}
#endif

#endif  // AISPEECH_AUDIO_HAL_INTERFACE_H
