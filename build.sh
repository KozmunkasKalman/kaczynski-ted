#!/bin/bash

set -ex

g++ -std=c++23 -lncurses -o bin/kaczynski src/main.cpp
