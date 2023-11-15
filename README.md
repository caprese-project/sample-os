# Caprese Sample Operating System

A sample operating system utilizing the Caprese microkernel. Use this as a reference to develop your own operating system powered by Caprese.

# How to Use

```sh
cmake -B build -G Ninja -DCMAKE_C_COMPILER:FILEPATH=/path/to/cc -DCMAKE_CXX_COMPILER:FILEPATH=/path/to/c++
cmake --build build
./script/simulate
```
