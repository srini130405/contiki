#include "contiki.h"
#include "sensor.h"

#include "lib/random.h"
#include "sys/rtimer.h"
#include "dev/watchdog.h"


/* One sampling window, milli-g */
static int16_t win_x[WINDOW_SAMPLES];
static int16_t win_y[WINDOW_SAMPLES];
static int16_t win_z[WINDOW_SAMPLES];


/* =========================================================
 * Simulated accelerometer
 *
 * Every sample is random noise in [-amplitude, +amplitude] mg per axis.
 * RMS of such noise ~ amplitude / sqrt(3):
 *   healthy window: +-100 mg -> RMS ~58 mg  (below the 90 mg threshold)
 *   fault window:   +-300 mg -> RMS ~173 mg (above it -> ALERT)
 *
 * A window is a fault window with probability 1/SIM_FAULT_ONE_IN.
 * Windows are taken every 5 s (12 per minute), so 1 in 12 gives on
 * average one ALERT per minute.
 * ========================================================= */

#define SIM_HEALTHY_MG    100
#define SIM_FAULT_MG      300
#define SIM_FAULT_ONE_IN  12

/* Random value in [-amp, +amp] */
static int16_t
noise(int16_t amp)
{
  return (int16_t)(random_rand() % (2 * amp + 1)) - amp;
}

/*
 * PLACEHOLDER: no accelerometer driver is wired up yet. Replace the
 * body with the real 3-axis driver read, returning milli-g.
 */
static void
accel_read_xyz(int16_t amp, int16_t *x, int16_t *y, int16_t *z)
{
  *x = noise(amp);
  *y = noise(amp);
  *z = noise(amp);
}

/*
 * Busy-waits for RMS_WINDOW_MS; TSCH runs from rtimer interrupts so it
 * keeps working.
 */
void
sensor_sample_window(void)
{
  const rtimer_clock_t period = RTIMER_SECOND / SAMPLE_RATE_HZ;
  rtimer_clock_t next = RTIMER_NOW();
  int16_t amp = (random_rand() % SIM_FAULT_ONE_IN == 0) ?
                SIM_FAULT_MG : SIM_HEALTHY_MG;
  uint16_t i;

  for(i = 0; i < WINDOW_SAMPLES; i++) {
    while(RTIMER_CLOCK_LT(RTIMER_NOW(), next)) {
      watchdog_periodic();
    }
    accel_read_xyz(amp, &win_x[i], &win_y[i], &win_z[i]);
    next += period;
  }
}

void
sensor_get_sample(uint16_t i, int16_t xyz[3])
{
  xyz[0] = win_x[i];
  xyz[1] = win_y[i];
  xyz[2] = win_z[i];
}


/* =========================================================
 * RMS
 * ========================================================= */

static uint32_t
isqrt32(uint32_t v)
{
  uint32_t res = 0;
  uint32_t bit = 1UL << 30;

  while(bit > v) {
    bit >>= 2;
  }
  while(bit != 0) {
    if(v >= res + bit) {
      v -= res + bit;
      res = (res >> 1) + bit;
    } else {
      res >>= 1;
    }
    bit >>= 2;
  }
  return res;
}

/* RMS of the AC part (mean removed, so gravity does not dominate) */
static uint16_t
axis_rms(const int16_t *s)
{
  int32_t sum = 0;
  int32_t mean;
  uint64_t sum_sq = 0;
  uint16_t i;

  for(i = 0; i < WINDOW_SAMPLES; i++) {
    sum += s[i];
  }
  mean = sum / WINDOW_SAMPLES;

  for(i = 0; i < WINDOW_SAMPLES; i++) {
    int32_t d = s[i] - mean;
    sum_sq += (uint64_t)((int64_t)d * d);
  }
  return (uint16_t)isqrt32((uint32_t)(sum_sq / WINDOW_SAMPLES));
}

void
sensor_window_rms(uint16_t rms[3])
{
  rms[0] = axis_rms(win_x);
  rms[1] = axis_rms(win_y);
  rms[2] = axis_rms(win_z);
}