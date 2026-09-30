# Sanitizer flags live in TEST_CFLAGS so a CFLAGS from the environment adds to them instead of replacing them.
TEST_CFLAGS = -std=c11 -Wall -Wextra -Werror -O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all

run: test_led_pattern
	./test_led_pattern

test_led_pattern: test_led_pattern.c ../led_pattern.c ../include/led_pattern.h
	$(CC) $(TEST_CFLAGS) $(CFLAGS) -I../include -o $@ test_led_pattern.c ../led_pattern.c

clean:
	rm -f test_led_pattern

.PHONY: run clean
