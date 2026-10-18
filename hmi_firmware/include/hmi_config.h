#pragma once
/*
 * Desktopia HMI board – every tunable of the HMI firmware.
 * Any value can also be overridden from platformio.ini with -DNAME=value.
 */

/* ---------------------------------------------------------------------------
 *  Link to the motherboard (protocol: shared/hmi_link/hmi_protocol.h)
 * ------------------------------------------------------------------------- */
#ifndef HMI_TURNAROUND_US
#define HMI_TURNAROUND_US          100      // pause before answering: the motherboard has released the bus
#endif
#ifndef HMI_POLARITY_SWAP_MS
#define HMI_POLARITY_SWAP_MS       150      // no valid frame for this long → try the other A/B polarity
#endif                                      // (a standard, "reversed" telephone cord swaps A and B)
#ifndef HMI_NO_LINK_MS
#define HMI_NO_LINK_MS             500      // no valid frame for this long → "NO LINK" screen
#endif
#ifndef HMI_WATCHDOG_MS
#define HMI_WATCHDOG_MS            2000     // RP2040 watchdog, fed only while both cores make progress
#endif

/* ---------------------------------------------------------------------------
 *  Buttons
 * ------------------------------------------------------------------------- */
#ifndef HMI_DEBOUNCE_MS
#define HMI_DEBOUNCE_MS            15       // a button level must be stable this long to be reported
#endif

/* ---------------------------------------------------------------------------
 *  Display (round 240x240 SPI LCD, GC9A01)
 * ------------------------------------------------------------------------- */
#ifndef DISPLAY_ROTATION
#define DISPLAY_ROTATION           1        // 0..3 (1 = 90° clockwise)
#endif
#ifndef DISPLAY_SPI_HZ
#define DISPLAY_SPI_HZ             20000000UL
#endif
#ifndef DISPLAY_MIN_PERIOD_MS
#define DISPLAY_MIN_PERIOD_MS      25       // refresh limiter
#endif
#ifndef DISPLAY_DEADBAND_MM
#define DISPLAY_DEADBAND_MM        2        // ignore +-2 mm jitter on the number while the desk is still
#endif
