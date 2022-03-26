CC=g++
CFLAGS=-Wall -DLOGS

demo : demo1 demo2 demo3

demo1: demo1.o libmemlab.a
	$(CC) $(CFLAGS) -o demo1 demo1.o -L. -lmemlab -lpthread

demo1.o: demo1.cpp
	$(CC) $(CFLAGS) -c demo1.cpp

demo2: demo2.o libmemlab.a
	$(CC) $(CFLAGS) -o demo2 demo2.o -L. -lmemlab -lpthread

demo2.o: demo2.cpp
	$(CC) $(CFLAGS) -c demo2.cpp

demo3: demo3.o libmemlab.a
	$(CC) $(CFLAGS) -o demo3 demo3.o -L. -lmemlab -lpthread

demo3.o: demo3.cpp
	$(CC) $(CFLAGS) -c demo3.cpp

lib: libmemlab.a

libmemlab.a: memlab.o
	ar -rcs libmemlab.a memlab.o

memlab.o: memlab.cpp memlab.h
	$(CC) $(CFLAGS) -c memlab.cpp -lpthread

clean:
	rm -f libmemlab.a memlab.o demo1 demo1.o demo2 demo2.o