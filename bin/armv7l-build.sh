#!/bin/bash

DIR=$(dirname "$(dirname "$(realpath "$0")")")

# use cmake from repository

CMD1="
apt-get update -qq >/dev/null ; \
DEBIAN_FRONTEND=noninteractive apt-get install -y -qq cmake ; \
sed -i \"s@    \\\"cmake@#    \\\"cmake@\" /opt/c104/pyproject.toml ; \
mkdir -p /opt/c104/dist ; \
rm -rf /opt/c104/build/* ; \
python3 -m pip wheel /opt/c104 ; \
mv ./c104-*.whl /opt/c104/dist/ ; \
sed -i \"s@#    \\\"cmake@    \\\"cmake@\" /opt/c104/pyproject.toml
"

docker run -it --rm -v "$DIR:/opt/c104" python:3.13-bullseye /bin/bash -c "$CMD1"
docker run -it --rm -v "$DIR:/opt/c104" python:3.12-bullseye /bin/bash -c "$CMD1"
docker run -it --rm -v "$DIR:/opt/c104" python:3.11-bullseye /bin/bash -c "$CMD1"
docker run -it --rm -v "$DIR:/opt/c104" python:3.10-bullseye /bin/bash -c "$CMD1"
docker run -it --rm -v "$DIR:/opt/c104" python:3.9-bullseye /bin/bash -c "$CMD1"
docker run -it --rm -v "$DIR:/opt/c104" python:3.8-bullseye /bin/bash -c "$CMD1"
docker run -it --rm -v "$DIR:/opt/c104" python:3.7-bullseye /bin/bash -c "$CMD1"
