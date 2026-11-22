#pragma once
// RP2040 SDK stand-in (simulation only): the state lives in sim.cpp
#include <stdint.h>
typedef unsigned int uint;
enum gpio_function { GPIO_FUNC_XIP = 0, GPIO_FUNC_I2C = 3, GPIO_FUNC_PWM = 4, GPIO_FUNC_SIO = 5, GPIO_FUNC_NULL = 0x1f };
enum gpio_override { GPIO_OVERRIDE_NORMAL = 0, GPIO_OVERRIDE_INVERT = 1, GPIO_OVERRIDE_LOW = 2, GPIO_OVERRIDE_HIGH = 3 };
void gpio_set_function(uint gpio, enum gpio_function fn);
void gpio_set_outover(uint gpio, uint value);
void gpio_put(uint gpio, bool value);
inline void gpio_pull_up(uint) {}
inline void gpio_disable_pulls(uint) {}
