OBJS=mkdown

CXX=clang++ -std=c++11
CXXFLAGS=-O2 -Wall

mkdown : markdown.o main.o
	$(CXX) -o mkdown markdown.o main.o

markdown.o : markdown.cpp markdown.hpp
	$(CXX) $(CXXFLAGS) -c markdown.cpp

main.o : main.cpp markdown.hpp
	$(CXX) $(CXXFLAGS) -c main.cpp

test : test_1_1 test_1_1p test_extra

test_1_1 : mkdown
	cd mdtest/1.1; make

test_1_1p : mkdown
	cd mdtest/1.1p; make

test_extra : mkdown
	cd mdtest/extra; make

clean :
	rm -f *.o $(OBJS)
