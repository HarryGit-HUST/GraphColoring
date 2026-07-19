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
//  PartialCol (scan) — for k < 80, classic O(|V|*k) scan
// =========================================================================
static int partialColScan(
	vector<ColorId>& sln, int k,
	const GraphColoring& gc, const vector<vector<NodeId>>& adj,
	TimeLeft restMilliSec, mt19937& rand, int maxIter)
{
	auto randBetween = [&](int lb, int ub) {
		return uniform_int_distribution<int>(lb, ub - 1)(rand);
	};

	vector<NodeId> uncoloredNodes;
	int uncoloredCount = 0;
	for (NodeId n = 0; n < gc.nodeNum; ++n) {
		if (sln[n] < 0 || sln[n] >= k) {
			sln[n] = -1;
			uncoloredNodes.push_back(n);
			++uncoloredCount;
		}
	}
	{
		vector<int> conflictCount(gc.nodeNum, 0);
		for (EdgeId e = 0; e < gc.edgeNum; ++e) {
			auto [u, v] = gc.edges[e];
			if (sln[u] >= 0 && sln[v] >= 0 && sln[u] == sln[v]) {
				++conflictCount[u]; ++conflictCount[v];
			}
		}
		for (NodeId n = 0; n < gc.nodeNum; ++n) {
			if (conflictCount[n] > 0) {
				sln[n] = -1; uncoloredNodes.push_back(n); ++uncoloredCount;
			}
		}
	}

	vector<vector<int>> adjColorCount(gc.nodeNum, vector<int>(k, 0));
	for (NodeId u = 0; u < gc.nodeNum; ++u)
		for (NodeId v : adj[u])
			if (sln[v] >= 0) ++adjColorCount[u][sln[v]];

	vector<vector<int>> tabu(gc.nodeNum, vector<int>(k, 0));
	int iter = 0, bestUncolored = uncoloredCount, lastImproveIter = 0;
	const int EARLY_STOP = max(100000, gc.nodeNum * 500);

	while (restMilliSec() > 0 && uncoloredCount > 0) {
		++iter;
		if (maxIter > 0 && iter > maxIter) break;
		if (iter - lastImproveIter > EARLY_STOP) break;

		int bestCost = INT_MAX;
		NodeId bestU = -1; ColorId bestC = -1;
		for (NodeId u : uncoloredNodes) {
			for (ColorId c = 0; c < k; ++c) {
				if (tabu[u][c] > iter) {
					if (adjColorCount[u][c] > 0) continue;
				}
				int cost = adjColorCount[u][c];
				if (cost < bestCost) { bestCost = cost; bestU = u; bestC = c; }
			}
		}
		if (bestU == -1) break;

		sln[bestU] = bestC;
		for (NodeId v : adj[bestU]) {
			if (sln[v] == bestC) {
				sln[v] = -1;
				uncoloredNodes.push_back(v);
				for (NodeId w : adj[v]) --adjColorCount[w][bestC];
				tabu[v][bestC] = iter + randBetween(1, 12) + static_cast<int>(0.6 * uncoloredCount);
			}
		}
		for (NodeId w : adj[bestU]) ++adjColorCount[w][bestC];

		uncoloredCount += bestCost - 1;
		{
			size_t write = 0;
			for (size_t i = 0; i < uncoloredNodes.size(); ++i) {
				NodeId n = uncoloredNodes[i];
				if (sln[n] < 0) uncoloredNodes[write++] = n;
			}
			uncoloredNodes.resize(write);
		}
		tabu[bestU][bestC] = iter + randBetween(1, 12) + static_cast<int>(0.6 * uncoloredCount);
		if (uncoloredCount < bestUncolored) { bestUncolored = uncoloredCount; lastImproveIter = iter; }
	}
	return uncoloredCount;
}


