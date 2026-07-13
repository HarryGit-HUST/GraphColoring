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
void GraphColoringSolver::solve(TimeLeft restMilliSec, int seed) {
	// init random number generator.
	mt19937 rand(seed);
	auto randBetween = [&](int lb, int ub) { return uniform_int_distribution<int>(lb, ub - 1)(rand); };

	// ---- build adjacency list ----
	vector<vector<NodeId>> adjacent(gc.nodeNum);
	for (EdgeId e = 0; e < gc.edgeNum; ++e) {
		auto [i, j] = gc.edges[e];
		adjacent[i].push_back(j);
		adjacent[j].push_back(i);
	}

	// ---- greedy initial solution ----
	vector<ColorId> bestSln(gc.nodeNum, -1);
	int colorNum = 0;
	for (NodeId n = 0; n < gc.nodeNum; ++n) {
		vector<bool> usedColors(colorNum, false);
		for (NodeId adj : adjacent[n]) {
			if (bestSln[adj] != -1 && bestSln[adj] < colorNum) {
				usedColors[bestSln[adj]] = true;
			}
		}
		ColorId color = 0;
		while (color < colorNum && usedColors[color]) { ++color; }
		if (color == colorNum) { ++colorNum; }
		bestSln[n] = color;
	}
	tester.reportNewOptima(bestSln, colorNum);

	// ---- working solution ----
	vector<ColorId> CurrentSln = bestSln;

	// ---- tabu search ----
	auto tabuSearch = [&](int k) -> bool {
		// 1. remap colors >= k to [0, k-1]
		for (NodeId n = 0; n < gc.nodeNum; ++n) {
			if (CurrentSln[n] >= k) { CurrentSln[n] = randBetween(0, k); }
		}

		// 2. build adjColorCount[n][c] = #neighbors of n with color c (core for O(1) delta)
		vector<vector<int>> adjColorCount(gc.nodeNum, vector<int>(k, 0));
		for (NodeId n = 0; n < gc.nodeNum; ++n) {
			for (NodeId adj : adjacent[n]) {
				++adjColorCount[n][CurrentSln[adj]];
			}
		}

		// 3. initial conflicts & conflict node list
		vector<int> conflictCount(gc.nodeNum, 0);
		vector<NodeId> conflictNodes;
		int totalConflicts = 0;
		for (EdgeId e = 0; e < gc.edgeNum; ++e) {
			auto [u, v] = gc.edges[e];
			if (CurrentSln[u] == CurrentSln[v]) {
				++conflictCount[u];
				++conflictCount[v];
				++totalConflicts;
			}
		}
		for (NodeId n = 0; n < gc.nodeNum; ++n) {
			if (conflictCount[n] > 0) conflictNodes.push_back(n);
		}

		// 4. tabu table & search state
		vector<vector<int>> tabu(gc.nodeNum, vector<int>(k, 0));
		int iter = 0;
		int bestTotalConflicts = totalConflicts;
		int lastImproveIter = 0;
		const int EARLY_STOP = max(100000, gc.nodeNum * 500);

		// 5. main loop
		while (restMilliSec() > 0 && totalConflicts > 0) {
			++iter;
			if (iter - lastImproveIter > EARLY_STOP) return false;

			// find best move  O(|conflictNodes| * k)
			int bestDelta = INT_MAX;
			NodeId bestNode = -1;
			ColorId bestColor = -1;

			for (NodeId n : conflictNodes) {
				ColorId oldColor = CurrentSln[n];
				for (ColorId c = 0; c < k; ++c) {
					if (c == oldColor) continue;
					// O(1) delta via adjColorCount
					int delta = adjColorCount[n][c] - adjColorCount[n][oldColor];
					bool isTabu = (iter < tabu[n][c]);
					// aspiration: accept tabu move if it beats all-time best
					if (isTabu && totalConflicts + delta >= bestTotalConflicts) continue;
					if (delta < bestDelta) {
						bestDelta = delta;
						bestNode = n;
						bestColor = c;
					}
				}
			}
			if (bestNode == -1) break; // all moves tabu

			// execute move
			ColorId oldColor = CurrentSln[bestNode];
			CurrentSln[bestNode] = bestColor;
			totalConflicts += bestDelta;

			// incremental update of adjColorCount & conflictCount
			for (NodeId adj : adjacent[bestNode]) {
				--adjColorCount[adj][oldColor];
				++adjColorCount[adj][bestColor];
				if (CurrentSln[adj] == oldColor) --conflictCount[adj];
				if (CurrentSln[adj] == bestColor) ++conflictCount[adj];
			}
			conflictCount[bestNode] = adjColorCount[bestNode][bestColor];

			// rebuild conflict node list
			conflictNodes.clear();
			for (NodeId n = 0; n < gc.nodeNum; ++n) {
				if (conflictCount[n] > 0) conflictNodes.push_back(n);
			}

			// randomized tenure (classic TabuCol formula)
			int tenure = randBetween(0, 10) + static_cast<int>(0.6 * totalConflicts);
			tabu[bestNode][oldColor] = iter + tenure;

			// track best
			if (totalConflicts < bestTotalConflicts) {
				bestTotalConflicts = totalConflicts;
				lastImproveIter = iter;
			}
		}
		return totalConflicts == 0;
	};

	// ---- outer loop: step down k ----
	for (colorNum = colorNum - 1; colorNum >= 2 && restMilliSec() > 0; --colorNum) {
		if (tabuSearch(colorNum)) {
			bestSln = CurrentSln;
			tester.reportNewOptima(bestSln, colorNum);
		} else {
			break;
		}
	}
}

}
