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

# Specify the target system details
echo "\n"
echo "Skylake with 6x DDR4-2666"
# this is the CPU frequency, we consider for our experiment (In integrated version, this will be the CPU frequency of your CPU simulator)
frequencyCPU=2.1
# This is the CPU frequency used to measure the bandwidth-latency curves. Since latency values are saved in cycles, we need this frequency to convert them into nanoseconds. We also need it to conver the saved cycles into the cycles of our CPU simulator which might work on different frequency.
frequencyCurve=2.1
# The Mess simulator reports latency from the memory controller to main memory. However, the curve data (except for CXL) includes the full latency—from the core to main memory. Therefore, before simulating the main memory, we need to adjust the curve values by subtracting the on-chip latency. 
onChipLatency=51


# We run Mess simulation multiple time, each with different pause value (different issued bandwidth).
# Mess simulation requires five inputs: 
# 1. address of the input curves
# 2. pause value which determing the final bandwdith (the lower pause result in higher bandwidth)
# 3. CPU frequency of our simulation. The higher the CPU frequency, we will issue higher bandwidth with the same pause values. 
# 4. Curve frequency which is the frequency of the CPU, in which we generate the curves. For each system, the CPU frequencies is reported in the Mess paper. 
# 5. On ship latency, which we estimated based on the latency to the LLC.  
./build/mess_example ./data/skylake-ddr4 20000 $frequencyCPU $frequencyCurve $onChipLatency
./build/mess_example ./data/skylake-ddr4 200 $frequencyCPU $frequencyCurve $onChipLatency
./build/mess_example ./data/skylake-ddr4 100 $frequencyCPU $frequencyCurve $onChipLatency
./build/mess_example ./data/skylake-ddr4 50 $frequencyCPU $frequencyCurve $onChipLatency
./build/mess_example ./data/skylake-ddr4 20 $frequencyCPU $frequencyCurve $onChipLatency
./build/mess_example ./data/skylake-ddr4 10 $frequencyCPU $frequencyCurve $onChipLatency
./build/mess_example ./data/skylake-ddr4 9 $frequencyCPU $frequencyCurve $onChipLatency
./build/mess_example ./data/skylake-ddr4 8 $frequencyCPU $frequencyCurve $onChipLatency
./build/mess_example ./data/skylake-ddr4 7 $frequencyCPU $frequencyCurve $onChipLatency
./build/mess_example ./data/skylake-ddr4 6 $frequencyCPU $frequencyCurve $onChipLatency
./build/mess_example ./data/skylake-ddr4 5 $frequencyCPU $frequencyCurve $onChipLatency
./build/mess_example ./data/skylake-ddr4 4 $frequencyCPU $frequencyCurve $onChipLatency
./build/mess_example ./data/skylake-ddr4 3 $frequencyCPU $frequencyCurve $onChipLatency
./build/mess_example ./data/skylake-ddr4 2 $frequencyCPU $frequencyCurve $onChipLatency
./build/mess_example ./data/skylake-ddr4 1 $frequencyCPU $frequencyCurve $onChipLatency
./build/mess_example ./data/skylake-ddr4 0 $frequencyCPU $frequencyCurve $onChipLatency