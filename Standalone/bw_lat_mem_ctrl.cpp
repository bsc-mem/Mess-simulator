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

#include "bw_lat_mem_ctrl.h"


BwLatMemCtrl::BwLatMemCtrl(string _curveAddress, double _curveFrequency, uint32_t _curveWindowSize, double frequencyRate, double _onCoreLatency) 
{
	curveAddress = _curveAddress;
	curveWindowSize = _curveWindowSize;

	frequencyCPU = frequencyRate;
	onCoreLatency = _onCoreLatency;

	// initialize with non null values
	leadOffLatency = 100000;
	maxBandwidth = 0;
	maxLatency = 0;

	// curves_data 
	// 101 different read percentage curves: 0, 2, 4, 6, 8, 10, 12, 14, 16, 18, ..., 100 
	for (uint32_t i=0; i<101; i+=2) {
		string fileAddress;
		ifstream curveFiles;

		double inputBandwidth, inputLatency; 

		// generate the address of the files
		fileAddress = curveAddress + "/bwlat_" + to_string(i) + ".txt";
		
		// test to make sure we are reading correct files.
		// cout << fileAddress << endl; 

		curveFiles.open(fileAddress, std::ifstream::in);

		// read the curve file into the curves_date 
		// also set leadOffLatency, maxBandwidth, maxLatency 
		
		vector<vector<double>> curve_data;
		double maxBandwidthTemp = 0;
		double maxLatencyTemp = 0;

		while(curveFiles >> inputBandwidth >> inputLatency) {
			vector<double> dataPoint;

			// curves BW values are in MB/s. we need to change it to accesses per cycles. 
			// we consider each access is 64 bytes. Otherwise this needs to be updated. 
			inputBandwidth = (inputBandwidth / 64) / (frequencyCPU*1000)  ;

			// decrease LLC latency; this is hard coded for skylake. it should read all parent 
			// cache latency and sum them at the end. for Skylake this value is 37 (L3) + 10 (L2) + 4 (L1) = 51
			
			// inputLatency from curves is in cycles based on the freuquency of the cuve data. 
			// we need to convert it to the cycles of the CPU simulator. 
			inputLatency = inputLatency * (frequencyCPU / _curveFrequency);

			// onCoreLatency= 0 --> this is for skylake+CXL (CXL curves are from the CXL Host to memory not from the core to memory. so we do not need to deacrease the latency)
			// onCoreLatency=51 --> this is for skylake+DDR4, Graviton+DDR5 and Fujitsu+HBM2 (curves are from core to main memory)
			inputLatency = inputLatency - onCoreLatency; 
						
			dataPoint.push_back(inputBandwidth);
			dataPoint.push_back(inputLatency);			
			// cout << dataPoint[0] << " " << dataPoint[1] << endl;

			if (leadOffLatency > inputLatency)
				leadOffLatency = inputLatency;
			if (maxLatency < inputLatency)
				maxLatency = inputLatency;
			if (maxLatencyTemp < inputLatency)
				maxLatencyTemp = inputLatency;
			if (maxBandwidth < inputBandwidth)
				maxBandwidth = inputBandwidth;
			if (maxBandwidthTemp < inputBandwidth)
				maxBandwidthTemp = inputBandwidth;

			curve_data.push_back(dataPoint);
		}

		maxBandwidthPerRdRatio.push_back(maxBandwidthTemp);
		maxLatencyPerRdRatio.push_back(maxLatencyTemp); 
		curves_data.push_back(curve_data);
		curveFiles.close();
	}

	// cout << "leadOffLatency: " << (leadOffLatency + 51) / 2.1 << " ns, cycles: " << (leadOffLatency + 51) << endl;
	// cout << "maxLatency: " << (maxLatency + 51) / 2.1 << " ns"<< endl;
	// cout << "maxBandwidth: " << maxBandwidth * 64  * 2.1 << " GB/s" << endl;

	// test to see if the data is loaded correctly.
	// for (uint32_t i=0; i< curves_data.size(); i++)
	// {
	// 	cout << "read percentage is: " << i*2 << endl;
	// 	for (uint32_t j = 0; j < curves_data[i].size(); ++j)
	// 	{
	// 		cout << curves_data[i][j][0] << " " << curves_data[i][j][1] << endl;
	// 	}
	// }


	// initialization of the counters
	currentWindowAccessCount = 0;
	currentWindowAccessCount_wr = 0;
	currentWindowAccessCount_rd = 0;

	// initialize the state
	lastEstimatedBandwidth = 0;
	lastEstimatedLatency = leadOffLatency;
	latency = (uint32_t) lastEstimatedLatency;

	// initial the overflow factor
	overflowFactor = 0;

}

uint32_t BwLatMemCtrl::getLeadOffLatency() {
	return (uint32_t) leadOffLatency;
}


