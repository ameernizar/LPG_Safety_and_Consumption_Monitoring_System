#ifndef SENSOR_HEALTH_H
#define SENSOR_HEALTH_H
#include <stdint.h>

/* Software freshness/debounce policy, not sensor accuracy or alarm criteria.
 * Wrap-safe for continuously serviced uint32_t millisecond ticks. */
#define TEMPERATURE_MAX_AGE_MS 2500U
#define FLAME_STABLE_MS 50U
#define FLAME_MAX_GAP_MS 100U
static inline uint8_t SensorSampleFresh(uint8_t valid, uint32_t sampled,
                                        uint32_t now, uint32_t max_age)
{
    return valid && (uint32_t)(now-sampled) < max_age;
}
typedef struct {
    uint32_t candidate_since, last_sample;
    uint8_t candidate, high, valid, initialized;
} FlameFilter;
/* No GPIO ownership or HIGH/LOW detection polarity. A long unsampled interval
 * restarts qualification; duplicate timestamps cannot qualify a candidate. */
static inline void FlameFilter_Update(FlameFilter *f, uint8_t high, uint32_t now)
{
    high = !!high;
    if (f->initialized && now == f->last_sample) return;
    if (!f->initialized || (uint32_t)(now-f->last_sample) >= FLAME_MAX_GAP_MS ||
        high != f->candidate) {
        f->candidate = high; f->candidate_since = now; f->valid = 0;
    }
    f->initialized = 1; f->last_sample = now;
    if ((uint32_t)(now-f->candidate_since) >= FLAME_STABLE_MS) {
        f->high = high; f->valid = 1;
    }
}
static inline uint8_t FlameFilter_Valid(const FlameFilter *f, uint32_t now)
{
    return SensorSampleFresh(f->valid, f->last_sample, now, FLAME_MAX_GAP_MS);
}
#endif
