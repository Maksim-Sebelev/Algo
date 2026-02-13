#!/bin/bash
g++ -Werror=vla -O2 -Wall -Werror -std=c++20 -lm "$1".cpp -o build/"$1" || exit 1

if [ $# -eq 2 ]; then
    build/"$1" < "$2"
else
    build/"$1"
fi
