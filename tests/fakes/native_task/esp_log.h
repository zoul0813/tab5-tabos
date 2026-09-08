#pragma once
void fake_native_log(const char* tag, const char* format, ...);
#define ESP_LOGE(tag, ...) fake_native_log(tag, __VA_ARGS__)
