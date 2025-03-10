#!/bin/bash

echo ""
echo "+----------------------------------------------------------+"
echo "| PREPARE: VALGRIND                                        |"
echo "+----------------------------------------------------------+"

# change working directory to repository directory
DIR=$(dirname "$(dirname "$(realpath "$0")")")
cd "$DIR" || exit 1

ARGS=""
if [ $# -eq 1 ] ; then
    ARGS=$1
fi

if [ -d cmake-build-valgrind ]; then
    rm -rf cmake-build-valgrind
fi

# generate make scripts, build application and install to bin folder
export CFLAGS="-O0"
export CXXFLAGS="-O0"
cmake . -Bcmake-build-valgrind -DCMAKE_BUILD_TYPE=Debug -DC104_VERSION_INFO=2.2.2 "$ARGS"
cd cmake-build-valgrind || exit 1
make -j"$(nproc+1)" _c104
cd .. || exit 1

rm ./c104/_c104*.so

# copy libraries for python package
cp ./cmake-build-valgrind/_c104*.so ./c104/

echo ""
echo "BUILD DONE!"
echo ""

export PYTHONPATH="$DIR:$PYTHONPATH"
export PYTHONUNBUFFERED=1

echo ""
echo "+----------------------------------------------------------+"
echo "| PROFILER: VALGRIND                                       |"
echo "+----------------------------------------------------------+"
#valgrind --leak-check=full --leak-resolution=med --track-origins=yes --read-inline-info=yes --suppressions=valgrind-python.supp python3 -E -tt ./test.py
valgrind --tool=memcheck --dsymutil=yes --leak-check=full --track-origins=yes --show-leak-kinds=all --leak-resolution=high --suppressions=./tests/valgrind-python.supp python3 -X showrefcount ./tests/test.py > valgrind.log 2>&1

# clean-up
rm ./c104/_c104*.so

echo ""
echo "+----------------------------------------------------------+"
echo "| PREPARE: GCC AddressSanitizer (ASan)                     |"
echo "+----------------------------------------------------------+"

if [ -d cmake-build-asan ]; then
    rm -rf cmake-build-asan
fi

# generate make scripts, build application and install to bin folder
export CFLAGS="-fsanitize=address,leak -O0"
export CXXFLAGS="-fsanitize=address,leak -O0"
cmake . -Bcmake-build-asan -DCMAKE_BUILD_TYPE=Debug -DC104_VERSION_INFO=2.2.2 "$ARGS"
cd cmake-build-asan || exit 1
make -j"$(nproc+1)" _c104
cd .. || exit 1

# copy libraries for python package
cp ./cmake-build-asan/_c104*.so ./c104/

echo ""
echo "BUILD DONE!"
echo ""

echo ""
echo "+----------------------------------------------------------+"
echo "| PROFILER: GCC AddressSanitizer (ASan)                    |"
echo "+----------------------------------------------------------+"

export ASAN_OPTIONS=verify_asan_link_order=0
LD_PRELOAD=$(gcc -print-file-name=libasan.so) python3 tests/test.py > asan.log 2>&1

# clean-up
rm ./c104/_c104*.so

echo ""
echo "+----------------------------------------------------------+"
echo "| DONE !!!                                                 |"
echo "+----------------------------------------------------------+"
echo ""
