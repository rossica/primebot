CC=g++
DEBUG=-g
CXXFLAGS=-std=c++14 -m64 -pipe -Wall $(DEBUG)
LFLAGS= $(DEBUG) -lgmp -lgmpxx -lpthread

all:    primebot

primebot:  main.o prime.o network.o
	$(CC) $(LFLAGS) -o bin/primebot obj/prime.o obj/main.o obj/network.o

main.o:
	$(CC) $(CXXFLAGS) -c main.cpp -o obj/main.o

prime.o:
	$(CC) $(CXXFLAGS) -c prime.cpp -o obj/prime.o

network.o:
	$(CC) $(CXXFLAGS) -c networkcontroller.cpp -o obj/network.o

clean:
	rm -f obj/*.o bin/primebot