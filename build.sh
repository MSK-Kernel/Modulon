#!/bin/sh

cc -std=c11 -Wall -Wextra -Wno-unused-parameter -Iinclude -o bin/modulon src/*.c
