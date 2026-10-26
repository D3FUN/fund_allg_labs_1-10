CC = cc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -Werror
LDLIBS = -lm
NUMBERS = 21 22 23 24 25 26 27 28 29 30
LABS = $(addprefix lab,$(NUMBERS))
UNIT_TESTS = .build/unit_lab24 .build/unit_lab25

all: $(LABS)

$(LABS): %: %.c
	$(CC) $(CFLAGS) $< -o $@ $(LDLIBS)

lab21 lab22 lab23: lab_input.h

.build/unit_lab%: lab%_unit.c lab%.c
	@mkdir -p .build
	$(CC) $(CFLAGS) $< -o $@ $(LDLIBS)

test: all $(UNIT_TESTS)
	./.build/unit_lab24
	./.build/unit_lab25

sanitize:
	@mkdir -p .build/sanitize
	@set -e; for n in $(NUMBERS); do \
	  $(CC) $(CFLAGS) -g -fsanitize=address,undefined -fno-omit-frame-pointer lab$$n.c -o .build/sanitize/lab$$n $(LDLIBS); \
	done
	@set -e; for n in 24 25; do \
	  $(CC) $(CFLAGS) -g -fsanitize=address,undefined -fno-omit-frame-pointer lab$${n}_unit.c -o .build/sanitize/unit_lab$$n $(LDLIBS); \
	  ./.build/sanitize/unit_lab$$n; \
	done

clean:
	rm -f $(LABS) $(UNIT_TESTS) .build/sanitize/unit_lab24 .build/sanitize/unit_lab25 $(addprefix .build/sanitize/,$(LABS))
	@rmdir .build/sanitize .build 2>/dev/null || true

.PHONY: all test sanitize clean
