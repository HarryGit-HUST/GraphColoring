////////////////////////////////
/// usage : 1.	interface for graph coloring solver.
/// 
/// note  : 1.	this file is a template file.
///			2.	this file will be replaced by standard implementation after submission.
////////////////////////////////

#ifndef CN_HUST_SZX_NPBENCHMARK_GRAPH_COLORING_H
#define CN_HUST_SZX_NPBENCHMARK_GRAPH_COLORING_H


#include <array>
#include <chrono>
#include <functional>
#include <iostream>
#include <string>
#include <vector>


namespace szx {

using NodeId = int; // ID that counts from 0, i.e., 0, 1, 2, 3, ...
using EdgeId = NodeId; // ID that counts from 0, i.e., 0, 1, 2, 3, ...
using ColorId = NodeId; // ID that counts from 0, i.e., 0, 1, 2, 3, ...

using Edge = std::array<NodeId, 2>; // undirected link.

using TimePoint = std::chrono::steady_clock::time_point;
using TimeLeft = std::function<long long()>; // return a integer for the time left for the solver in millisecond.


// input data.
struct GraphColoring {
	NodeId nodeNum;
	EdgeId edgeNum;
	ColorId refColorNum; // reference color number, may not be the optimal value.
	std::vector<Edge> edges; // `edges[e] == { i, j }` means the endpoints of the `e`th edge are `i` and `j`.
};

// output data.
using NodeColors = std::vector<ColorId>; // `NodeColors[n]` is the color of node `n`.

// best known solution recorder and reference checker implementation.
class GraphColoringTester {
public:
	struct Status {
		// objective value.
		ColorId colorNum;
		// constraint violations.
		NodeId nodeNumDiff;
		EdgeId conflictEdgeNum;
	};


	GraphColoringTester(const GraphColoring& input)
		: gc(input), bestObjVal(input.nodeNum), reportTime(std::chrono::steady_clock::now()) {}


	// call this method each time you find a better solution.
	// return true if the best known solution is updated.
	bool reportNewOptima(const NodeColors& output, ColorId objVal);

	// check if constraints are satisfied and calculate the objective value.
	Status check(const NodeColors& output) const;
	Status check() const { return check(bestSln); }

	const NodeColors& getBestSln() const { return bestSln; }
	TimePoint getReportTime() const { return reportTime; }

	// test the solver on all test cases.
	static void testAll(const std::string &exeName);
	static Status test(const GraphColoring& input, long long secTimeout, int randSeed,
		const std::string& exeName, const std::string& instanceName, int bestColorNum);
	static Status test(std::istream& inputStream, long long secTimeout, int randSeed,
		const std::string& exeName, const std::string& instanceName, int bestColorNum);
	
protected:
	const GraphColoring& gc;

	ColorId bestObjVal;
	NodeColors bestSln;
	TimePoint reportTime;
};


// solver interface.
class GraphColoringSolver {
public:
	GraphColoringTester tester;

	const GraphColoring& gc;


	GraphColoringSolver(const GraphColoring& input) : gc(input), tester(input) {}

	// solver entrance.
	void solve(TimeLeft restMilliSec, int seed);
};

}


#endif // CN_HUST_SZX_NPBENCHMARK_GRAPH_COLORING_H
