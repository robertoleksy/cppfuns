export N="b"   ; g++ --std=c++14 ${N}.cpp -static  -Wl,-Bstatic -lboost_locale -lboost_filesystem -lboost_system -o ${N}.bin && ldd ${N}.bin ; file ${N}.bin && ./${N}.bin