double round(double d) {
	return std::floor(d + 0.5);
}


uint32_t BwLatMemCtrl::searchForLatencyOnCurve(double bandwidth, double readPercentage) {

	double convergeSpeed = 0.05;
	// factor of latency over max latency when the bandwidth is maxed out.
	

	// curves_data 
	// first select the correct read to write ratio. an even number between 0 and 100
	uint32_t intReadPercentage;
	intReadPercentage = round( (100*readPercentage) * 0.5f ) * 2;
	lastIntReadPercentage = intReadPercentage;

	assert(intReadPercentage >= 0);
	assert(intReadPercentage <= 100);
	assert(intReadPercentage % 2 == 0);

	uint32_t curveDataIndex = intReadPercentage/2;

	// estimated data points
	double finalLatency=leadOffLatency;
	double finalBW=0;

	// TODO...
	// sanity check should be added later
	// cout << endl << "new" << endl;
	// cout << "maxBandwidth in all the curves : " << maxBandwidth  << " acesses/cycles" << endl;
	// cout << "measured bandwidth: " << bandwidth << " max from curve: "<< maxBandwidthPerRdRatio[curveDataIndex] << endl;
	// cout << "rd percentage: " << intReadPercentage << endl;

	// cout << "lastEstimatedLatency " << lastEstimatedLatency << endl;
	// cout << "lastEstimatedBandwidth: " << lastEstimatedBandwidth << endl;

	// do not jump. slowly converge. PID-like controller
	// Simple explanation: weithted average of the last estimated value and the new value.
	// complicated explanation: this equation represent integral operation in descrete domain for PID controller. 
	bandwidth = convergeSpeed * bandwidth + (1-convergeSpeed) * lastEstimatedBandwidth;


	// check if we overflow the maximum bandwidth. We need to add latency penaly for very high bandwidth applications
	// bool overflow=false;
	// if (maxBandwidth < bandwidth) {	
	if (maxBandwidthPerRdRatio[curveDataIndex]*0.99 < bandwidth) {
		

		// bandwidth =  maxBandwidthPerRdRatio[curveDataIndex];
		// cout << "more than max! bandwidth: " << bandwidth << " max from curve: "<< maxBandwidthPerRdRatio[curveDataIndex] << endl;
		// overflow=true;
		// let's converge slowly
		finalBW = convergeSpeed * maxBandwidthPerRdRatio[curveDataIndex] + (1-convergeSpeed) * lastEstimatedBandwidth;
		// simulate the wave form at least for a very high bandwidth
		// finalLatency = maxLatencyPerRdRatio[curveDataIndex];
		
		// add penalty for the wave form section...
		overflowFactor = overflowFactor + 0.02;
		finalLatency = (1+overflowFactor) * maxLatencyPerRdRatio[curveDataIndex];		
		finalLatency = convergeSpeed * finalLatency + (1-convergeSpeed) * lastEstimatedLatency;
		

		lastEstimatedBandwidth = finalBW;
		lastEstimatedLatency = finalLatency;

		// finalLatency = finalLatency + overflowFactor*(double)(finalLatency);
		// cout << "overflow..." << endl;
		
		// cout << "finalLatency " << finalLatency << endl;

		if(finalLatency<(uint32_t)leadOffLatency)
			finalLatency=(uint32_t)leadOffLatency;
		// put more assert
		assert(finalLatency>= (uint32_t)leadOffLatency);

		return (uint32_t) finalLatency;

		// cout << "more than max! bandwidth: " << bandwidth << " max from curve: "<< maxBandwidthPerRdRatio[curveDataIndex] << endl;
	}

	

	// find the latency that corresponds to the updated measured bandwidth: 
	// use j later for extrapolation
	uint32_t j;
	for (j = 0; j < curves_data[curveDataIndex].size(); ++j)
	{
		// cout << "finalLatency " << finalLatency << " finalBW: " << finalBW << endl;
		// cout << "curves_data[curveDataIndex][j][0]: " << curves_data[curveDataIndex][j][0] << " bandwidth " << bandwidth << endl;
		if (finalBW == 0) {
			finalBW = curves_data[curveDataIndex][j][0];
			finalLatency = curves_data[curveDataIndex][j][1];
		}
		if (curves_data[curveDataIndex][j][0] >= bandwidth) {
			finalBW = curves_data[curveDataIndex][j][0];
			finalLatency = curves_data[curveDataIndex][j][1];
		}
		else {
			break;
		}
		// cout << curves_data[curveDataIndex][j][0] << " " << curves_data[curveDataIndex][j][1] << endl;
	}
	// just a hack to see WTF is going on:
	if(j==curves_data[curveDataIndex].size())
		j--;

	if (j!=0) { // we are not at the first point (highest bandwidth)

		// for easier readability
		double x1 = curves_data[curveDataIndex][j][0];
		double y1 = curves_data[curveDataIndex][j][1];
		double x2 = curves_data[curveDataIndex][j-1][0];
		double y2 = curves_data[curveDataIndex][j-1][1];
		double x = bandwidth;
		// cout << "bandwidth: " << bandwidth << endl;
		// cout << "x1: " << x1 << "x2: " << x2 <<endl;
		// cout << "y1: " << y1 << "y2: " << y2 <<endl;
		// if we map between two data point, we draw a line between the two points
		// and then map find corresponding latency on the line that connected the two points.
		finalLatency = y1 + ((x - x1) / (x2 - x1)) * (y2 - y1);
		// cout << "final y " << finalLatency << endl;
		// overflow factor helps to stablize the system. maybe we do not need it after all. 
		// double check its importance at the end of the debugging... 
		// PID is a better idea
		finalLatency += overflowFactor*finalLatency;

		finalLatency = convergeSpeed * finalLatency + (1-convergeSpeed) * lastEstimatedLatency;
		
		// decrease overflow factor if we are not in the overflow mode anymore 
		if (overflowFactor>0.01) // never let the overflowFactor to become negative otherwise it will be a disaster
			overflowFactor = overflowFactor - 0.01;		
	}
	else {
		// cout << "bandwidth: " << bandwidth << endl;
		finalLatency += overflowFactor*finalLatency;
		finalLatency = convergeSpeed * finalLatency + (1-convergeSpeed) * lastEstimatedLatency;
		
		// decrease overflow factor if we are not in overflow mode
		if (overflowFactor>0.01) // never let the overflowFactor to overflow
			overflowFactor = overflowFactor - 0.01;
		// never let the overflowFactor to overflow
		if (overflowFactor<0)
		{
			overflowFactor=0;
		}
	}
	
	// update the last estimated point to new values
	lastEstimatedBandwidth = bandwidth;
	lastEstimatedLatency = finalLatency;

	// cout << "lastEstimatedBandwidth: " << lastEstimatedBandwidth << endl;
	// cout << "finalLatency " << finalLatency << "  leadOffLatency: " << leadOffLatency << endl;

	if(finalLatency<= (uint32_t)leadOffLatency)
		finalLatency = (uint32_t)leadOffLatency;
	// sanity check
	assert(finalLatency>= (uint32_t)leadOffLatency);


	return finalLatency;


}

