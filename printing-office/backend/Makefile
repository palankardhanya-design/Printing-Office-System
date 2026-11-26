CXX=g++
CXXFLAGS=-O2 -std=c++17 -I.
TARGET=server.exe
SRCS=server.cpp LinkedList.cpp MinHeap.cpp MergeEngine.cpp

all: $(TARGET)

$(TARGET): $(SRCS) httplib.h json.hpp
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRCS) -pthread

clean:
	del /Q $(TARGET)
