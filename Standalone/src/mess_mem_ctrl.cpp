/*
 * Copyright (c) 2024, Barcelona Supercomputing Center
 * Contact: mess             [at] bsc [dot] es
 *          pouya.esmaili [at] bsc [dot] es
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

#include "mess_mem_ctrl.h"

// Custom round function to avoid dependency issues
static double roundDouble(double d) {
    return std::floor(d + 0.5);
}

/**
 * @brief Constructs a MessMemCtrl object, initializing all internal states and
 *        loading bandwidth-latency curves from the specified directory.
 *
 * The constructor reads pre-characterized bandwidth-latency curves for various
 * read percentages, converts bandwidth and latency units, and stores them for
 * later use. It also calculates initial lead-off latency and sets up internal
 * variables needed for simulation.
 *
 * @param _curveAddress Path to the directory containing curve files.
 * @param _curveWindowSize Number of accesses per measurement window.
 * @param frequencyRate CPU frequency in GHz.
 */
MessMemCtrl::MessMemCtrl(const std::string& _curveAddress,
                           uint32_t _curveWindowSize, double frequencyRate)
    : curveAddress(_curveAddress),
      curveWindowSize(_curveWindowSize),
      frequencyCPU(frequencyRate),
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
            // The input latency is in ns; convert it to the CPU's cycles
            inputLatency *= frequencyCPU;

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

        // set initial latency to the lead-off latency
        lastEstimatedLatency = leadOffLatency;
        latency = static_cast<uint32_t>(leadOffLatency);

        // Close the curve file
        curveFile.close();
    }
}

/**
 * @brief Returns the minimum achievable latency (lead-off latency).
 *
 * The lead-off latency is the lowest possible latency observed among all
 * loaded curves and serves as a baseline for the memory system.
 *
 * @return Lead-off latency in cycles.
 */
uint32_t MessMemCtrl::getLeadOffLatency() {
    // Return the minimum achievable latency for memory access
    return static_cast<uint32_t>(leadOffLatency);
}


/**
 * @brief Searches for the appropriate latency given a measured bandwidth and read percentage.
 *
 * This method maps the current bandwidth and read percentage to the corresponding
 * point on the pre-loaded bandwidth-latency curves. It employs a PID-like controller
 * to smoothly converge to the right latency value, preventing abrupt changes. If the
 * bandwidth exceeds a threshold, it applies an overflow penalty to simulate
 * increased contention.
 *
 * @param bandwidth Current measured bandwidth in accesses per cycle.
 * @param readPercentage Fraction of accesses that are reads, from 0.0 to 1.0.
 * @return Estimated latency in cycles for the given bandwidth and read percentage.
 */
uint32_t MessMemCtrl::searchForLatencyOnCurve(double bandwidth, double readPercentage) {
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


/**
 * @brief Updates the latency at the end of a measurement window.
 *
 * After collecting a window's worth of memory accesses, this method calculates the
 * observed bandwidth and read percentage, then uses them to update the estimated
 * latency. It ensures that the latency value reflects the current memory load.
 *
 * @param currentWindowEndCycle The cycle number at which the current measurement window ends.
 */
void MessMemCtrl::updateLatency(uint64_t currentWindowEndCycle) {
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

/**
 * @brief Simulates a memory access at a given cycle and returns its latency.
 *
 * Each access is recorded. Once the number of accesses reaches the window size,
 * the latency is recalculated based on the collected statistics. Subsequent accesses
 * will reflect any changes in the memory system conditions.
 *
 * @param accessCycle The cycle at which the memory access occurs.
 * @param isWrite True if the access is a write, false if it is a read.
 * @return Latency in cycles for this access.
 */
uint64_t MessMemCtrl::access(uint64_t accessCycle, bool isWrite) {
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

/**
 * @brief Retrieves the additional latency penalty when exceeding maximum bandwidth.
 *
 * This method calculates how much the current latency surpasses the maximum latency
 * for the given read percentage. It helps model scenarios where bandwidth saturation
 * leads to latency penalties, aiding Quality of Service (QoS) simulations.
 *
 * @return The latency penalty in cycles if current latency exceeds max latency,
 *         otherwise 0.
 */
uint64_t MessMemCtrl::GetQsMemLoadCycleLimit() {
    // Calculate the index for the current read percentage
    uint32_t curveDataIndex = lastIntReadPercentage / 2;

    // Determine if there's a latency penalty due to bandwidth exceeding the maximum
    if (latency > static_cast<uint32_t>(maxLatencyPerRdRatio[curveDataIndex]))
        return latency - static_cast<uint32_t>(maxLatencyPerRdRatio[curveDataIndex]);
    else
        return 0;
}