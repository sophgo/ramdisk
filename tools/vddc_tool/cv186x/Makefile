CC = $(CROSS_COMPILE)gcc

source  = vddc_tool.c devmem.c
object  = $(source:%.cc=%.o)

all: $(object)
	$(CC) $(object) -o vddc_tool -Wall

%.o: %.c
	$(CC) $(CCCFLAGS) -c $<

clean:
	$(RM) *.o vddc_tool
