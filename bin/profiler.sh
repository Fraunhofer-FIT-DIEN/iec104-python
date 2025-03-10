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
cmake . -Bcmake-build-debug -DCMAKE_BUILD_TYPE=Debug -DC104_VERSION_INFO=2.2.2 "$ARGS"
cd cmake-build-debug || exit 1
make -j"$(nproc+1)" _c104
cd .. || exit 1

rm ./c104/_c104*.so

# copy libraries for python package
cp ./cmake-build-debug/_c104*.so ./c104/

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
