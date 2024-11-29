#!/bin/bash

# Clear the terminal for better readability
clear

# Get the name of the current folder
current_folder=$(basename "$PWD")

# Check if the script is being executed from the 'Standalone' folder
if [ "$current_folder" != "Standalone" ]; then
    # Move one directory up
    cd ..
    current_folder=$(basename "$PWD")
    # Check again if the folder is now 'Standalone'
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

# Specify the target system details
echo "\n"
echo "Skylake with HBM2"
# this is the CPU frequency, we consider for our experiment (In integrated version, this will be the CPU frequency of your CPU simulator)
frequencyCPU=2.1
# This is the CPU frequency used to measure the bandwidth-latency curves. Since latency values are saved in cycles, we need this frequency to convert them into nanoseconds. We also need it to conver the saved cycles into the cycles of our CPU simulator which might work on different frequency.
frequencyCurve=2.2
# The Mess simulator reports latency from the memory controller to main memory. However, the curve data (except for CXL) includes the full latency—from the core to main memory. Therefore, before simulating the main memory, we need to adjust the curve values by subtracting the on-chip latency. 
onChipLatency=51

# Run the Mess simulator for varying pause values
# Each pause value determines the bandwidth, with smaller values issuing higher bandwidth
./build/mess_example ./data/a64fx-hbm2e 20000 $frequencyCPU $frequencyCurve $onChipLatency
./build/mess_example ./data/a64fx-hbm2e 200 $frequencyCPU $frequencyCurve $onChipLatency
./build/mess_example ./data/a64fx-hbm2e 100 $frequencyCPU $frequencyCurve $onChipLatency
./build/mess_example ./data/a64fx-hbm2e 50 $frequencyCPU $frequencyCurve $onChipLatency
./build/mess_example ./data/a64fx-hbm2e 20 $frequencyCPU $frequencyCurve $onChipLatency
./build/mess_example ./data/a64fx-hbm2e 10 $frequencyCPU $frequencyCurve $onChipLatency
./build/mess_example ./data/a64fx-hbm2e 9 $frequencyCPU $frequencyCurve $onChipLatency
./build/mess_example ./data/a64fx-hbm2e 8 $frequencyCPU $frequencyCurve $onChipLatency
./build/mess_example ./data/a64fx-hbm2e 7 $frequencyCPU $frequencyCurve $onChipLatency
./build/mess_example ./data/a64fx-hbm2e 6 $frequencyCPU $frequencyCurve $onChipLatency
./build/mess_example ./data/a64fx-hbm2e 5 $frequencyCPU $frequencyCurve $onChipLatency
./build/mess_example ./data/a64fx-hbm2e 4 $frequencyCPU $frequencyCurve $onChipLatency
./build/mess_example ./data/a64fx-hbm2e 3 $frequencyCPU $frequencyCurve $onChipLatency
./build/mess_example ./data/a64fx-hbm2e 2 $frequencyCPU $frequencyCurve $onChipLatency
./build/mess_example ./data/a64fx-hbm2e 1 $frequencyCPU $frequencyCurve $onChipLatency
./build/mess_example ./data/a64fx-hbm2e 0 $frequencyCPU $frequencyCurve $onChipLatency