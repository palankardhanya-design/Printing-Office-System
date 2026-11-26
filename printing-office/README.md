# 🖨️ The Printing Office — Print Queue Consolidation System

> **K-Way Merge of department print queues into a single globally sorted timeline — powered by a manual min-heap and singly linked lists.**

---

## Overview

A large organisation runs multiple departments, each of which submits print jobs throughout the day. Every department maintains its own **sorted linked list** of pending jobs, ordered by submission time. When the central printing office needs to process jobs it must produce a single, globally sorted timeline that respects every department's internal ordering without losing a single job.

Naïvely collecting all jobs and re-sorting them wastes memory proportional to the total job count and throws away the per-department ordering that already exists. The **K-Way Merge** algorithm exploits that pre-sorted structure: it maintains a small min-heap of size *K* (one entry per department) and always extracts the globally earliest job in **O(log K)** time, advancing only the pointer in the department that yielded the job.

This project demonstrates the algorithm end-to-end: a C++ backend exposes a REST API that runs the merge and streams every heap operation as a step, and a browser frontend visualises each insertion, heapify, and extraction as an animated tree — letting you watch the data structure work in real time.

---

## Data Structures

### Singly Linked List

Each department's job queue is stored as a singly linked list (`DeptQueue`). The list is the natural fit here because:

- Jobs arrive pre-sorted by submission time, so only **append** (`O(1)` with a tail pointer) is needed during ingestion — no random-access insertion is required.
- The merge algorithm only ever reads the **head** of each list and advances the pointer, which is `O(1)` per step.
- No additional index or auxiliary array is needed; the `next` pointer on each `Job` node is the only overhead.

```
DeptQueue A:   [A2, t=2] → [A7, t=7] → [A9, t=9] → nullptr
DeptQueue B:   [B1, t=1] → [B7, t=7] → nullptr
DeptQueue C:   [C3, t=3] → [C10, t=10] → nullptr
```

### Min-Heap

The merge algorithm needs to answer one question repeatedly: *"across all department heads, which job comes first?"* A min-heap answers that in **O(log K)** time regardless of how many jobs exist in total.

The heap is implemented **manually** as a fixed-size array of `HeapNode` structs (no `std::priority_queue`). Each `HeapNode` stores a pointer to the current head `Job` of a department and the department's index. The comparison rule is:

| Priority | Rule |
|----------|------|
| 1st | `submitTime` ascending (earlier time = higher priority) |
| 2nd | `deptCode` lexicographically ascending (tie on time) |
| 3rd | `originalPos` ascending (tie on dept code too) |

`heapifyUp` runs after every `insert`; `heapifyDown` runs after every `extractMin`. Both are `O(log K)`.

### K-Way Merge

The merge proceeds in three phases:

**Phase 1 — Initialise.** Insert the head job of every non-empty department into the heap (K insertions, each `O(log K)`).

**Phase 2 — Extract loop.** While the heap is non-empty:
1. `extractMin()` → the globally earliest job → append to output timeline.
2. Advance that department's linked-list pointer.
3. If the department still has jobs, `insert` the new head into the heap.

**Phase 3 — Done.** Output is a fully sorted timeline of all N jobs.

#### Step-by-Step Example

Queues: **A**: A2→A7→A9 | **B**: B1→B7 | **C**: C3→C10

| Step | Action | Heap after action | Output so far |
|------|--------|-------------------|--------------|
| Init | Insert A2(t=2), B1(t=1), C3(t=3) | {B1, A2, C3} | — |
| 1 | Extract B1 (min t=1); insert B7 | {A2, B7, C3} | B1 |
| 2 | Extract A2 (min t=2); insert A7 | {C3, B7, A7} | B1, A2 |
| 3 | Extract C3 (min t=3); insert C10 | {A7, B7, C10} | B1, A2, C3 |
| 4 | Extract A7 (min t=7, dept=A); insert A9 | {B7, A9, C10} | B1, A2, C3, A7 |
| 5 | Extract B7 (min t=7, dept=B); no more B | {A9, C10} | B1, A2, C3, A7, B7 |
| 6 | Extract A9; no more A | {C10} | B1, A2, C3, A7, B7, A9 |
| 7 | Extract C10; heap empty | {} | B1, A2, C3, A7, B7, A9, C10 ✓ |

> **Note on step 4:** A7 and B7 share the same submit time. The tie is broken by department code — `A` < `B` lexicographically — so A7 is extracted first.

---

## Algorithm Explanation

### Pseudocode

```
function kWayMerge(queues[0..K-1]):
    heap = new MinHeap()
    output = []

    // Phase 1: initialise heap with the first job of each dept
    for i in 0..K-1:
        if not queues[i].isEmpty():
            heap.insert({ jobPtr: queues[i].getHead(), deptIndex: i })
            advance queues[i].pointer

    // Phase 2: merge loop
    while not heap.isEmpty():
        node = heap.extractMin()          // O(log K)
        output.append(node.jobPtr)        // O(1)
        dept = queues[node.deptIndex]
        if dept has remaining jobs:
            heap.insert({ jobPtr: dept.getHead(), deptIndex: node.deptIndex })
            advance dept.pointer

    return output
```

