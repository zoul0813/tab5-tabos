#include <tabos/device.h>

#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static const char* class_name(tabos_device_class_t device_class)
{
    switch (device_class) {
        case TABOS_DEVICE_CLASS_DISPLAY: return "display";
        case TABOS_DEVICE_CLASS_KEYBOARD: return "keyboard";
        case TABOS_DEVICE_CLASS_STORAGE: return "storage";
        case TABOS_DEVICE_CLASS_RTC: return "rtc";
        case TABOS_DEVICE_CLASS_BATTERY: return "battery";
        case TABOS_DEVICE_CLASS_SENSOR: return "sensor";
        case TABOS_DEVICE_CLASS_NETWORK: return "network";
        case TABOS_DEVICE_CLASS_AUDIO: return "audio";
        case TABOS_DEVICE_CLASS_POINTER: return "pointer";
        case TABOS_DEVICE_CLASS_CAMERA: return "camera";
        case TABOS_DEVICE_CLASS_EXPANSION: return "expansion";
        case TABOS_DEVICE_CLASS_COUNT: break;
    }
    return "unknown";
}

static const char* state_name(tabos_device_state_t state)
{
    switch (state) {
        case TABOS_DEVICE_READY: return "ready";
        case TABOS_DEVICE_OFFLINE: return "offline";
        case TABOS_DEVICE_FAULT: return "fault";
        case TABOS_DEVICE_STATE_COUNT: break;
    }
    return "unknown";
}

static void usage(FILE* stream)
{
    fputs("Usage: devices\n", stream);
}

static void print_feature(bool* printed, tabos_device_features_t features, tabos_device_features_t feature,
                          const char* name)
{
    if ((features & feature) == 0U) {
        return;
    }
    printf("%s%s", *printed ? ", " : "", name);
    *printed = true;
}

static void print_features(tabos_device_features_t features)
{
    const tabos_device_features_t known =
        TABOS_DEVICE_FEATURE_DISPLAY_FRAMEBUFFER | TABOS_DEVICE_FEATURE_KEYBOARD_INPUT |
        TABOS_DEVICE_FEATURE_STORAGE_REMOVABLE | TABOS_DEVICE_FEATURE_RTC_WALL_CLOCK |
        TABOS_DEVICE_FEATURE_BATTERY_TELEMETRY | TABOS_DEVICE_FEATURE_BATTERY_CHARGE_CONTROL |
        TABOS_DEVICE_FEATURE_NETWORK_WIFI | TABOS_DEVICE_FEATURE_AUDIO_PLAYBACK | TABOS_DEVICE_FEATURE_AUDIO_CAPTURE |
        TABOS_DEVICE_FEATURE_AUDIO_SPEAKER | TABOS_DEVICE_FEATURE_AUDIO_HEADPHONE |
        TABOS_DEVICE_FEATURE_AUDIO_MICROPHONE | TABOS_DEVICE_FEATURE_AUDIO_AEC | TABOS_DEVICE_FEATURE_POINTER_TOUCH |
        TABOS_DEVICE_FEATURE_POINTER_MULTICONTACT | TABOS_DEVICE_FEATURE_POINTER_PRESSURE;
    bool printed = false;
    fputs("   features: ", stdout);
    print_feature(&printed, features, TABOS_DEVICE_FEATURE_DISPLAY_FRAMEBUFFER, "framebuffer");
    print_feature(&printed, features, TABOS_DEVICE_FEATURE_KEYBOARD_INPUT, "input");
    print_feature(&printed, features, TABOS_DEVICE_FEATURE_STORAGE_REMOVABLE, "removable");
    print_feature(&printed, features, TABOS_DEVICE_FEATURE_RTC_WALL_CLOCK, "wall-clock");
    print_feature(&printed, features, TABOS_DEVICE_FEATURE_BATTERY_TELEMETRY, "telemetry");
    print_feature(&printed, features, TABOS_DEVICE_FEATURE_BATTERY_CHARGE_CONTROL, "charge-control");
    print_feature(&printed, features, TABOS_DEVICE_FEATURE_NETWORK_WIFI, "wifi");
    print_feature(&printed, features, TABOS_DEVICE_FEATURE_AUDIO_PLAYBACK, "playback");
    print_feature(&printed, features, TABOS_DEVICE_FEATURE_AUDIO_CAPTURE, "capture");
    print_feature(&printed, features, TABOS_DEVICE_FEATURE_AUDIO_SPEAKER, "speaker");
    print_feature(&printed, features, TABOS_DEVICE_FEATURE_AUDIO_HEADPHONE, "headphone");
    print_feature(&printed, features, TABOS_DEVICE_FEATURE_AUDIO_MICROPHONE, "microphone");
    print_feature(&printed, features, TABOS_DEVICE_FEATURE_AUDIO_AEC, "aec");
    print_feature(&printed, features, TABOS_DEVICE_FEATURE_POINTER_TOUCH, "touch");
    print_feature(&printed, features, TABOS_DEVICE_FEATURE_POINTER_MULTICONTACT, "multicontact");
    print_feature(&printed, features, TABOS_DEVICE_FEATURE_POINTER_PRESSURE, "pressure");
    const tabos_device_features_t unknown = features & ~known;
    if (unknown != 0U) {
        printf("%s0x%" PRIx64, printed ? ", " : "", unknown);
        printed = true;
    }
    if (!printed) {
        fputs("none", stdout);
    }
    putchar('\n');
}

int main(int argc, char** argv)
{
    if (argc == 2 && (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0)) {
        usage(stdout);
        return 0;
    }
    if (argc != 1) {
        usage(stderr);
        return 1;
    }

    const size_t count = tabos_device_count();
    for (size_t index = 0U; index < count; ++index) {
        tabos_device_info_t info;
        if (tabos_device_at(index, &info) != 0) {
            fprintf(stderr, "devices: cannot read device %lu: %s\n", (unsigned long) index, strerror(errno));
            return 1;
        }
        printf("%" PRIu32 " %s (%s, %s)\n", info.id, info.name, class_name(info.device_class), state_name(info.state));
        printf("   driver: %s\n", info.driver);
        print_features(info.features);
        if (info.last_error != 0) {
            printf("   error: %" PRId32 "\n", info.last_error);
        }
    }
    return 0;
}
