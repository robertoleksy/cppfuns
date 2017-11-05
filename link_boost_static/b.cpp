
#include <unistd.h>
#include <iostream>

#include <boost/filesystem.hpp>
#include <boost/locale.hpp>

using namespace boost::filesystem;

int main() {
	write(1, "Hello\n", 6);
	std::cout << "Hello from c++" << std::endl;
	std::cout << file_size("b.cpp") << std::endl;

	boost::locale::generator gen;

	std::cout << boost::locale::gettext("Yes") << std::endl;
}

