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

# Number of memory channels of the simulated system. Bandwidth values from
# the curve file are linearly scaled by `channels / measuredChannels`
# (where measuredChannels is read from the JSON file).
channels=6

# Run the Mess simulator for varying pause values
# Each pause value determines the bandwidth, with smaller values issuing higher bandwidth
./build/mess_example ./data/bw-lat/a64fx-hbm2.json 20000 $frequencyCPU $channels
./build/mess_example ./data/bw-lat/a64fx-hbm2.json 200 $frequencyCPU $channels
./build/mess_example ./data/bw-lat/a64fx-hbm2.json 100 $frequencyCPU $channels
./build/mess_example ./data/bw-lat/a64fx-hbm2.json 50 $frequencyCPU $channels
./build/mess_example ./data/bw-lat/a64fx-hbm2.json 20 $frequencyCPU $channels
./build/mess_example ./data/bw-lat/a64fx-hbm2.json 10 $frequencyCPU $channels
./build/mess_example ./data/bw-lat/a64fx-hbm2.json 9 $frequencyCPU $channels
./build/mess_example ./data/bw-lat/a64fx-hbm2.json 8 $frequencyCPU $channels
./build/mess_example ./data/bw-lat/a64fx-hbm2.json 7 $frequencyCPU $channels
./build/mess_example ./data/bw-lat/a64fx-hbm2.json 6 $frequencyCPU $channels
./build/mess_example ./data/bw-lat/a64fx-hbm2.json 5 $frequencyCPU $channels
./build/mess_example ./data/bw-lat/a64fx-hbm2.json 4 $frequencyCPU $channels
./build/mess_example ./data/bw-lat/a64fx-hbm2.json 3 $frequencyCPU $channels
./build/mess_example ./data/bw-lat/a64fx-hbm2.json 2 $frequencyCPU $channels
./build/mess_example ./data/bw-lat/a64fx-hbm2.json 1 $frequencyCPU $channels
./build/mess_example ./data/bw-lat/a64fx-hbm2.json 0 $frequencyCPU $channels