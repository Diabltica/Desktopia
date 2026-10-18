#pragma once
// The few RP2040-specific services used by HmiApp, implemented in main.cpp (and by the host simulation).

// Invert (or not) the UART TX and RX pads: compensates an A/B swap in the cord (see HmiApp).
void hmiHwSetLinkInverted(bool inverted);

// Protects the screen content handed from core 0 (link) to core 1 (drawing).
void hmiHwLock();
void hmiHwUnlock();
