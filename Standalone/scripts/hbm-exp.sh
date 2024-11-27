clear
make

echo "\n"
echo "increasing the bandwidth and printing the respective latency:\n"

echo "latency [ns], issue bandwidth [GB/s]\n"

echo "\n"
echo "Skylake with hbm2"
frequencyCPU=2.1
frequencyCurve=2.2
onChipLatency=51

./main ./curves_src/a64fx-hbm2e 20000 $frequencyCPU $frequencyCurve $onChipLatency
./main ./curves_src/a64fx-hbm2e 200 $frequencyCPU $frequencyCurve $onChipLatency
./main ./curves_src/a64fx-hbm2e 100 $frequencyCPU $frequencyCurve $onChipLatency
./main ./curves_src/a64fx-hbm2e 50 $frequencyCPU $frequencyCurve $onChipLatency
./main ./curves_src/a64fx-hbm2e 20 $frequencyCPU $frequencyCurve $onChipLatency
./main ./curves_src/a64fx-hbm2e 10 $frequencyCPU $frequencyCurve $onChipLatency
./main ./curves_src/a64fx-hbm2e 9 $frequencyCPU $frequencyCurve $onChipLatency
./main ./curves_src/a64fx-hbm2e 8 $frequencyCPU $frequencyCurve $onChipLatency
./main ./curves_src/a64fx-hbm2e 7 $frequencyCPU $frequencyCurve $onChipLatency
./main ./curves_src/a64fx-hbm2e 6 $frequencyCPU $frequencyCurve $onChipLatency
./main ./curves_src/a64fx-hbm2e 5 $frequencyCPU $frequencyCurve $onChipLatency
./main ./curves_src/a64fx-hbm2e 4 $frequencyCPU $frequencyCurve $onChipLatency
./main ./curves_src/a64fx-hbm2e 3 $frequencyCPU $frequencyCurve $onChipLatency
./main ./curves_src/a64fx-hbm2e 2 $frequencyCPU $frequencyCurve $onChipLatency
./main ./curves_src/a64fx-hbm2e 1 $frequencyCPU $frequencyCurve $onChipLatency
./main ./curves_src/a64fx-hbm2e 0 $frequencyCPU $frequencyCurve $onChipLatency