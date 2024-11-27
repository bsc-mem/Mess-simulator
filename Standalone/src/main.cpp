/*
 * Copyright (c) 2024, Barcelona Supercomputing Center
 * Contact: pouya.esmaili    [at] bsc [dot] es
 *          petar.radojkovic [at] bsc [dot] es
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *     * Redistributions of source code must retain the above copyright notice,
 *       this list of conditions and the following disclaimer.
 *
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *
 *     * Neither the name of the copyright holder nor the names
 *       of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

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
// with this fucntion, we can vary the issue rate and hence the bandwidth by changing the pause
uint64_t tick(uint64_t cycle, uint64_t pause)
{
	return cycle + pause;
}

// print usage
void print_usage() {
	cout << "Usage: ./main <address> <pause> <frequencyCPU> <frequencyCurve> <onChipLatency>" << endl;
}

int main(int argc, char* argv[]) {
	
	// sanity check
	if (argc != 6) {
		print_usage();
		return 1;
	}

	string myAddress = argv[1];
	int pause = atoi(argv[2]);
	float frequencyCPU = atof(argv[3]);
	float frequencyCurve = atof(argv[4]);
	float onChipLatency = atof(argv[5]);

	// sanity check
	if (pause < 0) {
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
	// 	cycle = tick(cycle, pause);
	// }

	// issue some reads
	for (int i = 0; i < loopSize; ++i) {
		for ( int j=0; j<innteLoopSize ; j++)
			lat = bwLatMemCtrl->access(cycle, 0); 
		cycle = tick(cycle, pause);
	}

	
	if(pause!=0) {
		cout << fixed << setprecision(2);
		cout << lat/frequencyCPU << ", " <<  innteLoopSize*frequencyCPU*64/(pause) << " GB/s" << endl;
	}

	delete bwLatMemCtrl; 
	return 0;
}
