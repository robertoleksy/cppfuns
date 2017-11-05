#include <pthread.h>

int Global;

void *Thread1(void *x) {
  Global = 42;
  return x;
}

int test(int delay1) {
  pthread_t t;
  pthread_create(&t, NULL, Thread1, NULL);

{
  int j=1;
  for (int i=0; i<delay1; ++i) j*=i;
}

  Global = 43;
  pthread_join(t, NULL);
  return Global;
}

int main() {
	int s=0;
	for (int i=0; i<1000; ++i) s += test(5000 + i%1000);
	return s;
}

