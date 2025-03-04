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

# Run the Mess simulator for varying pause values
# Each pause value determines the bandwidth, with smaller values issuing higher bandwidth
./build/mess_example ./data/cxl 20000 $frequencyCPU  
./build/mess_example ./data/cxl 200 $frequencyCPU  
./build/mess_example ./data/cxl 100 $frequencyCPU  
./build/mess_example ./data/cxl 50 $frequencyCPU  
./build/mess_example ./data/cxl 20 $frequencyCPU  
./build/mess_example ./data/cxl 10 $frequencyCPU  
./build/mess_example ./data/cxl 9 $frequencyCPU  
./build/mess_example ./data/cxl 8 $frequencyCPU  
./build/mess_example ./data/cxl 7 $frequencyCPU  
./build/mess_example ./data/cxl 6 $frequencyCPU  
./build/mess_example ./data/cxl 5 $frequencyCPU  
./build/mess_example ./data/cxl 4 $frequencyCPU  
./build/mess_example ./data/cxl 3 $frequencyCPU  
./build/mess_example ./data/cxl 2 $frequencyCPU  
./build/mess_example ./data/cxl 1 $frequencyCPU  
./build/mess_example ./data/cxl 0 $frequencyCPU  