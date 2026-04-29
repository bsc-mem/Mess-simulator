# Mess Simulator

## Contents
  [Overview](#overview) \
  [Repository Structure](#repository-structure) \
  [Cloning Options](#cloning-options) \
  [Citation](#citation) \
  [Getting Started](#getting-started)

---

## Overview

The Mess Simulator is an analytical memory model that utilizes bandwidth-latency curves to simulate memory system performance. By matching its behavior to input bandwidth--latency curves, it provides a highly customizable and accurate approach for simulating diverse memory systems. The input curves can either be:

- **Experimentally derived:** Obtained by running the [Mess Benchmark](https://github.com/bsc-mem/Mess-benchmark) on actual hardware.
- **Manufacturer-supplied:** Provided by memory manufacturers based on detailed RTL simulations.

The Mess Simulator employs a proportional–integral (PI) controller mechanism, derived from classical control theory, to dynamically align its output with the input bandwidth--latency curves. This approach ensures realistic simulation of memory systems for varying workloads and system configurations.

---

The Mess Simulator is organized into two primary components:


**1. Mess Integrated Mode**

The Integrated version of Mess Simulator is designed for seamless incorporation into popular CPU simulators. This mode currently supports:
- **ZSim**
- **gem5**
- **OpenPiton Metro-MPI**

By integrating with these simulators, Mess Integrated Simulator enables system-level simulations, capturing interactions between memory and compute workloads more comprehensively.

**2. Mess Standalone Mode**

The Standalone version of Mess Simulator operates independently and is ideal for:

- **Understanding the Mess Simulator:** The Mess simulator serves as a simple example for learning how it operates. Its user-friendly interface simplifies future integrations with other CPU simulators.

- **Regular Updates:** The standalone version will continually receive the latest features. Utilizing and integrating this version ensures that users can effortlessly keep their models up to date.

The Mess Standalone Mode is implemented in C++ and provides a simple interface for running memory simulations without additional dependencies.

*Note: The Mess simulator standalone version is not designed to function as a trace-driven simulator and should not be used as such. It is intended solely for learning and integration purposes.


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

#### Integrated Mode

The Integrated folder includes subdirectories for each supported simulator. Each subdirectory provides configuration files and integration instructions tailored for its respective simulator. 

*Note: This mode includes the version of Mess released during the Mess paper publication, enabling replication of the paper's results. Newer versions of the Mess simulator will be available as standalone versions. However, the standalone versions can be easily integrated in a manner similar to the current integrated version.



#### Standalone Mode

The Standalone folder contains all code and documentation necessary for running Mess simulator as an independent simulator. This mode is intended solely for learning and integration purposes and should not be used for trace-driven simulation.


## Cloning Options

Depending on your needs, there are several ways to obtain the Mess Simulator repository:

### Clone Only the Standalone Folder

If you only need the Standalone mode and want to avoid downloading the entire repository, you can use Git's sparse checkout feature. This downloads only the `Standalone/` directory:

```sh
git clone --depth=1 --filter=blob:none --sparse https://github.com/bsc-mem/Mess-simulator.git
cd Mess-simulator
git sparse-checkout set Standalone
```

- `--depth=1`: Creates a shallow clone with only the latest commit (no history). This significantly reduces download size and time.
- `--filter=blob:none`: Uses partial clone to avoid downloading objects until they are needed.
- `--sparse`: Enables sparse checkout mode.
- `git sparse-checkout set Standalone`: Specifies that only the `Standalone/` folder should be checked out.

If you need the full commit history, simply omit the `--depth=1` flag.

### Clone the Entire Repository

To clone the entire repository with all components (Standalone and Integrated modes):

```sh
git clone https://github.com/bsc-mem/Mess-simulator.git
cd Mess-simulator
```

If you don't need the full history, add `--depth=1`:

```sh
git clone --depth=1 https://github.com/bsc-mem/Mess-simulator.git
cd Mess-simulator
```

### Add as a Git Submodule

If you want to include the Mess Simulator as part of another Git project (e.g., as a dependency for your research project), you can add it as a submodule:

```sh
cd your-project
git submodule add https://github.com/bsc-mem/Mess-simulator.git external/Mess-simulator
```

This creates a reference to the Mess Simulator repository in the `external/Mess-simulator` directory. To clone a project with its submodules:

```sh
git clone --recurse-submodules https://github.com/your-username/your-project.git
```

Or if you've already cloned the project without submodules:

```sh
git submodule update --init --recursive
```

If you only need the Standalone folder as a submodule, you can use sparse checkout within the submodule:

```sh
git submodule add --depth=1 https://github.com/bsc-mem/Mess-simulator.git external/Mess-simulator
cd external/Mess-simulator
git sparse-checkout init --cone
git sparse-checkout set Standalone
```

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


## Contact Us

For any further questions and support, please contact us at the email below:

> 📧 **Email:** [mess@bsc.es](mailto:Mess@bsc.es)  

## License

Mess code is released under the [BSD 3-Clause License](LICENSE.txt).