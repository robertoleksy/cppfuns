// dual-licenced BSD licence (2-clause) and public domain
// example of "polymorhpic" keys in map

#include <iostream>

using namespace std;

template <typename TObj>
struct guard {

	TObj m_obj;

	template<typename TFun>
	typename std::result_of<TFun(TObj)>::type call(const TFun & fun) { return fun(m_obj); }

};

int main() {
	guard<int> g;
	g.m_obj=10;
	int a=5,b=10;
	auto result =	g.call( [&](int val) -> auto { a++; b++; return val * 3.1415; } );
	cout << result << " " << a << " " << b << endl;

}