### Priority Rules

```
compareJobs(a, b) → true if a should be extracted BEFORE b:
  if a.submitTime != b.submitTime  → return a.submitTime < b.submitTime
  if a.deptCode   != b.deptCode   → return a.deptCode   < b.deptCode   (lexicographic)
  return a.originalPos < b.originalPos
```

### Complexity Analysis

| Metric | Value | Reason |
|--------|-------|--------|
| **Time** | **O(N log K)** | Each of the N jobs is inserted once and extracted once from the heap; each heap operation is O(log K) because the heap never grows beyond K entries |
| **Space** | **O(K)** | The heap holds at most one node per department at any point; output storage is not counted as auxiliary space |

---

## Why Not Just Sort Everything?

| Approach | Time | Space | Notes |
|----------|------|-------|-------|
| Collect all N jobs → `std::sort` | O(N log N) | O(N) | Ignores existing per-dept ordering; needs N-sized buffer |
| **K-Way Merge (this project)** | **O(N log K)** | **O(K)** | Exploits pre-sorted input; tiny working set |
| Naive repeated scan of K heads | O(N · K) | O(1) | Simple but quadratic in K |

When the number of departments K is much smaller than the total job count N (the common case — e.g., 10 departments, 100 000 jobs), the difference between `log K` and `log N` is dramatic:

```
K = 10,  N = 100 000:
  log₂(K) ≈ 3.3   vs   log₂(N) ≈ 17   →  5× fewer comparisons per job
```

K-Way Merge also streams the result: it never needs all N jobs in memory at once, making it suitable for very large or even infinite streams.

---

## Project Structure

```
dsa/
└── printing-office/
    ├── README.md                  ← this file
    ├── backend/
    │   ├── Job.h                  ← Job struct definition
    │   ├── LinkedList.h           ← DeptQueue class declaration
    │   ├── LinkedList.cpp         ← DeptQueue implementation
    │   ├── MinHeap.h              ← MinHeap & HeapNode declaration
    │   ├── MinHeap.cpp            ← MinHeap implementation
    │   ├── MergeEngine.h          ← MergeResult, MergeStep, MergeEngine declaration
    │   ├── MergeEngine.cpp        ← kWayMerge implementation
    │   ├── server.cpp             ← HTTP server (cpp-httplib + nlohmann/json)
    │   ├── httplib.h              ← (download — see Setup)
    │   ├── json.hpp               ← (download — see Setup)
    │   ├── compile.bat            ← Windows one-click build
    │   └── Makefile               ← Linux/Mac build
    ├── frontend/
    │   ├── index.html             ← Single-page app
    │   ├── style.css              ← Styling & animations
    │   └── app.js                 ← API calls, heap tree SVG, step player
    └── tests/
        └── test.cpp               ← Standalone unit tests (no httplib)
```

---

## Installation & Setup

### Prerequisites

| Tool | Version | Notes |
|------|---------|-------|
| g++ | ≥ 9 | MinGW-w64 on Windows; `g++ --version` to check |
| Python 3 | any | Optional — only needed to serve the frontend over HTTP |

### Step 1 — Download Dependencies

