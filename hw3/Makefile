CC = gcc
CXX = g++
CXXFLAGS = -O3 -pthread
CFLAGS = -O3 -lm -pthread
TARGETS = hw3-1 hw3-2 hw3-3
NVFLAGS  := -std=c++11 -O3 -Xptxas="-v" -arch=sm_61 

.PHONY: all
all: $(TARGETS)

.PHONY: clean
clean:
	rm -f $(TARGETS)

seq: seq.cc
	g++ $(CXXFLAGS) -o $@ $?

hw3-2: hw3-2.cu
	nvcc $(NVFLAGS) -o $@ $?

hw3-3: hw3-3.cu
	nvcc $(NVFLAGS) -Xcompiler -fopenmp -o $@ $?
