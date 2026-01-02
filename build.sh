#!/bin/bash

set -ex

mkdir bin/
g++ -std=c++23 -lncurses -o bin/kaczynski src/main.cpp
