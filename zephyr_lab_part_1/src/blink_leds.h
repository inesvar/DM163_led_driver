#define MSGQ_CHECK_PERIOD_MS 100

/*
 * Blink period.
 * Values should be a multiple of MSGQ_CHECK_PERIOD_MS.
 * Any value higher than 2^15 means no blink.
 */
typedef enum {
  VERY_FAST_BLINK = 100,    // 10 Hz
  FAST_BLINK = 200,         // 5 Hz
  REGULAR_BLINK = 500,      // 2 Hz
  SLOW_BLINK = 1000,        // 1 Hz
  VERY_SLOW_BLINK = 33000,  // one toggle per hour
} blink_half_period_ms_t;

extern void led_main();