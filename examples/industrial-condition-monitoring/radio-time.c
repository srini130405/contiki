#include "radio-time.h"

#include "sys/rtimer.h"

#include <stdio.h>
#include <stdint.h>

/*
 * Measurement state.
 *
 * radio_time_set() updates the counters when the commanded radio
 * state changes. The application only reads a consistent snapshot
 * when reporting.
 */
static volatile uint64_t ticks_in[RADIO_TIME_STATE_COUNT];

static volatile uint32_t on_transitions;
static volatile uint32_t off_transitions;

static volatile radio_time_state_t current_state = RADIO_TIME_OFF;
static volatile rtimer_clock_t last_change;
static volatile uint8_t initialized;
static volatile uint32_t sequence;

/* Compiler memory barrier for the snapshot protocol. */
#define RADIO_TIME_BARRIER() __asm__ volatile ("" ::: "memory")

/*---------------------------------------------------------------------------*/
void
radio_time_init(void)
{
  int i;

  sequence = 0;

  for(i = 0; i < RADIO_TIME_STATE_COUNT; i++) {
    ticks_in[i] = 0;
  }

  on_transitions = 0;
  off_transitions = 0;

  current_state = RADIO_TIME_OFF;
  last_change = RTIMER_NOW();
  initialized = 1;
}

void radio_time_reset(void)
{
  rtimer_clock_t now = RTIMER_NOW();

  sequence++;
  RADIO_TIME_BARRIER();

  ticks_in[RADIO_TIME_ON] = 0;
  ticks_in[RADIO_TIME_OFF] = 0;
  on_transitions = 0;
  off_transitions = 0;
  current_state = RADIO_TIME_OFF;
  last_change = now;

  RADIO_TIME_BARRIER();
  sequence++;
}

/*---------------------------------------------------------------------------*/
void
radio_time_set(radio_time_state_t state)
{
  rtimer_clock_t now;

  if(!initialized || state >= RADIO_TIME_STATE_COUNT) {
    return;
  }

  now = RTIMER_NOW();

  /* Mark the update as in progress. */
  sequence++;
  RADIO_TIME_BARRIER();

  /* Repeated calls for the same state are not transitions. */
  if(state == current_state) {
    RADIO_TIME_BARRIER();
    sequence++;
    return;
  }

  /*
   * Close the previous state interval.
   * Unsigned subtraction handles one RTIMER counter wrap.
   */
  ticks_in[current_state] +=
      (rtimer_clock_t)(now - last_change);

  if(state == RADIO_TIME_ON) {
    on_transitions++;
  } else {
    off_transitions++;
  }

  current_state = state;
  last_change = now;

  RADIO_TIME_BARRIER();
  sequence++;
}
/*---------------------------------------------------------------------------*/
static uint64_t
ticks_to_us(uint64_t ticks)
{
  uint64_t seconds;
  uint64_t remainder;

  seconds = ticks / RTIMER_SECOND;
  remainder = ticks % RTIMER_SECOND;

  return seconds * 1000000ULL
       + (remainder * 1000000ULL) / RTIMER_SECOND;
}
/*---------------------------------------------------------------------------*/
void
radio_time_report(void)
{
  uint64_t ticks[RADIO_TIME_STATE_COUNT];
  uint64_t time_us[RADIO_TIME_STATE_COUNT];
  uint64_t total_us;
  uint64_t on_pct10;
  uint64_t off_pct10;

  uint32_t on_count;
  uint32_t off_count;
  uint32_t s;

  radio_time_state_t state;
  rtimer_clock_t last;
  rtimer_clock_t now;

  int i;

  if(!initialized) {
    printf("[RADIO-TIME] Measurement not initialized\n");
    return;
  }

  /*
   * Take a consistent snapshot.
   * If a TSCH interrupt updates the measurement during the copy,
   * retry rather than use partially updated 64-bit counters.
   */
  do {
    s = sequence;
    RADIO_TIME_BARRIER();

    for(i = 0; i < RADIO_TIME_STATE_COUNT; i++) {
      ticks[i] = ticks_in[i];
    }

    on_count = on_transitions;
    off_count = off_transitions;
    state = current_state;
    last = last_change;

    now = RTIMER_NOW();

    RADIO_TIME_BARRIER();
  } while((s & 1U) || s != sequence);

  /*
   * Include the currently open state interval without modifying
   * the shared counters.
   */
  ticks[state] += (rtimer_clock_t)(now - last);

  total_us = 0;

  for(i = 0; i < RADIO_TIME_STATE_COUNT; i++) {
    time_us[i] = ticks_to_us(ticks[i]);
    total_us += time_us[i];
  }

  if(total_us == 0) {
    printf("[RADIO-TIME] No elapsed time recorded\n");
    return;
  }

  /* Percentages with one decimal place, in units of 0.1%. */
  on_pct10 = time_us[RADIO_TIME_ON] * 1000ULL / total_us;
  off_pct10 = time_us[RADIO_TIME_OFF] * 1000ULL / total_us;

  printf("\n[RADIO-TIME] Elapsed: %lu.%03lu s\n",
         (unsigned long)(total_us / 1000000ULL),
         (unsigned long)((total_us % 1000000ULL) / 1000ULL));

  printf("[RADIO-TIME] ON:  %lu.%03lu s (%lu.%lu%%)\n",
         (unsigned long)(time_us[RADIO_TIME_ON] / 1000000ULL),
         (unsigned long)((time_us[RADIO_TIME_ON] % 1000000ULL) / 1000ULL),
         (unsigned long)(on_pct10 / 10ULL),
         (unsigned long)(on_pct10 % 10ULL));

  printf("[RADIO-TIME] OFF: %lu.%03lu s (%lu.%lu%%)\n",
         (unsigned long)(time_us[RADIO_TIME_OFF] / 1000000ULL),
         (unsigned long)((time_us[RADIO_TIME_OFF] % 1000000ULL) / 1000ULL),
         (unsigned long)(off_pct10 / 10ULL),
         (unsigned long)(off_pct10 % 10ULL));

  printf("[RADIO-TIME] ON transitions:  %lu\n",
         (unsigned long)on_count);

  printf("[RADIO-TIME] OFF transitions: %lu\n\n",
         (unsigned long)off_count);
}
/*---------------------------------------------------------------------------*/