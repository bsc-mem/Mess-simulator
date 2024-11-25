clear

make

echo "\n"

echo "increasing the bandwidth and printing the respective latency:\n"





echo "latency [ns], issue bandwidth [GB/s]\n"


echo "\n"
echo "Skylake with 6x DDR4-2666"
frequencyCPU=2.1
frequencyCurve=2.1
onChipLatency=51

./main ./curves_src/skylake-ddr4 20000 $frequencyCPU $frequencyCurve $onChipLatency
./main ./curves_src/skylake-ddr4 200 $frequencyCPU $frequencyCurve $onChipLatency
./main ./curves_src/skylake-ddr4 100 $frequencyCPU $frequencyCurve $onChipLatency
./main ./curves_src/skylake-ddr4 50 $frequencyCPU $frequencyCurve $onChipLatency
./main ./curves_src/skylake-ddr4 20 $frequencyCPU $frequencyCurve $onChipLatency
./main ./curves_src/skylake-ddr4 10 $frequencyCPU $frequencyCurve $onChipLatency
./main ./curves_src/skylake-ddr4 9 $frequencyCPU $frequencyCurve $onChipLatency
./main ./curves_src/skylake-ddr4 8 $frequencyCPU $frequencyCurve $onChipLatency
./main ./curves_src/skylake-ddr4 7 $frequencyCPU $frequencyCurve $onChipLatency
./main ./curves_src/skylake-ddr4 6 $frequencyCPU $frequencyCurve $onChipLatency
./main ./curves_src/skylake-ddr4 5 $frequencyCPU $frequencyCurve $onChipLatency
./main ./curves_src/skylake-ddr4 4 $frequencyCPU $frequencyCurve $onChipLatency
./main ./curves_src/skylake-ddr4 3 $frequencyCPU $frequencyCurve $onChipLatency
./main ./curves_src/skylake-ddr4 2 $frequencyCPU $frequencyCurve $onChipLatency
./main ./curves_src/skylake-ddr4 1 $frequencyCPU $frequencyCurve $onChipLatency
./main ./curves_src/skylake-ddr4 0 $frequencyCPU $frequencyCurve $onChipLatency

