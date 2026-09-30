#include "thread_channel.h"

#include <stdbool.h>

static bool preferred(int channel)
{
    return channel == 15 || channel == 20 || channel == 25;
}

int thread_channel_pick(const int8_t max_rssi[THREAD_CHANNEL_COUNT], uint16_t scanned)
{
    int quietest = -1;
    for (int i = 0; i < THREAD_CHANNEL_COUNT; i++) {
        if ((scanned >> i & 1) && (quietest < 0 || max_rssi[i] < max_rssi[quietest])) {
            quietest = i;
        }
    }
    if (quietest < 0) {
        return -1;
    }
    int best = -1;
    for (int i = 0; i < THREAD_CHANNEL_COUNT; i++) {
        int channel = THREAD_CHANNEL_FIRST + i;
        if ((scanned >> i & 1) && preferred(channel) && max_rssi[i] <= max_rssi[quietest] + THREAD_CHANNEL_MARGIN_DB &&
            (best < 0 || max_rssi[i] < max_rssi[best])) {
            best = i;
        }
    }
    return THREAD_CHANNEL_FIRST + (best >= 0 ? best : quietest);
}
