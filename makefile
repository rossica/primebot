CC=g++
DEBUG=-g
CXXFLAGS=-std=c++14 -m64 -pipe -Wall $(DEBUG)
LFLAGS= $(DEBUG) -lgmp -lgmpxx -lpthread

all:    primebot

primebot:  main.o async.o prime.o network.o parser.o fileio.o primetest.o pal.o
	$(CC) $(LFLAGS) -o bin/primebot obj/async.o obj/prime.o obj/main.o obj/network.o obj/parser.o obj/fileio.o obj/primetest.o obj/pal.o

main.o:
	$(CC) $(CXXFLAGS) -c main.cpp -o obj/main.o

prime.o:
	$(CC) $(CXXFLAGS) -c prime.cpp -o obj/prime.o

network.o:
	$(CC) $(CXXFLAGS) -c networkcontroller.cpp -o obj/network.o

parser.o:
	$(CC) $(CXXFLAGS) -c commandparser.cpp -o obj/parser.o

async.o:
	$(CC) $(CXXFLAGS) -c asyncPrimeSearching.cpp -o obj/async.o

fileio.o:
	$(CC) $(CXXFLAGS) -c fileio.cpp -o obj/fileio.o

primetest.o:
	$(CC) $(CXXFLAGS) -c PrimeTest.cpp -o obj/primetest.o

pal.o:
	$(CC) $(CXXFLAGS) -c pal-linux.cpp -o obj/pal.o

clean:
	rm -f obj/*.o bin/primebot