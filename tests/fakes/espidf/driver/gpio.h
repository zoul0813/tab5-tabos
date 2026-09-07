#pragma once

typedef int esp_err_t;
typedef int gpio_num_t;
typedef void (*gpio_isr_t)(void* context);
enum {
    ESP_OK                = 0,
    ESP_FAIL              = -1,
    ESP_ERR_NO_MEM        = 0x101,
    ESP_ERR_INVALID_STATE = 0x103
};
esp_err_t gpio_install_isr_service(int flags);
esp_err_t gpio_isr_handler_add(gpio_num_t pin, gpio_isr_t handler, void* context);
esp_err_t gpio_isr_handler_remove(gpio_num_t pin);
