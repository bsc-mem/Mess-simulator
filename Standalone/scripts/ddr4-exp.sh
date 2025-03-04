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
./build/mess_example ./data/skylake-ddr4 20000 $frequencyCPU  
./build/mess_example ./data/skylake-ddr4 200 $frequencyCPU  
./build/mess_example ./data/skylake-ddr4 100 $frequencyCPU  
./build/mess_example ./data/skylake-ddr4 50 $frequencyCPU  
./build/mess_example ./data/skylake-ddr4 20 $frequencyCPU  
./build/mess_example ./data/skylake-ddr4 10 $frequencyCPU  
./build/mess_example ./data/skylake-ddr4 9 $frequencyCPU  
./build/mess_example ./data/skylake-ddr4 8 $frequencyCPU  
./build/mess_example ./data/skylake-ddr4 7 $frequencyCPU  
./build/mess_example ./data/skylake-ddr4 6 $frequencyCPU  
./build/mess_example ./data/skylake-ddr4 5 $frequencyCPU  
./build/mess_example ./data/skylake-ddr4 4 $frequencyCPU  
./build/mess_example ./data/skylake-ddr4 3 $frequencyCPU  
./build/mess_example ./data/skylake-ddr4 2 $frequencyCPU  
./build/mess_example ./data/skylake-ddr4 1 $frequencyCPU  
./build/mess_example ./data/skylake-ddr4 0 $frequencyCPU  