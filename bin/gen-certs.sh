#!/bin/bash

DIR="$(dirname "$(dirname "$(realpath "$0")")")/tests/certs"

mkdir -p "$DIR"
cd "$DIR" || exit 1

echo "ROOT CA"
openssl genrsa -out ca.key 4096
openssl req -x509 -new -nodes -key ca.key -sha256 -subj '/CN=Local Test Certificate Authority' -days 3650 -out ca.crt

echo "SERVER1"
openssl genrsa -out server1.key 4096
openssl req -new -key server1.key -out server1.csr -subj '/CN=Local Test Server 1'
openssl x509 -req -in server1.csr -CA ca.crt -CAkey ca.key -CAcreateserial -out server1.crt -sha256 -days 3650

echo "SERVER2"
openssl genrsa -out server2.key 4096
openssl req -new -key server2.key -out server2.csr -subj '/CN=Local Test Server 2'
openssl x509 -req -in server2.csr -CA ca.crt -CAkey ca.key -CAcreateserial -out server2.crt -sha256 -days 3650

echo "CLIENT1"
openssl genrsa -out client1.key 4096
openssl req -new -key client1.key -out client1.csr -subj '/CN=Local Test Client 1'
openssl x509 -req -in client1.csr -CA ca.crt -CAkey ca.key -CAcreateserial -out client1.crt -sha256 -days 3650

echo "CLIENT2"
openssl genrsa -out client2.key 4096
openssl req -new -key client2.key -out client2.csr -subj '/CN=Local Test Client 2'
openssl x509 -req -in client2.csr -CA ca.crt -CAkey ca.key -CAcreateserial -out client2.crt -sha256 -days 3650
