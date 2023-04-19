#!/bin/bash

DIR=$(dirname "$(dirname "$(realpath "$0")")")

CMD="
  apt-get update ; \
  apt-get install -y --no-install-recommends build-essential cmake ninja-build ; \
  python3 -m pip wheel .
"

docker run -it --rm -v "$DIR:/opt/c104" -w /opt/c104 python:3.6 /bin/bash -c "$CMD"
docker run -it --rm -v "$DIR:/opt/c104" -w /opt/c104 python:3.7 /bin/bash -c "$CMD"
docker run -it --rm -v "$DIR:/opt/c104" -w /opt/c104 python:3.8 /bin/bash -c "$CMD"
docker run -it --rm -v "$DIR:/opt/c104" -w /opt/c104 python:3.9 /bin/bash -c "$CMD"
docker run -it --rm -v "$DIR:/opt/c104" -w /opt/c104 python:3.10 /bin/bash -c "$CMD"
docker run -it --rm -v "$DIR:/opt/c104" -w /opt/c104 python:3.11 /bin/bash -c "$CMD"
