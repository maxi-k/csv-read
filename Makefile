##
# csv-read
#
# @file
# @version 0.1
COMPILER = g++
INCLUDE = -Ivendor
FLAGS = -g -Wall -std=c++20 -pthread -mavx2 -fno-omit-frame-pointer -fsanitize=address

test: clean tester
ifeq ($(target),debug)
	gdb ./tester
else
	./tester
endif

tester:
	${COMPILER} ${INCLUDE} test/tester.cpp -O0 ${FLAGS} -o tester

clean:
	rm -f tester

.PHONY: clean test

# end
