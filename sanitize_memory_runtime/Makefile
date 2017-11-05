
.PHONY: run

all: run

run: a.out
	./a.out

a.out: a.cpp
	clang++-3.9 -fsanitize=address --std=c++14 -Wall a.cpp -o a.out -O2 -g3
	# clang++-3.9 -fsanitize=memory --std=c++14 -Wall a.cpp -o a.out -O2 -g3


