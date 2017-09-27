#include <iostream>
#include <thread>
#include <vector>
#include <system_error>

#include <boost/asio.hpp>
#include <iostream>
#include <chrono>
#include <atomic>
#include <mutex>

using namespace std;

int & give_ref() {
	int foo_local = 55;
	return foo_local;
}

void foo_local() {
	int & rrr = give_ref();
	cout << rrr << endl;
}

void foo() {
	int tab2[16];
	int a=5;
	int b=5;
	const int tab_size = 1024;
	int tab[tab_size];
	volatile int c;
	c=a;
	a=c;
	b=c;
	int d = a*b;
	int * ptr = &b;
	--ptr;
	*ptr = 5;
	if (a<0) a=-a;
	tab[  a % tab_size ] = b;
	cout << "Number:" << d << " and " << (*ptr) << endl;
}

void start_test(int nr) {
	if (nr==1) foo();
	if (nr==2) foo_local();
}

int main() {
	volatile int nr=1;
	start_test(nr);

	cout << "Test." << endl;

}


