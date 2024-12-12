# Standalone Mess Simulator Scripts

The `scripts/` directory contains pre-defined scripts for building and running the Standalone Mess Simulator with various memory technologies. These scripts allow users to quickly simulate bandwidth-latency behavior without needing to manually configure parameters for each experiment. 

Additionally, users can create their own custom scripts by following the provided template and instructions.

---

## Predefined Scripts

The following scripts are included in the `scripts/` directory:

- **`cxl-exp.sh`**  
  Runs the Standalone Mess Simulator for **CXL (Compute Express Link)** memory expanders.

- **`ddr4-exp.sh`**  
  Simulates the behavior of **DDR4** memory on the Standalone Mess Simulator.

- **`ddr5-exp.sh`**  
  Simulates the behavior of **DDR5** memory on the Standalone Mess Simulator.

- **`hbm-exp.sh`**  
  Runs the simulator for **HBM2 (High Bandwidth Memory)** technology.

- **`run-all.sh`**  
  A convenience script that executes **all predefined scripts** sequentially, allowing for simulations of all the above technologies in one go.

All the predefined scripts run the code inside ``src/example.cpp`` with different parameters.

---

## How to Use the Predefined Scripts

Each script includes predefined parameters for the memory technology it targets. These parameters include CPU frequency, curve frequency, and on-chip latency. Here’s how to run a script using `ddr4-exp.sh` as an example:

### Running a Script

1. Navigate to the `scripts/` directory.
2. Ensure the `Standalone` directory is the parent of your current folder (the script checks this automatically).
3. Run the script:
   ```bash
   bash ddr4-exp.sh
   ```

4. The script will:
   - Compile the Standalone Mess Simulator (if not already compiled).
   - Simulate the memory behavior for various configurations (using multiple pause values).

---

## Script Structure

All scripts follow a similar structure. Below is an explanation of the key sections:

1. **Directory Check**: Ensures the script is being run from the correct location. The script verifies if it is inside the `Standalone` folder or its immediate parent. If not, it exits with an error message:

   ```bash
   current_folder=$(basename "$PWD")
   if [ "$current_folder" != "Standalone" ]; then
       cd ..
       current_folder=$(basename "$PWD")
       if [ "$current_folder" != "Standalone" ]; then
           echo "Please run this script from the 'Standalone' folder."
           exit 1
       fi
   fi
   ```

2. **Compilation**: The script compiles the Standalone Mess Simulator using `make`, ensuring the executable is ready:

   ```bash
   make
   ```

3. **Parameter Setup**: Key variables for the simulation are set here:
   - **`frequencyCPU`**: The frequency of the CPU being simulated (e.g., 2.1 GHz for DDR4).
   - **`onChipLatency`**: Latency from the core to the memory controller (in nanoseconds).

   Example:
   ```bash
   frequencyCPU=2.1
   onChipLatency=51
   ```

4. **Simulation Execution**: Runs the Mess Simulator for multiple pause values to simulate different bandwidths:

   ```bash
   ./build/mess_example ./curves_src/skylake-ddr4 20000 $frequencyCPU $onChipLatency
   ./build/mess_example ./curves_src/skylake-ddr4 200 $frequencyCPU $onChipLatency
   ./build/mess_example ./curves_src/skylake-ddr4 100 $frequencyCPU $onChipLatency
   ```
---

## Creating Your Own Script

To create a custom script for a new memory technology or configuration, follow these steps:

1. **Copy an Existing Script**  
   Use one of the provided scripts as a starting point:
   ```bash
   cp ddr4-exp.sh my-custom-exp.sh
   ```

2. **Update Parameters**  
   Modify the parameters to reflect your desired configuration:
   - **Curve Path**: Change `./curves_src/skylake-ddr4` to the path of your custom curves.
   - **FrequencyCPU**: Set the frequency of your simulated CPU.
   - **OnChipLatency**: Specify the latency from core to memory controller. For more information on how obtain it read the [Standalone Mess Simulator documentation](../README.md).

3. **Add Your Curve Files**  
   Ensure the bandwidth-latency curves for your configuration are added to the `curves_src/` directory.

4. **Run Your Script**  
   Execute your custom script just like the predefined ones:
   ```bash
   bash my-custom-exp.sh
   ```