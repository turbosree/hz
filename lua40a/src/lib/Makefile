# makefile for Lua standard library

LUA= ../..

include $(LUA)/config

# actually only used in liolib.c
EXTRA_DEFS= $(POPEN)

OBJS= linit.o ldblib.o liolib.o lmathlib.o lstrlib.o
SRCS= linit.c ldblib.c liolib.c lmathlib.c lstrlib.c

T= $(LIB)/liblualib.a

all: $T

$T: $(OBJS)
	$(AR) $@ $(OBJS)
	$(RANLIB) $@

clean:
	rm -f $(OBJS) $T

co:
	co -q -f -M $(SRCS)

klean:	clean
	rm -f $(SRCS)
