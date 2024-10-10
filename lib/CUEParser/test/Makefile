# Run basic unit tests for the CUEParser library

all: CUEParser_test
	./CUEParser_test

CUEParser_test: CUEParser_test.cpp ../src/CUEParser.cpp
	g++ -Wall -Wextra -g -ggdb -o $@ -I ../src $^
