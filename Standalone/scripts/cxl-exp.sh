#!/bin/bash

# Clear the terminal for better readability
clear

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
echo "increasing the bandwidth and printing the respective latency:\n"
echo "latency [ns], issue bandwidth [GB/s]\n"

echo "\n"
echo "Skylake with cxl"
frequencyCPU=2.1            # Frequency of the simulated CPU in GHz
onChipLatency=0             # Latency (in nanoseconds) from the CPU core to the memory controller

# Run the Mess simulator for varying pause values
# Each pause value determines the bandwidth, with smaller values issuing higher bandwidth
./build/mess_example ./data/cxl 20000 $frequencyCPU  $onChipLatency
./build/mess_example ./data/cxl 200 $frequencyCPU  $onChipLatency
./build/mess_example ./data/cxl 100 $frequencyCPU  $onChipLatency
./build/mess_example ./data/cxl 50 $frequencyCPU  $onChipLatency
./build/mess_example ./data/cxl 20 $frequencyCPU  $onChipLatency
./build/mess_example ./data/cxl 10 $frequencyCPU  $onChipLatency
./build/mess_example ./data/cxl 9 $frequencyCPU  $onChipLatency
./build/mess_example ./data/cxl 8 $frequencyCPU  $onChipLatency
./build/mess_example ./data/cxl 7 $frequencyCPU  $onChipLatency
./build/mess_example ./data/cxl 6 $frequencyCPU  $onChipLatency
./build/mess_example ./data/cxl 5 $frequencyCPU  $onChipLatency
./build/mess_example ./data/cxl 4 $frequencyCPU  $onChipLatency
./build/mess_example ./data/cxl 3 $frequencyCPU  $onChipLatency
./build/mess_example ./data/cxl 2 $frequencyCPU  $onChipLatency
./build/mess_example ./data/cxl 1 $frequencyCPU  $onChipLatency
./build/mess_example ./data/cxl 0 $frequencyCPU  $onChipLatency