#include "console_cmds.h"

#include <stdio.h>

#include <esp_console.h>
#include <esp_log.h>

#include "app_state.h"
#include "command.h"
#include "sensor_link.h"

static const char *TAG = "console_cmds";

static void print_list()
{
    // Format under the lock, print after it: a slow console must not stall the LED task. Only used slots are listed.
    static char lines[SLOT_COUNT][96];
    int n = 0;
    {
        AppStateLock m;
        uint32_t now = app_now_ms();
        for (int i = 0; i < SLOT_COUNT; i++) {
            if (m->table.slot[i].used || i == m->pairing_slot) {
                sensor_mgr_format_slot(m.get(), i, now, lines[n++], sizeof(lines[0]));
            }
        }
    }
    for (int i = 0; i < n; i++) {
        printf("%s\n", lines[i]);
    }
    if (n == 0) {
        printf("No sensors paired.\n");
    } else {
        printf("%d of %d slots used\n", n, SLOT_COUNT);
    }
}

static int run(int argc, char **argv)
{
    command_t cmd;
    char err[128];
    if (!command_parse(argc, argv, &cmd, err, sizeof(err))) {
        printf("%s\n", err);
        return 1;
    }
    esp_err_t result = ESP_OK;
    switch (cmd.kind) {
    case CMD_PAIR:
        result = sensor_link_pair(&cmd.code, cmd.slot);
        break;
    case CMD_LIST:
        print_list();
        break;
    case CMD_REMOVE:
        result = sensor_link_remove(cmd.slot);
        break;
    case CMD_IDENTIFY:
        result = sensor_link_identify(cmd.slot);
        break;
    case CMD_MOVE:
        result = sensor_link_move(cmd.slot, cmd.slot2);
        break;
    case CMD_FACTORY_RESET:
        sensor_link_factory_reset();
        break;
    }
    return result == ESP_OK ? 0 : 1;
}

void console_cmds_register()
{
    static const esp_console_cmd_t cmds[] = {
        {.command = "pair", .help = "Pair a sensor into the given slot, or the lowest free one",
         .hint = "<11-digit code> [slot <1-64>]", .func = run},
        {.command = "list", .help = "Show the paired sensors", .hint = nullptr, .func = run},
        {.command = "remove", .help = "Unpair the sensor in a slot", .hint = "<slot 1-64>", .func = run},
        {.command = "identify", .help = "Make the sensor in a slot blink", .hint = "<slot 1-64>", .func = run},
        {.command = "move", .help = "Put a sensor on another LED; swaps if that slot is used", .hint = "<from> <to>",
         .func = run},
        {.command = "factory-reset", .help = "Remove every sensor, erase all settings and reboot",
         .hint = "confirm", .func = run},
    };
    for (const auto &cmd : cmds) {
        if (esp_console_cmd_register(&cmd) != ESP_OK) {
            ESP_LOGE(TAG, "Couldn't register '%s'", cmd.command);
        }
    }
}
