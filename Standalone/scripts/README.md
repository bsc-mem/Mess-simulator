## Scripts

Explain that you have set predefined examples of different technologies so that people don't have to do them.

But if anyone wants to do his/her own, explain what to do.


The idea is to explain what you had in the main branch: 


cxl-exp.sh: This script run the standalone version of Mess simulator for CXL technology.

ddr4-exp.sh: This script run the standalone version of Mess simulator for DDR4 technology.

ddr5-exp.sh: This script run the standalone version of Mess simulator for DDR5 technology.

hbm-exp.sh: This script run the standalone version of Mess simulator for HBM2 technology.

runner.sh: This script run the standalone version of Mess simulator for all the technologies (all the above scipts are a subset of this scipt).


+

How to Run the Script
In this section, with ddr4-exp.sh script as an example, we will explain how the scipts work:

# compile the standalone Mess simulator
make

echo "latency [ns], issue bandwidth [GB/s]\n"

echo "\n"
echo "Skylake with 6x DDR4-2666"

# this is the CPU frequency, we consider for our experiment (In integrated version, this will be the CPU frequency of your CPU simulator)
frequencyCPU=2.1

# This is the CPU frequency used to measure the bandwidth-latency curves. Since latency values are saved in cycles, we need this frequency to convert them into nanoseconds. We also need it to conver the saved cycles into the cycles of our CPU simulator which might work on different frequency.
frequencyCurve=2.1

# The Mess simulator reports latency from the memory controller to main memory. However, the curve data (except for CXL) includes the full latency—from the core to main memory. Therefore, before simulating the main memory, we need to adjust the curve values by subtracting the on-chip latency. 
onChipLatency=51
