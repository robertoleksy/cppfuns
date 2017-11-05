// test with UBSAN: e.g.: clang++ -fsanitize=undefined --std=c++14 -Wall a.cpp -o a.out
#include <iostream>
#include <memory>
#include <functional>
// #include <boost/any.hpp>
#include "myany.hpp"
int main() { using namespace std;
	cout << "string is " << typeid(string).name() << endl;
	cout << "const string is " << typeid(const string).name() << endl;
	const string cs("foo");
	boost::any any1(cs);
	cout << "any type is " << any1.type().name() << endl;
	string * ptr = boost::any_cast<string>(&any1); cout<<(ptr?"not-null":"NULL(good)");
}
