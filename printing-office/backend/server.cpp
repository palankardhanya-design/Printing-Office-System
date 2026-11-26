// server.cpp — Minimal Winsock HTTP/1.1 server for Printing Office DSA Project
// Compatible with MinGW GCC 6.x (no std::thread needed; single-threaded blocking)
// Compile: g++ -O2 -std=c++14 -o server.exe server.cpp LinkedList.cpp MinHeap.cpp MergeEngine.cpp -I. -lws2_32

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>

#include "json.hpp"
#include "LinkedList.h"
#include "MinHeap.h"
#include "MergeEngine.h"

#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <unordered_set>
#include <cstring>

#pragma comment(lib, "ws2_32.lib")

using json = nlohmann::json;

// ─── CORS / response helpers ─────────────────────────────────────────────────

static const std::string CORS_HEADERS =
    "Access-Control-Allow-Origin: *\r\n"
    "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
    "Access-Control-Allow-Headers: Content-Type\r\n";

static std::string httpOk(const std::string& body, const std::string& ct = "application/json") {
    std::ostringstream r;
    r << "HTTP/1.1 200 OK\r\n"
      << CORS_HEADERS
      << "Content-Type: " << ct << "\r\n"
      << "Content-Length: " << body.size() << "\r\n"
      << "Connection: close\r\n\r\n"
      << body;
    return r.str();
}

static std::string httpErr(int code, const std::string& msg) {
    std::string body = "{\"error\":\"" + msg + "\"}";
    std::string status = (code == 400) ? "400 Bad Request" : "500 Internal Server Error";
    std::ostringstream r;
    r << "HTTP/1.1 " << status << "\r\n"
      << CORS_HEADERS
      << "Content-Type: application/json\r\n"
      << "Content-Length: " << body.size() << "\r\n"
      << "Connection: close\r\n\r\n"
      << body;
    return r.str();
}

static std::string httpOptions() {
    std::ostringstream r;
    r << "HTTP/1.1 204 No Content\r\n"
      << CORS_HEADERS
      << "Content-Length: 0\r\n"
      << "Connection: close\r\n\r\n";
    return r.str();
}

// ─── Serialization helpers ────────────────────────────────────────────────────

static json serializeTimeline(const std::vector<Job*>& tl) {
    json arr = json::array();
    for (auto* j : tl) {
        arr.push_back({{"id",j->id},{"time",j->submitTime},{"dept",j->deptCode},{"pos",j->originalPos}});
    }
    return arr;
}

static json serializeSteps(const std::vector<MergeStep>& steps) {
    json arr = json::array();
    for (const auto& s : steps) {
        json hs = json::array();
        for (const auto& h : s.heapState)
            hs.push_back({{"jobId",h.jobId},{"deptCode",h.deptCode},{"submitTime",h.submitTime},{"originalPos",h.originalPos},{"heapIndex",h.heapIndex}});
        arr.push_back({{"type",s.type},{"jobId",s.jobId},{"deptCode",s.deptCode},{"submitTime",s.submitTime},{"heapState",hs},{"explanation",s.explanation}});
    }
    return arr;
}

static json serializeStats(const MergeResult& mr, int td, int tj) {
    return {{"totalDepts",td},{"totalJobs",tj},{"totalOutput",(int)mr.mergedTimeline.size()},
            {"heapInsertions",mr.heapInsertions},{"heapExtractions",mr.heapExtractions},
            {"heapComparisons",mr.heapComparisons},{"duplicateTimestamps",mr.duplicateTimestamps},
            {"emptyQueues",mr.emptyQueues},{"executionTimeMs",mr.executionTimeMs}};
}

static bool verifyMerged(const std::vector<Job*>& tl) {
    for (int i = 1; i < (int)tl.size(); i++) {
        Job* a = tl[i-1]; Job* b = tl[i];
        if (b->submitTime < a->submitTime) return false;
        if (b->submitTime == a->submitTime) {
            if (b->deptCode < a->deptCode) return false;
            if (b->deptCode == a->deptCode && b->originalPos < a->originalPos) return false;
        }
    }
    return true;
}

static void freeQueues(std::vector<DeptQueue*>& queues) {
    for (auto* q : queues) {
        Job* cur = q->getHead();
        while (cur) { Job* nxt = cur->next; delete cur; cur = nxt; }
        delete q;
    }
    queues.clear();
}

// ─── Build queues from JSON ───────────────────────────────────────────────────

