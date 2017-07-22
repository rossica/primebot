DEBUG=-Og -g3 -Wl,-Og
RELEASE=-O2 -finline-functions -fomit-frame-pointer -march=native -Wl,-O2 -DNDEBUG
CXXFLAGS=-std=c++14 -m64 -pipe -Wall -pedantic
LFLAGS=-pipe -lgmp -lgmpxx -lpthread

all:	CXXFLAGS += $(RELEASE)
all:	LFLAGS += $(RELEASE)
all:    primebot

debug:	CXXFLAGS += $(DEBUG)
debug:	LFLAGS += $(DEBUG)
debug:	primebot

primebot:  main.o async.o prime.o network.o parser.o fileio.o primetest.o pal.o
	$(CXX) $(LFLAGS) -o bin/primebot obj/async.o obj/prime.o obj/main.o obj/network.o obj/parser.o obj/fileio.o obj/primetest.o obj/pal.o

main.o:
	$(CXX) $(CXXFLAGS) -c main.cpp -o obj/main.o

prime.o:
	$(CXX) $(CXXFLAGS) -c prime.cpp -o obj/prime.o

network.o:
	$(CXX) $(CXXFLAGS) -c networkcontroller.cpp -o obj/network.o

parser.o:
	$(CXX) $(CXXFLAGS) -c commandparser.cpp -o obj/parser.o

async.o:
	$(CXX) $(CXXFLAGS) -c asyncPrimeSearching.cpp -o obj/async.o

fileio.o:
	$(CXX) $(CXXFLAGS) -c fileio.cpp -o obj/fileio.o

primetest.o:
	$(CXX) $(CXXFLAGS) -c PrimeTest.cpp -o obj/primetest.o

pal.o:
	$(CXX) $(CXXFLAGS) -c pal-linux.cpp -o obj/pal.o

clean:
	rm -f obj/*.o bin/primebot
