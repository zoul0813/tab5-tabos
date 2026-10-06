#pragma once

void fake_display_log(const char* tag, const char* format, ...);
#define ESP_LOGE(tag, ...) fake_display_log(tag, __VA_ARGS__)
