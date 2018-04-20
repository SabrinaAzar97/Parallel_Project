# Makefile
# For CSC446 Spring 2018, Project 1 (Monte Carlo)
#
#
#
# Include platform-dependent settings.
#
include Makefile.include


LDFLAGS += -Ldcmt0.4/lib -ldcmt

HW1_INCS = black_scholes.h gaussian.h parser.h random.h timer.h util.h
HW1_C_SRCS = black_scholes.c gaussian.c main.c parser.c random.c dcmt0.4/lib/random_seed.c timer.c util.c
HW1_C_OBJS = $(HW1_C_SRCS:.c=.o)
HW1_EXE = csc447.x


all: csc447.x

%.o: %.c
	$(CC) -c $(CFLAGS) $(CPPFLAGS) $< -o $@

csc447.x: $(HW1_C_OBJS) dcmt0.4/lib/libdcmt.a
	$(LINKER) $(CFLAGS) $(LDFLAGS) $(HW1_C_OBJS) -o $@

dcmt0.4/lib/libdcmt.a: 
	make -C dcmt0.4/lib

black_scholes.o: black_scholes.c black_scholes.h gaussian.h random.h util.h

gaussian.o: gaussian.c gaussian.h util.h

main.o: main.c black_scholes.h parser.h random.h timer.h

parser.o: parser.c parser.h

random.o: random.c random.h

dcmt0.4/lib/random_seed.o: dcmt0.4/lib/random_seed.c

timer.o: timer.c timer.h

util.o: util.c util.h

clean:
	make -C dcmt0.4/lib clean
	rm -f $(HW1_C_OBJS) $(HW1_EXE)
