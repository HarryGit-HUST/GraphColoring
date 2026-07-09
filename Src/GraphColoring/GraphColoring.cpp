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


	// ---- TODO: implement your own solver which fills the `output`  --------------------------------+
	//            to replace the following trivial solver.                                         // |
	//            sample solver: assign each node a color and                                      // |
	//            randomly reassign color for nodes in random color.                               // |
	for (NodeId n = 0; n < gc.nodeNum; ++n) { output[n] = n; } // assign each node a color.        // |
	//         [ exit before timeout ]-------+                                                     // |
	//                                       |                                                     // |
	for (ColorId colorNum = gc.nodeNum; restMilliSec() > 0;) {                                     // |
		//                +----[ use the random number generator initialized by the given seed ]   // |
		//                |                                                                        // |
		ColorId color = rand() % (colorNum--); // eliminate a random color.                        // |
		for (NodeId n = 0; n < gc.nodeNum; ++n) {                                                  // |
			if (output[n] == color) {                                                              // |
				output[n] = rand() % colorNum;                                                     // |
			} else if (output[n] == colorNum) {                                                    // |
				output[n] = color;                                                                 // |
			}                                                                                      // |
		}                                                                                          // |
																                                   // |
		// ---- TODO: the following two lines can be deleted for efficiency. ------+               // |
		GraphColoringTester::Status status = tester.check(output);              // |               // |
		if (status.conflictEdgeNum > 0) { break; }                              // |               // |
		// ------------------------------------------------------------------------+               // |
								                                                                   // |
		//           +----[ report to tester each time you find a better solution ]                // |
		//           |                                                                             // |
		tester.reportNewOptima(output, colorNum);                                                  // |
	}                                                                                              // |
	// -----------------------------------------------------------------------------------------------+
}

}