// =========================================================================
//  PartialCol (bucket) — for k >= 80, bucket-sort O(1) move selection
// =========================================================================
static int partialColBucket(
	vector<ColorId>& sln, int k,
	const GraphColoring& gc, const vector<vector<NodeId>>& adj,
	TimeLeft restMilliSec, mt19937& rand, int maxIter)
{
	auto randBetween = [&](int lb, int ub) {
		return uniform_int_distribution<int>(lb, ub - 1)(rand);
	};

	vector<NodeId> uncoloredNodes;
	uncoloredNodes.reserve(gc.nodeNum);
	vector<int> posInUncolored(gc.nodeNum, -1);
	int uncoloredCount = 0;

	auto addUncolored = [&](NodeId n) {
		sln[n] = -1;
		posInUncolored[n] = static_cast<int>(uncoloredNodes.size());
		uncoloredNodes.push_back(n); ++uncoloredCount;
	};
	auto removeUncolored = [&](NodeId n) {
		int pos = posInUncolored[n];
		NodeId last = uncoloredNodes.back();
		uncoloredNodes[pos] = last;
		posInUncolored[last] = pos;
		uncoloredNodes.pop_back();
		posInUncolored[n] = -1; --uncoloredCount;
	};

	for (NodeId n = 0; n < gc.nodeNum; ++n)
		if (sln[n] < 0 || sln[n] >= k) addUncolored(n);
	{
		vector<int> conflictCount(gc.nodeNum, 0);
		for (EdgeId e = 0; e < gc.edgeNum; ++e) {
			auto [u, v] = gc.edges[e];
			if (sln[u] >= 0 && sln[v] >= 0 && sln[u] == sln[v]) {
				++conflictCount[u]; ++conflictCount[v];
			}
		}
		for (NodeId n = 0; n < gc.nodeNum; ++n)
			if (conflictCount[n] > 0) addUncolored(n);
	}

	vector<vector<int>> adjColorCount(gc.nodeNum, vector<int>(k, 0));
	int maxDeg = 0;
	for (NodeId u = 0; u < gc.nodeNum; ++u) {
		maxDeg = max(maxDeg, static_cast<int>(adj[u].size()));
		for (NodeId v : adj[u])
			if (sln[v] >= 0) ++adjColorCount[u][sln[v]];
	}

	vector<vector<pair<NodeId, ColorId>>> buckets(gc.nodeNum + 1); // large enough
	vector<int> posInBucket(gc.nodeNum * k, -1);

	auto bucketAdd = [&](NodeId u, ColorId c) {
		int cost = adjColorCount[u][c];
		int idx = u * k + c;
		buckets[cost].emplace_back(u, c);
		posInBucket[idx] = static_cast<int>(buckets[cost].size()) - 1;
	};
	auto bucketRemove = [&](NodeId u, ColorId c, int useCost) {
		int idx = u * k + c;
		int pos = posInBucket[idx];
		if (pos < 0) return;
		auto& b = buckets[useCost];
		auto [lastU, lastC] = b.back();
		b[pos] = {lastU, lastC};
		posInBucket[lastU * k + lastC] = pos;
		b.pop_back();
		posInBucket[idx] = -1;
	};

	for (NodeId u : uncoloredNodes)
		for (ColorId c = 0; c < k; ++c) bucketAdd(u, c);

	vector<vector<int>> tabu(gc.nodeNum, vector<int>(k, 0));
	int iter = 0, bestUncolored = uncoloredCount, lastImproveIter = 0;
	const int EARLY_STOP = max(100000, gc.nodeNum * 500);

	while (restMilliSec() > 0 && uncoloredCount > 0) {
		++iter;
		if (maxIter > 0 && iter > maxIter) break;
		if (iter - lastImproveIter > EARLY_STOP) break;

		int bestU = -1, bestC = -1;
		int bucketCount = static_cast<int>(buckets.size());
	for (int cost = 0; cost < bucketCount; ++cost) {
			if (buckets[cost].empty()) continue;
			bool found = false;
			for (size_t i = 0; i < buckets[cost].size(); ++i) {
				auto [u, c] = buckets[cost][i];
				if (adjColorCount[u][c] != cost) continue;
				if (tabu[u][c] > iter && cost > 0) continue;
				bestU = u; bestC = c; found = true; break;
			}
			if (found) break;
		}
		if (bestU == -1) break;

		for (ColorId c = 0; c < k; ++c) bucketRemove(bestU, c, adjColorCount[bestU][c]);
		removeUncolored(bestU);
		sln[bestU] = bestC;

		for (NodeId v : adj[bestU]) {
			if (sln[v] == bestC) {
				sln[v] = -1; addUncolored(v);
				for (ColorId c = 0; c < k; ++c) bucketAdd(v, c);
				for (NodeId w : adj[v]) --adjColorCount[w][bestC];
				tabu[v][bestC] = iter + randBetween(1, 12) + static_cast<int>(0.6 * uncoloredCount);
			}
		}
		for (NodeId w : adj[bestU]) ++adjColorCount[w][bestC];

		{
			vector<int> delta(gc.nodeNum, 0);
			for (NodeId v : adj[bestU])
				if (sln[v] == bestC) for (NodeId w : adj[v]) --delta[w];
			for (NodeId w : adj[bestU]) ++delta[w];
			for (NodeId w = 0; w < gc.nodeNum; ++w) {
				if (delta[w] != 0 && sln[w] < 0 && w != bestU) {
					int oldCost = adjColorCount[w][bestC] - delta[w];
					bucketRemove(w, bestC, oldCost);
					bucketAdd(w, bestC);
				}
			}
		}

		tabu[bestU][bestC] = iter + randBetween(1, 12) + static_cast<int>(0.6 * uncoloredCount);
		if (uncoloredCount < bestUncolored) { bestUncolored = uncoloredCount; lastImproveIter = iter; }
	}
	return uncoloredCount;
}


