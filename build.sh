#!/bin/bash

set -ex

g++ -std=c++23 -lncursesw -o bin/kcz src/main.cpp
