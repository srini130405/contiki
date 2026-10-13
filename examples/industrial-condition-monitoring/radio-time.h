#ifndef RADIO_TIME_H_
#define RADIO_TIME_H_

#include <stdint.h>

/* Radio states for duration measurement only */
typedef enum {
  RADIO_TIME_OFF = 0,
  RADIO_TIME_ON = 1,
  RADIO_TIME_STATE_COUNT
} radio_time_state_t;

/* Initialize once, before TSCH radio activity starts */
void radio_time_init(void);

/* Record a commanded radio state change */
void radio_time_set(radio_time_state_t state);

/* Print elapsed time, ON/OFF durations, and transition counts */
void radio_time_report(void);

void radio_time_reset(void);

#endif /* RADIO_TIME_H_ */