static bool buildQueues(const json& body, std::vector<DeptQueue*>& queues,
                        int& totalJobs, std::string& errMsg) {
    if (!body.contains("departments") || !body["departments"].is_array()) {
        errMsg = "Missing departments array"; return false;
    }
    for (const auto& dept : body["departments"]) {
        if (!dept.contains("code") || !dept.contains("jobs")) {
            errMsg = "Department missing code or jobs"; return false;
        }
        std::string code = dept["code"].get<std::string>();
        DeptQueue* q = new DeptQueue();
        int pos = 0;
        for (const auto& jobj : dept["jobs"]) {
            q->addJob(jobj["id"].get<std::string>(), jobj["time"].get<int>(), code, pos++);
            totalJobs++;
        }
        queues.push_back(q);
    }
    return true;
}

// ─── Test Cases ───────────────────────────────────────────────────────────────

struct TCSpec { std::string code; std::vector<std::pair<std::string,int>> jobs; };

static std::vector<DeptQueue*> buildFromSpec(const std::vector<TCSpec>& specs) {
    std::vector<DeptQueue*> queues;
    for (const auto& s : specs) {
        DeptQueue* q = new DeptQueue();
        int pos = 0;
        for (const auto& jj : s.jobs) q->addJob(jj.first, jj.second, s.code, pos++);
        queues.push_back(q);
    }
    return queues;
}

static json runTestCase(int n) {
    std::vector<DeptQueue*> queues;
    std::vector<std::string> expected;
    int totalDepts = 0, totalJobs = 0;

    if (n == 1) {
        std::vector<TCSpec> spec = {
            {"A", {{"A2",2},{"A7",7},{"A9",9}}},
            {"B", {{"B1",1},{"B7",7}}},
            {"C", {{"C3",3},{"C10",10}}}
        };
        queues = buildFromSpec(spec);
        expected = {"B1","A2","C3","A7","B7","A9","C10"};
        totalDepts = 3; totalJobs = 7;
    } else if (n == 2) {
        std::vector<TCSpec> spec = {
            {"A", {{"A5a",5},{"A5b",5},{"A8",8}}},
            {"B", {{"B5a",5},{"B5b",5},{"B9",9}}},
            {"C", {{"C5",5},{"C7",7}}}
        };
        queues = buildFromSpec(spec);
        // t=5: A before B before C (dept lex order); then C7, A8, B9
        expected = {"A5a","A5b","B5a","B5b","C5","C7","A8","B9"};
        totalDepts = 3; totalJobs = 8;
    } else if (n == 3) {
        std::vector<TCSpec> spec = {
            {"A", {}},
            {"B", {{"B2",2},{"B5",5}}},
            {"C", {}},
            {"D", {{"D1",1},{"D8",8}}}
        };
        queues = buildFromSpec(spec);
        expected = {"D1","B2","B5","D8"};
        totalDepts = 4; totalJobs = 4;
    } else if (n == 4) {
        std::vector<TCSpec> spec = {
            {"A", {{"A1",1}}},
            {"B", {{"B2",2},{"B3",3},{"B4",4},{"B5",5},{"B6",6}}},
            {"C", {{"C7",7},{"C9",9}}}
        };
        queues = buildFromSpec(spec);
        expected = {"A1","B2","B3","B4","B5","B6","C7","C9"};
        totalDepts = 3; totalJobs = 8;
    } else if (n == 5) {
        // 200 depts x 500 jobs = 100k, strictly sorted per dept
        const int NDEPTS = 200, NJOBS = 500;
        totalDepts = NDEPTS;
        for (int d = 0; d < NDEPTS; d++) {
            std::string code = "D" + std::to_string(d);
            DeptQueue* q = new DeptQueue();
            int t = d + 1;
            for (int j = 0; j < NJOBS; j++) {
                q->addJob(code + "_J" + std::to_string(j), t, code, j);
                t += 2;
                totalJobs++;
            }
            queues.push_back(q);
        }
    } else {
        return {{"error","Invalid test case (1-5)"}};
    }

    MergeResult mr = MergeEngine::kWayMerge(queues);
    std::vector<std::string> actual;
    for (auto* j : mr.mergedTimeline) actual.push_back(j->id);

    bool pass = true;
    std::string failDetail = "";
    if (n <= 4) {
        if (actual != expected) {
            pass = false;
            // Find first mismatch
            for (int i = 0; i < (int)std::min(actual.size(), expected.size()); i++) {
                if (actual[i] != expected[i]) {
                    failDetail = "pos " + std::to_string(i) + ": got " + actual[i] + " want " + expected[i];
                    break;
                }
            }
            if (failDetail.empty()) failDetail = "size mismatch";
        }
    } else {
        // TC5: verify sorted, correct count, no duplicates
        if ((int)mr.mergedTimeline.size() != totalJobs) { pass = false; failDetail = "count mismatch"; }
        if (pass && !verifyMerged(mr.mergedTimeline)) { pass = false; failDetail = "not sorted"; }
        if (pass) {
            std::unordered_set<std::string> seen;
            for (auto& id : actual) {
                if (seen.count(id)) { pass = false; failDetail = "duplicate: " + id; break; }
                seen.insert(id);
            }
        }
    }

    json ea = json::array(); for (auto& s : expected) ea.push_back(s);
    json aa = json::array(); for (auto& s : actual) aa.push_back(s);

    json resp = {{"testCase",n},{"pass",pass},{"expected",ea},{"actual",aa},
                 {"failDetail",failDetail},{"stats",serializeStats(mr,totalDepts,totalJobs)}};

    freeQueues(queues);
    return resp;
}

