#pragma once

#include <sdkconfig.h>

#ifndef CONFIG_ESP_MATTER_ENABLE_MATTER_SERVER

#ifdef CONFIG_ESP_MATTER_COMMISSIONER_ENABLE
// Enable or disable whether this device advertises as a commissioner.
#define CHIP_DEVICE_CONFIG_ENABLE_COMMISSIONER_DISCOVERY 1
#endif // CONFIG_ESP_MATTER_COMMISSIONER_ENABLE

// Number of devices a controller can be simultaneously connected to
#define CHIP_CONFIG_CONTROLLER_MAX_ACTIVE_DEVICES 8

// CASE handshakes with up to 8 sensors at once after a boot, plus a pairing (PASE). The default 4 runs out.
#define CHIP_CONFIG_UNAUTHENTICATED_CONNECTION_POOL_SIZE 10

#endif // CONFIG_ESP_MATTER_ENABLE_MATTER_SERVER
