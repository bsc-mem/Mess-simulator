/*
 * Copyright (c) 2024, Barcelona Supercomputing Center
 * Contact: pouya.esmaili [at] bsc [dot] es
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

// Custom round function to avoid dependency issues
static double roundDouble(double d) {
    return std::floor(d + 0.5);
}

BwLatMemCtrl::BwLatMemCtrl(const std::string& _curveAddress, double _curveFrequency,
                           uint32_t _curveWindowSize, double frequencyRate, double _onCoreLatency)
    : curveAddress(_curveAddress),
      curveWindowSize(_curveWindowSize),
      frequencyCPU(frequencyRate),
      onCoreLatency(_onCoreLatency),
      leadOffLatency(100000), // Initialize with a large value; will be updated later
      maxBandwidth(0),
      maxLatency(0),
      currentWindowAccessCount(0),
      currentWindowAccessCount_wr(0),
      currentWindowAccessCount_rd(0),
      lastEstimatedBandwidth(0),
      lastEstimatedLatency(leadOffLatency),
      latency(static_cast<uint32_t>(leadOffLatency)),
      overflowFactor(0),
      lastIntReadPercentage(0) {
    // Load curve data for 101 different read percentages: 0%, 2%, ..., 100%
    for (uint32_t i = 0; i <= 100; i += 2) {
        // Generate the file path for the current read percentage
        std::string fileAddress = curveAddress + "/bwlat_" + std::to_string(i) + ".txt";
        std::ifstream curveFile(fileAddress);

        // Check if the file was successfully opened
        if (!curveFile.is_open()) {
            std::cerr << "Failed to open curve file: " << fileAddress << std::endl;
            continue;
        }

        // Temporary variables to hold the maximum bandwidth and latency for the current curve
        std::vector<std::vector<double>> curve_data;
        double maxBandwidthTemp = 0;
        double maxLatencyTemp = 0;

        double inputBandwidth, inputLatency;

        // Read the curve data from the file
        while (curveFile >> inputBandwidth >> inputLatency) {
            // Convert bandwidth from MB/s to accesses per cycle
            // Assuming each access is 64 bytes
            inputBandwidth = (inputBandwidth / 64) / (frequencyCPU * 1000);

            // Adjust input latency based on the CPU frequency
            // The input latency is in cycles at the curve's frequency; convert it to the CPU's cycles
            inputLatency *= (frequencyCPU / _curveFrequency);

            // Adjust latency based on on-core latency
            // onCoreLatency = 0 for curves measured from the memory controller
            // onCoreLatency > 0 for curves measured from the CPU core
            inputLatency -= onCoreLatency;

            // Store the data point (bandwidth, latency)
            curve_data.push_back({inputBandwidth, inputLatency});

            // Update lead-off latency (minimum latency)
            if (leadOffLatency > inputLatency)
                leadOffLatency = inputLatency;

            // Update maximum latency and bandwidth for all curves
            if (maxLatency < inputLatency)
                maxLatency = inputLatency;
            if (maxBandwidth < inputBandwidth)
                maxBandwidth = inputBandwidth;

            // Update maximum latency and bandwidth for the current read percentage
            if (maxLatencyTemp < inputLatency)
                maxLatencyTemp = inputLatency;
            if (maxBandwidthTemp < inputBandwidth)
                maxBandwidthTemp = inputBandwidth;
        }

        // Store the maximum bandwidth and latency for the current read percentage
        maxBandwidthPerRdRatio.push_back(maxBandwidthTemp);
        maxLatencyPerRdRatio.push_back(maxLatencyTemp);
        curves_data.push_back(curve_data);

        // Close the curve file
        curveFile.close();
    }
}

uint32_t BwLatMemCtrl::getLeadOffLatency() {
    // Return the minimum achievable latency for memory access
    return static_cast<uint32_t>(leadOffLatency);
}

uint32_t BwLatMemCtrl::searchForLatencyOnCurve(double bandwidth, double readPercentage) {
    const double convergeSpeed = 0.05; // Convergence factor for PID-like controller

    // Convert read percentage to an even integer between 0 and 100
    uint32_t intReadPercentage = static_cast<uint32_t>(roundDouble(100 * readPercentage * 0.5)) * 2;
    lastIntReadPercentage = intReadPercentage;

    // Ensure the read percentage is within valid bounds
    assert(intReadPercentage <= 100);
    assert(intReadPercentage % 2 == 0);

    // Determine the index for the curves_data based on read percentage
    uint32_t curveDataIndex = intReadPercentage / 2;

    // Initialize estimated data points
    double finalLatency = leadOffLatency;
    double finalBW = 0.0;

    // Apply a weighted average to bandwidth for smooth convergence (PID-like control)
    bandwidth = convergeSpeed * bandwidth + (1 - convergeSpeed) * lastEstimatedBandwidth;

    // Check if the bandwidth exceeds the maximum allowed for the current read percentage
    if (maxBandwidthPerRdRatio[curveDataIndex] * 0.99 < bandwidth) {
        // Limit the bandwidth to the maximum and apply a weighted average
        finalBW = convergeSpeed * maxBandwidthPerRdRatio[curveDataIndex] +
                  (1 - convergeSpeed) * lastEstimatedBandwidth;

        // Increase overflow factor to simulate latency penalty for high bandwidth
        overflowFactor += 0.02;

        // Calculate the latency penalty based on the overflow factor
        finalLatency = (1 + overflowFactor) * maxLatencyPerRdRatio[curveDataIndex];
        finalLatency = convergeSpeed * finalLatency + (1 - convergeSpeed) * lastEstimatedLatency;

        // Update the last estimated bandwidth and latency
        lastEstimatedBandwidth = finalBW;
        lastEstimatedLatency = finalLatency;

        // Ensure latency is not less than the lead-off latency
        if (finalLatency < leadOffLatency)
            finalLatency = leadOffLatency;

        // Sanity check
        assert(finalLatency >= leadOffLatency);
        return static_cast<uint32_t>(finalLatency);
    }

    // Find the appropriate latency corresponding to the current bandwidth
    uint32_t j;
    for (j = 0; j < curves_data[curveDataIndex].size(); ++j) {
        if (finalBW == 0) {
            // Initialize finalBW and finalLatency with the first data point
            finalBW = curves_data[curveDataIndex][j][0];
            finalLatency = curves_data[curveDataIndex][j][1];
        }
        if (curves_data[curveDataIndex][j][0] >= bandwidth) {
            // Update finalBW and finalLatency if the curve's bandwidth is greater than or equal to the current bandwidth
            finalBW = curves_data[curveDataIndex][j][0];
            finalLatency = curves_data[curveDataIndex][j][1];
        } else {
            // Break the loop when we find a bandwidth less than the current bandwidth
            break;
        }
    }

    // Adjust index if we've reached the end of the curve data
    if (j == curves_data[curveDataIndex].size())
        j--;

    if (j != 0) { // Not at the first data point (highest bandwidth)
        // Perform linear interpolation between two data points to estimate the latency
        double x1 = curves_data[curveDataIndex][j][0];
        double y1 = curves_data[curveDataIndex][j][1];
        double x2 = curves_data[curveDataIndex][j - 1][0];
        double y2 = curves_data[curveDataIndex][j - 1][1];
        double x = bandwidth;

        // Calculate the interpolated latency
        finalLatency = y1 + ((x - x1) / (x2 - x1)) * (y2 - y1);

        // Adjust latency with overflow factor to stabilize the system
        finalLatency += overflowFactor * finalLatency;

        // Apply a weighted average to latency for smooth convergence
        finalLatency = convergeSpeed * finalLatency + (1 - convergeSpeed) * lastEstimatedLatency;

        // Decrease overflow factor if not in overflow mode
        if (overflowFactor > 0.01)
            overflowFactor -= 0.01;
    } else {
        // At the first data point; use the current finalLatency
        finalLatency += overflowFactor * finalLatency;
        finalLatency = convergeSpeed * finalLatency + (1 - convergeSpeed) * lastEstimatedLatency;

        // Decrease overflow factor if not in overflow mode
        if (overflowFactor > 0.01)
            overflowFactor -= 0.01;
        if (overflowFactor < 0)
            overflowFactor = 0;
    }

    // Update the last estimated bandwidth and latency
    lastEstimatedBandwidth = bandwidth;
    lastEstimatedLatency = finalLatency;

    // Ensure latency is not less than the lead-off latency
    if (finalLatency <= leadOffLatency)
        finalLatency = leadOffLatency;

    // Sanity check
    assert(finalLatency >= leadOffLatency);

    return static_cast<uint32_t>(finalLatency);
}

void BwLatMemCtrl::updateLatency(uint64_t currentWindowEndCycle) {
    // Calculate bandwidth in accesses per cycle
    double bandwidth =
        static_cast<double>(currentWindowAccessCount) / (currentWindowEndCycle - currentWindowStartCycle);

    // Calculate read percentage
    double readPercentage =
        static_cast<double>(currentWindowAccessCount_rd) /
        (currentWindowAccessCount_rd + currentWindowAccessCount_wr);

    // Update latency based on the calculated bandwidth and read percentage
    latency = searchForLatencyOnCurve(bandwidth, readPercentage);

    // Sanity check
    assert(latency >= 0);
}

uint64_t BwLatMemCtrl::access(uint64_t accessCycle, bool isWrite) {
    // Start cycle of the measurement window
    if (currentWindowAccessCount == 0) {
        currentWindowStartCycle = accessCycle;
    }

    // Increment access counts
    currentWindowAccessCount++;
    if (isWrite) {
        currentWindowAccessCount_wr++;
    } else {
        currentWindowAccessCount_rd++;
    }

    // Check if we've reached the end of the measurement window
    if (currentWindowAccessCount == curveWindowSize) {
        // Update latency based on the current window's statistics
        updateLatency(accessCycle);

        // Reset counts for the new window
        currentWindowAccessCount = 0;
        currentWindowAccessCount_wr = 0;
        currentWindowAccessCount_rd = 0;
    }

    // Return the latency in cycles (CPU frequency)
    return static_cast<uint64_t>(latency);
}

uint64_t BwLatMemCtrl::GetQsMemLoadCycleLimit() {
    // Calculate the index for the current read percentage
    uint32_t curveDataIndex = lastIntReadPercentage / 2;

    // Determine if there's a latency penalty due to bandwidth exceeding the maximum
    if (latency > static_cast<uint32_t>(maxLatencyPerRdRatio[curveDataIndex]))
        return latency - static_cast<uint32_t>(maxLatencyPerRdRatio[curveDataIndex]);
    else
        return 0;
}