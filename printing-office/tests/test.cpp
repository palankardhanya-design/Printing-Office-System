/**
 * test.cpp — Standalone test runner for The Printing Office K-Way Merge.
 *
 * Does NOT use httplib. Tests the DSA core directly.
 *
 * Compile:
 *   g++ -std=c++17 -o test.exe test.cpp ../backend/LinkedList.cpp \
 *       ../backend/MinHeap.cpp ../backend/MergeEngine.cpp -I../backend
 *
 * Run:
 *   test.exe
 */

#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <set>
#include <iomanip>
#include <chrono>

#include "../backend/LinkedList.h"
#include "../backend/MinHeap.h"
#include "../backend/MergeEngine.h"

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────

static int gPass = 0;
static int gFail = 0;

/** Extract job IDs from a MergeResult in timeline order. */
static std::vector<std::string> extractIds(const MergeResult& result) {
    std::vector<std::string> ids;
    ids.reserve(result.mergedTimeline.size());
    for (Job* j : result.mergedTimeline)
        ids.push_back(j->id);
    return ids;
}

/** Free all queues and the jobs they point at via the timeline. */
static void freeQueues(std::vector<DeptQueue*>& queues) {
    for (DeptQueue* q : queues) delete q;
    queues.clear();
}

/** Free the job nodes returned in a MergeResult's timeline. */
static void freeTimeline(MergeResult& result) {
    for (Job* j : result.mergedTimeline) delete j;
    result.mergedTimeline.clear();
}

/** Report pass/fail for a single test case. */
static void report(const std::string& name,
                   bool pass,
                   const std::string& detail = "") {
    std::string label = "[" + name + "]";
    // Pad to width 42
    std::string line = label;
    while ((int)line.size() < 42) line += '.';
    if (pass) {
        std::cout << line << " PASS\n";
        ++gPass;
    } else {
        std::cout << line << " FAIL";
        if (!detail.empty()) std::cout << "  ← " << detail;
        std::cout << "\n";
        ++gFail;
    }
}