// ─── HTTP Request Parser ──────────────────────────────────────────────────────

struct HttpRequest {
    std::string method, path, body;
};

static HttpRequest parseRequest(const std::string& raw) {
    HttpRequest req;
    // Parse first line: METHOD PATH HTTP/1.1
    size_t line1end = raw.find("\r\n");
    if (line1end == std::string::npos) return req;
    std::istringstream ls(raw.substr(0, line1end));
    std::string proto;
    ls >> req.method >> req.path >> proto;

    // Find body start
    size_t bodyStart = raw.find("\r\n\r\n");
    if (bodyStart != std::string::npos) {
        req.body = raw.substr(bodyStart + 4);
    }
    return req;
}

// ─── Route Handler ────────────────────────────────────────────────────────────

static std::string handleRequest(const HttpRequest& req) {
    if (req.method == "OPTIONS") return httpOptions();

    if (req.method == "GET" && req.path == "/api/health") {
        return httpOk("{\"status\":\"ok\"}");
    }

    if (req.method == "POST" && req.path == "/api/merge") {
        try {
            json body = json::parse(req.body);
            std::vector<DeptQueue*> queues;
            int totalJobs = 0;
            std::string errMsg;
            if (!buildQueues(body, queues, totalJobs, errMsg))
                return httpErr(400, errMsg);
            int totalDepts = (int)queues.size();
            bool traceSteps = (totalJobs <= 5000);
            MergeResult mr = MergeEngine::kWayMerge(queues, traceSteps);
            bool verified = verifyMerged(mr.mergedTimeline) &&
                            ((int)mr.mergedTimeline.size() == totalJobs);
            json stepsJson = traceSteps ? serializeSteps(mr.steps) : json::array();
            json resp = {
                {"mergedTimeline", serializeTimeline(mr.mergedTimeline)},
                {"steps", stepsJson},
                {"stats", serializeStats(mr, totalDepts, totalJobs)},
                {"verified", verified},
                {"stepsOmitted", !traceSteps}
            };
            freeQueues(queues);
            return httpOk(resp.dump());
        } catch (const std::exception& e) {
            return httpErr(500, e.what());
        }
    }

    if (req.method == "POST" && req.path == "/api/test") {
        try {
            json body = json::parse(req.body);
            int n = body.value("testCase", 1);
            return httpOk(runTestCase(n).dump());
        } catch (const std::exception& e) {
            return httpErr(500, e.what());
        }
    }

    if (req.method == "POST" && req.path == "/api/test/stress") {
        try {
            // TC5: 200 depts x 500 jobs — run without step tracing to avoid OOM
            const int NDEPTS = 200, NJOBS = 500;
            std::vector<DeptQueue*> queues;
            int totalJobs = 0;
            for (int d = 0; d < NDEPTS; d++) {
                std::string code = "D" + std::to_string(d);
                DeptQueue* q = new DeptQueue();
                int t = d + 1;
                for (int j = 0; j < NJOBS; j++) {
                    q->addJob(code + "_J" + std::to_string(j), t, code, j);
                    t += 2; totalJobs++;
                }
                queues.push_back(q);
            }
            MergeResult mr = MergeEngine::kWayMerge(queues, false);
            bool pass = verifyMerged(mr.mergedTimeline) &&
                        ((int)mr.mergedTimeline.size() == totalJobs);
            if (pass) {
                std::unordered_set<std::string> seen;
                for (auto* j : mr.mergedTimeline) {
                    if (seen.count(j->id)) { pass = false; break; }
                    seen.insert(j->id);
                }
            }
            // Build a small sample (first 10 + last 10) without serialising full 100k
            int sz = (int)mr.mergedTimeline.size();
            json sampleArr = json::array();
            for (int i = 0; i < std::min(sz, 10); i++)
                sampleArr.push_back({{"id",mr.mergedTimeline[i]->id},{"time",mr.mergedTimeline[i]->submitTime}});
            if (sz > 10) {
                sampleArr.push_back({{"id","..."},{"time",-1}});
                for (int i = std::max(10, sz-10); i < sz; i++)
                    sampleArr.push_back({{"id",mr.mergedTimeline[i]->id},{"time",mr.mergedTimeline[i]->submitTime}});
            }
            json resp = {
                {"testCase", 5},
                {"pass", pass},
                {"stats", serializeStats(mr, NDEPTS, totalJobs)},
                {"sampleOutput", sampleArr},
                {"note", "200 depts x 500 jobs = 100,000 total. Full timeline omitted for performance."}
            };
            freeQueues(queues);
            return httpOk(resp.dump());
        } catch (const std::exception& e) {
            return httpErr(500, e.what());
        }
    }

    return httpErr(400, "Unknown endpoint");
}

