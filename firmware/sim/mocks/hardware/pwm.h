#pragma once
#include "hardware/gpio.h"
typedef struct { uint32_t div; uint16_t top; } pwm_config;
inline uint pwm_gpio_to_slice_num(uint gpio) { return (gpio >> 1) & 7u; }
inline uint pwm_gpio_to_channel(uint gpio) { return gpio & 1u; }
inline pwm_config pwm_get_default_config() { pwm_config c = {1, 0xFFFF}; return c; }
inline void pwm_config_set_clkdiv_int(pwm_config* c, uint32_t div) { c->div = div; }
inline void pwm_config_set_wrap(pwm_config* c, uint16_t wrap) { c->top = wrap; }
void pwm_init(uint slice, pwm_config* c, bool start);
void pwm_set_gpio_level(uint gpio, uint16_t level);
void pwm_set_enabled(uint slice, bool enabled);
