////////////////////////////////
/// usage : 1.	implementation for graph coloring solver.
///
/// note  : 1.	this file can be modified.
////////////////////////////////

#include "GraphColoring.h"

#include <algorithm>
#include <iostream>
#include <random>
#include <set>


using namespace std;


namespace szx {

// =========================================================================
//  DSATUR — initial proper coloring
//    Saturates the node with most distinct neighbor colors first.
//    Returns: colors array and colorNum (count of distinct colors used).
// =========================================================================
static void dsaturInit(
	const GraphColoring& gc,
	const vector<vector<NodeId>>& adj,
	vector<ColorId>& outColors,
	int& outColorNum,
	mt19937& rand)
{
	outColors.assign(gc.nodeNum, -1);
	vector<int> saturation(gc.nodeNum, 0);
	vector<int> degree(gc.nodeNum);
	for (NodeId n = 0; n < gc.nodeNum; ++n) degree[n] = static_cast<int>(adj[n].size());
	vector<set<ColorId>> neighborColors(gc.nodeNum);
	vector<bool> colored(gc.nodeNum, false);
	int maxColor = 0;

	for (int step = 0; step < gc.nodeNum; ++step) {
		int bestN = -1, bestSat = -1, bestDeg = -1;
		for (NodeId n = 0; n < gc.nodeNum; ++n) {
			if (colored[n]) continue;
			if (saturation[n] > bestSat || (saturation[n] == bestSat && degree[n] > bestDeg)) {
				bestSat = saturation[n]; bestDeg = degree[n]; bestN = n;
			}
		}
		vector<bool> used(maxColor + 1, false);
		for (NodeId a : adj[bestN]) {
			if (colored[a]) used[outColors[a]] = true;
		}
		ColorId c = 0;
		while (c <= maxColor && used[c]) ++c;
		outColors[bestN] = c;
		colored[bestN] = true;
		if (c > maxColor) maxColor = c;

		for (NodeId a : adj[bestN]) {
			if (!colored[a] && neighborColors[a].insert(c).second) ++saturation[a];
		}
	}
	outColorNum = maxColor + 1;
}


// =========================================================================
//  PartialCol — partial-coloring local search (replaces conflict-based Tabu)
//
//  Maintains a *proper* partial k-coloring (no conflicts among colored nodes).
//  Each move: pick an uncolored node u and a color c, color u with c,
//             then "kick out" (uncolor) all neighbors of u that have color c.
//  Cost = number of kicked neighbors = adjColorCount[u][c].
//  Goal: reach 0 uncolored nodes (= valid k-coloring).
//
//  Key advantage over TabuCol: the state is ALWAYS conflict-free,
//  and evaluation is O(1) per (node, color) pair.
// =========================================================================
static int partialColSearch(
	vector<ColorId>& sln,
	int k,
	const GraphColoring& gc,
	const vector<vector<NodeId>>& adj,
	TimeLeft restMilliSec,
	mt19937& rand,
	int maxIter = 0)
{
	auto randBetween = [&](int lb, int ub) {
		return uniform_int_distribution<int>(lb, ub - 1)(rand);
	};

	// ---- 1. build proper partial k-coloring ----
	vector<NodeId> uncoloredNodes;
	int uncoloredCount = 0;

	for (NodeId n = 0; n < gc.nodeNum; ++n) {
		if (sln[n] < 0 || sln[n] >= k) {
			sln[n] = -1;
			uncoloredNodes.push_back(n);
			++uncoloredCount;
		}
	}

	// resolve conflicts: if two adjacent colored nodes share a color, uncolor one
	{
		vector<int> conflictCount(gc.nodeNum, 0);
		for (EdgeId e = 0; e < gc.edgeNum; ++e) {
			auto [u, v] = gc.edges[e];
			if (sln[u] >= 0 && sln[v] >= 0 && sln[u] == sln[v]) {
				++conflictCount[u];
				++conflictCount[v];
			}
		}
		for (NodeId n = 0; n < gc.nodeNum; ++n) {
			if (conflictCount[n] > 0) {
				sln[n] = -1;
				uncoloredNodes.push_back(n);
				++uncoloredCount;
			}
		}
	}

	// ---- 2. adjColorCount[u][c] = #colored-neighbors of u with color c ----
	vector<vector<int>> adjColorCount(gc.nodeNum, vector<int>(k, 0));
	for (NodeId u = 0; u < gc.nodeNum; ++u) {
		for (NodeId v : adj[u]) {
			if (sln[v] >= 0) ++adjColorCount[u][sln[v]];
		}
	}

	// ---- 3. tabu table & search state ----
	// tabu[n][c] = iteration until which assigning color c to node n is forbidden
	vector<vector<int>> tabu(gc.nodeNum, vector<int>(k, 0));
	int iter = 0;
	int bestUncolored = uncoloredCount;
	int lastImproveIter = 0;
	const int EARLY_STOP = max(100000, gc.nodeNum * 500);

	// ---- 4. main loop ----
	while (restMilliSec() > 0 && uncoloredCount > 0) {
		++iter;
		if (maxIter > 0 && iter > maxIter) break;
		if (iter - lastImproveIter > EARLY_STOP) break;

		int bestCost = INT_MAX;
		NodeId bestU = -1;
		ColorId bestC = -1;

		// evaluate all (uncolored-node, color) pairs
		for (NodeId u : uncoloredNodes) {
			for (ColorId c = 0; c < k; ++c) {
				if (tabu[u][c] > iter) {
					// aspiration: a cost-0 move (greedy fill) always accepted
					if (adjColorCount[u][c] > 0) continue;
				}
				int cost = adjColorCount[u][c];
				if (cost < bestCost) {
					bestCost = cost;
					bestU = u;
					bestC = c;
				}
			}
		}
		if (bestU == -1) break; // all moves tabu

		// ---- execute move (bestU, bestC) ----
		// step A: uncolor all neighbors of bestU that have color bestC
		sln[bestU] = bestC;
		for (NodeId v : adj[bestU]) {
			if (sln[v] == bestC) {
				sln[v] = -1;
				uncoloredNodes.push_back(v);
				// v lost its color → its neighbors lose one adjColorCount entry
				for (NodeId w : adj[v]) {
					--adjColorCount[w][bestC];
				}
				// tabu: prevent immediately re-assigning bestC to v
				tabu[v][bestC] = iter + randBetween(1, 12)
					+ static_cast<int>(0.6 * uncoloredCount);
			}
		}

		// step B: bestU is now colored → its neighbors gain one adjColorCount entry
		for (NodeId w : adj[bestU]) {
			++adjColorCount[w][bestC];
		}

		// step C: update uncolored set
		// bestU was uncolored, now colored → -1
		// each kicked neighbor was colored, now uncolored → +1 each (= bestCost)
		// net: +bestCost - 1
		uncoloredCount += bestCost - 1;

		// remove all colored nodes from uncoloredNodes
		{
			size_t write = 0;
			for (size_t i = 0; i < uncoloredNodes.size(); ++i) {
				NodeId n = uncoloredNodes[i];
				if (sln[n] < 0) {
					uncoloredNodes[write++] = n;
				}
			}
			uncoloredNodes.resize(write);
		}

		// tabu on bestU: prevent changing its color for a while
		tabu[bestU][bestC] = iter + randBetween(1, 12)
			+ static_cast<int>(0.6 * uncoloredCount);

		// track best
		if (uncoloredCount < bestUncolored) {
			bestUncolored = uncoloredCount;
			lastImproveIter = iter;
		}
	}
	return uncoloredCount;
}


// =========================================================================
//  GCPX — Greedy Coloring Partition Crossover
//    Alternates between parents, each turn grafts the color class with
//    max connectivity to remaining unassigned nodes.
// =========================================================================
static vector<ColorId> GCPX(
	const vector<ColorId>& p1,
	const vector<ColorId>& p2,
	int k,
	const GraphColoring& gc,
	const vector<vector<NodeId>>& adj,
	mt19937& rand)
{
	auto randBetween = [&](int lb, int ub) {
		return uniform_int_distribution<int>(lb, ub - 1)(rand);
	};

	vector<vector<NodeId>> classes1(k), classes2(k);
	for (NodeId n = 0; n < gc.nodeNum; ++n) {
		if (p1[n] >= 0 && p1[n] < k) classes1[p1[n]].push_back(n);
		if (p2[n] >= 0 && p2[n] < k) classes2[p2[n]].push_back(n);
	}

	vector<bool> taken(gc.nodeNum, false);
	vector<ColorId> child(gc.nodeNum, 0);
	int colorIdx = 0;

	while (colorIdx < k) {
		auto& donor = (colorIdx % 2 == 0) ? classes1 : classes2;

		int bestC = -1, bestScore = 0;
		for (int c = 0; c < k; ++c) {
			int score = 0;
			for (NodeId n : donor[c]) {
				if (taken[n]) continue;
				score += 1;
				for (NodeId a : adj[n]) {
					if (!taken[a]) score += 2;
				}
			}
			if (score > bestScore) { bestScore = score; bestC = c; }
		}
		if (bestScore == 0) break;

		for (NodeId n : donor[bestC]) {
			if (!taken[n]) {
				child[n] = colorIdx;
				taken[n] = true;
			}
		}
		++colorIdx;
	}

	// remaining → greedy
	for (NodeId n = 0; n < gc.nodeNum; ++n) {
		if (!taken[n]) {
			vector<bool> used(k, false);
			for (NodeId a : adj[n]) {
				if (taken[a]) used[child[a]] = true;
			}
			ColorId c = 0;
			while (c < k && used[c]) ++c;
			child[n] = (c < k) ? c : randBetween(0, k);
		}
	}
	return child;
}


// =========================================================================
//  HEA — Hybrid Evolutionary Algorithm for a fixed k
//    Population + GCPX crossover + PartialCol local search
//    + diversity guard + perturbation restart
// =========================================================================
static bool HEA(
	int k,
	vector<ColorId>& currentSln,
	const GraphColoring& gc,
	const vector<vector<NodeId>>& adj,
	TimeLeft restMilliSec,
	mt19937& rand)
{
	auto randBetween = [&](int lb, int ub) {
		return uniform_int_distribution<int>(lb, ub - 1)(rand);
	};

	const int POP_SIZE = (gc.nodeNum <= 250) ? 10 : 15;
	const double DIVERSITY_THRESHOLD = 0.9;
	const int RESTART_THRESHOLD = 120;

	struct Individual { vector<ColorId> genes; int uncolored; };
	vector<Individual> pop;

	// ---- init population (light PartialCol for speed + diversity) ----
	for (int i = 0; i < POP_SIZE && restMilliSec() > 0; ++i) {
		Individual ind;
		ind.genes = currentSln;
		for (NodeId n = 0; n < gc.nodeNum; ++n) {
			if (randBetween(0, 3) == 0) ind.genes[n] = randBetween(0, k);
		}
		ind.uncolored = partialColSearch(ind.genes, k, gc, adj, restMilliSec, rand,
			static_cast<int>(gc.nodeNum * 100));
		pop.push_back(ind);
		if (ind.uncolored == 0) { currentSln = ind.genes; return true; }
	}

	// ---- diversity check ----
	auto similarity = [&](const vector<ColorId>& a, const vector<ColorId>& b) {
		int same = 0;
		for (NodeId n = 0; n < gc.nodeNum; ++n)
			if (a[n] == b[n]) ++same;
		return static_cast<double>(same) / gc.nodeNum;
	};
	auto isTooSimilar = [&](const vector<ColorId>& sol) {
		for (auto& ind : pop)
			if (similarity(sol, ind.genes) > DIVERSITY_THRESHOLD) return true;
		return false;
	};

	// ---- tournament selection ----
	auto tournamentSelect = [&]() -> int {
		int a = randBetween(0, static_cast<int>(pop.size()));
		int b = randBetween(0, static_cast<int>(pop.size()));
		return (pop[a].uncolored <= pop[b].uncolored) ? a : b;
	};

	// ---- track best ----
	int bestPopUncolored = INT_MAX;
	for (auto& ind : pop) bestPopUncolored = min(bestPopUncolored, ind.uncolored);
	int noImproveGens = 0;

	// ---- main HEA loop ----
	while (restMilliSec() > 0) {
		int pi1 = tournamentSelect();
		int pi2 = tournamentSelect();
		while (pi1 == pi2) pi2 = randBetween(0, static_cast<int>(pop.size()));

		vector<ColorId> child = GCPX(pop[pi1].genes, pop[pi2].genes, k, gc, adj, rand);
		int childBudget = (bestPopUncolored < 30) ? 0 : static_cast<int>(gc.nodeNum * 250);
		int childUncolored = partialColSearch(child, k, gc, adj, restMilliSec, rand, childBudget);
		if (childUncolored == 0) { currentSln = child; return true; }

		if (!isTooSimilar(child)) {
			int worstIdx = 0;
			for (int i = 1; i < POP_SIZE; ++i)
				if (pop[i].uncolored > pop[worstIdx].uncolored) worstIdx = i;
			if (childUncolored < pop[worstIdx].uncolored) {
				pop[worstIdx].genes = move(child);
				pop[worstIdx].uncolored = childUncolored;
			}
		}

		int curBest = INT_MAX;
		for (auto& ind : pop) curBest = min(curBest, ind.uncolored);
		if (curBest < bestPopUncolored) {
			bestPopUncolored = curBest;
			noImproveGens = 0;
		} else {
			++noImproveGens;
		}

		// ---- perturbation restart ----
		if (noImproveGens > RESTART_THRESHOLD) {
			for (auto& ind : pop) {
				for (NodeId n = 0; n < gc.nodeNum; ++n)
					if (randBetween(0, 10) == 0) ind.genes[n] = randBetween(0, k);
				ind.uncolored = partialColSearch(ind.genes, k, gc, adj, restMilliSec, rand,
					static_cast<int>(gc.nodeNum * 50));
				if (ind.uncolored == 0) { currentSln = ind.genes; return true; }
			}
			noImproveGens = 0;
			bestPopUncolored = INT_MAX;
			for (auto& ind : pop) bestPopUncolored = min(bestPopUncolored, ind.uncolored);
		}
	}
	return false;
}


// =========================================================================
//  IslandHEA — Island Model HEA for large/hard graphs
//    4 independent small populations ("islands") evolve in parallel.
//    Every MIGRATION_INTERVAL generations, best individuals migrate
//    to neighbor islands (ring topology). Isolation replaces diversity
//    guard and perturbation restart with natural exploration.
// =========================================================================
static bool islandHEA(
	int k,
	vector<ColorId>& currentSln,
	const GraphColoring& gc,
	const vector<vector<NodeId>>& adj,
	TimeLeft restMilliSec,
	mt19937& rand)
{
	auto randBetween = [&](int lb, int ub) {
		return uniform_int_distribution<int>(lb, ub - 1)(rand);
	};

	const int NUM_ISLANDS = 4;
	const int POP_SIZE = 5;              // per island (total 20 individuals)
	const int MIGRATION_INTERVAL = 5;    // frequent migration with light search

	struct Individual { vector<ColorId> genes; int uncolored; };
	vector<vector<Individual>> islands(NUM_ISLANDS);

	// ---- init each island with different perturbation strength ----
	int perturbRates[4] = { 2, 3, 4, 5 };
	for (int i = 0; i < NUM_ISLANDS && restMilliSec() > 0; ++i) {
		for (int j = 0; j < POP_SIZE && restMilliSec() > 0; ++j) {
			Individual ind;
			ind.genes = currentSln;
			for (NodeId n = 0; n < gc.nodeNum; ++n) {
				if (randBetween(0, perturbRates[i]) == 0) {
					ind.genes[n] = randBetween(0, k);
				}
			}
			ind.uncolored = partialColSearch(ind.genes, k, gc, adj, restMilliSec, rand,
				static_cast<int>(gc.nodeNum * 100));
			islands[i].push_back(ind);
			if (ind.uncolored == 0) { currentSln = ind.genes; return true; }
		}
	}

	int gen = 0;

	// ---- main loop: round-robin over islands ----
	while (restMilliSec() > 0) {
		for (int i = 0; i < NUM_ISLANDS && restMilliSec() > 0; ++i) {
			auto& pop = islands[i];

			// tournament selection (local to this island)
			auto tournamentSelect = [&]() -> int {
				int a = randBetween(0, POP_SIZE);
				int b = randBetween(0, POP_SIZE);
				return (pop[a].uncolored <= pop[b].uncolored) ? a : b;
			};

			int pi1 = tournamentSelect();
			int pi2 = tournamentSelect();
			while (pi1 == pi2) pi2 = randBetween(0, POP_SIZE);

			// crossover + local search (light for exploration, deep only when close)
			vector<ColorId> child = GCPX(pop[pi1].genes, pop[pi2].genes, k, gc, adj, rand);

			int islandBest = INT_MAX;
			for (auto& ind : pop) islandBest = min(islandBest, ind.uncolored);
			// three-tier budget: deep only when within striking distance
			int budget;
			if (islandBest <= 5)       budget = 0;    // full depth — almost there
			else if (islandBest <= 20)  budget = static_cast<int>(gc.nodeNum * 20);
			else                        budget = 3000;  // very light — just scout

			int childUncolored = partialColSearch(child, k, gc, adj, restMilliSec, rand, budget);
			if (childUncolored == 0) { currentSln = child; return true; }

			// replace worst in this island
			int worstIdx = 0;
			for (int j = 1; j < POP_SIZE; ++j) {
				if (pop[j].uncolored > pop[worstIdx].uncolored) worstIdx = j;
			}
			if (childUncolored < pop[worstIdx].uncolored) {
				pop[worstIdx].genes = move(child);
				pop[worstIdx].uncolored = childUncolored;
			}
		}

		++gen;

		// ---- migration (ring topology) ----
		if (gen % MIGRATION_INTERVAL == 0) {
			for (int i = 0; i < NUM_ISLANDS; ++i) {
				int dst = (i + 1) % NUM_ISLANDS;

				// find best in source island
				int bestSrc = 0;
				for (int j = 1; j < POP_SIZE; ++j) {
					if (islands[i][j].uncolored < islands[i][bestSrc].uncolored) bestSrc = j;
				}

				// find worst in destination island
				int worstDst = 0;
				for (int j = 1; j < POP_SIZE; ++j) {
					if (islands[dst][j].uncolored > islands[dst][worstDst].uncolored) worstDst = j;
				}

				// migrate if source is better
				if (islands[i][bestSrc].uncolored < islands[dst][worstDst].uncolored) {
					islands[dst][worstDst].genes = islands[i][bestSrc].genes;
					islands[dst][worstDst].uncolored = islands[i][bestSrc].uncolored;
				}
			}
		}
	}
	return false;
}


// =========================================================================
//  Main solver entry
// =========================================================================
void GraphColoringSolver::solve(TimeLeft restMilliSec, int seed) {
	mt19937 rand(seed);

	// ---- adjacency list ----
	vector<vector<NodeId>> adj(gc.nodeNum);
	for (EdgeId e = 0; e < gc.edgeNum; ++e) {
		auto [i, j] = gc.edges[e];
		adj[i].push_back(j);
		adj[j].push_back(i);
	}

	// ---- initial solution (DSATUR) ----
	vector<ColorId> bestSln;
	int colorNum;
	dsaturInit(gc, adj, bestSln, colorNum, rand);
	tester.reportNewOptima(bestSln, colorNum);

	vector<ColorId> curSln = bestSln;

	// ---- step down k with retries ----
	const int MAX_ATTEMPTS = 3;
	for (colorNum = colorNum - 1; colorNum >= 2 && restMilliSec() > 0; --colorNum) {
		bool found = false;
		vector<ColorId> backup = curSln;

		for (int attempt = 0; attempt < MAX_ATTEMPTS && restMilliSec() > 0; ++attempt) {
			if (attempt > 0) {
				curSln = backup;
				auto randBetween = [&](int lb, int ub) {
					return uniform_int_distribution<int>(lb, ub - 1)(rand);
				};
				for (NodeId n = 0; n < gc.nodeNum; ++n) {
					if (randBetween(0, 4) == 0) curSln[n] = randBetween(0, colorNum);
				}
			}
			bool ok = (gc.nodeNum >= 500)
				? islandHEA(colorNum, curSln, gc, adj, restMilliSec, rand)
				: HEA(colorNum, curSln, gc, adj, restMilliSec, rand);
			if (ok) {
				bestSln = curSln;
				tester.reportNewOptima(bestSln, colorNum);
				found = true;
				break;
			}
		}
		if (!found) break;
	}
}

}
