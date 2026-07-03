#include "GraphColoring.h"


using namespace std;
using namespace std::chrono;
using namespace szx;


int main(int argc, char* argv[]) {
    string exeName(File::extractFileName(argv[0]));

	if (argc > 2) {
		long long secTimeout = atoll(argv[1]);
		int randSeed = atoi(argv[2]);
        GraphColoringTester::test(cin, secTimeout, randSeed, exeName, "unknown");
	} else {
        GraphColoringTester::testAll(exeName);
	}
	return 0;
}
