#include "command.h"

#include <stdio.h>
#include <string.h>

#include "slot_table.h"

static bool parse_slot(const char *text, int *slot)
{
    if (text[0] < '1' || text[0] > '0' + SLOT_COUNT || text[1] != '\0') {
        return false;
    }
    *slot = text[0] - '1';
    return true;
}

bool command_parse(int argc, const char *const *argv, command_t *out, char *err, size_t err_len)
{
    memset(out, 0, sizeof(*out));
    err[0] = '\0';
    if (argc < 1) {
        snprintf(err, err_len, "empty command");
        return false;
    }
    const char *name = argv[0];

    if (strcmp(name, "pair") == 0) {
        if (argc < 2) {
            snprintf(err, err_len, "usage: pair <11-digit code>");
            return false;
        }
        char joined[64] = "";
        for (int i = 1; i < argc; i++) {
            if (strlen(joined) + strlen(argv[i]) + 1 >= sizeof(joined)) {
                snprintf(err, err_len, "pair: %s", pairing_code_strerror(PAIRING_CODE_BAD_LENGTH));
                return false;
            }
            strcat(joined, argv[i]);
        }
        pairing_code_err_t e = pairing_code_parse(joined, &out->code);
        if (e != PAIRING_CODE_OK) {
            snprintf(err, err_len, "pair: %s", pairing_code_strerror(e));
            return false;
        }
        out->kind = CMD_PAIR;
        return true;
    }

    if (strcmp(name, "remove") == 0 || strcmp(name, "identify") == 0) {
        if (argc != 2 || !parse_slot(argv[1], &out->slot)) {
            snprintf(err, err_len, "usage: %s <slot 1-%d>", name, SLOT_COUNT);
            return false;
        }
        out->kind = name[0] == 'r' ? CMD_REMOVE : CMD_IDENTIFY;
        return true;
    }

    if (strcmp(name, "factory-reset") == 0) {
        if (argc != 2 || strcmp(argv[1], "confirm") != 0) {
            snprintf(err, err_len, "this removes every sensor; type: factory-reset confirm");
            return false;
        }
        out->kind = CMD_FACTORY_RESET;
        return true;
    }

    if (strcmp(name, "list") == 0) {
        if (argc != 1) {
            snprintf(err, err_len, "usage: list");
            return false;
        }
        out->kind = CMD_LIST;
        return true;
    }

    snprintf(err, err_len, "unknown command '%s'", name);
    return false;
}
