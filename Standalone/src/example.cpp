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
#include <iostream>
#include <iomanip>
#include <memory>
#include <stdexcept>
#include <string>

#include "mess_mem_ctrl.h"

// Simulation constants
constexpr uint64_t DEFAULT_LOOP_SIZE = 1200000;  // Number of iterations for the main simulation loop
constexpr uint64_t DEFAULT_INNER_LOOP_SIZE = 10; // Number of accesses per outer loop iteration

/**
 * @brief Simulates the passage of clock cycles.
 * 
 * @param currentCycle The current clock cycle count.
 * @param pause The number of cycles to pause before the next operation.
 * @return Updated clock cycle count.
 */
uint64_t simulateCycle(uint64_t currentCycle, uint64_t pause) {
    return currentCycle + pause;
}

/**
 * @brief Prints usage instructions for the program.
 */
void printUsage() {
    std::cerr << "Usage: ./example <curve_path> <pause_value> <cpu_frequency> <on_chip_latency>" << std::endl;
}

/**
 * @brief Entry point for the example program demonstrating the Mess simulator.
 * 
 * @param argc Number of command-line arguments.
 * @param argv Array of command-line arguments.
 * @return 0 on success, 1 on failure.
 * 
 * This program simulates memory performance by leveraging the Mess simulator's `BwLatMemCtrl` class.
 * It calculates memory latency and bandwidth for a series of read operations using pre-generated
 * bandwidth-latency curves provided as input.
 */
int main(int argc, char* argv[]) {
    // Validate argument count
    if (argc != 5) {
        printUsage();
        return 1;
    }

    try {
        // Parse command-line arguments
        std::string curvePath = argv[1];               // Path to bandwidth-latency curve file
        int pauseValue = std::stoi(argv[2]);           // Pause between operations (controls bandwidth)
        float cpuFrequency = std::stof(argv[3]);       // CPU frequency for the simulation
        float onChipLatency = std::stof(argv[4]);      // On-chip latency to adjust the curve latency values

        // Validate pause value
        if (pauseValue < 0) {
            throw std::invalid_argument("Pause value must be non-negative.");
        }

        // Create an instance of the memory controller
        // MessMemCtrl handles memory latency and bandwidth computations based on input curves
        std::unique_ptr<MessMemCtrl> memoryController(new MessMemCtrl(curvePath, 1000, cpuFrequency, onChipLatency));

        uint32_t latency = 0;    // Variable to store the latency of memory operations
        uint64_t cycle = 0;      // Simulated clock cycle count

        // Simulate read operations
        // The outer loop runs `DEFAULT_LOOP_SIZE` iterations.
        // Each outer loop iteration runs `DEFAULT_INNER_LOOP_SIZE` read operations.
        for (uint64_t i = 0; i < DEFAULT_LOOP_SIZE; ++i) {
            for (uint64_t j = 0; j < DEFAULT_INNER_LOOP_SIZE; ++j) {
                latency = memoryController->access(cycle, false); // Perform a read operation
            }
            cycle = simulateCycle(cycle, pauseValue); // Increment the simulated clock cycle
        }

        // Output the results
        if (pauseValue != 0) {
            std::cout << std::fixed << std::setprecision(2);
            std::cout << latency / cpuFrequency << " ns, " 
                      << (DEFAULT_INNER_LOOP_SIZE * cpuFrequency * 64 / pauseValue) << " GB/s" << std::endl;
        }
    } catch (const std::exception& ex) {
        // Catch and display any runtime errors
        std::cerr << "Error: " << ex.what() << std::endl;
        printUsage();
        return 1;
    }

    return 0;
}