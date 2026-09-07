#pragma once

#include <driver/gpio.h>

/* Boot/task-context registration is serialized by the platform lifecycle.
 * The non-IRAM GPIO service lives for the entire boot. Consumers remove only
 * their own handlers; no consumer may uninstall the shared service.
 */
esp_err_t tab5_gpio_interrupt_add(gpio_num_t pin, gpio_isr_t handler, void* context);
