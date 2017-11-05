
.PHONY: run

all: run

run: a.out
	./a.out

a.out: a.cpp
	clang++-3.9 -fsanitize=undefined --std=c++14 -Wall a.cpp -o a.out


