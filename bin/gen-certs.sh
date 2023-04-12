#!/bin/bash

DIR="$(dirname "$(dirname "$(realpath "$0")")")/tests/certs"

mkdir -p "$DIR"
cd "$DIR" || exit 1

echo "ROOT CA"
openssl genrsa -out ca.key 4096
openssl req -x509 -new -nodes -key ca.key -sha256 -days 3650 -out ca.crt

echo "SERVER"
openssl genrsa -out server.key 4096
openssl req -new -key server.key -out server.csr
openssl x509 -req -in server.csr -CA ca.crt -CAkey ca.key -CAcreateserial -out server.crt -sha256 -days 3650

echo "CLIENT1"
openssl genrsa -out client1.key 4096
openssl req -new -key client1.key -out client1.csr
openssl x509 -req -in client1.csr -CA ca.crt -CAkey ca.key -CAcreateserial -out client1.crt -sha256 -days 3650

echo "CLIENT2"
openssl genrsa -out client2.key 4096
openssl req -new -key client2.key -out client2.csr
openssl x509 -req -in client2.csr -CA ca.crt -CAkey ca.key -CAcreateserial -out client2.crt -sha256 -days 3650
