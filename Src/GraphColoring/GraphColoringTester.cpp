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
#include <mutex>
#include <sstream>
#include <thread>
#include <unordered_set>


using namespace std;
using namespace std::chrono;


namespace szx {

static constexpr char DataFileDir[] = "Data/GraphColoring/";
static constexpr char BaselineFilePath[] = "Data/GraphColoring/0.Baseline.txt";
static constexpr char OutputFileExt[] = ".log";
static constexpr char LogFileDir[] = "Submission/GraphColoring/";
static constexpr char LogFileExt[] = ".log";
static constexpr int EstInstanceNum = 64;
static constexpr int RandSeedInit = 612;
static constexpr int RandSeedInc = 611;
static constexpr int RandSeedMult = 4082;
enum ExitCode { Ok = 0, TimeLimitExceeded = -1, OutOfMemory = -2 };


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

GraphColoringTester::Status GraphColoringTester::test(istream& inputStream, long long secTimeout, int randSeed, const string& exeName, const string& instanceName, int bestColorNum) {
	cerr << "load input." << endl;
	GraphColoring gc;
	loadInput(inputStream, gc);

	return test(gc, secTimeout, randSeed, exeName, instanceName, bestColorNum);
}

GraphColoringTester::Status GraphColoringTester::test(const GraphColoring& input, long long secTimeout, int randSeed, const std::string& exeName, const std::string& instanceName, int bestColorNum) {
	cerr << "solve." << endl;
	NodeColors nodeColors(input.nodeNum, -1);
	GraphColoringSolver gcSolver(input);

	volatile bool* running = new bool(true);
	thread watchDog([&]() { // terminate the process if time limit exceeded.
		this_thread::sleep_for(seconds(secTimeout + 8));
		if (running) { exit(TimeLimitExceeded); }
		delete running;
	});	// TODO: 一旦内存占用过高，强制结束整个进程
	watchDog.detach();

	TimePoint begTime = steady_clock::now();
	TimePoint endTime = begTime + seconds(secTimeout);
	gcSolver.solve(nodeColors, [&]() { return Timer::msDuration(steady_clock::now(), endTime); }, randSeed);
	*running = false;

	GraphColoringTester::Status status = gcSolver.tester.check(nodeColors);

	if (status.colorNum < bestColorNum) {
		cerr << "save output." << endl;
		string slnPath(DataFileDir + instanceName + '[' + to_string(status.colorNum) + ']' + exeName + OutputFileExt);
		ofstream slnFile(slnPath);
		saveOutput(slnFile, nodeColors);
	}

	string logPath(LogFileDir + exeName + LogFileExt);
	static mutex logFileMutex;
	lock_guard<mutex> logFileGuard(logFileMutex);
	ofstream log(logPath, ios::app);
	log << instanceName << '\t' << randSeed << '\t' << status.colorNum << '\t'
		<< Timer::msDuration(begTime, gcSolver.tester.getReportTime()) << '\t'
		<< status.nodeNumDiff << '\t' << status.conflictEdgeNum << endl;

	return status;
}

void GraphColoringTester::testAll(const string& exeName) {
	struct Test {
		int level;
		string name;
		int repeat;
		int minOptRun; // if the number of runs that finds the optima is less than total `minOptRun` of this level, the following levels will be skipped.
		long long secTimeout;
		ColorId bestColorNum;
	};

	// load instance list.
	vector<Test> tests; tests.reserve(EstInstanceNum);
	istringstream baselineStream(File::readAllByte(BaselineFilePath));
	for (Test t; baselineStream >> t.level >> t.name >> t.repeat >> t.minOptRun >> t.secTimeout >> t.bestColorNum; tests.push_back(t)) {}

	vector<int> totalRunNums(tests.size(), 0);
	vector<int> totalOptRunNums(tests.size(), 0);
	size_t levelNum = 0;
	for (size_t i = 0; i < tests.size(); ++i) {
		totalRunNums[tests[i].level] += tests[i].repeat;
		totalOptRunNums[tests[i].level] += tests[i].minOptRun;
		if (tests[i].level > levelNum) { levelNum = tests[i].level; }
	}
	++levelNum;
	vector<atomic<int>> runNums(levelNum);
	vector<atomic<int>> optRunNums(levelNum);
	for (size_t l = 0; l < levelNum; ++l) {
		runNums[l] = totalRunNums[l];
		optRunNums[l] = totalOptRunNums[l];
	}

	// load instances.
	vector<GraphColoring> inputs(tests.size());
	JobFighter::fightForJob(thread::hardware_concurrency(), [&](JobFighter::IsJobTaken isJobTaken) {
		for (size_t i = 0; i < tests.size(); ++i) {
			if (isJobTaken()) { continue; }
			istringstream inputStream(File::readAllByte(DataFileDir + tests[i].name));
			loadInput(inputStream, inputs[i]);
		}
	}, 0);

	// launch solver.
	volatile bool tooManyNonOptRuns = false;
	auto nextSeed = [](int& seed) { return (seed *= RandSeedMult) += RandSeedInc; };
	JobFighter::fightForJob(thread::hardware_concurrency(), [&](JobFighter::IsJobTaken isJobTaken) {
		int seed = RandSeedInit;
		for (size_t i = 0; i < tests.size(); ++i) {
			for (int k = 0; k < tests[i].repeat; ++k, nextSeed(seed)) {
				if (tooManyNonOptRuns) { return; }
				if (isJobTaken()) { continue; }

				Status status = test(inputs[i], tests[i].secTimeout, seed, exeName, tests[i].name, tests[i].bestColorNum);

				if (status.colorNum <= tests[i].bestColorNum) { --optRunNums[tests[i].level]; }
				if (--runNums[tests[i].level] <= 0) {
					if (optRunNums[tests[i].level] > 0) {
						tooManyNonOptRuns = true;
						return;
					}
				}
			}
		}
	});
}

}
