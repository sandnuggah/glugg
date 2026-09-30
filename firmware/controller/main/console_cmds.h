#pragma once

// Registers pair / list / remove / identify / factory-reset (confirm) with esp_console.
// Call after esp_matter::console::init(), which initialises esp_console. Matter's shell loop runs every
// command on the Matter thread, so handlers call sensor_link directly.
void console_cmds_register();
