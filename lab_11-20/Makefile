CC = cc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -Werror
LDLIBS = -lm
PYTHON = python3

NUMBERS = 11 12 13 14 15 16 17 18 19 20
LABS = $(addprefix lab,$(NUMBERS))
TESTS = $(addprefix test_lab,$(NUMBERS))

all: $(LABS)

$(LABS): %: %.c
	$(CC) $(CFLAGS) $< -o $@ $(LDLIBS)

lab12 lab14: lab_io.h

$(TESTS): test_lab%: test_lab%.c lab%.c
	$(CC) $(CFLAGS) $< -o $@ $(LDLIBS)

test_lab12 test_lab14: lab_io.h

test_format_roundtrip: test_format_roundtrip.c lab15.c lab16.c
	$(CC) $(CFLAGS) -DLAB_NO_MAIN lab15.c lab16.c test_format_roundtrip.c -o $@ $(LDLIBS)

test: all $(TESTS) test_format_roundtrip
	@set -e; for program in $(TESTS); do ./$$program; done
	./test_format_roundtrip
	$(PYTHON) test_cli.py

sanitize:
	@mkdir -p .build/sanitize
	@set -e; for n in $(NUMBERS); do \
	  $(CC) $(CFLAGS) -g -fsanitize=address,undefined -fno-omit-frame-pointer lab$$n.c -o .build/sanitize/lab$$n $(LDLIBS); \
	  $(CC) $(CFLAGS) -g -fsanitize=address,undefined -fno-omit-frame-pointer test_lab$$n.c -o .build/sanitize/test_lab$$n $(LDLIBS); \
	  ./.build/sanitize/test_lab$$n; \
	done
	$(CC) $(CFLAGS) -g -fsanitize=address,undefined -fno-omit-frame-pointer -DLAB_NO_MAIN lab15.c lab16.c test_format_roundtrip.c -o .build/sanitize/test_format_roundtrip $(LDLIBS)
	./.build/sanitize/test_format_roundtrip
	LAB_BIN_DIR=.build/sanitize $(PYTHON) test_cli.py

clean:
	rm -f $(LABS) $(TESTS) test_format_roundtrip .build/sanitize/test_format_roundtrip $(addprefix .build/sanitize/,$(LABS) $(TESTS))

.PHONY: all test sanitize clean
