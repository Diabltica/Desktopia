#pragma once
#include <stdint.h>
typedef struct repeating_timer repeating_timer_t;
typedef bool (*repeating_timer_callback_t)(repeating_timer_t* rt);
struct repeating_timer {                 // simulation: run from Rig::step() every simulated millisecond
  uint32_t period_ms;
  uint32_t next_ms;
  repeating_timer_callback_t callback;
  void* user_data;
};
bool add_repeating_timer_ms(int32_t delay_ms, repeating_timer_callback_t callback, void* user_data, repeating_timer_t* out);
