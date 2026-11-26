#pragma once
#include <vector>
#include "Job.h"

struct HeapNode {
    Job* jobPtr;
    int deptIndex;
};

class MinHeap {
private:
    static const int MAX_SIZE = 500;
    HeapNode data[MAX_SIZE];
    int sz;

public:
    MinHeap();

    void insert(HeapNode node);
    HeapNode extractMin();
    void heapifyUp(int idx);
    void heapifyDown(int idx);
    bool compareJobs(Job* a, Job* b) const; // returns true if a has HIGHER priority (should come first)
    bool isEmpty() const;
    int size() const;
    std::vector<HeapNode> getAll() const;
};
