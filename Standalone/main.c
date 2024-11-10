#include <stdio.h>
#include <string>	
	
#include <stdint.h> // for uint32_t

#include "bw_lat_mem_ctrl.h"


// 1 highest bw. The biffet the lower bw
#define ISSUE_INTENSITY 1


using namespace std;

// each tick is cycle. 
// just for playing. i do not know the number. 
uint64_t tick(uint64_t cycle)
{
	return cycle + ISSUE_INTENSITY;
}


int main() {
	string myAddress("./cxl-curve");
	uint32_t lat1, lat2, lat3, lat4;	

	// initialize cycles
	uint64_t cycle=0;


	// creating the memory instance
	BwLatMemCtrl* bwLatMemCtrl = new BwLatMemCtrl(myAddress, 1000, 50, 1.5);


	/* 	access function has two inputs (1) cycle, (2) isWrite 0: read 1: wirte  	*/
	
	// sending writes 
	for (int i = 0; i < 1200; ++i)
	{
		bwLatMemCtrl->access(cycle,1);
		cycle = tick(cycle);
	}

	// sending reads
	for (int i = 0; i < 1200; ++i)
	{
		lat1 = bwLatMemCtrl->access(cycle,0);
		cycle = tick(cycle);
	}
	
	cout << "latency from function: " << lat1 <<  endl;



	return 0;
}
