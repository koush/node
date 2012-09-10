export CC='gcc -m32'
./configure
make -j32
cp node node-32
unset CC
./configure
make -j32
cp node node-64
rm node
lipo node-64 -arch i386 node-32 -output node -create
