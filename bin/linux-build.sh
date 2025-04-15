#!/bin/bash

DIR=$(dirname "$(dirname "$(realpath "$0")")")

CMD1="
mkdir -p /opt/c104/dist ; \
rm -rf /opt/c104/build/*linux-x86_64* ; \
rm -rf /opt/c104/dist/*manylinux*.whl ; \
/opt/python/cp39-cp39/bin/python3 -m pip wheel /opt/c104 ; \
/opt/python/cp310-cp310/bin/python3 -m pip wheel /opt/c104 ; \
/opt/python/cp311-cp311/bin/python3 -m pip wheel /opt/c104 ; \
/opt/python/cp312-cp312/bin/python3 -m pip wheel /opt/c104 ; \
/opt/python/cp313-cp313/bin/python3 -m pip wheel /opt/c104 ; \
/opt/python/cp313-cp313t/bin/python3 -m pip wheel /opt/c104 ; \
auditwheel repair ./c104-*-linux_x86_64.whl ; \
mv ./wheelhouse/* /opt/c104/dist/
"

docker run -it --rm -v "$DIR:/opt/c104" quay.io/pypa/manylinux_2_28_x86_64 /bin/bash -c "$CMD1"

CMD2="
/opt/python/cp37-cp37m/bin/python3 -m pip wheel /opt/c104 ; \
/opt/python/cp38-cp38/bin/python3 -m pip wheel /opt/c104 ; \
auditwheel repair ./c104-*-linux_x86_64.whl ; \
mv ./wheelhouse/* /opt/c104/dist/
"

docker run -it --rm -v "$DIR:/opt/c104" quay.io/pypa/manylinux2014_x86_64 /bin/bash -c "$CMD2"
