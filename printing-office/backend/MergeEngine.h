#pragma once
#include <string>
#include <vector>
#include "LinkedList.h"
#include "MinHeap.h"

struct HeapSnapshot {
    std::string jobId;
    std::string deptCode;
    int submitTime;
    int originalPos;
    int heapIndex;
};

struct MergeStep {
    std::string type;        // "insert", "extract", "append", "heapify_up", "heapify_down"
    std::string jobId;
    std::string deptCode;
    int submitTime;
    std::vector<HeapSnapshot> heapState;
    std::string explanation;
};

struct MergeResult {
    std::vector<Job*> mergedTimeline;
    std::vector<MergeStep> steps;
    int heapInsertions;
    int heapExtractions;
    int heapComparisons;
    int duplicateTimestamps;
    int emptyQueues;
    double executionTimeMs;
};

class MergeEngine {
public:
    // recordSteps=false skips step tracing (for large inputs / stress test)
    static MergeResult kWayMerge(std::vector<DeptQueue*>& queues, bool recordSteps = true);

private:
    static std::vector<HeapSnapshot> snapshotHeap(const MinHeap& heap);
};