/** Check that actual == expected (element-wise). */
static bool checkOrder(const std::vector<std::string>& actual,
                        const std::vector<std::string>& expected,
                        std::string& failDetail) {
    if (actual.size() != expected.size()) {
        failDetail = "size mismatch: got " + std::to_string(actual.size())
                   + ", want " + std::to_string(expected.size());
        return false;
    }
    for (size_t i = 0; i < expected.size(); ++i) {
        if (actual[i] != expected[i]) {
            failDetail = "pos " + std::to_string(i)
                       + ": got \"" + actual[i]
                       + "\", want \"" + expected[i] + "\"";
            return false;
        }
    }
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// TC1 — Basic three-department merge
// ─────────────────────────────────────────────────────────────────────────────
// A: A2(t=2), A7(t=7), A9(t=9)
// B: B1(t=1), B7(t=7)
// C: C3(t=3), C10(t=10)
// Expected: B1, A2, C3, A7, B7, A9, C10
static void tc1_basic_merge() {
    std::vector<DeptQueue*> queues;

    DeptQueue* a = new DeptQueue();
    a->addJob("A2",  2, "A", 0);
    a->addJob("A7",  7, "A", 1);
    a->addJob("A9",  9, "A", 2);
    queues.push_back(a);

    DeptQueue* b = new DeptQueue();
    b->addJob("B1",  1, "B", 0);
    b->addJob("B7",  7, "B", 1);
    queues.push_back(b);

    DeptQueue* c = new DeptQueue();
    c->addJob("C3",  3, "C", 0);
    c->addJob("C10", 10, "C", 1);
    queues.push_back(c);

    MergeResult result = MergeEngine::kWayMerge(queues);

    std::vector<std::string> expected = {"B1","A2","C3","A7","B7","A9","C10"};
    std::vector<std::string> actual   = extractIds(result);

    std::string detail;
    bool pass = checkOrder(actual, expected, detail);

    // Verify stats
    if (pass && (int)result.mergedTimeline.size() != 7) {
        pass = false;
        detail = "timeline size should be 7";
    }

    report("TC1 Basic Merge", pass, detail);

    freeTimeline(result);
    freeQueues(queues);
}

// ─────────────────────────────────────────────────────────────────────────────
// TC2 — Duplicate timestamps (tie-breaking by dept code then original pos)
// ─────────────────────────────────────────────────────────────────────────────
// A: A5a(t=5,pos=0), A5b(t=5,pos=1), A8(t=8,pos=2)
// B: B5a(t=5,pos=0), B5b(t=5,pos=1), B9(t=9,pos=2)
// C: C5(t=5,pos=0),  C7(t=7,pos=1)
// Priority: same time → dept code ASC → originalPos ASC
// At t=5: A5a < A5b < A8... but dept A before B before C
// Expected order at t=5: A5a, A5b, B5a, B5b, C5; then C7, A8, B9
static void tc2_duplicate_timestamps() {
    std::vector<DeptQueue*> queues;

    DeptQueue* a = new DeptQueue();
    a->addJob("A5a", 5, "A", 0);
    a->addJob("A5b", 5, "A", 1);
    a->addJob("A8",  8, "A", 2);
    queues.push_back(a);

    DeptQueue* b = new DeptQueue();
    b->addJob("B5a", 5, "B", 0);
    b->addJob("B5b", 5, "B", 1);
    b->addJob("B9",  9, "B", 2);
    queues.push_back(b);

    DeptQueue* c = new DeptQueue();
    c->addJob("C5",  5, "C", 0);
    c->addJob("C7",  7, "C", 1);
    queues.push_back(c);

    MergeResult result = MergeEngine::kWayMerge(queues);
    std::vector<std::string> actual = extractIds(result);

    // The expected sequence given the priority rules:
    // t=5: heap initially holds A5a, B5a, C5
    //   extract A5a (dept A first) → push A5b
    //   extract A5b (t=5, dept A) → push A8
    //   extract B5a (t=5, dept B) → push B5b
    //   extract B5b (t=5, dept B) → push B9
    //   extract C5  (t=5, dept C) → push C7
    // t=7: extract C7 → push nothing (C exhausted)
    // t=8: extract A8 → push nothing
    // t=9: extract B9 → done
    std::vector<std::string> expected = {
        "A5a","A5b","B5a","B5b","C5","C7","A8","B9"
    };

    std::string detail;
    bool pass = checkOrder(actual, expected, detail);
    report("TC2 Duplicate Timestamps", pass, detail);

    freeTimeline(result);
    freeQueues(queues);
}

// ─────────────────────────────────────────────────────────────────────────────
// TC3 — Empty department queues
// ─────────────────────────────────────────────────────────────────────────────
// A: (empty)
// B: B2(t=2), B5(t=5)
// C: (empty)
// D: D1(t=1), D8(t=8)
// Expected: D1, B2, B5, D8
static void tc3_empty_queues() {
    std::vector<DeptQueue*> queues;

    DeptQueue* a = new DeptQueue();   // empty
    queues.push_back(a);

    DeptQueue* b = new DeptQueue();
    b->addJob("B2", 2, "B", 0);
    b->addJob("B5", 5, "B", 1);
    queues.push_back(b);

    DeptQueue* c = new DeptQueue();   // empty
    queues.push_back(c);

    DeptQueue* d = new DeptQueue();
    d->addJob("D1", 1, "D", 0);
    d->addJob("D8", 8, "D", 1);
    queues.push_back(d);

    MergeResult result = MergeEngine::kWayMerge(queues);
    std::vector<std::string> actual   = extractIds(result);
    std::vector<std::string> expected = {"D1","B2","B5","D8"};

    std::string detail;
    bool pass = checkOrder(actual, expected, detail);

    // Also verify that emptyQueues stat == 2
    if (pass && result.emptyQueues != 2) {
        pass = false;
        detail = "emptyQueues should be 2, got "
               + std::to_string(result.emptyQueues);
    }

    report("TC3 Empty Queues", pass, detail);

    freeTimeline(result);
    freeQueues(queues);
}

// ─────────────────────────────────────────────────────────────────────────────
// TC4 — Severely unbalanced queues
// ─────────────────────────────────────────────────────────────────────────────
// A: A1(t=1)
// B: B2(t=2),B3(t=3),B4(t=4),B5(t=5),B6(t=6)
// C: C7(t=7),C9(t=9)
// Expected: A1,B2,B3,B4,B5,B6,C7,C9
static void tc4_unbalanced_queues() {
    std::vector<DeptQueue*> queues;

    DeptQueue* a = new DeptQueue();
    a->addJob("A1", 1, "A", 0);
    queues.push_back(a);

    DeptQueue* b = new DeptQueue();
    b->addJob("B2", 2, "B", 0);
    b->addJob("B3", 3, "B", 1);
    b->addJob("B4", 4, "B", 2);
    b->addJob("B5", 5, "B", 3);
    b->addJob("B6", 6, "B", 4);
    queues.push_back(b);

    DeptQueue* c = new DeptQueue();
    c->addJob("C7", 7, "C", 0);
    c->addJob("C9", 9, "C", 1);
    queues.push_back(c);

    MergeResult result = MergeEngine::kWayMerge(queues);
    std::vector<std::string> actual   = extractIds(result);
    std::vector<std::string> expected = {"A1","B2","B3","B4","B5","B6","C7","C9"};

    std::string detail;
    bool pass = checkOrder(actual, expected, detail);
    report("TC4 Unbalanced Queues", pass, detail);

    freeTimeline(result);
    freeQueues(queues);
}

// ─────────────────────────────────────────────────────────────────────────────
// TC5 — Stress test: 200 departments × 500 jobs = 100 000 jobs
// ─────────────────────────────────────────────────────────────────────────────
// Generate: dept i has jobs with times [i*500+0 .. i*500+499] (all unique).
// Verify: output is globally sorted, no duplicates, correct count.
static void tc5_stress() {
    const int K = 200;    // departments
    const int N = 500;    // jobs per department

    std::vector<DeptQueue*> queues;
    queues.reserve(K);

    for (int i = 0; i < K; ++i) {
        DeptQueue* q = new DeptQueue();
        // Build dept code as two-letter string to keep ordering predictable
        // e.g., dept 0 → "DA", dept 1 → "DB", …, dept 25 → "DZ", dept 26 → "EA" …
        std::string code;
        code += (char)('D' + i / 26);
        code += (char)('A' + i % 26);

        for (int j = 0; j < N; ++j) {
            int t = i * N + j;  // unique time per job globally
            std::string id = code + std::to_string(j);
            q->addJob(id, t, code, j);
        }
        queues.push_back(q);
    }

    auto t0 = std::chrono::high_resolution_clock::now();
    MergeResult result = MergeEngine::kWayMerge(queues);
    auto t1 = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

    bool pass = true;
    std::string detail;

    // 1. Correct count
    int total = K * N;
    if ((int)result.mergedTimeline.size() != total) {
        pass = false;
        detail = "count " + std::to_string(result.mergedTimeline.size())
               + " != " + std::to_string(total);
    }

    // 2. Globally sorted by submitTime
    if (pass) {
        for (size_t i = 1; i < result.mergedTimeline.size(); ++i) {
            if (result.mergedTimeline[i]->submitTime
                    < result.mergedTimeline[i-1]->submitTime) {
                pass = false;
                detail = "out of order at index " + std::to_string(i);
                break;
            }
        }
    }

    // 3. No duplicate job IDs
    if (pass) {
        std::set<std::string> seen;
        for (Job* j : result.mergedTimeline) {
            if (!seen.insert(j->id).second) {
                pass = false;
                detail = "duplicate job id: " + j->id;
                break;
            }
        }
    }

    // Report timing even if passing
    std::string fullDetail;
    if (!pass) fullDetail = detail;
    // Append timing info
    std::ostringstream oss;
    if (!pass) oss << detail << " | ";
    oss << std::fixed << std::setprecision(1) << ms << "ms for " << total << " jobs";
    report("TC5 Stress Test (100k jobs)", pass, oss.str());

    freeTimeline(result);
    freeQueues(queues);
}

// ─────────────────────────────────────────────────────────────────────────────
// main
// ─────────────────────────────────────────────────────────────────────────────
int main() {
    std::cout << "====================================================\n";
    std::cout << " The Printing Office — K-Way Merge Test Suite\n";
    std::cout << "====================================================\n\n";

    tc1_basic_merge();
    tc2_duplicate_timestamps();
    tc3_empty_queues();
    tc4_unbalanced_queues();
    tc5_stress();

    std::cout << "\n----------------------------------------------------\n";
    std::cout << gPass << "/" << (gPass + gFail) << " tests passed.";
    if (gFail == 0)
        std::cout << "  ✓ All good!\n";
    else
        std::cout << "  ✗ " << gFail << " failure(s).\n";
    std::cout << "====================================================\n";

    return (gFail == 0) ? 0 : 1;
}
