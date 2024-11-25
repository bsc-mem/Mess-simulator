clear

make

echo "\n"

echo "increasing the bandwidth and printing the respective latency:\n"





echo "latency [ns], issue bandwidth [GB/s]\n"

echo "Skylake with cxl"
frequencyCPU=2.1
frequencyCurve=2.1
onChipLatency=0

./main ./curves_src/cxl 20000 $frequencyCPU $frequencyCurve $onChipLatency
./main ./curves_src/cxl 200 $frequencyCPU $frequencyCurve $onChipLatency
./main ./curves_src/cxl 100 $frequencyCPU $frequencyCurve $onChipLatency
./main ./curves_src/cxl 50 $frequencyCPU $frequencyCurve $onChipLatency
./main ./curves_src/cxl 20 $frequencyCPU $frequencyCurve $onChipLatency
./main ./curves_src/cxl 10 $frequencyCPU $frequencyCurve $onChipLatency
./main ./curves_src/cxl 9 $frequencyCPU $frequencyCurve $onChipLatency
./main ./curves_src/cxl 8 $frequencyCPU $frequencyCurve $onChipLatency
./main ./curves_src/cxl 7 $frequencyCPU $frequencyCurve $onChipLatency
./main ./curves_src/cxl 6 $frequencyCPU $frequencyCurve $onChipLatency
./main ./curves_src/cxl 5 $frequencyCPU $frequencyCurve $onChipLatency
./main ./curves_src/cxl 4 $frequencyCPU $frequencyCurve $onChipLatency
./main ./curves_src/cxl 3 $frequencyCPU $frequencyCurve $onChipLatency
./main ./curves_src/cxl 2 $frequencyCPU $frequencyCurve $onChipLatency
./main ./curves_src/cxl 1 $frequencyCPU $frequencyCurve $onChipLatency
./main ./curves_src/cxl 0 $frequencyCPU $frequencyCurve $onChipLatency
