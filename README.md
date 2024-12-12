# Mess-simulator

The Mess Simulator is an abstract memory model that utilizes bandwidth-latency curves to simulate memory system performance. By matching its behavior to input bandwidth-latency curves, it provides a highly customizable and accurate approach for simulating diverse memory systems. The input curves can either be:

- **Experimentally derived:** Obtained by running the [Mess Benchmark](https://github.com/bsc-mem/Mess-benchmark) on actual hardware.
- **Manufacturer-supplied:** Provided by memory manufacturers based on detailed hardware models.

The Mess Simulator employs a proportional–integral (PI) controller mechanism, derived from control theory, to dynamically align its output with the input bandwidth-latency curves. This approach ensures realistic simulation of memory systems, accommodating varying workloads and system configurations.

---

If you find Mess simulator useful, please cite the following paper accepted in MICRO 2024, best paper runner-up award.

The Mess Simulator is organized into two primary components:

**1. Mess Standalone Mode**

The Standalone version of Mess Simulator operates independently and is ideal for:
- **Benchmarking:** Using predefined bandwidth-latency curves to test memory system performance.
- **Research and Development:** Experimenting with memory behavior in isolation from other system components.
- **Ease of Integration:** Serving as a reference for integrating the simulator with other CPU simulators.

The Mess Standalone Mode is implemented in C++ and provides a simple interface for running memory simulations without additional dependencies.

**2. Mess Integrated Mode**

The Integrated version of Mess Simulator is designed for seamless incorporation into popular CPU simulators. This mode currently supports:
- **ZSim**
- **gem5**
- **OpenPiton Metro-MPI**

By integrating with these simulators, Mess Integrated Simulator enables system-level simulations, capturing interactions between memory and compute workloads more comprehensively.

---

## Repository Structure

The repository is organized as follows:

```txt
/Mess-simulator
│
├── figures/             # Figures used in the README 
│
├── Standalone/
│   ├── README.md        # Documentation for Standalone Mode
│   ├── src/             # Source code for the Standalone Mess Simulator
│   ├── scripts/         # Scripts to run pre-defined examples
│   └── data/            # Set of pre-measured bandwidth-latency curves
│
├── Integrated/
│   ├── ZSim/            # Integration with ZSim
│   ├── gem5/            # Integration with gem5
│   ├── OpenPiton/       # Integration with OpenPiton Metro-MPI
│   ├── README.md        # Documentation for Integrated Mode
│
├── LICENSE.txt          # License information
└── README.md            # This file
```

#### Standalone Mode

The Standalone folder contains all code and documentation necessary for running Mess as an independent simulator. This mode is particularly suited for quick benchmarking or detailed analysis of memory system behavior.

#### Integrated Mode

The Integrated folder includes subdirectories for each supported simulator. Each subdirectory provides configuration files and integration instructions tailored for its respective simulator.

## Citation

If you find this simulator useful, please cite the following paper presented at MICRO 2024, which was awarded Best Paper Runner-Up:

```tex
@INPROCEEDINGS{10764561,
  author={Esmaili-Dokht, Pouya and Sgherzi, Francesco and Girelli, Valéria Soldera and Boixaderas, Isaac and Carmin, Mariana and Monemi, Alireza and Armejach, Adrià and Mercadal, Estanislao and Llort, Germán and Radojković, Petar and Moreto, Miquel and Giménez, Judit and Martorell, Xavier and Ayguadé, Eduard and Labarta, Jesus and Confalonieri, Emanuele and Dubey, Rishabh and Adlard, Jason},
  booktitle={57th IEEE/ACM International Symposium on Microarchitecture (MICRO)}, 
  title={{A Mess of Memory System Benchmarking, Simulation and Application Profiling}}, 
  year={2024}}
```

## Getting Started

**Prerequisites**
- C++11 or higher.
- For integrated mode: Compatible versions of ZSim, gem5, or OpenPiton.

**Quick Start**

1. Clone the repository:

```sh
git clone https://github.com/your-repository/Mess-simulator.git
```

2. Navigate to the desired mode:

```sh
cd Mess-simulator/Standalone
```

3. Follow the specific README instructions in each mode folder to build and run simulations.

