#include <sstream>
#include <iostream>
#include <string>
int main() {
	std::ostringstream oss;
	oss << 1 << 1 << "xxxxx";
	std::string x=oss.str();
}
//	oss << 5 << 5 << "foooooo";
//	string x = oss.str();



