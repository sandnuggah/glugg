#pragma once

// The Matter side of the app: commissions sensors, keeps one BooleanState subscription per paired sensor,
// and feeds everything into sensor_mgr. All functions run on the Matter thread (console handlers already do).

#include <esp_err.h>

#include "pairing_code.h"

// Starts the 1 s tick that (re)subscribes sensors as sensor_mgr asks, and feeds the task watchdog from it.
// Call once after the commissioner is set up.
void sensor_link_start();

// Matter can now look sensors up (DeviceEventType::kDnssdInitialized). Subscribing and pairing start here.
void sensor_link_network_ready();

// Starts commissioning into the lowest free slot. The result is logged when commissioning finishes.
esp_err_t sensor_link_pair(const pairing_code_t *code);

// Frees the slot at once, drops the subscription and asks the sensor to leave our fabric (best effort).
esp_err_t sensor_link_remove(int slot);

// Sends Identify (10 s) to the sensor, which blinks its own LED if it supports the Identify cluster.
esp_err_t sensor_link_identify(int slot);

// Cancels any pairing and asks every sensor to leave our fabric. Once all have answered (or after 2 min), erases all
// settings (slots, Matter fabric, Thread network) and reboots.
void sensor_link_factory_reset();
