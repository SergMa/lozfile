CFLAGS += -Wall -O0
#EXEC = test
EXEC = loz

OBJS =  loz.o \
        lozfile.o \
        crc8.o \
        mylog.o \
        fastlz.o \
        compress_rle.o \
        compress_rle2.o \
        compress_lz.o

all: $(EXEC)

$(EXEC): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LIBM) $(LDLIBS) $(LIBGCC)

clean:
	-rm -f $(EXEC) *.elf *.gdb *.o

.PHONY: all clean
