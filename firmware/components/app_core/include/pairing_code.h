#pragma once

// Parses the 11-digit Matter manual pairing code printed on a MYGGBETT (e.g. "3497-011-2332").
// Matter Core spec 5.1.4: digit 1 | 5 digits | 4 digits | Verhoeff check digit.

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    PAIRING_CODE_OK,
    PAIRING_CODE_BAD_CHAR,        // something other than digits, '-' or ' '
    PAIRING_CODE_BAD_LENGTH,      // not 11 digits
    PAIRING_CODE_BAD_CHECK,       // Verhoeff check digit doesn't match (typo)
    PAIRING_CODE_BAD_FIRST_DIGIT, // 8 or 9: not a Matter manual code
    PAIRING_CODE_LONG_FORMAT,     // VID/PID flag set: that is a 21-digit code
    PAIRING_CODE_BAD_PASSCODE,    // out of range or one of the spec's disallowed passcodes
} pairing_code_err_t;

typedef struct {
    uint32_t passcode;            // setup PIN, 1..99999998
    uint8_t short_discriminator;  // upper 4 bits of the 12-bit discriminator
    char digits[12];              // the 11 digits without separators
} pairing_code_t;

pairing_code_err_t pairing_code_parse(const char *text, pairing_code_t *out);
const char *pairing_code_strerror(pairing_code_err_t err);

#ifdef __cplusplus
}
#endif