// ─── Main / Winsock Server ────────────────────────────────────────────────────

int main() {
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2,2), &wsa) != 0) {
        std::cerr << "WSAStartup failed\n"; return 1;
    }

    SOCKET srv = socket(AF_INET, SOCK_STREAM, 0);
    if (srv == INVALID_SOCKET) { std::cerr << "socket failed\n"; return 1; }

    int opt = 1;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(8080);

    if (bind(srv, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        std::cerr << "bind failed on port 8080: " << WSAGetLastError() << "\n"; return 1;
    }
    if (listen(srv, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "listen failed\n"; return 1;
    }

    std::cout << "================================================\n";
    std::cout << " The Printing Office — Backend API\n";
    std::cout << " Listening on http://localhost:8080\n";
    std::cout << " Press Ctrl+C to stop\n";
    std::cout << "================================================\n";

    while (true) {
        SOCKET client = accept(srv, nullptr, nullptr);
        if (client == INVALID_SOCKET) continue;

        // Read request — read until we have all headers + declared body
        std::string rawReq;
        char buf[8192];
        int received;

        // Phase 1: read until we see the end of headers (\r\n\r\n)
        while (rawReq.find("\r\n\r\n") == std::string::npos) {
            received = recv(client, buf, sizeof(buf)-1, 0);
            if (received <= 0) break;
            buf[received] = '\0';
            rawReq += buf;
        }

        // Phase 2: find Content-Length, read remaining body bytes
        int contentLen = 0;
        {
            // Case-insensitive search for content-length
            std::string lower = rawReq;
            for (char& c : lower) c = (char)tolower((unsigned char)c);
            size_t pos = lower.find("content-length:");
            if (pos != std::string::npos) {
                size_t eol = rawReq.find("\r\n", pos);
                std::string val = rawReq.substr(pos + 15, eol - pos - 15);
                contentLen = std::stoi(val);
            }
        }
        // Find where body starts
        size_t bodyStart = rawReq.find("\r\n\r\n");
        if (bodyStart != std::string::npos) bodyStart += 4;
        else bodyStart = rawReq.size();

        int bodyHave = (int)(rawReq.size() - bodyStart);
        while (bodyHave < contentLen) {
            received = recv(client, buf, sizeof(buf)-1, 0);
            if (received <= 0) break;
            buf[received] = '\0';
            rawReq += buf;
            bodyHave += received;
        }

        std::string response;
        try {
            HttpRequest req = parseRequest(rawReq);
            std::cout << req.method << " " << req.path << "\n";
            response = handleRequest(req);
        } catch (...) {
            response = httpErr(500, "Internal error");
        }

        send(client, response.c_str(), (int)response.size(), 0);
        closesocket(client);
    }

    closesocket(srv);
    WSACleanup();
    return 0;
}
