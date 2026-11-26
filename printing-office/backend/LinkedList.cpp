#include "LinkedList.h"
#include <iostream>

DeptQueue::DeptQueue() : head(nullptr), tail(nullptr), count(0) {}

DeptQueue::~DeptQueue() {
    // Note: Jobs are managed by MergeEngine; don't delete here
    // if jobs are transferred. Only free if this queue owns them.
    // For safety, we do NOT free here — MergeEngine owns cleanup.
}

void DeptQueue::addJob(const std::string& id, int time, const std::string& dept, int pos) {
    Job* newJob = new Job();
    newJob->id = id;
    newJob->submitTime = time;
    newJob->deptCode = dept;
    newJob->originalPos = pos;
    newJob->next = nullptr;

    if (tail == nullptr) {
        head = tail = newJob;
    } else {
        tail->next = newJob;
        tail = newJob;
    }
    count++;
}

Job* DeptQueue::getHead() const {
    return head;
}

void DeptQueue::display() const {
    Job* cur = head;
    while (cur) {
        std::cout << "[" << cur->id << ", t=" << cur->submitTime << "] ";
        cur = cur->next;
    }
    std::cout << std::endl;
}

bool DeptQueue::isEmpty() const {
    return head == nullptr;
}

int DeptQueue::size() const {
    return count;
}
