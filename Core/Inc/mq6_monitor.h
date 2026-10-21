#ifndef MQ6_MONITOR_H
#define MQ6_MONITOR_H
#include <stdint.h>

/* Experimental ADC-count criteria, NOT LPG thresholds or heater conditioning.
 * Samples are existing 16-conversion batch averages, approximately 1 Hz. */
#ifndef MQ6_SETTLING_MS
#define MQ6_SETTLING_MS 60000U
#endif
#ifndef MQ6_BASELINE_SAMPLES
#define MQ6_BASELINE_SAMPLES 32U
#endif
#ifndef MQ6_EMA_SHIFT
#define MQ6_EMA_SHIFT 3U
#endif
#ifndef MQ6_STABILITY_SAMPLES
#define MQ6_STABILITY_SAMPLES 16U
#endif
#ifndef MQ6_STABILITY_SPAN_MS
#define MQ6_STABILITY_SPAN_MS 15000U
#endif
#ifndef MQ6_MAX_SAMPLE_GAP_MS
#define MQ6_MAX_SAMPLE_GAP_MS 2500U
#endif
#ifndef MQ6_STABILITY_RANGE
#define MQ6_STABILITY_RANGE 12U
#endif
#ifndef MQ6_STABILITY_DRIFT
#define MQ6_STABILITY_DRIFT 4U
#endif
#ifndef MQ6_CAPTURE_RANGE
#define MQ6_CAPTURE_RANGE 12U
#endif
#ifndef MQ6_CAPTURE_DRIFT
#define MQ6_CAPTURE_DRIFT 4U
#endif
#if MQ6_SETTLING_MS > 2147483647U || MQ6_EMA_SHIFT > 8 || \
    MQ6_BASELINE_SAMPLES < 2 || MQ6_BASELINE_SAMPLES > 1024 || (MQ6_BASELINE_SAMPLES % 2) || \
    MQ6_STABILITY_SAMPLES < 2 || MQ6_STABILITY_SAMPLES > 64 || (MQ6_STABILITY_SAMPLES % 2) || \
    MQ6_STABILITY_SPAN_MS < 1 || MQ6_STABILITY_SPAN_MS > 2147483647U || \
    MQ6_MAX_SAMPLE_GAP_MS < 1 || MQ6_MAX_SAMPLE_GAP_MS > 2147483647U || \
    MQ6_STABILITY_RANGE > 4095 || MQ6_STABILITY_DRIFT > 4095 || \
    MQ6_CAPTURE_RANGE > 4095 || MQ6_CAPTURE_DRIFT > 4095
#error "MQ6 monitor configuration out of bounds"
#endif
#if MQ6_STABILITY_SPAN_MS > ((MQ6_STABILITY_SAMPLES - 1ULL) * MQ6_MAX_SAMPLE_GAP_MS)
#error "MQ6 stability span cannot fit within the allowed sample gaps"
#endif

typedef enum {
    MQ6_MON_WARMUP, MQ6_MON_BASELINE, MQ6_MON_MONITORING,
    MQ6_MON_ACQ_ERROR, MQ6_MON_SAT_LOW, MQ6_MON_SAT_HIGH,
    MQ6_MON_STABILIZING
} MQ6_MonitorStatus;

typedef struct {
    MQ6_MonitorStatus status;
    uint32_t settling_started_ms, baseline_sum;
    int32_t filtered_q8;
    uint16_t raw, filtered, baseline, baseline_count;
    int16_t deviation;
    uint8_t raw_valid, valid, baseline_valid, settled, filter_seeded;
    uint32_t sample_ticks[MQ6_STABILITY_SAMPLES];
    uint16_t samples[MQ6_STABILITY_SAMPLES];
    uint32_t last_sample_ms, capture_first_half_sum;
    uint16_t capture_min, capture_max;
    uint8_t window_count, window_next, last_sample_valid, capturing;
} MQ6_Monitor;

/* Call on actual initialization or a known sensor power interruption.
 * The firmware cannot detect heater power loss from ADC readings alone. */
void MQ6_MonitorInit(MQ6_Monitor *monitor, uint32_t now_ms);
/* Explicit API only, not connected to a button or automatic caller.
 * Invalidates baseline/delta and qualification; retains startup settling deadline
 * and live EMA. If warm-up is unfinished, its original remaining time applies. */
void MQ6_MonitorRequestBaseline(MQ6_Monitor *monitor, uint32_t now_ms);
/* Invalid samples reset qualification, not the minimum settling timer.
 * Duplicate timestamps do not count. Caller must service more often than one
 * full uint32_t tick wrap. batch_saturated: bit0 low rail, bit1 high rail. */
void MQ6_MonitorUpdate(MQ6_Monitor *monitor, uint32_t now_ms,
                       uint8_t acquired, uint16_t raw, uint8_t batch_saturated);
const char *MQ6_MonitorLabel(MQ6_MonitorStatus status);
#endif
