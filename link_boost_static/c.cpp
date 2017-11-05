
// export N="c"   ; g++ --std=c++14 ${N}.cpp -static  -Wl,-Bstatic -lboost_locale  -lboost_system -licuuc -licui18n -pthread -o ${N}.bin && ldd ${N}.bin ; file ${N}.bin && ./${N}.bin

#include <boost/locale.hpp>

int main() {
	boost::locale::generator gen;
}

