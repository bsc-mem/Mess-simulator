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


		double leadOffLatency, maxBandwidth, maxLatency;
		double onChipLatency;


		// we need this two numbers to define a PID-like controller to converge smoothly.
		double lastEstimatedBandwidth;
		double lastEstimatedLatency;

		// overflow factor needs to be play with
		double overflowFactor;


		double frequencyCPU;
		
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
		// 
		uint32_t currentWindowAccessCount;
		uint32_t currentWindowAccessCount_wr;
		uint32_t currentWindowAccessCount_rd;
		
		uint32_t curveWindowSize;
		
		uint64_t currentWindowStartCycle;


		uint32_t lastIntReadPercentage;



		// // profiling counters
        // Counter profReads;
        // Counter profWrites;
        // Counter profTotalRdLat;
        // Counter profTotalWrLat;



        // update the bound phase
        void updateLatency(uint64_t currentWindowEndCyclen);
        // a hack to make lead-off latency of all rd_percentage equal. 
        void fitTheCurves();
        // use the data of the curves to find appropriate latency
        uint32_t searchForLatencyOnCurve(double bandwidth, double readPercentage);


	public:
		BwLatMemCtrl(string curveAddress, uint32_t curveWindowSize, double frequencyRate);
		uint64_t access(uint64_t accessCycle, bool isWrite);
		// for ensuring the quality of service of memory system 
		uint64_t GetQsMemLoadCycleLimit();
		uint32_t getLeadOffLatency();
		
};




#endif  // BW_LAT_MEM_CTRL_H_
