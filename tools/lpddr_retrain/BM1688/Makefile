CC = aarch64-linux-gnu-gcc

SRCS = ddrtest.c
SRCS += test.c

OBJS = $(SRCS:.c=.o)

CFLAGS = --static

all : retrain

retrain : $(OBJS)
	$(CC) -o retrain $(SRCS) $(CFLAGS)
	rm -f $(OBJS)

.PHONY : clean
clean:
	rm -f $(OBJS) retrain
