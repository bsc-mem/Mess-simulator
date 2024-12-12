# Mess-simulator

Mess simulators is an abstract memory model based on bandwidth--latency curves. It matches the performance of the main memory to the input bandwidth--latency curves. The input bandwidth--latency curves can be obtained by running Mess benchmark on actual hardware or recieved from memory manufacturer, e.g. based on their detailed hardware model. 

The Mess simulator approach is based on the proportional–integral controller mechanism from the control theory to match its output to its input bandwidth--latency curves. 

Currently Mess simulator exists in standalone mode and integrated mode. It is integrated with ZSim, gem5 and OpenPiton Metro-MPI simulators. The simulator is implemented in C++ with a simple interface to be integrated to any CPU simulator. 
 

## Citation

If you find Mess simulator useful, please cite the following paper accepted in MICRO 2024, best paper runner-up award.

```
@INPROCEEDINGS{10764561,
  author={Esmaili-Dokht, Pouya and Sgherzi, Francesco and Girelli, Valéria Soldera and Boixaderas, Isaac and Carmin, Mariana and Monemi, Alireza and Armejach, Adrià and Mercadal, Estanislao and Llort, Germán and Radojković, Petar and Moreto, Miquel and Giménez, Judit and Martorell, Xavier and Ayguadé, Eduard and Labarta, Jesus and Confalonieri, Emanuele and Dubey, Rishabh and Adlard, Jason},
  booktitle={57th IEEE/ACM International Symposium on Microarchitecture (MICRO)}, 
  title={{A Mess of Memory System Benchmarking, Simulation and Application Profiling}}, 
  year={2024}}
```








