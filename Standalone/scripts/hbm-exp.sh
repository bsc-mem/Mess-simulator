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

echo "\n"
echo "Skylake with hbm2"
frequencyCPU=2.1
frequencyCurve=2.2
onChipLatency=51

./build/mess_example ./curves_src/a64fx-hbm2e 20000 $frequencyCPU $frequencyCurve $onChipLatency
./build/mess_example ./curves_src/a64fx-hbm2e 200 $frequencyCPU $frequencyCurve $onChipLatency
./build/mess_example ./curves_src/a64fx-hbm2e 100 $frequencyCPU $frequencyCurve $onChipLatency
./build/mess_example ./curves_src/a64fx-hbm2e 50 $frequencyCPU $frequencyCurve $onChipLatency
./build/mess_example ./curves_src/a64fx-hbm2e 20 $frequencyCPU $frequencyCurve $onChipLatency
./build/mess_example ./curves_src/a64fx-hbm2e 10 $frequencyCPU $frequencyCurve $onChipLatency
./build/mess_example ./curves_src/a64fx-hbm2e 9 $frequencyCPU $frequencyCurve $onChipLatency
./build/mess_example ./curves_src/a64fx-hbm2e 8 $frequencyCPU $frequencyCurve $onChipLatency
./build/mess_example ./curves_src/a64fx-hbm2e 7 $frequencyCPU $frequencyCurve $onChipLatency
./build/mess_example ./curves_src/a64fx-hbm2e 6 $frequencyCPU $frequencyCurve $onChipLatency
./build/mess_example ./curves_src/a64fx-hbm2e 5 $frequencyCPU $frequencyCurve $onChipLatency
./build/mess_example ./curves_src/a64fx-hbm2e 4 $frequencyCPU $frequencyCurve $onChipLatency
./build/mess_example ./curves_src/a64fx-hbm2e 3 $frequencyCPU $frequencyCurve $onChipLatency
./build/mess_example ./curves_src/a64fx-hbm2e 2 $frequencyCPU $frequencyCurve $onChipLatency
./build/mess_example ./curves_src/a64fx-hbm2e 1 $frequencyCPU $frequencyCurve $onChipLatency
./build/mess_example ./curves_src/a64fx-hbm2e 0 $frequencyCPU $frequencyCurve $onChipLatency