# Mess-simulator

Mess simulators is an abstract memory model based on bandwidth--latency curves. It matches the performance of the main memory to the input bandwidth--latency curves. The input bandwidth--latency curves can be obtained by running Mess benchmark on actual hardware or recieved from memory manufacturer, e.g. based on their detailed hardware model. 

The Mess simulator approach is based on the proportional–integral controller mechanism from the control theory to match its output to its input bandwidth--latency curves. 

Currently Mess simulator exists in standalone mode and integrated mode. It is integrated with ZSim, gem5 and OpenPiton Metro-MPI simulators. The simulator is implemented in C++ with a simple interface to be integrated to any CPU simulator. 
 

## Citation

Please cite the following paper if you find this benchmark useful:

```
@misc{esmailidokht2024messmemorybenchmarkingsimulation,
      title={A Mess of Memory System Benchmarking, Simulation and Application Profiling}, 
      author={Pouya Esmaili-Dokht and Francesco Sgherzi and Valeria Soldera Girelli and Isaac Boixaderas and Mariana Carmin and Alireza Monemi and Adria Armejach and Estanislao Mercadal and German Llort and Petar Radojkovic and Miquel Moreto and Judit Gimenez and Xavier Martorell and Eduard Ayguade and Jesus Labarta and Emanuele Confalonieri and Rishabh Dubey and Jason Adlard},
      year={2024},
      eprint={2405.10170},
      archivePrefix={arXiv},
      primaryClass={cs.AR},
      url={https://arxiv.org/abs/2405.10170}, 
}
```

---To be published in MICRO 2024







