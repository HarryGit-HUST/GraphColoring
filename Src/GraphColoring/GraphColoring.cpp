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
	// init random number generator (it is recommended to avoid using `rand()` in `stdlib.h`).
	// `rand() % n` produces biased random number (numbers less than $2^32 mod n$ may be selected with slightly higher probability).
	mt19937 rand(seed);
	// random number in uniform distribution.
	auto randBetween = [&](int lb, int ub) { return uniform_int_distribution<int>(lb, ub - 1)(rand); };

	/*
	 * Generate an initial solution for the graph coloring problem.
	 */
	vector<ColorId> bestSln(gc.nodeNum, -1);// `bestSln[n]` is the color of node `n`.output solution.

	vector<vector<NodeId>> adjacent(gc.nodeNum); // `adjacent[n]` is the list of nodes adjacent to node `n`.
	// build the adjacency list of the graph.
	for (EdgeId e = 0; e < gc.edgeNum; ++e) {
		auto [i, j] = gc.edges[e];
		adjacent[i].push_back(j);
		adjacent[j].push_back(i);
	}
	// generate an initial solution for the graph coloring problem. using greedy algorithm.
	int colorNum = 0;
	for(NodeId n = 0; n < gc.nodeNum; ++n) {
		vector<bool> usedColors(colorNum, false);
		for (NodeId adj : adjacent[n]) {
			if (bestSln[adj] != -1 && bestSln[adj] < colorNum) {
				usedColors[bestSln[adj]] = true;
			}
		}
		ColorId color = 0;
		while (color < colorNum && usedColors[color]) {
			++color;
		}
		if (color == colorNum) {
			++colorNum;
		}
		bestSln[n] = color;
	}
	tester.reportNewOptima(bestSln, colorNum);
	/*initial data structure*/
	vector<ColorId> CurrentSln = bestSln;//禁忌搜索的工作解
	
	/*start tabu search*/
	auto tabuSearch = [&](int k) -> bool
	{
		// 1. 只允许颜色 [0, k-1]，把当前解中 >= k 的颜色随机换掉
		for (NodeId n = 0; n < gc.nodeNum; ++n)
		{
			if (CurrentSln[n] >= k)
			{
				CurrentSln[n] = randBetween(0, k);
			}
		}

		// 2. 计算初始冲突
		vector<int> conflictCount(gc.nodeNum, 0);
		int totalConflicts = 0;
		for (EdgeId e = 0; e < gc.edgeNum; ++e)
		{
			auto [u, v] = gc.edges[e];
			if (CurrentSln[u] == CurrentSln[v])
			{
				++conflictCount[u];
				++conflictCount[v];
				++totalConflicts;
			}
		}

		// 3. 禁忌表: tabu[n][c] = 迭代号，在此之前不能把 n 改成 c
		vector<vector<int>> tabu(gc.nodeNum, vector<int>(k, 0));
		int iter = 0;

		// 4. 主循环
		cerr << "k=" << k << " initial conflicts=" << totalConflicts << endl;
		while (restMilliSec() > 0 && totalConflicts > 0)
		{
			++iter;

			// 找最优移动...
			int bestDelta = INT_MAX;
			NodeId bestNode = -1;
			ColorId bestColor = -1;
			for (NodeId n = 0; n < gc.nodeNum; ++n)
			{
				if (conflictCount[n] > 0)
				{
					// 尝试改变 n 的颜色，计算冲突变化量
					for (ColorId newColor = 0; newColor < k; ++newColor)
					{
						if (newColor == CurrentSln[n]) continue;
						if (iter >= tabu[n][newColor])
						{
							int deltaConflicts = 0;
							for (NodeId adj : adjacent[n])
							{
								if (CurrentSln[adj] == CurrentSln[n]) deltaConflicts--;
								if (CurrentSln[adj] == newColor) deltaConflicts++;
							}
							// 如果是最优移动，记录下来
							if (deltaConflicts < bestDelta)
							{
								bestDelta = deltaConflicts;
								bestNode = n;
								bestColor = newColor;
							}
						}
					}
				}
			}
			if (bestNode == -1)
				break; // 所有移动都被禁忌了
			// 执行移动...
			ColorId oldColor = CurrentSln[bestNode]; // 先存

			CurrentSln[bestNode] = bestColor;
			totalConflicts += bestDelta; // bestDelta 已经算好了

			// 更新冲突...
			conflictCount[bestNode] += bestDelta;
			for (NodeId adj : adjacent[bestNode])
			{
				if (CurrentSln[adj] == oldColor)
				{
					// 这个邻居原来和 bestNode 冲突，现在不冲突了
					--conflictCount[adj];
				}
				if (CurrentSln[adj] == bestColor)
				{
					// 这个邻居现在和 bestNode 同色，新产生冲突
					++conflictCount[adj];
				}
			}
			int tenure = 10; // 先写死，后面再调
			// 更新禁忌表...
			tabu[bestNode][oldColor] = iter + tenure;
		}

		cerr << "tabuSearch k=" << k << " exit: iter=" << iter
			<< " conflicts=" << totalConflicts
			<< " time=" << restMilliSec() << "ms" << endl;
		return totalConflicts == 0;

	};


	//main loop: try to find a better solution with fewer colors.
	for (colorNum=colorNum-1; colorNum >= 2 && restMilliSec() > 0; --colorNum)
	{
		// 尝试用 k 种颜色（0..k-1）着色整个图
		if (tabuSearch(colorNum))
		{
			bestSln = CurrentSln; 
			tester.reportNewOptima(bestSln, colorNum);
		}
		else
		{
			break; // k 种颜色找不到，更少的就更不可能了
		}
	}
}



}