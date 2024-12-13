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
#include <cmath>        // for floor and round functions
#include <cstdint>      // for uint32_t and other integer types
#include <cassert>
#include <vector>
#include <string>

using namespace std;

/**
 * @brief This class models a bandwidth-latency memory controller.
 * 
 * The controller uses pre-characterized bandwidth-latency curves to model
 * how memory behaves under various read/write traffic conditions.
 * 
 * This is designed to be used within CPU simulators, enabling fast and accurate
 * memory performance simulation. It abstracts memory system complexities, using 
 * pre-measured or simulated curves as its input.
 */
class MessMemCtrl {
private:

    /**
     * @brief Latency metrics related to the core and memory.
     */
    double onCoreLatency;           ///< Latency from the core to the memory controller (cycles).
    double leadOffLatency;          ///< Minimum achievable latency for memory access (cycles).
    double maxBandwidth;            ///< Maximum bandwidth (accesses per cycle).
    double maxLatency;              ///< Maximum latency for high contention scenarios (cycles).

    /**
     * @brief Current latency for the memory system (cycles).
     * This value is dynamically updated based on memory traffic.
     */
    uint32_t latency;
    /**
     * @brief Adjustments for on-chip latency.
     * 
     * Curves typically include the latency from the core to the main memory. 
     * However, memory simulators only simulate the latency from the memory 
     * controller to main memory. This value is subtracted from curve latencies 
     * to align them with simulation requirements.
     */
    double onChipLatency;

    /**
     * @brief PID-like controller metrics for smooth latency-bandwidth convergence.
     * 
     * These values ensure that updates to latency and bandwidth are gradual
     * and avoid abrupt transitions, especially when the system is under heavy load.
     */
    double lastEstimatedBandwidth;  ///< The most recently estimated bandwidth.
    double lastEstimatedLatency;    ///< The most recently estimated latency.
    double overflowFactor;          ///< Penalty factor for bandwidth overflow scenarios.

    /**
     * @brief Simulation frequency metrics.
     */
    double frequencyCPU;            ///< Frequency (GHz) of the simulated CPU.

    /**
     * @brief Pre-characterized bandwidth-latency curve data.
     * 
     * - `curves_data` stores the curves:
     *   - First dimension: Read percentage (0% to 100%, in 2% increments).
     *   - Second dimension: Bandwidth-latency pairs for each read percentage.
     * - `maxBandwidthPerRdRatio`: Maximum bandwidth for each read percentage.
     * - `maxLatencyPerRdRatio`: Maximum latency for each read percentage.
     */
    vector<vector<vector<double>>> curves_data;
    vector<double> maxBandwidthPerRdRatio;
    vector<uint32_t> maxLatencyPerRdRatio;

    /**
     * @brief Path to the curve data directory.
     */
    string curveAddress;

    /**
     * @brief Counters for tracking memory access behavior.
     * 
     * These are used to calculate bandwidth and update latency at the end of 
     * each measurement window.
     */
    uint32_t currentWindowAccessCount;      ///< Total memory accesses in the current window.
    uint32_t currentWindowAccessCount_wr;   ///< Total write accesses in the current window.
    uint32_t currentWindowAccessCount_rd;   ///< Total read accesses in the current window.
    uint64_t currentWindowStartCycle;       ///< Start cycle of the current measurement window.

    /**
     * @brief Miscellaneous parameters.
     */
    uint32_t lastIntReadPercentage;         ///< Last integer value of read percentage.
    uint32_t curveWindowSize;               ///< Size of the measurement window (smaller is more accurate but slower).

    /**
     * @brief Updates latency at the end of each measurement window.
     * 
     * This method calculates the average bandwidth and adjusts the latency
     * based on the pre-characterized curves.
     * 
     * @param currentWindowEndCycle The cycle at which the current window ends.
     */
    void updateLatency(uint64_t currentWindowEndCycle);

    /**
     * @brief Normalizes curves to ensure lead-off latency consistency.
     * 
     * This is a one-time adjustment applied to the pre-characterized curves
     * to ensure that all read percentages have the same initial latency.
     */
    void fitTheCurves();

    /**
     * @brief Searches for the appropriate latency for a given bandwidth and read percentage.
     * 
     * This method interpolates between points in the curve to determine
     * the latency corresponding to a specific bandwidth and read percentage.
     * 
     * @param bandwidth Bandwidth in accesses per cycle.
     * @param readPercentage Percentage of read operations (0% to 100%).
     * @return Calculated latency in cycles.
     */
    uint32_t searchForLatencyOnCurve(double bandwidth, double readPercentage);

public:
    /**
     * @brief Constructor to initialize the memory controller.
     * 
     * This method loads the bandwidth-latency curves from the specified directory
     * and prepares the memory controller for simulation.
     * 
     * @param curveAddress Path to the directory containing bandwidth-latency curve files.
     * @param curveWindowSize Size of the measurement window (number of accesses).
     * @param frequencyRate Frequency (GHz) of the simulated CPU.
     * @param _onCoreLatency Latency from the core to the memory controller (cycles).
     */
    MessMemCtrl(const std::string& _curveAddress, uint32_t _curveWindowSize, double frequencyRate, double _onCoreLatency);

    /**
     * @brief Simulates a memory access.
     * 
     * This method is called for every memory access in the simulation. It calculates
     * the latency for the access based on the current state of the memory system.
     * 
     * @param accessCycle The cycle at which the memory access occurs.
     * @param isWrite Flag indicating whether the access is a write (true) or read (false).
     * @return The latency of the memory access (in cycles).
     */
    uint64_t access(uint64_t accessCycle, bool isWrite);

    /**
     * @brief Retrieves the lead-off latency.
     * 
     * Lead-off latency is the minimum achievable latency for memory access.
     * 
     * @return Lead-off latency in cycles.
     */
    uint32_t getLeadOffLatency();

    /**
     * @brief Retrieves the memory load penalty for QoS simulations.
     * 
     * This value represents the additional latency caused by exceeding
     * the maximum bandwidth of the memory system.
     * 
     * @return The latency penalty in cycles.
     */
    uint64_t GetQsMemLoadCycleLimit();
};

#endif  // BW_LAT_MEM_CTRL_H_