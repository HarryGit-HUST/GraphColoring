solve(restMilliSec, seed)
│
├─ DSATUR (第34-73行)
│   └─ 产生第一个合法着色 → bestSln, colorNum
│      复杂度: O(|V|²·log deg), 一次性
│
└─ 外层循环: for k = colorNum-1 down to 2 (第364行)
   每个 k 最多 3 次尝试
   │
   └─ HEA(k) (第243行)
      │
      ├─ 种群初始化 (第254-267行)
      │   10~15 个个体
      │   每个: CurrentSln + 33%随机扰动 → 浅层Tabu(gc.nodeNum×100步)
      │
      └─ 主循环 (第300行)
          每代:
          ├─ 锦标赛选择 2 个父代
          ├─ GCPX 交叉 (第185行) — 邻接加权选类
          ├─ Tabu Search 局部搜索 (第84行) — 自适应深度
          ├─ 多样性检查 (>90%相似则拒绝)
          ├─ 替换最差个体
          └─ 120代无改善 → 扰动重启 (第338行)


短线 (性价比最高, 你自己能做)
├─ 增量 conflictNodes: 目前每轮重建 O(|V|) → 改成 swap-and-pop O(1)
├─ "有希望的"颜色筛选: 不是 130 种颜色全试, 只试邻域中实际出现的 ~min(deg,k) 种
└─ 预期: 加速 ~3x, 1-2 色改善

中线 (需要改架构)
├─ Island Model: 4 个独立小 HEA 并行, 定期交换最优个体
└─ 预期: 3-5 色改善

长线 (论文级)
├─ 替换 LS: PartialCol / QA-inspired heuristics
└─ 预期: 逼近文献最优

## 3.1 编译
cd d:/program/NPBenchmark-SDK/Build/VisualStudio2019
"C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\MSBuild\Current\Bin\MSBuild.exe" \
  GraphColoring/GraphColoring.vcxproj -p:Configuration=Release -p:Platform=x64

## 3.2 运行

#### 单实例（推荐调试）
cd d:/program/NPBenchmark-SDK
./Build/.../GraphColoring.exe 超时秒 种子 实例名 参考颜色数 < 数据文件

#### 例: 10秒, 种子612, DSJC0125.1, 参考125色
./Build/.../GraphColoring.exe 10 612 DSJC0125.1 125 < Data/GraphColoring/DSJC0125.1.txt
## 3.3 查看结果
cat Submission/GraphColoring/GraphColoring.exe.log