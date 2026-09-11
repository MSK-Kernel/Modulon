#!/bin/bash

#compile the compiler

cc -O2 -std=c11 -Wall -Wextra -Wpedantic -static src/compiler.c -o Modulon

