
.PHONY: run

all: asan.out

run: asan

asan: asan.out
	ASAN_OPTIONS=check_initialization_order=true,detect_stack_use_after_return=true,debug=true,print_stats=true,detect_invalid_pointer_pairs=100,dump_instruction_bytes=true  ./asan.out	./asan.out

asan.out: a.cpp
	clang++ a.cpp --std=c++14 -fsanitize=address,undefined -g3 -Wall -o asan.out -lboost_system -pthread -O0 \
		-fno-omit-frame-pointer

normal.out: a.cpp
	clang++ a.cpp --std=c++14 -fsanitize=undefined -g3 -Wall -o normal.out -lboost_system -pthread -O0

