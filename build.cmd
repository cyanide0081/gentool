@rem build script for your cracked windows
@echo off
clang -o gentool.exe gentool.c^
    -Os^
    -Wall^
    -Wextra^
    -pedantic^
    -std=c99^
    -luser32^
    -lbcrypt^
    -lmsvcrt^
    -Wl,/NODEFAULTLIB:libcmt
