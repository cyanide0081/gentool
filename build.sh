#!/bin/sh
# build script for *nixes

clang -o gentool gentool.c \
    -Os \
    -Wall \
    -Wextra \
    -pedantic \
    -std=c99 \
    $(pkg-config --cflags --libs x11) \
    -D_DEFAULT_SOURCE
