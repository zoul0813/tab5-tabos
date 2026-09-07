#include "gpio_interrupt.h"

#include <stdbool.h>

static bool service_installed;

esp_err_t tab5_gpio_interrupt_add(gpio_num_t pin, gpio_isr_t handler, void* context)
{
    if (!service_installed) {
        const esp_err_t result = gpio_install_isr_service(0);
        if (result != ESP_OK) {
            return result;
        }
        service_installed = true;
    }
    return gpio_isr_handler_add(pin, handler, context);
}
