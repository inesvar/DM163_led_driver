#define MSGQ_CHECK_PERIOD_MS 100

/*
 * Blink period.
 * Values should be a multiple of MSGQ_CHECK_PERIOD_MS.
 */
typedef enum {
  FAST_BLINK=200,
  SLOW_BLINK=600,
} blink_half_period_ms_t;

extern void led_main();