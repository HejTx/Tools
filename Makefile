CXX = g++
LDLIBS = -lsodium

all: todolist.exe
	@:

todolist.exe: todolist.cpp todolist.hpp sifrovani.hpp
	$(CXX) todolist.cpp -o $@ $(LDLIBS)

test_todolist.exe: test_todolist.cpp todolist.hpp sifrovani.hpp
	$(CXX) test_todolist.cpp -o $@ $(LDLIBS)

test: test_todolist.exe
	./test_todolist.exe

clean:
	rm -f todolist.exe test_todolist.exe

.PHONY: all test clean
