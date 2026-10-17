#pragma once

#include <sdkconfig.h>

#ifndef CONFIG_ESP_MATTER_ENABLE_MATTER_SERVER

#ifdef CONFIG_ESP_MATTER_COMMISSIONER_ENABLE
// Enable or disable whether this device advertises as a commissioner.
#define CHIP_DEVICE_CONFIG_ENABLE_COMMISSIONER_DISCOVERY 1
#endif // CONFIG_ESP_MATTER_COMMISSIONER_ENABLE

// Up to 64 sensors (one per LED), each with its own CASE session and subscription. These pools are static and the
// build places static data in PSRAM, so 64 sensors cost about 123 KB of PSRAM and no internal RAM (measured).
// Number of devices a controller can be simultaneously connected to.
#define CHIP_CONFIG_CONTROLLER_MAX_ACTIVE_DEVICES 64
// 64 sensors + a pairing + spare. The default (3 per fabric + 2) is 17.
#define CHIP_CONFIG_SECURE_SESSION_POOL_SIZE 72
// One session-resumption entry per sensor, so reconnecting after a reboot skips the full CASE handshake.
#define CHIP_CONFIG_CASE_SESSION_RESUME_CACHE_SIZE 64

// CASE handshakes with up to 8 sensors at once (sensor_mgr caps it), plus a pairing (PASE). The default 4 runs out.
#define CHIP_CONFIG_UNAUTHENTICATED_CONNECTION_POOL_SIZE 10

// A sensor registers with our SRP server a few seconds after Matter's first lookup (after joining, or after we
// reboot). Each lookup asks once and then waits this long, so the default 45 s made pairing take ~60 s. The lookup
// server is on this chip and answers at once. Pairing makes 3 attempts, so the sensor still gets 2 x 15 s to register.
#define CHIP_CONFIG_ADDRESS_RESOLVE_MAX_LOOKUP_TIME_MS 15000

#endif // CONFIG_ESP_MATTER_ENABLE_MATTER_SERVER
