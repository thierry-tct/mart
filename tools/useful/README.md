generate the c++ code to generate llvm instructions for driver.h with the following commands:

#clang -c -emit-llvm driver.c -o driver.bc
#llc -O0 -march=cpp driver.bc -o gen_driver.cpp

