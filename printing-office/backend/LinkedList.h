#pragma once
#include <string>
#include "Job.h"

class DeptQueue {
private:
    Job* head;
    Job* tail;
    int count;

public:
    DeptQueue();
    ~DeptQueue();

    void addJob(const std::string& id, int time, const std::string& dept, int pos);
    Job* getHead() const;
    void display() const;
    bool isEmpty() const;
    int size() const;
};
