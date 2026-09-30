# Sanitizer flags live in TEST_CFLAGS so a CFLAGS from the environment adds to them instead of replacing them.
TEST_CFLAGS = -std=c11 -Wall -Wextra -Werror -O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all
SRCS = test_app_core.c $(wildcard ../*.c) ../../led_pattern/led_pattern.c
INCS = -I../include -I../../led_pattern/include

run: test_app_core
	./test_app_core

test_app_core: $(SRCS) $(wildcard ../include/*.h)
	$(CC) $(TEST_CFLAGS) $(CFLAGS) $(INCS) -o $@ $(SRCS)

clean:
	rm -f test_app_core

.PHONY: run clean
