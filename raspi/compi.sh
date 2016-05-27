#!/bin/bash
cc -o ii5 ii5.c -DCMAKE_BUILD -O3 -fvisibility=hidden -I. -pthread -g -fvisibility=hidden -lz -lwebsockets -lssl -lcrypto -ldl -lm
