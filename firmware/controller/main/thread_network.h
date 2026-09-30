#pragma once

// Offline Thread network owned by the S3: forms a new network on first boot on the quietest channel (the dataset
// persists in NVS), starts Thread, and enables the SRP server that sensors register with.
// Call on the Matter thread or with the Matter stack lock held, after esp_matter::start().
void thread_network_start();

// Logs a clear error and turns the LEDs red if the H2 radio doesn't answer or runs incompatible firmware; OpenThread
// then restarts the S3. Call before esp_matter::start().
void thread_network_watch_radio();
