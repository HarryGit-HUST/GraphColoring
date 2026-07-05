#include "GraphColoring.h"
#include "Util.h"


using namespace std;
using namespace std::chrono;
using namespace szx;


int main(int argc, char* argv[]) {
	string exeName(File::extractFileName(argv[0]));

	if (argc > 2) {
		long long secTimeout = atoll(argv[1]);
		int randSeed = atoi(argv[2]);
		string instanceName((argc > 3) ? argv[3] : "unknown");
		ColorId bestColorNum = (argc > 4) ? atoi(argv[4]) : 0x7fffffff;
		GraphColoringTester::test(cin, secTimeout, randSeed, exeName, instanceName, bestColorNum);
	} else {
		GraphColoringTester::testAll(exeName);
	}
	return 0;
}
