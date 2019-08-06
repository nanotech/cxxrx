CXXFLAGS=-std=c++14 -Wall -Wextra

test: rx_test
	./rx_test

rx_test: rx_test.cc rx.h Maybe.h
	$(CXX) $(CXXFLAGS) rx_test.cc -o $@

.PHONY: test
