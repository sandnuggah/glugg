#include "pairing_code.h"

#include <stdbool.h>
#include <string.h>

#define CODE_DIGITS 11

// Verhoeff tables: dihedral group D5 multiplication, and the position permutation (row i is row 1 applied i times).
static const uint8_t k_d[10][10] = {
    {0, 1, 2, 3, 4, 5, 6, 7, 8, 9}, {1, 2, 3, 4, 0, 6, 7, 8, 9, 5}, {2, 3, 4, 0, 1, 7, 8, 9, 5, 6},
    {3, 4, 0, 1, 2, 8, 9, 5, 6, 7}, {4, 0, 1, 2, 3, 9, 5, 6, 7, 8}, {5, 9, 8, 7, 6, 0, 4, 3, 2, 1},
    {6, 5, 9, 8, 7, 1, 0, 4, 3, 2}, {7, 6, 5, 9, 8, 2, 1, 0, 4, 3}, {8, 7, 6, 5, 9, 3, 2, 1, 0, 4},
    {9, 8, 7, 6, 5, 4, 3, 2, 1, 0},
};
static const uint8_t k_p[8][10] = {
    {0, 1, 2, 3, 4, 5, 6, 7, 8, 9}, {1, 5, 7, 6, 2, 8, 3, 0, 9, 4}, {5, 8, 0, 3, 7, 9, 6, 1, 4, 2},
    {8, 9, 1, 6, 0, 4, 3, 5, 2, 7}, {9, 4, 5, 3, 1, 2, 6, 8, 7, 0}, {4, 2, 8, 6, 5, 7, 3, 9, 0, 1},
    {2, 7, 9, 3, 8, 0, 6, 4, 1, 5}, {7, 0, 4, 6, 9, 1, 3, 2, 5, 8},
};

// Validates a digit string whose last digit is the Verhoeff check digit.
static bool verhoeff_valid(const char *digits, int len)
{
    int c = 0;
    for (int i = 0; i < len; i++) {
        c = k_d[c][k_p[i % 8][digits[len - 1 - i] - '0']];
    }
    return c == 0;
}

static uint32_t digits_value(const char *digits, int count)
{
    uint32_t v = 0;
    for (int i = 0; i < count; i++) {
        v = v * 10 + (uint32_t)(digits[i] - '0');
    }
    return v;
}

static bool passcode_allowed(uint32_t passcode)
{
    static const uint32_t k_disallowed[] = {
        0,        11111111, 22222222, 33333333, 44444444, 55555555,
        66666666, 77777777, 88888888, 99999999, 12345678, 87654321,
    };
    if (passcode > 99999998) {
        return false;
    }
    for (size_t i = 0; i < sizeof(k_disallowed) / sizeof(k_disallowed[0]); i++) {
        if (passcode == k_disallowed[i]) {
            return false;
        }
    }
    return true;
}

pairing_code_err_t pairing_code_parse(const char *text, pairing_code_t *out)
{
    char digits[CODE_DIGITS + 1];
    int n = 0;
    for (const char *s = text; *s; s++) {
        if (*s == '-' || *s == ' ') {
            continue;
        }
        if (*s < '0' || *s > '9') {
            return PAIRING_CODE_BAD_CHAR;
        }
        if (n == CODE_DIGITS) {
            return PAIRING_CODE_BAD_LENGTH;
        }
        digits[n++] = *s;
    }
    if (n != CODE_DIGITS) {
        return PAIRING_CODE_BAD_LENGTH;
    }
    digits[n] = '\0';

    if (!verhoeff_valid(digits, CODE_DIGITS)) {
        return PAIRING_CODE_BAD_CHECK;
    }

    uint32_t chunk1 = digits_value(digits, 1);
    uint32_t chunk2 = digits_value(digits + 1, 5);
    uint32_t chunk3 = digits_value(digits + 6, 4);
    if (chunk1 > 7) {
        return PAIRING_CODE_BAD_FIRST_DIGIT;
    }
    if (chunk1 & 0x4) {
        return PAIRING_CODE_LONG_FORMAT;
    }
    if (chunk2 > 0xFFFF) {
        return PAIRING_CODE_BAD_PASSCODE;
    }

    uint32_t passcode = (chunk2 & 0x3FFF) | (chunk3 << 14);
    if (!passcode_allowed(passcode)) {
        return PAIRING_CODE_BAD_PASSCODE;
    }

    out->passcode = passcode;
    out->short_discriminator = (uint8_t)(((chunk1 & 0x3) << 2) | (chunk2 >> 14));
    memcpy(out->digits, digits, sizeof(digits));
    return PAIRING_CODE_OK;
}

const char *pairing_code_strerror(pairing_code_err_t err)
{
    switch (err) {
    case PAIRING_CODE_OK:
        return "ok";
    case PAIRING_CODE_BAD_CHAR:
        return "only digits, '-' and spaces are allowed";
    case PAIRING_CODE_BAD_LENGTH:
        return "the code must have 11 digits";
    case PAIRING_CODE_BAD_CHECK:
        return "check digit doesn't match; is there a typo?";
    case PAIRING_CODE_BAD_FIRST_DIGIT:
        return "the first digit must be 0-7; check the code";
    case PAIRING_CODE_LONG_FORMAT:
        return "this is the first part of a 21-digit code; only 11-digit codes are supported";
    case PAIRING_CODE_BAD_PASSCODE:
        return "the code contains an invalid setup passcode";
    }
    return "unknown error";
}