The backend needs two single-header libraries. Download them into `printing-office\backend\`:

```powershell
cd printing-office\backend
Invoke-WebRequest -Uri "https://raw.githubusercontent.com/yhirose/cpp-httplib/master/httplib.h" -OutFile httplib.h
Invoke-WebRequest -Uri "https://raw.githubusercontent.com/nlohmann/json/develop/single_include/nlohmann/json.hpp" -OutFile json.hpp
```

### Step 2 — Compile

**Windows:**
```bat
cd printing-office\backend
compile.bat
```

**Linux / macOS:**
```bash
cd printing-office/backend
make
```

Both produce `server.exe` (Windows) or `server` (Linux/Mac).

### Step 3 — Start the Backend

```bat
server.exe
```

The server starts and listens on **http://localhost:8080**. Leave this terminal open.

### Step 4 — Open the Frontend

**Option A — Direct file open (simplest):**  
Open `printing-office\frontend\index.html` in any modern browser.

**Option B — Serve over HTTP (recommended to avoid CORS issues with some browsers):**
```bash
cd printing-office\frontend
python -m http.server 3000
```
Then visit **http://localhost:3000**.

---

## API Reference

All endpoints return JSON. All responses include `Content-Type: application/json` and CORS headers.

---

### `GET /api/health`

Health check.

**Response:**
```json
{ "status": "ok" }
```

---

### `POST /api/merge`

Run K-Way Merge on arbitrary department data.

**Request body:**
```json
{
  "departments": [
    {
      "code": "A",
      "jobs": [
        { "id": "A2", "time": 2 },
        { "id": "A7", "time": 7 },
        { "id": "A9", "time": 9 }
      ]
    },
    {
      "code": "B",
      "jobs": [
        { "id": "B1", "time": 1 },
        { "id": "B7", "time": 7 }
      ]
    }
  ]
}
```

**Response:**
```json
{
  "mergedTimeline": [
    { "id": "B1", "time": 1, "dept": "B", "pos": 0 },
    { "id": "A2", "time": 2, "dept": "A", "pos": 0 }
  ],
  "steps": [
    {
      "type": "insert",
      "jobId": "A2",
      "deptCode": "A",
      "submitTime": 2,
      "heapState": [ { "jobId": "A2", "deptCode": "A", "submitTime": 2, "originalPos": 0, "heapIndex": 0 } ],
      "explanation": "Inserted A2 (t=2, dept=A) into the heap."
    }
  ],
  "stats": {
    "totalDepts": 2,
    "totalJobs": 5,
    "totalOutput": 5,
    "heapInsertions": 5,
    "heapExtractions": 5,
    "heapComparisons": 8,
    "duplicateTimestamps": 1,
    "emptyQueues": 0,
    "executionTimeMs": 0.12
  },
  "verified": true
}
```

---

### `POST /api/test`

Run one of the five built-in test cases.

**Request body:**
```json
{ "testCase": 1 }
```
`testCase` must be an integer from **1 to 5**.

**Response:**
```json
{
  "testCase": 1,
  "pass": true,
  "expected": ["B1", "A2", "C3", "A7", "B7", "A9", "C10"],
  "actual":   ["B1", "A2", "C3", "A7", "B7", "A9", "C10"],
  "stats": { ... }
}
```

---

### `POST /api/test/stress`

Run the stress test (TC5: 200 departments × 500 jobs = 100 000 total). Returns pass/fail and timing.

**Request body:** `{}` (empty)

**Response:**
```json
{
  "testCase": 5,
  "pass": true,
  "totalJobs": 100000,
  "stats": { "executionTimeMs": 142.3, ... }
}
```

---

## Test Cases

| # | Description | Departments & Jobs | Key Validation |
|---|-------------|-------------------|----------------|
| **TC1** | Basic three-department merge | A: A2,A7,A9 · B: B1,B7 · C: C3,C10 | Expected: B1,A2,C3,A7,B7,A9,C10 |
| **TC2** | Duplicate timestamps (tie-breaking) | A: A5a(t=5),A5b(t=5),A8 · B: B5a(t=5),B5b(t=5),B9 · C: C5(t=5),C7 | Dept code then original position tie-breaks correctly |
| **TC3** | Empty department queues | A: (empty) · B: B2,B5 · C: (empty) · D: D1,D8 | Empty queues are skipped; output D1,B2,B5,D8 |
| **TC4** | Severely unbalanced queues | A: A1 · B: B2,B3,B4,B5,B6 · C: C7,C9 | Long department dominates output; still correct |
| **TC5** | Stress — 200 depts × 500 jobs | Generated programmatically | Sorted order verified, no missing/duplicate jobs, completes in reasonable time |

---

## Running Tests

### Via the Frontend (Browser)

1. Start the backend (`server.exe`).
2. Open `frontend/index.html`.
3. Click **Run Test 1** through **Run Test 4** in the Test Panel.
4. Click **Stress Test** to run TC5 (100 000 jobs).

Each test card shows PASS ✅ or FAIL ❌ with expected vs. actual output.

### Via the Standalone Test Runner (Command Line)

```bat
cd printing-office\tests
g++ -std=c++14 -o test.exe test.cpp ..\backend\LinkedList.cpp ..\backend\MinHeap.cpp ..\backend\MergeEngine.cpp -I..\backend
test.exe
```

Expected output:
```
[TC1] Basic Merge ................... PASS
[TC2] Duplicate Timestamps .......... PASS
[TC3] Empty Queues .................. PASS
[TC4] Unbalanced Queues ............. PASS
[TC5] Stress Test (100k jobs) ....... PASS
5/5 tests passed.
```

---

## Complexity Reference Table

| Approach | Time Complexity | Space Complexity | Notes |
|----------|----------------|-----------------|-------|
| **K-Way Merge (Min Heap)** | **O(N log K)** | **O(K)** | Exploits sorted input; heap size stays at K |
| Collect all + sort | O(N log N) | O(N) | Ignores per-dept ordering; needs N-sized buffer |
| Naive repeated scan | O(N · K) | O(1) | Simple but degrades badly with more departments |

Where **N** = total number of jobs, **K** = number of departments.

---

## License

MIT License — see [LICENSE](LICENSE) or use freely for educational and commercial purposes.

```
MIT License

Copyright (c) 2024 The Printing Office Contributors

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
```
