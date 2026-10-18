#ifndef MQ6_MONITOR_H
#define MQ6_MONITOR_H
#include <stdint.h>

/* Experimental software settings, NOT heater preconditioning or gas calibration.
 * Samples arrive about once per second. Override here or via compiler defines. */
#ifndef MQ6_SETTLING_MS
#define MQ6_SETTLING_MS 60000U
#endif
#ifndef MQ6_BASELINE_SAMPLES
#define MQ6_BASELINE_SAMPLES 32U
#endif
#ifndef MQ6_EMA_SHIFT
#define MQ6_EMA_SHIFT 3U /* alpha = 1 / 2^shift; zero disables smoothing */
#endif
#if MQ6_SETTLING_MS > 2147483647U || MQ6_BASELINE_SAMPLES < 1 || MQ6_BASELINE_SAMPLES > 1024 || MQ6_EMA_SHIFT > 8
#error "MQ6 monitor configuration out of bounds"
#endif

typedef enum {
    MQ6_MON_WARMUP, MQ6_MON_BASELINE, MQ6_MON_MONITORING,
    MQ6_MON_ACQ_ERROR, MQ6_MON_SAT_LOW, MQ6_MON_SAT_HIGH
} MQ6_MonitorStatus;

typedef struct {
    MQ6_MonitorStatus status;
    uint32_t settling_started_ms;
    uint32_t baseline_sum;
    int32_t filtered_q8;
    uint16_t raw, filtered, baseline, baseline_count;
    int16_t deviation;
    uint8_t raw_valid, valid, baseline_valid, settled, filter_seeded;
} MQ6_Monitor;

void MQ6_MonitorInit(MQ6_Monitor *monitor, uint32_t now_ms);
/* raw is the existing 16-conversion batch average, not a single conversion.
 * batch_saturated flags ANY conversion at either ADC rail. No HAL or delays. */
void MQ6_MonitorUpdate(MQ6_Monitor *monitor, uint32_t now_ms,
                       uint8_t acquired, uint16_t raw, uint8_t batch_saturated);
const char *MQ6_MonitorLabel(MQ6_MonitorStatus status);
#endif
