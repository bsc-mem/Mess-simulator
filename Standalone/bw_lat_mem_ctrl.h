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

#ifndef BW_LAT_MEM_CTRL_H_
#define BW_LAT_MEM_CTRL_H_

#include <fstream>
#include <iostream>

#include <cmath> // for floor function on implement round
#include <stdint.h> // for uint32_t
#include <cassert>
#include <vector>
#include <string>


using namespace std;


class BwLatMemCtrl {
	private:
		// bound latency
		uint32_t latency;

		// latency for core --> memory controller  (in cycles)
		double onCoreLatency;

		double leadOffLatency, maxBandwidth, maxLatency;


		// onChipLatency is very important. Curves usually includes all the latency from core to main memory.
		// However, a memory simulator only return tha latency from memory controller to the main memory.
		// therefore, the data in the curves should be adjusted by onChipLatency. 
		// on other words: simulatorLatency = curveLatency - onChipLatency
		// if we consider onChipLatency we are not adding much error to the system. 
		// this latency does not change significantly during the Mess benchmarking of actual system. 
		double onChipLatency;


		// we need this two numbers to define a PID-like controller to converge smoothly.
		double lastEstimatedBandwidth;
		double lastEstimatedLatency;

		// overflow factor needs to be play with
		double overflowFactor;


		//  The latency unit of input curves is in cycles, and we need to know the frequency in which they are generated,
		//	Also the frequcy of our currecnt CPU simulator is needed to simulate everything correctly. 
		//	In simulators everything is in cycle, so we should be very careful. 
		double frequencyCPU; // frequency of the CPU
		double inputCurveFrequency; // frequency of the input curve data
		
		// fisrt dimention: read percentafe
		// second dimention data for each read percentage
		// second dimention has two value 
		// [0]: bandwidth (accesses/cycles) 
		// [1]: latency (cycles)
		vector<vector<vector<double> > > curves_data;

		// maximum bandwidth and latency for the corresponding rd_percentage
		vector<double> maxBandwidthPerRdRatio;
		vector<uint32_t> maxLatencyPerRdRatio;

		// address of the curve data
		string curveAddress;

		// counters for measuring the access count in a window of accesses
		uint32_t currentWindowAccessCount;
		uint32_t currentWindowAccessCount_wr;
		uint32_t currentWindowAccessCount_rd;
		
		// A metaparameter for our simulator. the smaller, the more accurate, but the slower
		// For ZSim, we should also consider that it cannot be smaller than each phase size of the ZSim
		uint32_t curveWindowSize;
		
		// it hols the value of the current Window. 
		// it will be used to calculate the bandwidth for updating the latency at the end of the window
		uint64_t currentWindowStartCycle;


		uint32_t lastIntReadPercentage;


        // update the bound phase
        void updateLatency(uint64_t currentWindowEndCyclen);
        // a hack to make lead-off latency of all rd_percentage equal. 
        void fitTheCurves();
        // use the data of the curves to find appropriate latency
        uint32_t searchForLatencyOnCurve(double bandwidth, double readPercentage);


	public:
		BwLatMemCtrl(string curveAddress, double curveFrequency, uint32_t curveWindowSize, double frequencyRate, double _onCoreLatency);
		
		// the function to access the memory upon each read or write
		uint64_t access(uint64_t accessCycle, bool isWrite);
		// for ensuring the quality of service of memory system 
		uint64_t GetQsMemLoadCycleLimit();
		uint32_t getLeadOffLatency();
		
};




#endif  // BW_LAT_MEM_CTRL_H_
