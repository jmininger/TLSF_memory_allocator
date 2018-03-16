
CC=g++
BASE =/Users/Jacquin/Desktop/Recurse_Center/TLSF_memory_allocator
GT_BASE=${BASE}/googletest/googletest
GT_STATIC_LIB=${GT_BASE}/BUILD/libgtest.a
OBJDIR=${BASE}/obj


test1: tlsf.o tlsf.h
	$(CC) -isystem ${GT_BASE}/include ${BASE}/test/test1.cc ${GT_STATIC_LIB} tlsf.o -o bin/test1

tlsf.o: tlsf.c tlsf.h
	$(CC) -c tlsf.c $(OBJDIR)/tlsf.o

clean: rm -rf obj memTest