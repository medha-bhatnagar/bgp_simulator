CXX      = g++
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra -Iinclude

SRC = src/policy.cpp src/as_node.cpp src/as_graph.cpp
OBJ = $(SRC:.cpp=.o)

.PHONY: all clean test

all: bgp_sim run_tests

# Main simulator binary
bgp_sim: $(SRC) src/main.cpp include/*.h
	$(CXX) $(CXXFLAGS) $(SRC) src/main.cpp -o bgp_sim

# Test binary
run_tests: $(SRC) tests/tests.cpp include/*.h
	$(CXX) $(CXXFLAGS) $(SRC) tests/tests.cpp -o run_tests

# Build and run tests
test: run_tests
	./run_tests

clean:
	rm -f bgp_sim run_tests src/*.o
