#pragma once
#include <string>

struct Job {
    std::string id;
    int submitTime;
    std::string deptCode;
    int originalPos;
    Job* next = nullptr;
};