void BwLatMemCtrl::updateLatency(uint64_t currentWindowEndCyclen) {
	// bandwidth unit is accesses per cycles
	
	// calculate bandwidth and rd_percentage
	double bandwidth = ((double)currentWindowAccessCount) / (double)(currentWindowEndCyclen - currentWindowStartCycle);
	double readPercentage = ((double)(currentWindowAccessCount_rd))/ (currentWindowAccessCount_rd+currentWindowAccessCount_wr);


	latency = searchForLatencyOnCurve(bandwidth, readPercentage);
	assert(latency>=0); 
	// below sanity check is not correct. we cannot be that accurate. remember !!!
	// assert(lastEstimatedLatency<=maxLatency);

	// cout << "latency: " << latency << " cycles. readPercentage: " << readPercentage << endl;

}

uint64_t BwLatMemCtrl::access(uint64_t accessCycle, bool isWrite) {
	

    // start cycle of our measuremnt window
    if (currentWindowAccessCount==0) {
    	currentWindowStartCycle = accessCycle;
    	// cout << "currentWindowStartCycle: " << currentWindowStartCycle << endl;
    }

    // counting the number of accesses, reads and writes 
    currentWindowAccessCount++;
    if (isWrite) {
    	currentWindowAccessCount_wr++;
    	// isWrite = true;
    }
    else {
    	currentWindowAccessCount_rd++;
    	// isWrite = false;
    }

    // check if we are an the end of our measurment window
    if(currentWindowAccessCount==curveWindowSize) {
    	updateLatency(accessCycle);
    	// cout << "currentWindowEndCycle: " << accessCycle << endl; 
    	currentWindowAccessCount = 0; // reset the access as we enter a new window
    	currentWindowAccessCount_wr = 0; // reset the access as we enter a new window
    	currentWindowAccessCount_rd = 0; // reset the access as we enter a new window
    }


    // return the latency based on cycles (CPU frequency)
    return (uint64_t) (latency);
}

uint64_t BwLatMemCtrl::GetQsMemLoadCycleLimit() {
	uint32_t curveDataIndex = lastIntReadPercentage/2;
	if(latency> (uint32_t)maxLatencyPerRdRatio[curveDataIndex])
		return latency - (uint32_t)maxLatencyPerRdRatio[curveDataIndex];
	else
		return 0;
}


