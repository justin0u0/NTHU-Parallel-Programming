CC = gcc
CXX = g++
CFLAGS = -lm -O3
lab2_pthread: CFLAGS += -pthread
lab2_omp: CFLAGS += -fopenmp
lab2_hybrid: CC = mpicc
lab2_hybrid: CXX = mpicxx
lab2_hybrid: CFLAGS += -fopenmp
CXXFLAGS = $(CFLAGS)
TARGETS = lab2_pthread lab2_omp lab2_hybrid

.PHONY: all
all: $(TARGETS)

.PHONY: clean
clean:
	rm -f $(TARGETS) $(TARGETS:=.o)
