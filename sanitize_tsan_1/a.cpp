#include <pthread.h>
#include <iostream>

int Global;
void *Thread1(void *x) {
  Global = 42;
  return x;
}
void some_wait() {
  volatile long x=0;
  for (long a=0; a<1000*1000; ++a) {
  	for (long b=0; b<1000*1000; ++b) {
  		x = (x+1) % 10;
		}
	}
}
int main() {
	std::cout << "in main" << std::endl;
	for (int i=0; i<100; ++i) {
  pthread_t t;
  pthread_create(&t, NULL, Thread1, NULL);
  Global = 43;
  pthread_join(t, NULL);
	}
	std::cout << "after the racy code" << std::endl;
  some_wait();

	return Global;
}


