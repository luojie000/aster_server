CXX=g++
CXXFLAGS=-std=c++17 -Wall -Wextra -g -Iinclude
src_files=$(wildcard src/*.cpp)
target=aster_server

$(target):
	$(CXX) $(CXXFLAGS) $(src_files) -o $(target)

clean:
	rm -f $(target)