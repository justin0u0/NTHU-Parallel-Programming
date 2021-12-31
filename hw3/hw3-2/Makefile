NVFLAGS  := -std=c++11 -O3 -Xptxas="-v" -arch=sm_61 
LDFLAGS  := -lm
EXES     := hw3-2 hw3-3

alls: $(EXES)

clean:
	rm -f $(EXES)

seq: seq.cc
	g++ $(CXXFLAGS) -o $@ $?

hw3-2: hw3-2.cu
	nvcc $(NVFLAGS) -o $@ $?

hw3-3: hw3-3.cu
	nvcc $(NVFLAGS) -Xcompiler -fopenmp -o $@ $?
