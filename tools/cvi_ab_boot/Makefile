CC = aarch64-none-linux-gnu-gcc

SRCS = cvi_ab_boot.c

OBJS = $(SRCS:.c=.o)

CFLAGS = --static

all : cvi_ab_boot

cvi_ab_boot : $(OBJS)
	$(CC) -o cvi_ab_boot $(SRCS) $(CFLAGS)
	rm -f $(OBJS)

.PHONY : clean
clean:
	rm -f $(OBJS) cvi_ab_boot
