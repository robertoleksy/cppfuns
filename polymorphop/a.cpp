// dual-licenced BSD licence (2-clause) and public domain 
// example of "polymorhpic" keys in map

#include <array>
#include <map>
#include <iostream>
#include <memory>
#include <functional>
#include <exception>
#include <cstring>

using namespace std;

enum class t_kind : unsigned char { http=1, pubkey=2 };

class addr {
	public:
		addr(t_kind k) : kind(k) { };
		virtual void show() { cout<<__func__<<" GENERAL! "; }
		virtual bool operator<(const addr & other) { throw runtime_error("addr op<"); }
		bool compare_kind_less(const addr & other) { return this->kind < other.kind; }

		t_kind kind;
};

class addr_http : public addr {
	public:
		string url;

		addr_http(const string & x) : addr(t_kind::http), url(x) {}

		void show() { cout<<url; }
		virtual bool operator<(const addr & other) {
			try {
				auto other_obj = dynamic_cast<const addr_http&>(other);
				return url < other_obj.url;
			} catch(std::bad_cast) { return compare_kind_less(other); }
		}
};

class addr_pubkey : public addr {
	public:
		addr_pubkey(unsigned char test) : addr(t_kind::pubkey) {
			memset(&pkey, test, pkey.size());
		}
		array<char,4096> pkey;
		void show() { cout<<"pubkey #"<<static_cast<int>(pkey[0]); }

		virtual bool operator<(const addr & other) {
			try {
				auto other_obj = dynamic_cast<const addr_pubkey&>(other);
				return memcmp(pkey.data(), other_obj.pkey.data(), pkey.size()) < 0;
			} catch(std::bad_cast) { return compare_kind_less(other); }
		}
};

string yesno(bool b) { if (b) return "YES "; return "no  "; }

#define TEST(X) { cout << #X << " is " << yesno(X) << endl; }

int main() {
	addr * a = new addr_pubkey(1);
	addr * b = new addr_pubkey(2);
	TEST( (*a) < (*b) );
	TEST( (*b) < (*a) );

	std::function<bool(const std::unique_ptr<addr>& a , const std::unique_ptr<addr>& b)> mycompare =
		[](const std::unique_ptr<addr>& a , const std::unique_ptr<addr>& b) {
		return *a < *b;
	};

	map< unique_ptr<addr> , std::string, decltype(mycompare) > themap(mycompare);

	themap.emplace( make_unique<addr_pubkey>(3) , "pubkey nr 3 ... data");
	themap.emplace( make_unique<addr_pubkey>(1) , "pubkey nr 1 ... data");
	themap.emplace( make_unique<addr_http>("bing.com") , "http to bing.com ... some state data here ..." );
	themap.emplace( make_unique<addr_http>("zzzzz.com") , "http for zzzz ... data here ... zzz" );
	themap.emplace( make_unique<addr_http>("aaa.com") , "http to aaa.com ... some state data here ..." );
	themap.emplace( make_unique<addr_pubkey>(2) , "pubkey nr 2");

	for(const auto & p : themap) {
		cout << "key [";
		p.first->show();
		cout << "] maps to value [" << (p.second) << "]" << endl;
	}
	cout << "All done " << endl;
}


