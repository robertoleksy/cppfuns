#include <vector>
#include <iostream>
#include <memory>
using namespace std;

int main()
{	typedef unsigned int uint; vector<shared_ptr<uint>> A; vector<uint> B; uint n=0; while (1) { ++n; if (A.size()<100) A.push_back(make_shared<uint>(n)); if (B.size()<A.size()) B.push_back(* A.back());
		B[n%B.size()] *= 2; } }

/*
int main() {
	typedef unsigned int uint;
	vector<shared_ptr<uint>> A;
	vector<uint> B;
	uint n=0;
	while (1) {
		++n;
		if (A.size()<100) A.push_back(make_shared<uint>(n));
		if (B.size()<A.size()) B.push_back(* A.back());
		else A.pop_back();
		auto p=n % B.size();
		B.at(p) *= 2;
		//cout << A.size() << " " << B.size() << " " << n << " " << p << endl;
	}
}

*/
