
.PHONY: run

all: a.out

run: a.out
	./a.out

a.out: a.cpp
	g++ -O2 --std=c++14 -Wall a.cpp -o a.out -lboost_system -pthread -g3

tsan.out: a.cpp
	clang++ -fPIE -pie -O2 --std=c++14 -Wall a.cpp -o tsan.out -lboost_system -pthread -fsanitize=thread -g3

