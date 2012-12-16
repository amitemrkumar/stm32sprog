CC := gcc
LD := gcc
RM := rm -f
CFLAGS := -std=gnu99 -g -Wall -Wextra -pedantic -Werror

PRJ := stm32sprog
SRCS := stm32sprog.c firmware.c serial.c sparse-buffer.c

all: $(PRJ)

$(PRJ): $(SRCS:.c=.o)
	$(LD) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.d: %.c
	@set -e; $(RM) $@; \
	$(CC) -M $(CFLAGS) $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
	$(RM) $@.$$$$

-include $(SRCS:.c=.d)

clean:
	$(RM) $(PRJ)
	$(RM) $(SRCS:.c=.o)
	$(RM) $(SRCS:.c=.d)

.PHONY: all clean

