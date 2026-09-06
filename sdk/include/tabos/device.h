#ifndef TABOS_DEVICE_H
#define TABOS_DEVICE_H

#include <stdint.h>
#include <stddef.h>

enum {
    TABOS_DEVICE_NAME_MAX   = 15,
    TABOS_DEVICE_DRIVER_MAX = 31,
};

typedef uint32_t tabos_device_id_t;
typedef uint64_t tabos_device_features_t;
typedef int32_t tabos_device_subscription_t;

#define TABOS_DEVICE_ID_INVALID           UINT32_MAX
#define TABOS_DEVICE_SUBSCRIPTION_INVALID (-1)

#define TABOS_DEVICE_NAME_DISPLAY  "display0"
#define TABOS_DEVICE_NAME_KEYBOARD "keyboard0"
#define TABOS_DEVICE_NAME_STORAGE  "storage0"
#define TABOS_DEVICE_NAME_RTC      "rtc0"
#define TABOS_DEVICE_NAME_BATTERY  "battery0"
#define TABOS_DEVICE_NAME_IMU      "imu0"
#define TABOS_DEVICE_NAME_WIFI     "wifi0"
#define TABOS_DEVICE_NAME_AUDIO    "audio0"
#define TABOS_DEVICE_NAME_TOUCH    "touch0"
#define TABOS_DEVICE_NAME_CAMERA   "camera0"

enum {
    TABOS_DEVICE_FEATURE_DISPLAY_FRAMEBUFFER    = UINT64_C(1) << 0U,
    TABOS_DEVICE_FEATURE_KEYBOARD_INPUT         = UINT64_C(1) << 1U,
    TABOS_DEVICE_FEATURE_STORAGE_REMOVABLE      = UINT64_C(1) << 2U,
    TABOS_DEVICE_FEATURE_RTC_WALL_CLOCK         = UINT64_C(1) << 3U,
    TABOS_DEVICE_FEATURE_BATTERY_TELEMETRY      = UINT64_C(1) << 4U,
    TABOS_DEVICE_FEATURE_BATTERY_CHARGE_CONTROL = UINT64_C(1) << 5U,
    TABOS_DEVICE_FEATURE_NETWORK_WIFI           = UINT64_C(1) << 6U,
    TABOS_DEVICE_FEATURE_AUDIO_PLAYBACK         = UINT64_C(1) << 7U,
    TABOS_DEVICE_FEATURE_AUDIO_CAPTURE          = UINT64_C(1) << 8U,
    TABOS_DEVICE_FEATURE_AUDIO_SPEAKER          = UINT64_C(1) << 9U,
    TABOS_DEVICE_FEATURE_AUDIO_HEADPHONE        = UINT64_C(1) << 10U,
    TABOS_DEVICE_FEATURE_AUDIO_MICROPHONE       = UINT64_C(1) << 11U,
    TABOS_DEVICE_FEATURE_AUDIO_AEC              = UINT64_C(1) << 12U,
    TABOS_DEVICE_FEATURE_POINTER_TOUCH          = UINT64_C(1) << 13U,
    TABOS_DEVICE_FEATURE_POINTER_MULTICONTACT   = UINT64_C(1) << 14U,
    TABOS_DEVICE_FEATURE_POINTER_PRESSURE       = UINT64_C(1) << 15U,
    TABOS_DEVICE_FEATURE_CAMERA_CAPTURE         = UINT64_C(1) << 16U,
    TABOS_DEVICE_FEATURE_CAMERA_RAW             = UINT64_C(1) << 17U,
};

typedef enum {
    TABOS_DEVICE_CLASS_DISPLAY = 0,
    TABOS_DEVICE_CLASS_KEYBOARD,
    TABOS_DEVICE_CLASS_STORAGE,
    TABOS_DEVICE_CLASS_RTC,
    TABOS_DEVICE_CLASS_BATTERY,
    TABOS_DEVICE_CLASS_SENSOR,
    TABOS_DEVICE_CLASS_NETWORK,
    TABOS_DEVICE_CLASS_AUDIO,
    TABOS_DEVICE_CLASS_POINTER,
    TABOS_DEVICE_CLASS_CAMERA,
    TABOS_DEVICE_CLASS_EXPANSION,
    TABOS_DEVICE_CLASS_COUNT,
} tabos_device_class_t;

typedef enum {
    TABOS_DEVICE_READY = 0,
    TABOS_DEVICE_OFFLINE,
    TABOS_DEVICE_FAULT,
    TABOS_DEVICE_STATE_COUNT,
} tabos_device_state_t;

typedef struct {
        tabos_device_id_t id;
        tabos_device_class_t device_class;
        tabos_device_state_t state;
        tabos_device_features_t features;
        int32_t last_error;
        char name[TABOS_DEVICE_NAME_MAX + 1U];
        char driver[TABOS_DEVICE_DRIVER_MAX + 1U];
} tabos_device_info_t;

typedef enum {
    TABOS_DEVICE_EVENT_ADDED = 0,
    TABOS_DEVICE_EVENT_REMOVED,
    TABOS_DEVICE_EVENT_READY,
    TABOS_DEVICE_EVENT_OFFLINE,
    TABOS_DEVICE_EVENT_FAULT,
    TABOS_DEVICE_EVENT_TYPE_COUNT,
} tabos_device_event_type_t;

enum {
    TABOS_DEVICE_EVENT_OVERFLOW = 1U << 0U,
};

typedef struct {
        tabos_device_event_type_t type;
        uint32_t flags;
        tabos_device_info_t device;
} tabos_device_event_t;

size_t tabos_device_count(void);
int tabos_device_at(size_t index, tabos_device_info_t* info);
int tabos_device_get(tabos_device_id_t id, tabos_device_info_t* info);
int tabos_device_find(const char* name, tabos_device_info_t* info);
tabos_device_subscription_t tabos_device_subscribe(void);
int tabos_device_subscription_close(tabos_device_subscription_t subscription);
int tabos_device_event_read(tabos_device_subscription_t subscription, tabos_device_event_t* event);

#endif
