#
# Students' Makefile for the Malloc Lab
#
ALLOCATOR_DIR =../..
CC = gcc
CFLAGS = -Wall -O2 -m32 -g -I $(ALLOCATOR_DIR)


OBJS = mdriver.o mm.o memlib.o fsecs.o fcyc.o clock.o ftimer.o ${ALLOCATOR_DIR}/tlsf.o

mdriver: $(OBJS)
	$(CC) $(CFLAGS) -o mdriver $(OBJS)

mdriver.o: mdriver.c fsecs.h fcyc.h clock.h memlib.h config.h mm.h 
memlib.o: memlib.c memlib.h
mm.o: mm.c mm.h memlib.h ${ALLOCATOR_DIR}/tlsf.h
fsecs.o: fsecs.c fsecs.h config.h
fcyc.o: fcyc.c fcyc.h
ftimer.o: ftimer.c ftimer.h config.h
clock.o: clock.c clock.h
tlsf.o: ${ALLOCATOR_DIR}/tlsf.c tlsf.h

clean:
	rm -f *~ *.o mdriver


