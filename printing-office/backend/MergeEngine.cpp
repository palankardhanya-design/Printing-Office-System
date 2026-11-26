#include "MergeEngine.h"
#include <chrono>
#include <set>

std::vector<HeapSnapshot> MergeEngine::snapshotHeap(const MinHeap& heap) {
    std::vector<HeapSnapshot> snap;
    std::vector<HeapNode> nodes = heap.getAll();
    for (int i = 0; i < (int)nodes.size(); i++) {
        HeapSnapshot hs;
        hs.jobId      = nodes[i].jobPtr->id;
        hs.deptCode   = nodes[i].jobPtr->deptCode;
        hs.submitTime = nodes[i].jobPtr->submitTime;
        hs.originalPos= nodes[i].jobPtr->originalPos;
        hs.heapIndex  = i;
        snap.push_back(hs);
    }
    return snap;
}

MergeResult MergeEngine::kWayMerge(std::vector<DeptQueue*>& queues, bool recordSteps) {
    MergeResult result;
    result.heapInsertions     = 0;
    result.heapExtractions    = 0;
    result.heapComparisons    = 0;
    result.duplicateTimestamps= 0;
    result.emptyQueues        = 0;

    auto startTime = std::chrono::high_resolution_clock::now();

    MinHeap heap;
    std::vector<Job*> ptrs(queues.size(), nullptr);

    // ── Phase 1: insert head of every non-empty queue ────────────────────────
    for (int i = 0; i < (int)queues.size(); i++) {
        if (queues[i]->isEmpty()) {
            result.emptyQueues++;
            continue;
        }
        ptrs[i] = queues[i]->getHead();
        HeapNode node; node.jobPtr = ptrs[i]; node.deptIndex = i;
        heap.insert(node);
        result.heapInsertions++;

        if (recordSteps) {
            MergeStep step;
            step.type        = "insert";
            step.jobId       = ptrs[i]->id;
            step.deptCode    = ptrs[i]->deptCode;
            step.submitTime  = ptrs[i]->submitTime;
            step.heapState   = snapshotHeap(heap);
            step.explanation = "Initial insert: " + ptrs[i]->id +
                               " (dept " + ptrs[i]->deptCode +
                               ", t=" + std::to_string(ptrs[i]->submitTime) + ") into heap";
            result.steps.push_back(step);
        }
    }

    int lastTime = -1;

    // ── Phase 2: main merge loop ─────────────────────────────────────────────
    while (!heap.isEmpty()) {
        HeapNode minNode = heap.extractMin();
        result.heapExtractions++;
        result.heapComparisons++;

        Job* extractedJob = minNode.jobPtr;
        int  di           = minNode.deptIndex;

        if (extractedJob->submitTime == lastTime && lastTime != -1)
            result.duplicateTimestamps++;
        lastTime = extractedJob->submitTime;

        if (recordSteps) {
            MergeStep step;
            step.type        = "extract";
            step.jobId       = extractedJob->id;
            step.deptCode    = extractedJob->deptCode;
            step.submitTime  = extractedJob->submitTime;
            step.heapState   = snapshotHeap(heap);
            step.explanation = "Extracted min: " + extractedJob->id +
                               " (dept " + extractedJob->deptCode +
                               ", t=" + std::to_string(extractedJob->submitTime) + ") from heap";
            result.steps.push_back(step);
        }

        result.mergedTimeline.push_back(extractedJob);

        if (recordSteps) {
            MergeStep step;
            step.type        = "append";
            step.jobId       = extractedJob->id;
            step.deptCode    = extractedJob->deptCode;
            step.submitTime  = extractedJob->submitTime;
            step.heapState   = snapshotHeap(heap);
            step.explanation = "Appended " + extractedJob->id + " to merged timeline";
            result.steps.push_back(step);
        }

        // Advance this department's pointer
        ptrs[di] = extractedJob->next;
        if (ptrs[di] != nullptr) {
            HeapNode nextNode; nextNode.jobPtr = ptrs[di]; nextNode.deptIndex = di;
            heap.insert(nextNode);
            result.heapInsertions++;
            result.heapComparisons++;

            if (recordSteps) {
                MergeStep step;
                step.type        = "insert";
                step.jobId       = ptrs[di]->id;
                step.deptCode    = ptrs[di]->deptCode;
                step.submitTime  = ptrs[di]->submitTime;
                step.heapState   = snapshotHeap(heap);
                step.explanation = "Inserted next: " + ptrs[di]->id +
                                   " from dept " + ptrs[di]->deptCode +
                                   " (t=" + std::to_string(ptrs[di]->submitTime) + ")";
                result.steps.push_back(step);
            }
        }
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed = endTime - startTime;
    result.executionTimeMs = elapsed.count();

    return result;
}
