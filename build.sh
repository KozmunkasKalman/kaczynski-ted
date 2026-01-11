#!/bin/bash

set -ex

g++ src/main.cpp -o bin/kcz -std=c++23 -lncursesw
