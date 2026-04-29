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


# We run Mess simulation multiple time, each with different pause value (different issued bandwidth).
# Mess simulation requires three inputs: 
# 1. address of the input curves
# 2. pause value which determing the final bandwdith (the lower pause result in higher bandwidth)
# 3. CPU frequency of our simulation. The higher the CPU frequency, we will issue higher bandwidth with the same pause values.
# 4. Number of memory channels of the simulated system. Bandwidth values from
#    the curve file are linearly scaled by `channels / measuredChannels`
#    (where measuredChannels is read from the JSON file).
channels=6
./build/mess_example ./data/bw-lat/skylake-ddr4.json 20000 $frequencyCPU $channels 
./build/mess_example ./data/bw-lat/skylake-ddr4.json 200 $frequencyCPU $channels 
./build/mess_example ./data/bw-lat/skylake-ddr4.json 100 $frequencyCPU $channels 
./build/mess_example ./data/bw-lat/skylake-ddr4.json 50 $frequencyCPU $channels 
./build/mess_example ./data/bw-lat/skylake-ddr4.json 20 $frequencyCPU $channels 
./build/mess_example ./data/bw-lat/skylake-ddr4.json 10 $frequencyCPU $channels 
./build/mess_example ./data/bw-lat/skylake-ddr4.json 9 $frequencyCPU $channels 
./build/mess_example ./data/bw-lat/skylake-ddr4.json 8 $frequencyCPU $channels 
./build/mess_example ./data/bw-lat/skylake-ddr4.json 7 $frequencyCPU $channels 
./build/mess_example ./data/bw-lat/skylake-ddr4.json 6 $frequencyCPU $channels 
./build/mess_example ./data/bw-lat/skylake-ddr4.json 5 $frequencyCPU $channels 
./build/mess_example ./data/bw-lat/skylake-ddr4.json 4 $frequencyCPU $channels 
./build/mess_example ./data/bw-lat/skylake-ddr4.json 3 $frequencyCPU $channels 
./build/mess_example ./data/bw-lat/skylake-ddr4.json 2 $frequencyCPU $channels 
./build/mess_example ./data/bw-lat/skylake-ddr4.json 1 $frequencyCPU $channels 
./build/mess_example ./data/bw-lat/skylake-ddr4.json 0 $frequencyCPU $channels 