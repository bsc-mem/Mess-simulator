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


make

echo "\n"

echo "increasing the bandwidth and printing the respective latency:\n"



echo "latency [ns], issue bandwidth [GB/s]\n"

echo "Skylake with DDR5"
frequencyCPU=2.1
frequencyCurve=2.6
onChipLatency=51

./build/mess_example ./curves_src/graviton3-ddr5 20000 $frequencyCPU $frequencyCurve $onChipLatency
./build/mess_example ./curves_src/graviton3-ddr5 200 $frequencyCPU $frequencyCurve $onChipLatency
./build/mess_example ./curves_src/graviton3-ddr5 100 $frequencyCPU $frequencyCurve $onChipLatency
./build/mess_example ./curves_src/graviton3-ddr5 50 $frequencyCPU $frequencyCurve $onChipLatency
./build/mess_example ./curves_src/graviton3-ddr5 20 $frequencyCPU $frequencyCurve $onChipLatency
./build/mess_example ./curves_src/graviton3-ddr5 10 $frequencyCPU $frequencyCurve $onChipLatency
./build/mess_example ./curves_src/graviton3-ddr5 9 $frequencyCPU $frequencyCurve $onChipLatency
./build/mess_example ./curves_src/graviton3-ddr5 8 $frequencyCPU $frequencyCurve $onChipLatency
./build/mess_example ./curves_src/graviton3-ddr5 7 $frequencyCPU $frequencyCurve $onChipLatency
./build/mess_example ./curves_src/graviton3-ddr5 6 $frequencyCPU $frequencyCurve $onChipLatency
./build/mess_example ./curves_src/graviton3-ddr5 5 $frequencyCPU $frequencyCurve $onChipLatency
./build/mess_example ./curves_src/graviton3-ddr5 4 $frequencyCPU $frequencyCurve $onChipLatency
./build/mess_example ./curves_src/graviton3-ddr5 3 $frequencyCPU $frequencyCurve $onChipLatency
./build/mess_example ./curves_src/graviton3-ddr5 2 $frequencyCPU $frequencyCurve $onChipLatency
./build/mess_example ./curves_src/graviton3-ddr5 1 $frequencyCPU $frequencyCurve $onChipLatency
./build/mess_example ./curves_src/graviton3-ddr5 0 $frequencyCPU $frequencyCurve $onChipLatency
