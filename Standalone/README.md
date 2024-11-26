# Mess-simulator standalone version

This is the standalone version of the Mess simulator. It is developed to provide an easy approach to understand how Mess simulator works. Before integrating the mess simulator into any CPU simulator, it is advised to understand this version well. 


## Structure 

This folder includes multiple files. we explain each in detail. 

```sh

Standalone
 ├── curves-src
 │   ├── skylake-ddr4 
 │   ├── Skylake-numa-ddr4 
 │   ├── graviton3-ddr5 
 │   ├── a64fx-hbm2e 
 │   ├── cxl 
 ├── cxl-exp.sh
 ├── ddr4-exp.sh
 ├── ddr5-exp.sh
 ├── hbm-exp.sh
 ├── runner.sh
 ├── main.cpp
 └── bw_lat_mem_ctrl.*


```

 - curves-src: This folder contains bandwidth-latency curves, which are used as inputs for the Mess simulator. The curves are generated using the Mess benchmark (for actual hardware) or detailed RTL simulations (for CXL).


 - cxl-exp.sh: This script run the standalone version of Mess simulator for CXL technology.  
 - ddr4-exp.sh: This script run the standalone version of Mess simulator for DDR4 technology.  
 - ddr5-exp.sh: This script run the standalone version of Mess simulator for DDR5 technology.  
 - hbm-exp.sh: This script run the standalone version of Mess simulator for HBM2 technology.  
 - runner.sh: This script run the standalone version of Mess simulator for all the technologies (all the above scipts are a subset of this scipt).  


 - main.cpp: This is small example how to instantiate Mess simulator and use it.  
 - bw_lat_mem_ctrl.*: These files are the implementation of the Mess simulator. 



## How to Run the Script 

In this section, with `ddr4-exp.sh` script as an example, we will explain how the scipts work:

```sh
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


# We run Mess simulation multiple time, each with different pause value (different issued bandwidth).
# Mess simulation requires five inputs: 
# 1. address of the input curves
# 2. pause value which determing the final bandwdith (the lower pause result in higher bandwidth)
# 3. CPU frequency of our simulation. The higher the CPU frequency, we will issue higher bandwidth with the same pause values. 
# 4. Curve frequency which is the frequency of the CPU, in which we generate the curves. For each system, the CPU frequencies is reported in the Mess paper. 
# 5. On ship latency, which we estimated based on the latency to the LLC.  
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
``` 



## example outputs

Below we show the example output of running the script `ddr4-exp.sh` as (latency in ns and BW in GB/s):
 

```sh

70.00, 0.07 GB/s
71.43, 6.72 GB/s
73.81, 13.44 GB/s
77.62, 26.88 GB/s
90.00, 67.20 GB/s
50835.71, 134.40 GB/s
50873.81, 149.33 GB/s
50899.53, 168.00 GB/s
50916.19, 192.00 GB/s
50933.34, 224.00 GB/s
50945.71, 268.80 GB/s
50954.29, 336.00 GB/s
50967.14, 448.00 GB/s
50975.71, 672.00 GB/s
50984.29, 1344.00 GB/s
```

As observed, latency increases when the bandwidth exceeds the maximum bandwidth of the input curves (116 GB/s in this experiment). Since this simulation is trace-based and lacks a feedback loop, it cannot prevent the increase in issued bandwidth. If a CPU simulator were used, Long latencies would introduce significant stalls, resulting in a decrease in bandwidth.

## Suggestions

We can change the Pause values, input curve values and look at the final reported latency and BW to understand better  how Mess simulator works. In all these experiment, we only test 100% read workload. If one wants in the main file it can change the issue accesses to also inlcudes writes. If you decide to integrate the Mess simulator into your CPU simulator, please do not hessitate to send me an email. I will be more than glad to help. 

