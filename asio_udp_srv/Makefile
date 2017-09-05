
.PHONY: run

all: a.out

run: a.out
	./a.out 32   2 0  4 4  0  mport

tsan: tsan.out
	./tsan.out

callgrind: g.out
	rm callgrind.out*
	valgrind --tool=callgrind ./g.out
	kcachegrind callgrind.out*

valgrind: g.out
	valgrind ./g.out

helgrind: g.out
	valgrind --tool=helgrind ./g.out

a.out: a.cpp
	clang++ -O3 --std=c++14 -Wall a.cpp -o a.out -lboost_system -pthread

g.out: a.cpp
	clang++ --std=c++14 -Wall -g3 a.cpp -o g.out -lboost_system -pthread

tsan.out: a.cpp
	clang++ a.cpp --std=c++14 -fsanitize=thread -g3 -Wall -o tsan.out -lboost_system -pthread

