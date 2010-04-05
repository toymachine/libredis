cd Release
make clean
make
cd ..
rm -rf lib
mkdir -p lib
cp Release/libredis.so lib

