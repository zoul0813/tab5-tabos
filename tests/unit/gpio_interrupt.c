#include "gpio_interrupt.h"

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>

static unsigned int installs;
static esp_err_t install_result = ESP_ERR_NO_MEM;
static esp_err_t add_result     = ESP_OK;
static gpio_isr_t handlers[55];
static void* contexts[55];

esp_err_t gpio_install_isr_service(int flags)
{
    assert(flags == 0);
    ++installs;
    return install_result;
}

esp_err_t gpio_isr_handler_add(gpio_num_t pin, gpio_isr_t handler, void* context)
{
    assert(pin >= 0 && pin < 55);
    if (add_result == ESP_OK) {
        assert(handlers[pin] == NULL);
        handlers[pin] = handler;
        contexts[pin] = context;
    }
    return add_result;
}

esp_err_t gpio_isr_handler_remove(gpio_num_t pin)
{
    handlers[pin] = NULL;
    contexts[pin] = NULL;
    return ESP_OK;
}

static void interrupt(void* context)
{
    ++*(unsigned int*) context;
}

int main(void)
{
    unsigned int keyboard = 0U;
    unsigned int touch    = 0U;
    /* Failed installation must not admit a handler or poison a later retry. */
    assert(tab5_gpio_interrupt_add(50, interrupt, &keyboard) == ESP_ERR_NO_MEM);
    assert(installs == 1U && handlers[50] == NULL);
    /* An unknown external owner is an ownership error, not claimed as ours. */
    install_result = ESP_ERR_INVALID_STATE;
    assert(tab5_gpio_interrupt_add(23, interrupt, &touch) == ESP_ERR_INVALID_STATE);
    assert(handlers[23] == NULL);
    install_result = ESP_OK;
    assert(tab5_gpio_interrupt_add(50, interrupt, &keyboard) == ESP_OK);
    const unsigned int successful_install_count = installs;
    add_result                                  = ESP_FAIL;
    assert(tab5_gpio_interrupt_add(23, interrupt, &touch) == ESP_FAIL);
    assert(handlers[50] != NULL && handlers[23] == NULL);
    add_result = ESP_OK;
    assert(tab5_gpio_interrupt_add(23, interrupt, &touch) == ESP_OK);
    handlers[50](contexts[50]);
    handlers[23](contexts[23]);
    assert(keyboard == 1U && touch == 1U);
    assert(gpio_isr_handler_remove(50) == ESP_OK);
    handlers[23](contexts[23]);
    assert(touch == 2U);
    assert(gpio_isr_handler_remove(23) == ESP_OK);
    /* Reinitialization in reverse order reuses the boot-lifetime service. */
    assert(tab5_gpio_interrupt_add(23, interrupt, &touch) == ESP_OK);
    assert(tab5_gpio_interrupt_add(50, interrupt, &keyboard) == ESP_OK);
    assert(installs == successful_install_count);
    return 0;
}
