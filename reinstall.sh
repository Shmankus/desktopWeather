#!/bin/bash


cmake -B build/ --install-prefix ~/.local
cmake --build build/
cmake --install build/