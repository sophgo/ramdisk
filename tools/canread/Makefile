CC = $(CROSS_COMPILE)gcc

source  = canread.c devmem.c
object  = $(source:%.cc=%.o)

all: $(object)
	$(CC) $(object) -o canread -Wall

%.o: %.c
	$(CC) $(CCCFLAGS) -c $<

clean:
	$(RM) *.o canread
