#include <stdio.h>
#include <string>	
#include <iostream>
#include <cstdlib>

#include <stdint.h> // for uint32_t

#include "bw_lat_mem_ctrl.h"



using namespace std;

// each tick is cycle. 
// just for playing. i do not know the number. 
uint64_t tick(uint64_t cycle, uint64_t intensity)
{
	return cycle + intensity;
}


void print_usage() {
	cout << "Usage: ./main <address> <intensity>" << endl;
}

int main(int argc, char* argv[]) {
	if (argc != 3) {
		print_usage();
		return 1;
	}

	string myAddress = argv[1];
	int intensity = atoi(argv[2]);

	if (intensity < 0) {
		print_usage();
		return 1;
	}

	uint32_t lat1, lat2, lat3, lat4;    
	uint64_t cycle = 0;

	BwLatMemCtrl* bwLatMemCtrl = new BwLatMemCtrl(myAddress, 1000, 1.5);

	for (int i = 0; i < 1200; ++i) {
		bwLatMemCtrl->access(cycle, 1);
		cycle = tick(cycle, intensity);
	}

	for (int i = 0; i < 1200; ++i) {
		lat1 = bwLatMemCtrl->access(cycle, 0);
		cycle = tick(cycle, intensity);
	}

	cout << "latency from function: " << lat1 << endl;

	delete bwLatMemCtrl;
	return 0;
}
