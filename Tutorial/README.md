

# Mess Simulator Tutorial (MICRO 2025)

This folder provides a detailed, step-by-step explanation on how Mess simulator works. Mess simulator uses bandwidth--latency curves as input and using PI controller mechanism from classical control theory, it matches memory system simulation performance to the input bandwidth--latency curves. It accurately follows the performance of bandwidth--latency curves and therefore, given a accurate curves, it can proide you highly accurate memory system performance simulator.  
 
*Note: Please remember Mess is not a detailed simulator and does not include bulding blocks of main memory such as channel, rank, banks, and etc., instead Mess is an abstract perforamance simulator and it should be used as such. Scenarios that we think Mess is or is not usefull: 

Usefull scenario: 


- You need an accurate and simple memory system simulator. 
- You need an accurate immediate response memory model. 
- You need a very fast memory simulator. 
- You want to simulate a very new technology that does not have any detailed model yet due to IP or development time issues. Remember that being detaield are not necessarily means more accurate. 


Not usefull scenario: 

- Mess is not a detailed memory simulator. Therefore, you cannot explore the peroframcen of memory system when tRCD decreases fro 14.25ns to 10ns 

- Mess simulator does not work without a CPU simulator. So you cannot expect the same thing that you expect from Ramulator/DRAMsim3 trace based simulation. 

- Design space exploraton on memory side: e.g., haveing more bandwidth or latnecy. (work in progress). As of now your exploration is limtited to realsitic measurments on actual hardware: Graviton3's 8x DDR5-4800, Intel max 4x HBM2E, Intel Skylake 6x DDR4-2666.  
 


## How to convert the curves from Mess benchmark to input curves of Mess simulator 

- Write allocate policy in cache system (load/store percentage Vs read/write percentage). 

- load-to-use-latency from core Vs memory access response time. 


We wrote an script to address this issue for majority of the systems we studies so far. This script works as follows: 


*Note: We estimate on-chip latency with load-to-use-latency of LLC accesses. Please remind this is not exactly the latency we want (core to memory controller), but this is the closest we can get. And reaching this point give us already a very good accuracy.  







## Notes

1. Integration is not as easy as you think: ZSim example. 

	- Lock for parallel simulators 
	- Data allocation policy 


2. Deep dive into important parameter of the simulator

















## Refrences

[[1]](https://www.bsc.es/marenostrum/marenostrum-5) [https://www.bsc.es/marenostrum/marenostrum-5](https://www.bsc.es/marenostrum/marenostrum-5 ) 

[[2]](https://www.bsc.es/supportkc/docs/MareNostrum5/overview) [https://www.bsc.es/supportkc/docs/MareNostrum5/overview](https://www.bsc.es/supportkc/docs/MareNostrum5/overview ) 
