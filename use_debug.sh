cd Debug
make clean
make
cd ..
rm -rf lib
mkdir -p lib
cp Debug/libredis.so lib

