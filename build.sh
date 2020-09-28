echo "-- COMPILING --"
mv runexec runexec.bak
g++ erasurecodetest.cpp wirehair/*.cpp -g -O0 -march=native -o runexec
echo "-- RUNNING --"
./runexec
