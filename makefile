CC=g++
DEBUG=-g
CXXFLAGS=-std=c++14 -m64 -pipe -Wall $(DEBUG)
LFLAGS= $(DEBUG) -lgmp -lpthread

all:    primebot

primebot:  main.o prime.o
	$(CC) $(LFLAGS) -o bin/primebot obj/prime.o obj/main.o

main.o:
	$(CC) $(CXXFLAGS) -c main.cpp -o obj/main.o

prime.o:
	$(CC) $(CXXFLAGS) -c prime.cpp -o obj/prime.o

clean:
	rm -f obj/*.o bin/primebot