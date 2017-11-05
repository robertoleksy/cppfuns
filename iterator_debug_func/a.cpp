#include <iostream>
#include <thread>
#include <list>
#include <system_error>
#include <vector>

using namespace std;

template <typename T>
const void * iterator_to_voidptr(T iter) {
	typedef typename std::iterator_traits<T>::value_type value_type;
	const value_type & obj = * iter; // pointed object, as some type
	const void* addr = static_cast<const void*>(&obj); // as raw address
	return addr;
}

void debug(int X) {
	cout << "I:" << X << endl;
}

void debug(string X) {
	cout << "S:" << X << endl;
}

template <typename T>
void debug(const T & X,
	typename std::enable_if<
	std::is_same<
		typename std::iterator_traits<T>::iterator_category
   , std::random_access_iterator_tag
	>
	::value >::type * = 0


/*
	typename std::enable_if<
		std::is_same<
			std::iterator_traits<X>::iterator_category
			, std::random_access_iterator_tag
		>
	::value>::type * = 0
	*/
) {
	cout << "Iter:" << iterator_to_voidptr(X) << endl;
}

int main() {
	vector<int> vec(2);
	debug(42);
	debug(vec.begin());

	list<int> lll;
	lll.push_back( 999 );
	debug( lll.begin() );
}


