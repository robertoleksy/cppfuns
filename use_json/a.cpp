#include "json.hpp"
#include <iostream>
#include <fstream>
#include <map>
#include <list>
#include <vector>

using json = nlohmann::json;

using namespace std;

string makepw(int len) {
	string ret;
	for (int x=0; x<len; ++x) ret += (char)( 'a' + (random()%('z'-'a')) );
	return ret;
};

json read_json_file(const string & input_name) {
	ifstream input_file(input_name);
	string input_data;
	while (input_file.good()) {
		string line; getline(input_file,line);
		input_data += line;
	}
	json j_in = json::parse( input_data );
	return j_in;
}

class c_user {
	protected:
		string m_name, m_pass;
		map<string,string> m_addrbook; // username => userkey
	public:
		c_user(const string &name);
		void addrbook_add(const string & name , const string & pubkey);

		void save(const string &fname) const;
		void load(const string &fname);
};

c_user::c_user(const string &name)
	: m_name(name), m_pass("")
{
	m_pass = makepw(20);
}


void c_user::save(const string &fname) const {
	json j;
	j["uname"]=m_name;
	j["password"]=m_pass;
	j["address_book"] = m_addrbook;
	ofstream out_file(fname);
	out_file << j << endl;
}

void c_user::load(const string &fname) {
	m_name=""; m_pass=""; m_addrbook.clear();

	json j_in = read_json_file("a.json");

	m_name = j_in["my_login"];
	m_pass = j_in["password"];

	auto j = j_in["address_book"];
	for (json::iterator it = j.begin(); it != j.end(); ++it) {
//		  std::cout << it.key() << " " << *it << '\n';
		  m_addrbook[ it.key() ] = it.value();
	}
}

void c_user::addrbook_add(const string & name , const string & pubkey) {
	m_addrbook[name] = pubkey;

}


void read() {


#if 0
json j;
j.push_back("foo");
j.push_back(1);
j.push_back(true);

// iterate the array
for (json::iterator it = j.begin(); it != j.end(); ++it) {
	  std::cout << *it << '\n';
}
#endif


#if 1
	json j_in = read_json_file("a.json");

	cout << j_in.size() << endl;

	cout << j_in["my_login"] << endl;
	cout << j_in["password"] << endl;

	auto j_addr = j_in["address_book"];

//	auto x = j_addr.get< const map<const string, const string> >();

	auto & j = j_addr;

	for (json::iterator it = j.begin(); it != j.end(); ++it) {
		  std::cout << it.key() << " " << *it << '\n';
	}

//	for(const auto j_addr_entry : j_addr) {
//		cout << j_addr_entry.key() << " = " << j_addr_entry.value() << endl;
//	}

#endif
}

void write() {
	json j;

	map<string,string> uname_m; uname_m["uname"]="john";
	json uname( uname_m );
	//j.push_back(uname);
	j["uname"]="john";
	j["password"]=makepw(20);


	map<string,string> addrbook;
	addrbook["kamil"]="1kamilkey222";
	addrbook["bob"]="1bob63665223";

	j["address_book"] = addrbook;

	cout << "\n\n" << j << endl;
}

int main() {
	c_user alice("Alice");
	alice.addrbook_add("Bob","1bbbbbbb");
	alice.save("alice.json");

	c_user loaded("new");
	loaded.load("a.json");
	loaded.addrbook_add("newfriend","1fffff");
	loaded.addrbook_add("newfriend2","1fffff22222");
	loaded.save("b.json");

	//read();
	//write();
}



