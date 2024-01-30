#!/bin/bash

echo ""
echo "+----------------------------------------------------------+"
echo "| PROFILER                                                 |"
echo "+----------------------------------------------------------+"

# change working directory to repository directory
DIR=$(dirname "$(dirname "$(realpath "$0")")")
cd "$DIR" || exit 1

ARGS=""
if [ $# -eq 1 ] ; then
    ARGS=$1
fi

# generate make scripts, build application and install to bin folder
cmake . -Bcmake-build-debug -DCMAKE_BUILD_TYPE=Debug "$ARGS"
cd cmake-build-debug || exit 1
make c104
cd .. || exit 1

# copy libraries for python package
cp cmake-build-debug/c104.so tests/

echo ""
echo "BUILD DONE!"
echo ""

cd tests || exit 1

echo ""
echo "+----------------------------------------------------------+"
echo "| PROFILER: VALGRIND                                       |"
echo "+----------------------------------------------------------+"
#valgrind --leak-check=full --leak-resolution=med --track-origins=yes --read-inline-info=yes --suppressions=valgrind-python.supp python3 -E -tt ./test.py
valgrind --tool=memcheck --dsymutil=yes --track-origins=yes --show-leak-kinds=all --trace-children=yes --leak-resolution=high --suppressions=valgrind-python.supp python3 -X showrefcount ./test.py > valgrind.log 2>&1

echo ""
echo "+----------------------------------------------------------+"
echo "| PROFILER: GOOGLE-PERFTOOLS                               |"
echo "+----------------------------------------------------------+"
python3 -m yep -- test.py
pprof --callgrind c104.so test.py.prof > callgrind.c104

cd ..

echo ""
echo "Open file tests/callgrind.c104 in KCachegrind or QCachegrind to visualize metrics."

echo ""
echo "+----------------------------------------------------------+"
echo "| DONE !!!                                                 |"
echo "+----------------------------------------------------------+"
echo ""
