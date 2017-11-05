#include <iostream>
#include <system_error>
using namespace std;
struct errBase : public virtual std::runtime_error { errBase(const char *w) : std::runtime_error(w)  {} };
struct errSys : public virtual std::system_error , public virtual errBase {	errSys(const char *w) : std::runtime_error(w), errBase(w) {} };
int main() { try { throw errSys("Warp core offline, Geordi"); }	catch (std::runtime_error &ex) { cout << ex.what(); } }



