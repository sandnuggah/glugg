#pragma once

// The one sensor_mgr instance, shared by the Matter thread (console, callbacks) and the LED task.
// Hold the lock only briefly and never across a Matter call, which may call back synchronously.

#include <stdint.h>

#include "sensor_mgr.h"

// Loads the slot table from NVS (empty on first boot or if the stored blob is corrupt).
void app_state_init();

sensor_mgr_t *app_state_lock();
void app_state_unlock();

// Writes m->table to NVS. Call while holding the lock.
void app_state_save(const sensor_mgr_t *m);

// Free-running millisecond clock for sensor_mgr (wraps after ~49 days; sensor_mgr handles that).
uint32_t app_now_ms();

class AppStateLock {
public:
    AppStateLock() : m_mgr(app_state_lock()) {}
    ~AppStateLock() { app_state_unlock(); }
    AppStateLock(const AppStateLock &) = delete;
    AppStateLock &operator=(const AppStateLock &) = delete;
    sensor_mgr_t *get() { return m_mgr; }
    sensor_mgr_t *operator->() { return m_mgr; }

private:
    sensor_mgr_t *m_mgr;
};
