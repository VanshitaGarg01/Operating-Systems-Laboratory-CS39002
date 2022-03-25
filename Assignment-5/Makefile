CC=g++
CFLAGS=-Wall

demo : demo1 demo2

demo1: demo1.o libmemlab.a
	$(CC) $(CFLAGS) -o demo1 demo1.o -L. -lmemlab -lpthread

demo1.o: demo1.cpp
	$(CC) $(CFLAGS) -c demo1.cpp

demo2: demo2.o libmemlab.a
	$(CC) $(CFLAGS) -o demo2 demo2.o -L. -lmemlab -lpthread

demo2.o: demo2.cpp
	$(CC) $(CFLAGS) -c demo2.cpp

libmemlab.a: memlab.o
	ar -rcs libmemlab.a memlab.o

memlab.o: memlab.cpp memlab.h
	$(CC) $(CFLAGS) -c memlab.cpp -lpthread

clean:
	rm libmemlab.a memlab.o demo1 demo1.o demo2 demo2.o