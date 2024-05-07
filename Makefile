ALL = client1 client2
SRCS = client1.c client2.c

# Dependencies
all: $(ALL)
client1: client1.o
client2: client2.o

# Static part of makefile

# Default flags for build
CFLAGS += -std=gnu99 -O3
LFLAGS += 

# Flags for debug build
debug: CFLAGS += -Wundef -Wpointer-arith -Wwrite-strings -Wfloat-equal -W -Wno-missing-field-initializers -g
debug: LFLAGS +=
debug: all

MAKEFLAGS = -s

# Compile the C files to .o files
.c.o:
	/bin/echo -e "$<\t->\t$(<:.c=.o)"
	$(CC) $(CFLAGS) -c $<

# Link .o files to programs
.o:
	echo Linking $@
	$(CXX) $< -o $@ $(LFLAGS)

# General cleanup
clean:
	$(RM) core *.o $(ALL)

# Install programs to filesystem
install: all
	install -pD $(ALL) $(DESTDIR)/usr/bin/$(ALL)

