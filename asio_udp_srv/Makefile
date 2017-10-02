
.PHONY: run run_bug

all: a.out

run: a.out
	./a.out 192.168.1.107 2345 crypto=-1 wire_buf=4 wire_sock=2 wire_ios=2 wire_ios_thr=2 tuntap_weld=2 tuntap_sock=1 tuntap_ios=-1 tuntap_ios_thr=2 tuntap_weld_sleep=1 tuntap_block mt_strand mport

run_args: a.out
	./a.out $(ARGS)

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
	clang++ -g3 -O3 --std=c++14 -Wall a.cpp -o a.out -lboost_system -lsodium -pthread

g.out: a.cpp
	clang++ --std=c++14 -Wall -g3 a.cpp -o g.out -lboost_system -lsodium -pthread

tsan.out: a.cpp
	clang++ a.cpp --std=c++14 -fsanitize=thread -g3 -Wall -o tsan.out -lboost_system -lsodium -pthread
