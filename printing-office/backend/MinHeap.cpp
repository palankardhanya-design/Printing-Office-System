#include "MinHeap.h"
#include <stdexcept>

MinHeap::MinHeap() : sz(0) {}

// compareJobs: returns true if 'a' has higher priority (should be extracted before 'b')
// Priority: submitTime ASC -> deptCode LEX ASC -> originalPos ASC
bool MinHeap::compareJobs(Job* a, Job* b) const {
    if (a->submitTime != b->submitTime)
        return a->submitTime < b->submitTime;
    if (a->deptCode != b->deptCode)
        return a->deptCode < b->deptCode;
    return a->originalPos < b->originalPos;
}

void MinHeap::heapifyUp(int idx) {
    while (idx > 0) {
        int parent = (idx - 1) / 2;
        // If current has higher priority than parent, swap
        if (compareJobs(data[idx].jobPtr, data[parent].jobPtr)) {
            HeapNode tmp = data[idx];
            data[idx] = data[parent];
            data[parent] = tmp;
            idx = parent;
        } else {
            break;
        }
    }
}

void MinHeap::heapifyDown(int idx) {
    while (true) {
        int left = 2 * idx + 1;
        int right = 2 * idx + 2;
        int highest = idx;

        if (left < sz && compareJobs(data[left].jobPtr, data[highest].jobPtr))
            highest = left;
        if (right < sz && compareJobs(data[right].jobPtr, data[highest].jobPtr))
            highest = right;

        if (highest != idx) {
            HeapNode tmp = data[idx];
            data[idx] = data[highest];
            data[highest] = tmp;
            idx = highest;
        } else {
            break;
        }
    }
}

void MinHeap::insert(HeapNode node) {
    if (sz >= MAX_SIZE)
        throw std::overflow_error("MinHeap is full");
    data[sz] = node;
    heapifyUp(sz);
    sz++;
}

HeapNode MinHeap::extractMin() {
    if (sz == 0)
        throw std::underflow_error("MinHeap is empty");
    HeapNode minNode = data[0];
    sz--;
    if (sz > 0) {
        data[0] = data[sz];
        heapifyDown(0);
    }
    return minNode;
}

bool MinHeap::isEmpty() const {
    return sz == 0;
}

int MinHeap::size() const {
    return sz;
}

std::vector<HeapNode> MinHeap::getAll() const {
    std::vector<HeapNode> result;
    for (int i = 0; i < sz; i++)
        result.push_back(data[i]);
    return result;
}
