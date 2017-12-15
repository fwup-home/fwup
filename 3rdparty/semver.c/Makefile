CC      ?= cc
CFLAGS   = -std=c89 -Ideps -Wall -Wextra -pedantic -Wno-missing-field-initializers -Wno-unused-function -Wno-declaration-after-statement
VALGRIND = valgrind
RM       = rm -rf

# Call make with DEBUG=anything to compile with debug flag and with extra output
ifneq ($(DEBUG),)
CFLAGS += -g -DDEBUG=1
endif

test: semver.c semver_test.c
	@$(CC) $(CFLAGS) -o $@ $^
	@./$@

unittest: semver_unit.c
	@$(CC) $(CFLAGS) -o $@ $^
	@./$@

valgrind: ./test
	@$(VALGRIND) --leak-check=full --error-exitcode=1 $^

clean:
	$(RM) test unittest

%.o: %.c
	$(CC) -std=c89 $(CFLAGS) -c -o $@ $^

.PHONY: test unittest clean