// =========================================================================
//  PartialCol — dispatcher
// =========================================================================
static int partialColSearch(
	vector<ColorId>& sln, int k,
	const GraphColoring& gc, const vector<vector<NodeId>>& adj,
	TimeLeft restMilliSec, mt19937& rand, int maxIter = 0)
{
	return partialColScan(sln, k, gc, adj, restMilliSec, rand, maxIter);
}


// =========================================================================
//  GCPX — Greedy Coloring Partition Crossover
// =========================================================================
static vector<ColorId> GCPX(
	const vector<ColorId>& p1, const vector<ColorId>& p2,
	int k, const GraphColoring& gc, const vector<vector<NodeId>>& adj,
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
				for (NodeId a : adj[n])
					if (!taken[a]) score += 2;
			}
			if (score > bestScore) { bestScore = score; bestC = c; }
		}
		if (bestScore == 0) break;
		for (NodeId n : donor[bestC])
			if (!taken[n]) { child[n] = colorIdx; taken[n] = true; }
		++colorIdx;
	}

	for (NodeId n = 0; n < gc.nodeNum; ++n) {
		if (!taken[n]) {
			vector<bool> used(k, false);
			for (NodeId a : adj[n])
				if (taken[a]) used[child[a]] = true;
			ColorId c = 0;
			while (c < k && used[c]) ++c;
			child[n] = (c < k) ? c : randBetween(0, k);
		}
	}
	return child;
}


