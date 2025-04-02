/*
 * Copyright (c) 2024, Barcelona Supercomputing Center
 * Contact: mess             [at] bsc [dot] es
 *          pouya.esmaili    [at] bsc [dot] es
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
#include <cmath> // for floor function on implement round

#include "timing_event.h"
#include "zsim.h"
#include "memory_hierarchy.h"
#include "g_std/g_string.h"
#include "pad.h"
#include "stats.h"

using namespace std;


class BwLatMemCtrl : public MemObject {
	private:
		
		const uint32_t domain;
		// bound latency
		uint32_t latency;


		double leadOffLatency, maxBandwidth, maxLatency;
		// maximum theoritical bandwidth. We should get this from config file later 
		// or we can remove it at the end. it is good for debugging but no use in our memory model
		double MaxTheoreticalBW;

		// we need this two numbers to define a PID-like controller to converge smoothly.
		double lastEstimatedBandwidth;
		double lastEstimatedLatency;

		// overflow factor needs to be play with
		double overflowFactor;


		// fisrt dimention: read percentafe
		// second dimention data for each read percentage
		// second dimention has two value 
		// [0]: bandwidth (accesses/cycles) 
		// [1]: latency (cycles)
		g_vector<g_vector<g_vector<double>>> curves_data;

		// maximum bandwidth and latency for the corresponding rd_percentage
		g_vector<double> maxBandwidthPerRdRatio;
		g_vector<uint32_t> maxLatencyPerRdRatio;

		// address of the curve data
		string curveAddress;
		// 
		uint32_t currentWindowAccessCount;
		uint32_t currentWindowAccessCount_wr;
		uint32_t currentWindowAccessCount_rd;
		
		uint32_t curveWindowSize;
		
		uint64_t currentWindowStartCycle;


		uint32_t lastIntReadPercentage;


		PAD();

		// profiling counters
        Counter profReads;
        Counter profWrites;
        Counter profTotalRdLat;
        Counter profTotalWrLat;


        g_string name; //barely used
        lock_t updateLock;
        PAD();

        // update the bound phase
        void updateLatency(uint64_t currentWindowEndCyclen);
        // a hack to make lead-off latency of all rd_percentage equal. 
        void fitTheCurves();
        // use the data of the curves to find appropriate latency
        uint32_t searchForLatencyOnCurve(double bandwidth, double readPercentage);


	public:
		BwLatMemCtrl(string curveAddress, uint32_t curveWindowSize, double MaxTheoreticalBW, uint32_t _domain, const g_string &_name);
		void initStats(AggregateStat *parentStat);
		uint64_t access(MemReq &req);
		const char *getName() { return name.c_str(); }
		// for ensuring the quality of service of memory system 
		uint64_t GetQsMemLoadCycleLimit();

		uint32_t getLeadOffLatency();
		
};




#endif  // BW_LAT_MEM_CTRL_H_
