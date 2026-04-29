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

# Number of memory channels of the simulated system. Bandwidth values from
# the curve file are linearly scaled by `channels / measuredChannels`
# (where measuredChannels is read from the JSON file).
channels=6

# Run the Mess simulator for varying pause values
# Each pause value determines the bandwidth, with smaller values issuing higher bandwidth
./build/mess_example ./data/bw-lat/cxl.json 20000 $frequencyCPU $channels
./build/mess_example ./data/bw-lat/cxl.json 200 $frequencyCPU $channels
./build/mess_example ./data/bw-lat/cxl.json 100 $frequencyCPU $channels
./build/mess_example ./data/bw-lat/cxl.json 50 $frequencyCPU $channels
./build/mess_example ./data/bw-lat/cxl.json 20 $frequencyCPU $channels
./build/mess_example ./data/bw-lat/cxl.json 10 $frequencyCPU $channels
./build/mess_example ./data/bw-lat/cxl.json 9 $frequencyCPU $channels
./build/mess_example ./data/bw-lat/cxl.json 8 $frequencyCPU $channels
./build/mess_example ./data/bw-lat/cxl.json 7 $frequencyCPU $channels
./build/mess_example ./data/bw-lat/cxl.json 6 $frequencyCPU $channels
./build/mess_example ./data/bw-lat/cxl.json 5 $frequencyCPU $channels
./build/mess_example ./data/bw-lat/cxl.json 4 $frequencyCPU $channels
./build/mess_example ./data/bw-lat/cxl.json 3 $frequencyCPU $channels
./build/mess_example ./data/bw-lat/cxl.json 2 $frequencyCPU $channels
./build/mess_example ./data/bw-lat/cxl.json 1 $frequencyCPU $channels
./build/mess_example ./data/bw-lat/cxl.json 0 $frequencyCPU $channels