# Tunguska
Tunguska is a UCI-compliant chess engine written in C++11.

## Compiling
# On linux
>g++ -std=c++11 -O2 -DNDEBUG *.cpp -o tunguska_1_1

# On Windows
>cl /EHsc /O2 /Oi /GL /DNDEBUG /std=c++11 /Fe:tunguska_1_1_ns.exe *.cpp /link /LTCG && del *.obj

## Engine Strenght
2450 at CCRL.

## Special thanks
- The Chess Programming Wiki:
https://www.chessprogramming.org/Main_Page
- The *Programming a chess engine in C* series of videos:
https://www.youtube.com/playlist?list=PLZ1QII7yudbc-Ky058TEaOstZHVbT-2hg

## LICENSE
MIT License

Copyright (c) [2023] [Fernando T.]

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
