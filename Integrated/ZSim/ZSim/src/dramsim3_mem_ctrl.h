/** $lic$
 * Copyright (C) 2012-2015 by Massachusetts Institute of Technology
 * Copyright (C) 2010-2013 by The Board of Trustees of Stanford University
 *
 * This file is part of zsim.
 *
 * zsim is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, version 2.
 *
 * If you use this software in your research, we request that you reference
 * the zsim paper ("ZSim: Fast and Accurate Microarchitectural Simulation of
 * Thousand-Core Systems", Sanchez and Kozyrakis, ISCA-40, June 2013) as the
 * source of the simulator in any publications that use this software, and that
 * you send us a citation of your work.
 *
 * zsim is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef DRAMSIM3_MEM_CTRL_H_
#define DRAMSIM3_MEM_CTRL_H_

#include <functional>
#include <map>
#include <string>
#include "g_std/g_string.h"
#include "g_std/g_unordered_map.h"
#include "memory_hierarchy.h"
#include "pad.h"
#include "stats.h"
#include "acm.h"

namespace dramsim3
{
class MemorySystem;
struct Address;
}; // namespace dramsim3

class DRAMsim3AccEvent;

/*struct DS3Request
{
    DS3Request(uint64_t addr, uint64_t cycle) : 
        addr(addr), channel(0), rank(0), bank(0), row(0),
        added_cycle(cycle) {}
    uint64_t addr;
    uint64_t channel;
    uint64_t rank;
    uint64_t bank;
    uint64_t row;
    uint64_t added_cycle;
};*/

class DRAMsim3Memory : public MemObject
{ //one DRAMsim3 controller
  private:
    g_string name;
    uint32_t minLatency;
    uint32_t domain;

    dramsim3::MemorySystem *dramCore;

    std::multimap<uint64_t, DRAMsim3AccEvent *> inflightRequests;

    uint64_t curCycle; //processor cycle, used in callbacks
    uint64_t dramCycle;
    uint64_t ACMAvailCycle; // When ACM@RowBuffer is free.
    uint64_t ACMMinLatency;

    // R/W stats
    PAD();
    Counter profReads;
    Counter profWrites;
    Counter profTotalRdLat;
    Counter profTotalWrLat;
    PAD();

  public:
    DRAMsim3Memory(std::string &ConfigName, std::string &OutputDir,
                   int cpuFreqMHz, uint32_t _domain, const g_string &_name);

    const char *getName() { return name.c_str(); }

    void initStats(AggregateStat *parentStat);

    // Record accesses
    uint64_t access(MemReq &req);
    uint64_t accessACM(MemReq& req); // ACM@RowBuffer access function
    void setDRAMsimConfiguration(uint32_t delayQueue); // Enable sim config in DRAMsim3 side.
    void setDRAMsimConfigurationWithACM(ACMInfo& acmInfo, uint32_t delayQueue); // Enable ACM@RowBuffer in DRAMsim3 side.
    void setWriteAddressACM(Address startAddress);
    void setSortedReadAddressACM(uint64_t startAddress);
    void setSizeOfACM(uint64_t numnerOfElement, uint64_t elementSize);
    uint64_t wait_for_PIM_cycles();

    // Event-driven simulation (phase 2)
    uint32_t tick(uint64_t cycle);
    void enqueue(DRAMsim3AccEvent *ev, uint64_t cycle);

  private:
    // if you want to use any data structure at all you in access()
    // it has to be on the zsim side
   // using BankQueue = g_vector<DS3Request>;
   // using RankQueue = g_unordered_map<uint64_t, BankQueue>;
   // using ChannelQueue = g_unordered_map<uint64_t, RankQueue>;
   // g_unordered_map<uint64_t, ChannelQueue> requestQueues;

    uint64_t channelMask, rankMask, bankMask, rowMask;
    void DRAM_read_return_cb(uint64_t addr);
    void DRAM_write_return_cb(uint64_t addr);
    std::function<void(uint64_t)> callBackFn;
    uint64_t dramPsPerClk, cpuPsPerClk;
    uint64_t dramPs, cpuPs;
    int tCL;
};

class SplitAddrMemory : public MemObject
{
  private:
    const g_vector<MemObject *> mems;
    const g_string name;

  public:
    SplitAddrMemory(const g_vector<MemObject *> &_mems, const char *_name) : mems(_mems), name(_name) {}

    uint64_t wait_for_PIM_cycles() {
        return mems[0]->wait_for_PIM_cycles();
    }

    // set the delay for no man's land delay buffer (Rommel Sanchez et al)
    void setDRAMsimConfiguration(uint32_t delayQueue)
    {
        // printf("size of mems available at ZSim side is: %d \n", mems.size()); // size is two
        mems[0]->setDRAMsimConfiguration(delayQueue);
    }

    // ACM@RowBuffer currently managed by only one MC.
    void setDRAMsimConfigurationWithACM(ACMInfo& acmInfo, uint32_t delayQueue)
    {
        // printf("size of mems available at ZSim side is: %d \n", mems.size()); // size is two
        mems[0]->setDRAMsimConfigurationWithACM(acmInfo, delayQueue);
    }

    void setWriteAddressACM(Address startAddress)
    {
        mems[0]->setWriteAddressACM(startAddress);
    }

    void setSortedReadAddressACM(uint64_t startAddress)
    {
        mems[0]->setSortedReadAddressACM(startAddress);
    }

    void setSizeOfACM(uint64_t numnerOfElement, uint64_t elementSize)
    {
        mems[0]->setSizeOfACM(numnerOfElement, elementSize);
    }

    // ACM@RowBuffer currently managed by only one MC.
    uint64_t accessACM(MemReq& req)
    {
        mems[0]->accessACM(req);
    }

    uint64_t access(MemReq &req)
    {
        Address addr = req.lineAddr;
        uint32_t mem = addr % mems.size();
        Address ctrlAddr = addr / mems.size();
        req.lineAddr = ctrlAddr;
        uint64_t respCycle = mems[mem]->access(req);
        req.lineAddr = addr;
        return respCycle;
    }

    const char *getName()
    {
        return name.c_str();
    }

    void initStats(AggregateStat *parentStat)
    {
        for (auto mem : mems)
            mem->initStats(parentStat);
    }
};

#endif // DRAMSIM3_MEM_CTRL_H_
