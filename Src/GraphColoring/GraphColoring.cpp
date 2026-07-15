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

	// solver entrance.
	void GraphColoringSolver::solve(TimeLeft restMilliSec, int seed) {
		// ---- random ----
		mt19937 rand(seed);
		auto randBetween = [&](int lb, int ub) { return uniform_int_distribution<int>(lb, ub - 1)(rand); };

		// ---- build adjacency list ----
		vector<vector<NodeId>> adjacent(gc.nodeNum);
		for (EdgeId e = 0; e < gc.edgeNum; ++e) {
			auto [i, j] = gc.edges[e];
			adjacent[i].push_back(j);
			adjacent[j].push_back(i);
		}

		bool DEBUG = false; // set true to see profiling output

		// ---- DSATUR initial solution ----
		//   picks the uncolored node with max "saturation" (distinct neighbor colors)
		//   first, then ties broken by degree. Produces better (fewer-color) solutions.
		vector<ColorId> bestSln(gc.nodeNum, -1);
		vector<int> saturation(gc.nodeNum, 0);
		vector<int> degree(gc.nodeNum);
		for (NodeId n = 0; n < gc.nodeNum; ++n) degree[n] = static_cast<int>(adjacent[n].size());
		vector<set<ColorId>> neighborColors(gc.nodeNum); // distinct colors among colored neighbors
		vector<bool> colored(gc.nodeNum, false);
		int colorNum = 0;

		for (int step = 0; step < gc.nodeNum; ++step) {
			// pick best node: max saturation, tie-break by degree
			int bestN = -1, bestSat = -1, bestDeg = -1;
			for (NodeId n = 0; n < gc.nodeNum; ++n) {
				if (colored[n]) continue;
				if (saturation[n] > bestSat || (saturation[n] == bestSat && degree[n] > bestDeg)) {
					bestSat = saturation[n]; bestDeg = degree[n]; bestN = n;
				}
			}
			// color with smallest unused
			vector<bool> used(colorNum + 1, false);
			for (NodeId adj : adjacent[bestN]) {
				if (colored[adj]) used[bestSln[adj]] = true;
			}
			ColorId c = 0;
			while (c <= colorNum && used[c]) ++c;
			bestSln[bestN] = c;
			colored[bestN] = true;
			if (c > colorNum) colorNum = c;

			// update saturation of uncolored neighbors
			for (NodeId adj : adjacent[bestN]) {
				if (!colored[adj] && neighborColors[adj].insert(c).second) {
					++saturation[adj];
				}
			}
		}
		++colorNum; // convert max-index to count
		tester.reportNewOptima(bestSln, colorNum);

		// ---- working solution ----
		vector<ColorId> CurrentSln = bestSln;

		// ====================================================================
		//  core: tabu search on an arbitrary solution
		//    sln  — in/out: the coloring to optimize
		//    k    — target #colors
		//    returns final conflict count (0 = k-coloring found)
		// ====================================================================
		auto tabuSearchOptimize = [&](vector<ColorId>& sln, int k, int maxIter = 0) -> int {
			// 1. remap colors >= k to [0, k-1]
			for (NodeId n = 0; n < gc.nodeNum; ++n) {
				if (sln[n] >= k) { sln[n] = randBetween(0, k); }
			}

			// 2. adjColorCount[n][c] = #neighbors of n with color c
			vector<vector<int>> adjColorCount(gc.nodeNum, vector<int>(k, 0));
			for (NodeId n = 0; n < gc.nodeNum; ++n) {
				for (NodeId adj : adjacent[n]) {
					++adjColorCount[n][sln[adj]];
				}
			}

			// 3. initial conflicts
			vector<int> conflictCount(gc.nodeNum, 0);
			vector<NodeId> conflictNodes;
			int totalConflicts = 0;
			for (EdgeId e = 0; e < gc.edgeNum; ++e) {
				auto [u, v] = gc.edges[e];
				if (sln[u] == sln[v]) {
					++conflictCount[u];
					++conflictCount[v];
					++totalConflicts;
				}
			}
			for (NodeId n = 0; n < gc.nodeNum; ++n) {
				if (conflictCount[n] > 0) conflictNodes.push_back(n);
			}

			// 4. tabu table & state
			vector<vector<int>> tabu(gc.nodeNum, vector<int>(k, 0));
			int iter = 0;
			int bestTotalConflicts = totalConflicts;
			int lastImproveIter = 0;
			const int EARLY_STOP = max(100000, gc.nodeNum * 500);

			// 5. main loop
			while (restMilliSec() > 0 && totalConflicts > 0) {
				++iter;
				if (maxIter > 0 && iter > maxIter) break;    // early exit for quick mode
				if (iter - lastImproveIter > EARLY_STOP) break;

				int bestDelta = INT_MAX;
				NodeId bestNode = -1;
				ColorId bestColor = -1;

				for (NodeId n : conflictNodes) {
					ColorId oldColor = sln[n];
					for (ColorId c = 0; c < k; ++c) {
						if (c == oldColor) continue;
						int delta = adjColorCount[n][c] - adjColorCount[n][oldColor];
						bool isTabu = (iter < tabu[n][c]);
						if (isTabu && totalConflicts + delta >= bestTotalConflicts) continue;
						if (delta < bestDelta) {
							bestDelta = delta;
							bestNode = n;
							bestColor = c;
						}
					}
				}
				if (bestNode == -1) break;

				ColorId oldColor = sln[bestNode];
				sln[bestNode] = bestColor;
				totalConflicts += bestDelta;

				for (NodeId adj : adjacent[bestNode]) {
					--adjColorCount[adj][oldColor];
					++adjColorCount[adj][bestColor];
					if (sln[adj] == oldColor) --conflictCount[adj];
					if (sln[adj] == bestColor) ++conflictCount[adj];
				}
				conflictCount[bestNode] = adjColorCount[bestNode][bestColor];

				conflictNodes.clear();
				for (NodeId n = 0; n < gc.nodeNum; ++n) {
					if (conflictCount[n] > 0) conflictNodes.push_back(n);
				}

				int tenure = randBetween(0, 10) + static_cast<int>(0.6 * totalConflicts);
				tabu[bestNode][oldColor] = iter + tenure;

				if (totalConflicts < bestTotalConflicts) {
					bestTotalConflicts = totalConflicts;
					lastImproveIter = iter;
				}
			}
			return totalConflicts;
		};

		// wrapper: tabu search on the global CurrentSln
		auto tabuSearch = [&](int k) -> bool {
			return tabuSearchOptimize(CurrentSln, k) == 0;
		};

		// ====================================================================
		//  GCPX (Greedy Coloring Partition Crossover)
		//    GPX variant: picks class with max connectivity to remaining nodes,
		//    not just max size — favors "central" color classes
		// ====================================================================
		auto GCPX = [&](const vector<ColorId>& p1, const vector<ColorId>& p2, int k) -> vector<ColorId> {
			vector<vector<NodeId>> classes1(k), classes2(k);
			for (NodeId n = 0; n < gc.nodeNum; ++n) {
				classes1[p1[n]].push_back(n);
				classes2[p2[n]].push_back(n);
			}

			vector<bool> taken(gc.nodeNum, false);
			vector<ColorId> child(gc.nodeNum, 0);
			int colorIdx = 0;

			while (colorIdx < k) {
				auto& donor = (colorIdx % 2 == 0) ? classes1 : classes2;

				// GCPX: score = node_count + 2 * (edges to remaining nodes)
				int bestC = -1, bestScore = 0;
				for (int c = 0; c < k; ++c) {
					int score = 0;
					for (NodeId n : donor[c]) {
						if (taken[n]) continue;
						score += 1;
						for (NodeId adj : adjacent[n]) {
							if (!taken[adj]) score += 2;
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

			// remaining nodes → greedy
			for (NodeId n = 0; n < gc.nodeNum; ++n) {
				if (!taken[n]) {
					vector<bool> used(k, false);
					for (NodeId adj : adjacent[n]) {
						if (taken[adj]) used[child[adj]] = true;
					}
					ColorId c = 0;
					while (c < k && used[c]) ++c;
					child[n] = (c < k) ? c : randBetween(0, k);
				}
			}
			return child;
		};

		// ====================================================================
		//  HEA (Hybrid Evolutionary Algorithm) for a fixed k
		//    + diversity guard   — reject offspring too similar to population
		//    + perturbation restart — shake population when stuck
		// ====================================================================
		auto HEA = [&](int k) -> bool {
			const int POP_SIZE = (gc.nodeNum <= 250) ? 10 : 15; // larger pop for harder graphs
			const double DIVERSITY_THRESHOLD = 0.9;
			const int RESTART_THRESHOLD = 120;           // more patience per generation

			struct Individual {
				vector<ColorId> genes;
				int conflicts;
			};
			vector<Individual> pop;

			// --- initialize population (LIGHT tabu — fast, diverse) ---
			for (int i = 0; i < POP_SIZE && restMilliSec() > 0; ++i) {
				Individual ind;
				ind.genes = CurrentSln;
				for (NodeId n = 0; n < gc.nodeNum; ++n) {
					if (randBetween(0, 3) == 0) {           // ~33% perturbed
						ind.genes[n] = randBetween(0, k);
					}
				}
				// light optimization only — preserves diversity, saves time
				ind.conflicts = tabuSearchOptimize(ind.genes, k, gc.nodeNum * 100);
				pop.push_back(ind);
				if (ind.conflicts == 0) { CurrentSln = ind.genes; return true; }
			}

			// --- diversity check helper ---
			auto similarity = [&](const vector<ColorId>& a, const vector<ColorId>& b) {
				int same = 0;
				for (NodeId n = 0; n < gc.nodeNum; ++n) {
					if (a[n] == b[n]) ++same;
				}
				return static_cast<double>(same) / gc.nodeNum;
			};

			auto isTooSimilar = [&](const vector<ColorId>& sol) {
				for (auto& ind : pop) {
					if (similarity(sol, ind.genes) > DIVERSITY_THRESHOLD) return true;
				}
				return false;
			};

			// --- tournament selection ---
			auto tournamentSelect = [&]() -> int {
				int a = randBetween(0, static_cast<int>(pop.size()));
				int b = randBetween(0, static_cast<int>(pop.size()));
				return (pop[a].conflicts <= pop[b].conflicts) ? a : b;
			};

			// --- track best for restart ---
			int bestPopConflicts = INT_MAX;
			for (auto& ind : pop) {
				if (ind.conflicts < bestPopConflicts) bestPopConflicts = ind.conflicts;
			}
			int noImproveGens = 0;

			// --- main HEA loop ---
			while (restMilliSec() > 0) {
				// select parents
				int pi1 = tournamentSelect();
				int pi2 = tournamentSelect();
				while (pi1 == pi2) pi2 = randBetween(0, static_cast<int>(pop.size()));

				// crossover + local search  (adaptive depth: full when close to solution)
				vector<ColorId> child = GCPX(pop[pi1].genes, pop[pi2].genes, k);
				int childBudget = (bestPopConflicts < 30) ? 0 : gc.nodeNum * 250;
				int childConflicts = tabuSearchOptimize(child, k, childBudget);
				if (childConflicts == 0) { CurrentSln = child; return true; }

				// --- diversity guard ---
				if (!isTooSimilar(child)) {
					// replace worst
					int worstIdx = 0;
					for (int i = 1; i < POP_SIZE; ++i) {
						if (pop[i].conflicts > pop[worstIdx].conflicts) worstIdx = i;
					}
					if (childConflicts < pop[worstIdx].conflicts) {
						pop[worstIdx].genes = move(child);
						pop[worstIdx].conflicts = childConflicts;
					}
				}

				// --- track progress for restart ---
				int currentBest = INT_MAX;
				for (auto& ind : pop) {
					if (ind.conflicts < currentBest) currentBest = ind.conflicts;
				}
				if (currentBest < bestPopConflicts) {
					bestPopConflicts = currentBest;
					noImproveGens = 0;
				} else {
					++noImproveGens;
				}

				// --- perturbation restart (light tabu: O(|V|*50) per member) ---
				if (noImproveGens > RESTART_THRESHOLD) {
					for (auto& ind : pop) {
						for (NodeId n = 0; n < gc.nodeNum; ++n) {
							if (randBetween(0, 10) == 0) {       // 10% perturbed
								ind.genes[n] = randBetween(0, k);
							}
						}
						// light tabu — moderate budget, fast enough
						ind.conflicts = tabuSearchOptimize(ind.genes, k, gc.nodeNum * 50);
						if (ind.conflicts == 0) { CurrentSln = ind.genes; return true; }
					}
					noImproveGens = 0;
					bestPopConflicts = INT_MAX;
					for (auto& ind : pop) {
						if (ind.conflicts < bestPopConflicts) bestPopConflicts = ind.conflicts;
					}
				}
			}

			return false;
		};

		// ---- outer loop: step down k with retries ----
		const int MAX_ATTEMPTS = 3;
		for (colorNum = colorNum - 1; colorNum >= 2 && restMilliSec() > 0; --colorNum) {
			bool found = false;
			vector<ColorId> backupSln = CurrentSln; // save for retry

			for (int attempt = 0; attempt < MAX_ATTEMPTS && restMilliSec() > 0; ++attempt) {
				if (attempt > 0) {
					// stronger perturbation for fresh start
					CurrentSln = backupSln;
					for (NodeId n = 0; n < gc.nodeNum; ++n) {
						if (randBetween(0, 4) == 0) {        // 25% perturb
							CurrentSln[n] = randBetween(0, colorNum);
						}
					}
				}
				if (HEA(colorNum)) {
					bestSln = CurrentSln;
					tester.reportNewOptima(bestSln, colorNum);
					found = true;
					break;
				}
			}
			if (!found) break;
		}
	}

	}
