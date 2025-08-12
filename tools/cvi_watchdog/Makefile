CC = aarch64-none-linux-gnu-gcc

SRCS = watchdog.c

OBJS = $(SRCS:.c=.o)

CFLAGS = --static

all : watchdog

retrain : $(OBJS)
	$(CC) -o watchdog $(SRCS) $(CFLAGS)
	rm -f $(OBJS)

.PHONY : clean
clean:
	rm -f $(OBJS) watchdog
