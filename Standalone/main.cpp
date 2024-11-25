#include <stdio.h>
#include <string>	
#include <iostream>
#include <cstdlib>

#include <stdint.h> // for uint32_t

#include "bw_lat_mem_ctrl.h"

using namespace std;

uint64_t loopSize = 1200000;
uint64_t innteLoopSize = 10;

// each tick is a cycle. This is for test. 
// with this fucntion, we can vary the issue rate and hence the bandwidth by changing the intensity
uint64_t tick(uint64_t cycle, uint64_t intensity)
{
	return cycle + intensity;
}

// print usage
void print_usage() {
	cout << "Usage: ./main <address> <intensity> <frequencyCPU> <frequencyCurve> <onChipLatency>" << endl;
}

int main(int argc, char* argv[]) {
	
	// sanity check
	if (argc != 6) {
		print_usage();
		return 1;
	}

	string myAddress = argv[1];
	int intensity = atoi(argv[2]);
	float frequencyCPU = atof(argv[3]);
	float frequencyCurve = atof(argv[4]);
	float onChipLatency = atof(argv[5]);

	// sanity check
	if (intensity < 0) {
		print_usage();
		return 1;
	}

	uint32_t lat; 
	
	// it functions as the simulated clock cycle of CPU simulator  
	uint64_t cycle = 0;

	// create a new BwLatMemCtrl object.  

	// For CXL (latency of curves: CXL host to memory)
	BwLatMemCtrl* bwLatMemCtrl = new BwLatMemCtrl(myAddress, frequencyCurve, 1000, frequencyCPU, onChipLatency);

	// For other curves (latency of curves: core to memory)
	// BwLatMemCtrl* bwLatMemCtrl = new BwLatMemCtrl(myAddress, 2.1, 1000, 1.5, 51);

	// issue some writes
	// for (int i = 0; i < 1200; ++i) {
	// 	bwLatMemCtrl->access(cycle, 1);
	// 	cycle = tick(cycle, intensity);
	// }

	// issue some reads
	for (int i = 0; i < loopSize; ++i) {
		for ( int j=0; j<innteLoopSize ; j++)
			lat = bwLatMemCtrl->access(cycle, 0); 
		cycle = tick(cycle, intensity);
	}

	
	if(intensity!=0) {
		cout << fixed << setprecision(2);
		cout << lat/frequencyCPU << ", " <<  innteLoopSize*frequencyCPU*64/(intensity) << " GB/s" << endl;
	}

	delete bwLatMemCtrl; 
	return 0;
}
