OBJS=mkdown

CXX=clang++ -std=c++11
CXXFLAGS=-O2 -Wall

mkdown : markdown.o main.o
	$(CXX) -o mkdown markdown.o main.o

markdown.o : markdown.cpp markdown.hpp
	$(CXX) $(CXXFLAGS) -c markdown.cpp

main.o : main.cpp markdown.hpp
	$(CXX) $(CXXFLAGS) -c main.cpp

clean :
	rm -f *.o $(OBJS)
