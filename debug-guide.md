# 图着色求解器调试指南

## 1. 单线程调试配置

编辑 `Src/GraphColoring/GraphColoringTester.cpp` 第 154 行：

```cpp
// 改前（多线程）:
JobFighter::fightForJob(thread::hardware_concurrency(), [&](JobFighter::IsJobTaken isJobTaken) {

// 改后（单线程）:
JobFighter::fightForJob(1, [&](JobFighter::IsJobTaken isJobTaken) {
```

这行出现两次（第 151 行加载实例、第 162 行启动求解器），都改成 1。

## 2. 控制测试范围

编辑 `Data/GraphColoring/0.Baseline.txt`：

```
级别  实例名              重复次数  minOptRun  超时秒  已知最优
0     DSJC0125.1.txt      2        0          10      5
0     DSJC0500.5.txt      1        0          60      48
```

- **级别**：全设 0 就不会被跳过
- **重复次数**：1 即可，调试时不需要多次
- **minOptRun**：设 0 不会触发"没找到最优就停"
- **超时秒**：调试时设大一点（如 120），免得超时
- **已知最优**：设一个合理的上界即可，比最优值大也只会影响日志输出

## 3. 编译

```bash
cd d:/program/NPBenchmark-SDK/Build/VisualStudio2019
"C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\MSBuild\Current\Bin\MSBuild.exe" \
  GraphColoring/GraphColoring.vcxproj -p:Configuration=Release -p:Platform=x64
```

## 4. 运行

### 方式A：跑全部（testAll）
```bash
cd d:/program/NPBenchmark-SDK
./Build/VisualStudio2019/GraphColoring/x64/Release/GraphColoring.exe
```
读取 baseline，按行跑所有测试。

### 方式B：跑单个实例
```bash
cd d:/program/NPBenchmark-SDK
./Build/.../GraphColoring.exe 超时秒 种子 实例名 参考颜色数 < 数据文件
# 例:
./Build/.../GraphColoring.exe 30 612 DSJC0125.1 125 < Data/GraphColoring/DSJC0125.1.txt
```

### 方式C：VS 断点调试
1. 打开 `Build/VisualStudio2019/NPBenchmark.sln`
2. 项目 → 属性 → 调试 → 命令参数：`30 612 DSJC0125.1 125`
3. 设置工作目录为 `d:\program\NPBenchmark-SDK`
4. F5 启动调试

## 5. 查看结果

```bash
# 运行日志（每次运行追加一行）
cat Submission/GraphColoring/GraphColoring.exe.log

# 格式:
# 实例名    种子  颜色数  耗时ms  节点差  冲突边数
# DSJC0125.1  612  5      3       0      0
#                        ↑找到的  ↑合法性  ↑0=无冲突
#                        颜色数   (0=OK)
```

**读日志技巧**：
- 颜色数 ≤ 已知最优 → 找到了更优解，会在 Data/GraphColoring/ 下保存 `.log` 文件
- 冲突边数 > 0 → 解非法（不应该出现）
- 耗时很小 + 颜色数 = 节点数 → trivial 解，算法没工作
- 同实例多行 → 看最小颜色数那行 = 历史最佳

## 6. 加调试输出

只能用 `cerr`（`stdout` 被数据流占用），而且最后要删掉（提交规范禁止 `<iostream>`）。

```cpp
// 关键位置加:
cerr << "k=" << k << " uncolored=" << uncoloredCount << endl;
cerr << "iter=" << iter << " bestCost=" << bestCost << endl;
```

最值得加调试输出的位置：

| 位置 | 文件行号 | 想看的 |
|------|---------|--------|
| HEA 每代结束 | GraphColoring.cpp HEA() 内 | pop 最好 uncolored, 代数 |
| PartialCol 每 N 迭代 | partialColSearch() 主循环 | uncoloredCount, iter |
| 外层循环 | solve() 外层 for | k, found? |
| GCPX 返回后 | HEA() 内 | child 初始 uncolored（优化前） |

## 7. 关键可调参数速查

| 参数 | 文件位置 | 当前值 | 作用 |
|------|---------|--------|------|
| `POP_SIZE` | HEA(): L305 | 10 或 15 | 种群大小 |
| `EARLY_STOP` | partialColSearch(): L136 | max(100000, gc.nodeNum*500) | 无改善多少迭代后退出 |
| `RESTART_THRESHOLD` | HEA(): L307 | 120 | 多少代无改善触发重启 |
| `DIVERSITY_THRESHOLD` | HEA(): L306 | 0.9 | 相似度超过此值拒绝子代 |
| `MIGRATION_INTERVAL` | islandHEA(): L420 | 5 | Island 模型迁移间隔 |
| `MAX_ATTEMPTS` | solve(): ~L535 | 3 | 每 k 重试次数 |
| `tenure 公式` | partialColSearch() L177 | 1~12 + 0.6*uncolored | 禁忌长度 |

## 8. 常见问题排查

| 症状 | 可能原因 | 查什么 |
|------|---------|--------|
| 颜色数=节点数 | DSATUR/初始化没工作 | 打断点看 bestSln |
| 耗时极大，无输出 | PartialCol 内循环卡死 | k 是否 = 0？uncolored 是否 > 0？|
| 冲突边数 > 0 | 解不合法 | PartialCol 退出时 uncolored>0 且没踢干净 |
| 每次结果不一样 | 随机种子未固定 | 确认传入了 seed 参数 |
| 编译错误 C2429 | 没开 C++17 | 检查 vcxproj 的 LanguageStandard |
