# FirmSmith

Randomly generates FIRM graphs and applies libFirm optimizations
to check if they break.

Inspired by [CSmith](https://embed.cs.utah.edu/csmith/)
and implemented by Jeff Wagner in his
[bachelor thesis](http://pp.ipd.kit.edu/thesis.php?id=276).

## Requirements

Requires a C compiler and Make for building.
Uses LLDB via Python for investigating crashes.

For Ubuntu 16.04 the following works:

    sudo apt install build-essential liblldb-3.8 python-lldb-3.8

Unfortunately, the symlinks of the `python-lldb-3.8` are broken.
See [bug report](https://bugs.launchpad.net/ubuntu/+source/llvm-toolchain-3.8/+bug/1674926).
Fixing it with more symlinks:

    sudo ln -s ../../x86_64-linux-gnu/liblldb-3.8.so.1 /usr/lib/llvm-3.8/lib/liblldb.so
    sudo ln -s ../../x86_64-linux-gnu /usr/lib/llvm-3.8/lib/x86_64-linux-gnu

## Building

    make

This builds the `firmsmith` binary.

## Executing

You can simply run it:

    ./run-fuzzer.py

However, this will lead to quite unreadable reports,
because firmsmith never generates a main function,
which means linking will always fail.
Still, run-fuzzer correctly detects crashes,
which look different than a linker error.

Usually, you want to test a single optimization like `-fthread-jumps`.
In this case, run it like this:

    ./run-fuzzer.py --cparser-options="-fthread-jumps -c"

The fuzzer creates a lot of temporary files.
For cleanup run:

    bash clean

## Licence MIT

Copyright (c) 2017 Jeff Wagner

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
