# Tunguska
Tunguska is a UCI-compliant chess engine written in C++11.

A compiled 64-bit binary for Mac OS X is included.

## Compiling
The flags -O2 and -DNDEBUG are important for performance:

>g++ -std=c++11 -O2 -DNDEBUG *.cpp -o tunguska_1_0

## Engine Strenght
It plays around 2150-2200 ELO. 

## To Dos
 - PVS + aspiration windows
 - Pawn and evaluation hashtables
 - Improve evaluation code (it's too simply by now)
 - Improve qsearch
 - Improve prunning and move ordering
 - Tunning
 - Provide a makefile

## Special thanks
- The Chess Programming Wiki, which is a great resource for beginners like me:
https://chessprogramming.wikispaces.com
- The *Programming a chess engine in C* series of videos, which helped me a lot:
https://www.youtube.com/playlist?list=PLZ1QII7yudbc-Ky058TEaOstZHVbT-2hg