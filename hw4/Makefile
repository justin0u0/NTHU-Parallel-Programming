CXX = mpicxx
CXXFLAGS = -std=c++11 -O3 # -fsanitize=address -g
LDFLAGS = -pthread
TARGETS = mapreduce reducer_test mapper_test thread_pool_test logger_test

.PHONY: all
all: $(TARGETS)

.PHONY: clean
clean:
	rm -f $(TARGETS)

%: %.cc
	$(CXX) -o $@ $(CXXFLAGS) $(LDFLAGS) $^
