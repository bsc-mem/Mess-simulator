#!/bin/bash

# Clear the terminal for better readability
clear

# Get the name of the current folder
current_folder=$(basename "$PWD")

# Check if the current folder is named "Standalone"
if [ "$current_folder" != "Standalone" ]; then
    # Go one directory up
    cd ..
    current_folder=$(basename "$PWD")
    # Check again if it is "Standalone"
    if [ "$current_folder" != "Standalone" ]; then
        echo "Please run this script from the 'Standalone' folder."
        exit 1
    fi
fi

# Compile the standalone Mess simulator
make

# Output information about the experiment
echo "\n"
echo "Increasing the bandwidth and printing the respective latency:\n"
echo "latency [ns], issue bandwidth [GB/s]\n"


echo "Skylake with DDR5"
# this is the CPU frequency, we consider for our experiment (In integrated version, this will be the CPU frequency of your CPU simulator)
frequencyCPU=2.1
# This is the CPU frequency used to measure the bandwidth-latency curves. Since latency values are saved in cycles, we need this frequency to convert them into nanoseconds. We also need it to conver the saved cycles into the cycles of our CPU simulator which might work on different frequency.
frequencyCurve=2.6
# The Mess simulator reports latency from the memory controller to main memory. However, the curve data (except for CXL) includes the full latency—from the core to main memory. Therefore, before simulating the main memory, we need to adjust the curve values by subtracting the on-chip latency. 
onChipLatency=51

# Run the Mess simulator for varying pause values
# Each pause value determines the bandwidth, with smaller values issuing higher bandwidth
./build/mess_example ./data/graviton3-ddr5 20000 $frequencyCPU $frequencyCurve $onChipLatency
./build/mess_example ./data/graviton3-ddr5 200 $frequencyCPU $frequencyCurve $onChipLatency
./build/mess_example ./data/graviton3-ddr5 100 $frequencyCPU $frequencyCurve $onChipLatency
./build/mess_example ./data/graviton3-ddr5 50 $frequencyCPU $frequencyCurve $onChipLatency
./build/mess_example ./data/graviton3-ddr5 20 $frequencyCPU $frequencyCurve $onChipLatency
./build/mess_example ./data/graviton3-ddr5 10 $frequencyCPU $frequencyCurve $onChipLatency
./build/mess_example ./data/graviton3-ddr5 9 $frequencyCPU $frequencyCurve $onChipLatency
./build/mess_example ./data/graviton3-ddr5 8 $frequencyCPU $frequencyCurve $onChipLatency
./build/mess_example ./data/graviton3-ddr5 7 $frequencyCPU $frequencyCurve $onChipLatency
./build/mess_example ./data/graviton3-ddr5 6 $frequencyCPU $frequencyCurve $onChipLatency
./build/mess_example ./data/graviton3-ddr5 5 $frequencyCPU $frequencyCurve $onChipLatency
./build/mess_example ./data/graviton3-ddr5 4 $frequencyCPU $frequencyCurve $onChipLatency
./build/mess_example ./data/graviton3-ddr5 3 $frequencyCPU $frequencyCurve $onChipLatency
./build/mess_example ./data/graviton3-ddr5 2 $frequencyCPU $frequencyCurve $onChipLatency
./build/mess_example ./data/graviton3-ddr5 1 $frequencyCPU $frequencyCurve $onChipLatency
./build/mess_example ./data/graviton3-ddr5 0 $frequencyCPU $frequencyCurve $onChipLatency
