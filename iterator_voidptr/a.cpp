#include <iostream>
#include <thread>
#include <system_error>
#include <vector>

template <typename T>
const void * iterator_to_voidptr(T iter) {
	typedef typename std::iterator_traits<T>::value_type value_type;
	const value_type & obj = * iter; // pointed object, as some type
	const void* addr = static_cast<const void*>(&addr); // as raw address
	return addr;
}

int main() {
	std::vector<int> tab(10);

	auto iter = tab.cbegin();
	auto ptr = iterator_to_voidptr( iter );

	//std::cout << ptr << std::endl;
}


