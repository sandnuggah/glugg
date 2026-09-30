#pragma once

// Picks the Thread channel for a new network from an energy scan. The channel is effectively fixed once sensors are
// paired, so it's worth avoiding a busy one.

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define THREAD_CHANNEL_FIRST 11
#define THREAD_CHANNEL_COUNT 16 // 11-26
// Channels within this many dB of the quietest one count as equally quiet.
#define THREAD_CHANNEL_MARGIN_DB 3

// max_rssi[i] is the loudest signal (dBm) seen on channel 11 + i; bit i of scanned is set if it was measured.
// Returns the quietest channel, preferring 15, 20 and 25 (between Wi-Fi channels 1, 6 and 11) when they are about as
// quiet, or -1 if nothing was scanned.
int thread_channel_pick(const int8_t max_rssi[THREAD_CHANNEL_COUNT], uint16_t scanned);

#ifdef __cplusplus
}
#endif
