CFLAGS = -O3 -Wall -Wextra -g -D_GNU_SOURCE -DHAVE_STRLCPY=0 -DWITH_TIMESTAMP=0

.PHONY: all
all: led_group
%: %.c
	$(CC) $(CFLAGS) -o $@ $^

.PHONY: clean
clean:
	$(RM) led_group

