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


echo "\n"
echo "Skylake with DDR5"
frequencyCPU=2.1
frequencyCurve=2.6
onChipLatency=51

./main ./curves_src/graviton3-ddr5 20000 $frequencyCPU $frequencyCurve $onChipLatency
./main ./curves_src/graviton3-ddr5 200 $frequencyCPU $frequencyCurve $onChipLatency
./main ./curves_src/graviton3-ddr5 100 $frequencyCPU $frequencyCurve $onChipLatency
./main ./curves_src/graviton3-ddr5 50 $frequencyCPU $frequencyCurve $onChipLatency
./main ./curves_src/graviton3-ddr5 20 $frequencyCPU $frequencyCurve $onChipLatency
./main ./curves_src/graviton3-ddr5 10 $frequencyCPU $frequencyCurve $onChipLatency
./main ./curves_src/graviton3-ddr5 9 $frequencyCPU $frequencyCurve $onChipLatency
./main ./curves_src/graviton3-ddr5 8 $frequencyCPU $frequencyCurve $onChipLatency
./main ./curves_src/graviton3-ddr5 7 $frequencyCPU $frequencyCurve $onChipLatency
./main ./curves_src/graviton3-ddr5 6 $frequencyCPU $frequencyCurve $onChipLatency
./main ./curves_src/graviton3-ddr5 5 $frequencyCPU $frequencyCurve $onChipLatency
./main ./curves_src/graviton3-ddr5 4 $frequencyCPU $frequencyCurve $onChipLatency
./main ./curves_src/graviton3-ddr5 3 $frequencyCPU $frequencyCurve $onChipLatency
./main ./curves_src/graviton3-ddr5 2 $frequencyCPU $frequencyCurve $onChipLatency
./main ./curves_src/graviton3-ddr5 1 $frequencyCPU $frequencyCurve $onChipLatency
./main ./curves_src/graviton3-ddr5 0 $frequencyCPU $frequencyCurve $onChipLatency

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