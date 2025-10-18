# Mess Simulator Tutorial (MICRO 2025)

This folder provides a detailed, step-by-step explanation of how the **Mess simulator** works.  
Mess uses **bandwidth–latency curves** as input and applies a **PI controller** mechanism from classical control theory to match simulated performance to those curves.  
Given accurate input curves, Mess can provide **highly accurate memory system performance simulation**.

> **Note:** Mess is an *abstract performance simulator* — not a detailed main-memory model. It does **not** include components such as channels, ranks, or banks. It should be used as a lightweight, high-level simulator for performance modeling.

---

## When to Use Mess

### ✅ Useful Scenarios
- You need an accurate yet simple memory system simulator.  
- You need a fast model that provides immediate responses.  
- You want to model a new memory technology that lacks a detailed simulator due to IP or development constraints.  
- You value *accuracy over microarchitectural detail* — remember, being detailed does not always mean being accurate.

### 🚫 Not Useful Scenarios
- You want to explore detailed timing effects (e.g., reducing tRCD from 14.25 ns to 10 ns).  
- You need a standalone memory simulator — Mess requires integration with a CPU simulator.  
- You aim to perform design-space exploration on memory parameters (e.g., "What happens if bandwidth increases?").  
  > This feature is under development. Currently, exploration is limited to realistic, hardware-measured systems such as:
  > - AWS Graviton3 (8× DDR5-4800)
  > - Intel Max (4× HBM2E)
  > - Intel Skylake (6× DDR4-2666)

---

## Converting Curves from Mess Benchmark to Mess Simulator Input

To convert benchmark-generated curves into input for the simulator:

1. Define the **write-allocate policy** in the cache system (load/store vs. read/write percentage).  
2. Relate **load-to-use latency** (from the core) to **memory access response time**.

We provide a script to automate this process for most systems studied so far.  
The script is located in the `scriptConvertCurve` folder and requires the following input:

- **On-chip latency:** Estimated using the load-to-use latency of LLC accesses.  
  > Note: This is not the exact core-to-memory-controller latency, but it provides a very close approximation and yields acceptable accuracy.

---

## Notes

1. **Integration challenges (e.g., ZSim example):**
   - Locking mechanisms for parallel simulators  
   - Data allocation policies  

2. **Deep dives into key parameters and codes of the simulator**.
