////////////////////////////////
/// usage : 1.	implementation for graph coloring solver.
/// 
/// note  : 1.	this file can be modified.
////////////////////////////////

#include "GraphColoring.h"

#include <iostream>
#include <random>


using namespace std;


namespace szx {

// solver entrance.
void GraphColoringSolver::solve(NodeColors& output, TimeLeft restMilliSec, int seed) {
	// init random number generator (it is recommended to avoid using `rand()` in `stdlib.h`).
	// `rand() % n` produces biased random number (numbers less than $2^32 mod n$ may be selected with slightly higher probability).
	mt19937 rand(seed);
	// random number in uniform distribution.
	auto randBetween = [&](int lb, int ub) { return uniform_int_distribution<int>(lb, ub - 1)(rand); };


	// TODO: implement your own solver which fills the `output` to replace the following trivial solver.
	// sample solver: assign colors randomly (the solution can be infeasible).
	for (int colorNum = gc.refColorNum * 32; (restMilliSec() > 0) && (colorNum > 0); --colorNum) {
		//                                            |
		//       [ exit before timeout ]------+-------+
		//                                    |
		for (auto e = gc.edges.begin(); (restMilliSec() > 0) && (e != gc.edges.end()); ++e) {
			if (output[(*e)[0]] < 0) { output[(*e)[0]] = rand() % colorNum; }
			if (output[(*e)[1]] < 0) { output[(*e)[1]] = rand() % colorNum; }
			if (output[(*e)[0]] == output[(*e)[1]]) {
				output[(*e)[1]] = rand() % colorNum;
			} //                   |
		} //                       +----[ use the random number generator initialized by the given seed ]

		GraphColoringTester::Status status = tester.check(output); // TODO: this line can be deleted for efficiency.
		if (status.conflictEdgeNum > 0) { break; } // TODO: this line can be deleted for efficiency.

		//           +----[ report to tester each time you find a better solution ]
		//           |
		tester.reportNewOptima(output, colorNum);
	}
}

}