// =========================================================================
//  HEA — Hybrid Evolutionary Algorithm for a fixed k
// =========================================================================
static bool HEA(int k, vector<ColorId>& currentSln,
	const GraphColoring& gc, const vector<vector<NodeId>>& adj,
	TimeLeft restMilliSec, mt19937& rand)
{
	auto randBetween = [&](int lb, int ub) {
		return uniform_int_distribution<int>(lb, ub - 1)(rand);
	};

	const int POP_SIZE = (gc.nodeNum <= 250) ? 10 : 15;
	const double DIVERSITY_THRESHOLD = 0.9;
	const int RESTART_THRESHOLD = 120;

	struct Individual { vector<ColorId> genes; int uncolored; };
	vector<Individual> pop;

	for (int i = 0; i < POP_SIZE && restMilliSec() > 0; ++i) {
		Individual ind;
		ind.genes = currentSln;
		for (NodeId n = 0; n < gc.nodeNum; ++n)
			if (randBetween(0, 3) == 0) ind.genes[n] = randBetween(0, k);
		ind.uncolored = partialColSearch(ind.genes, k, gc, adj, restMilliSec, rand,
			static_cast<int>(gc.nodeNum * 100));
		pop.push_back(ind);
		if (ind.uncolored == 0) { currentSln = ind.genes; return true; }
	}

	auto similarity = [&](const vector<ColorId>& a, const vector<ColorId>& b) {
		int same = 0;
		for (NodeId n = 0; n < gc.nodeNum; ++n) if (a[n] == b[n]) ++same;
		return static_cast<double>(same) / gc.nodeNum;
	};
	auto isTooSimilar = [&](const vector<ColorId>& sol) {
		for (auto& ind : pop) if (similarity(sol, ind.genes) > DIVERSITY_THRESHOLD) return true;
		return false;
	};
	auto tournamentSelect = [&]() -> int {
		int a = randBetween(0, static_cast<int>(pop.size()));
		int b = randBetween(0, static_cast<int>(pop.size()));
		return (pop[a].uncolored <= pop[b].uncolored) ? a : b;
	};

	int bestPopUncolored = INT_MAX;
	for (auto& ind : pop) bestPopUncolored = min(bestPopUncolored, ind.uncolored);
	int noImproveGens = 0;

	while (restMilliSec() > 0) {
		int pi1 = tournamentSelect(), pi2 = tournamentSelect();
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
		if (curBest < bestPopUncolored) { bestPopUncolored = curBest; noImproveGens = 0; }
		else ++noImproveGens;

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
//  IslandHEA — Island Model for large graphs
// =========================================================================
static bool islandHEA(int k, vector<ColorId>& currentSln,
	const GraphColoring& gc, const vector<vector<NodeId>>& adj,
	TimeLeft restMilliSec, mt19937& rand)
{
	auto randBetween = [&](int lb, int ub) {
		return uniform_int_distribution<int>(lb, ub - 1)(rand);
	};

	const int NUM_ISLANDS = 4, POP_SIZE = 5, MIGRATION_INTERVAL = 5;
	struct Individual { vector<ColorId> genes; int uncolored; };
	vector<vector<Individual>> islands(NUM_ISLANDS);

	int perturbRates[4] = { 2, 3, 4, 5 };
	for (int i = 0; i < NUM_ISLANDS && restMilliSec() > 0; ++i) {
		for (int j = 0; j < POP_SIZE && restMilliSec() > 0; ++j) {
			Individual ind;
			ind.genes = currentSln;
			for (NodeId n = 0; n < gc.nodeNum; ++n)
				if (randBetween(0, perturbRates[i]) == 0) ind.genes[n] = randBetween(0, k);
			ind.uncolored = partialColSearch(ind.genes, k, gc, adj, restMilliSec, rand,
				static_cast<int>(gc.nodeNum * 100));
			islands[i].push_back(ind);
			if (ind.uncolored == 0) { currentSln = ind.genes; return true; }
		}
	}

	int gen = 0;
	while (restMilliSec() > 0) {
		for (int i = 0; i < NUM_ISLANDS && restMilliSec() > 0; ++i) {
			auto& pop = islands[i];
			auto tournamentSelect = [&]() -> int {
				int a = randBetween(0, POP_SIZE), b = randBetween(0, POP_SIZE);
				return (pop[a].uncolored <= pop[b].uncolored) ? a : b;
			};
			int pi1 = tournamentSelect(), pi2 = tournamentSelect();
			while (pi1 == pi2) pi2 = randBetween(0, POP_SIZE);

			vector<ColorId> child = GCPX(pop[pi1].genes, pop[pi2].genes, k, gc, adj, rand);
			int islandBest = INT_MAX;
			for (auto& ind : pop) islandBest = min(islandBest, ind.uncolored);
			int budget = (islandBest < 30) ? 0 : static_cast<int>(gc.nodeNum * 250);
			int childUncolored = partialColSearch(child, k, gc, adj, restMilliSec, rand, budget);
			if (childUncolored == 0) { currentSln = child; return true; }

			int worstIdx = 0;
			for (int j = 1; j < POP_SIZE; ++j)
				if (pop[j].uncolored > pop[worstIdx].uncolored) worstIdx = j;
			if (childUncolored < pop[worstIdx].uncolored) {
				pop[worstIdx].genes = move(child);
				pop[worstIdx].uncolored = childUncolored;
			}
		}
		++gen;
		if (gen % MIGRATION_INTERVAL == 0) {
			for (int i = 0; i < NUM_ISLANDS; ++i) {
				int dst = (i + 1) % NUM_ISLANDS;
				int bestSrc = 0, worstDst = 0;
				for (int j = 1; j < POP_SIZE; ++j) {
					if (islands[i][j].uncolored < islands[i][bestSrc].uncolored) bestSrc = j;
					if (islands[dst][j].uncolored > islands[dst][worstDst].uncolored) worstDst = j;
				}
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

	vector<vector<NodeId>> adj(gc.nodeNum);
	for (EdgeId e = 0; e < gc.edgeNum; ++e) {
		auto [i, j] = gc.edges[e];
		adj[i].push_back(j);
		adj[j].push_back(i);
	}

	vector<ColorId> bestSln;
	int colorNum;
	dsaturInit(gc, adj, bestSln, colorNum, rand);
	tester.reportNewOptima(bestSln, colorNum);

	vector<ColorId> curSln = bestSln;

	for (colorNum = colorNum - 1; colorNum >= 2 && restMilliSec() > 0; --colorNum) {
		bool ok = (gc.nodeNum >= 500)
			? islandHEA(colorNum, curSln, gc, adj, restMilliSec, rand)
			: HEA(colorNum, curSln, gc, adj, restMilliSec, rand);
		if (ok) {
			bestSln = curSln;
			tester.reportNewOptima(bestSln, colorNum);
		} else break;
	}
}

}
