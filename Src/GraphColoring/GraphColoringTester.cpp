////////////////////////////////
/// usage : 1.	implementation for graph coloring tester and checker.
/// 
/// note  : 1.	this file is a template file.
///			2.	this file will be replaced by standard implementation after submission.
////////////////////////////////

#include "GraphColoring.h"
#include "Util.h"

#include <atomic>
#include <fstream>
#include <unordered_set>
#include <sstream>
#include <thread>


using namespace std;
using namespace std::chrono;


namespace szx {

static constexpr char DataFileDir[] = "Data/GraphColoring/";
static constexpr char OutputFileExt[] = ".log";
static constexpr char LogFileDir[] = "Submission/GraphColoring/";
static constexpr char LogFileExt[] = ".log";
static constexpr int RandSeedInit = 612;
static constexpr int RandSeedInc = 611;
static constexpr int RandSeedMult = 4082;
static constexpr int ThreadStartGapInSec = 4;


bool GraphColoringTester::reportNewOptima(const NodeColors& output, ColorId objVal) {
    if (objVal >= bestObjVal) { return false; }
    reportTime = steady_clock::now();
    bestObjVal = objVal;
    bestSln = output;
    return true;
}

GraphColoringTester::Status GraphColoringTester::check(const NodeColors& output) const {
    Status s = { 0, 0, 0 };

    s.nodeNumDiff = static_cast<NodeId>(output.size()) - gc.nodeNum;
    if (s.nodeNumDiff != 0) { return s; }

    for (auto e = gc.edges.begin(); e != gc.edges.end(); ++e) {
        if (output[(*e)[0]] == output[(*e)[1]]) { ++s.conflictEdgeNum; }
    }

    unordered_set<ColorId> colors;
    for (auto n = output.begin(); n != output.end(); ++n) { colors.insert(*n); }
    s.colorNum = static_cast<ColorId>(colors.size());

    return s;
}


void loadInput(istream& is, GraphColoring& gc) {
    is >> gc.nodeNum >> gc.edgeNum >> gc.refColorNum;
    gc.edges.resize(gc.edgeNum);
    for (auto edge = gc.edges.begin(); edge != gc.edges.end(); ++edge) { is >> (*edge)[0] >> (*edge)[1]; }
}

void saveOutput(ostream& os, NodeColors& nodeColors) {
    for (auto color = nodeColors.begin(); color != nodeColors.end(); ++color) { os << *color << '\t'; }
}

void GraphColoringTester::test(istream& inputStream, long long secTimeout, int randSeed, const string& exeName, const string& instanceName) {
    cerr << "load input." << endl;
    GraphColoring gc;
    loadInput(inputStream, gc);

    cerr << "solve." << endl;
    TimePoint begTime = steady_clock::now();
    TimePoint endTime = begTime + seconds(secTimeout);
    NodeColors nodeColors(gc.nodeNum, -1);
    GraphColoringSolver gcSolver(gc);
    // TODO: 一旦超时、内存占用过高，强制结束整个进程
    gcSolver.solve(nodeColors, [&]() { return Timer::msDuration(steady_clock::now(), endTime); }, randSeed);

    GraphColoringTester::Status status = gcSolver.tester.check(nodeColors);

    // TODO: 仅当持平平台已知最优解时才保存解文件
    cerr << "save output." << endl;
    string slnPath(DataFileDir + instanceName + '[' + to_string(status.colorNum) + ']' + exeName + OutputFileExt);
    ofstream slnFile(slnPath);
    saveOutput(slnFile, nodeColors);

    // TODO: 日志文件加lock_guard
    string logPath(LogFileDir + exeName + LogFileExt);
    ofstream log(logPath, ios::app);
    log << instanceName << '\t' << randSeed << '\t' << status.colorNum << '\t'
        << Timer::msDuration(begTime, gcSolver.tester.getReportTime()) << '\t'
        << status.nodeNumDiff << '\t' << status.conflictEdgeNum << endl;
}

void GraphColoringTester::testAll(const string& exeName) {
    struct Test {
        int level;
        string name;
        int repeat;
        long long secTimeout;
    };

    // if the ratio of finding the optima is less than `minOptRate` of this level, 
    // the following levels will be skipped.
    vector<double> minOptRateOflevels = { 1, 0.5, 0 };
    vector<Test> tests = {
        { 0, "DSJC0125.1.txt", 5, 50 },
        { 0, "DSJC0125.5.txt", 5, 200 },
        { 0, "DSJC0125.9.txt", 5, 250 },
        { 0, "DSJC0250.1.txt", 5, 150 },

        { 1, "DSJC0250.5.txt", 10, 50 },
        { 1, "DSJC0250.9.txt", 10, 200 },
        { 1, "DSJC0500.1.txt", 10, 100 },

        { 2, "DSJC0500.5.txt", 10, 1000 },
        { 2, "DSJC0500.9.txt", 10, 2000 },
        { 2, "DSJC1000.1.txt", 10, 3000 },
        { 2, "DSJC1000.5.txt", 10, 4000 },
        { 2, "DSJC1000.9.txt", 10, 3000 },
        { 2, "C2000.5.txt", 10, 6000 },
        { 2, "C2000.9.txt", 10, 8000 },
        { 2, "C4000.5.txt", 10, 10000 },
    };

	vector<string> inputTexts(tests.size());
	for (size_t i = 0; i < tests.size(); ++i) {
		inputTexts[i] = File::readAllByte(DataFileDir + tests[i].name);
	}

	atomic<int> atomicIndex(0); // the index of next job to do for the pool of threads.
	auto run = [&]() {
		int seed = RandSeedInit;
		int thisIndex = 0;
		int nextIndex = -1;

		for (size_t i = 0; i < tests.size(); ++i) {
			for (int k = 0; k < tests[i].repeat; ++k, (seed *= RandSeedMult) += RandSeedInc) {
				if (thisIndex > nextIndex) { nextIndex = atomicIndex++; }
				if (thisIndex++ != nextIndex) { continue; }

				istringstream iss(inputTexts[i]);
				test(iss, tests[i].secTimeout, seed, exeName, tests[i].name);
			}
		}

        // TODO: 低难度最优解命中率不达标提前退出
		int optCount = 0;
	};

    int benchmarkThreadNum = thread::hardware_concurrency();
    vector<thread> ts;
    ts.reserve(benchmarkThreadNum);
	for (int t = 0; t < benchmarkThreadNum; ++t) {
		ts.emplace_back(run);
		this_thread::sleep_for(chrono::seconds(ThreadStartGapInSec));
	}
	for (auto t = ts.begin(); t != ts.end(); ++t) { t->join(); }
}

}
