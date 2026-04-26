#include <driver/gpio.h>

namespace audio::pa_ctrl {

void init(gpio_num_t pin) {
    gpio_config_t c = {};
    c.pin_bit_mask = 1ULL << pin;
    c.mode = GPIO_MODE_OUTPUT;
    c.pull_up_en = GPIO_PULLUP_DISABLE;
    c.pull_down_en = GPIO_PULLDOWN_DISABLE;
    c.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&c);
    gpio_set_level(pin, 0);  // amp off at boot
}
void enable(gpio_num_t pin)  { gpio_set_level(pin, 1); }
void disable(gpio_num_t pin) { gpio_set_level(pin, 0); }

}  // namespace audio::pa_ctrl
