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

#include "dramsim3_mem_ctrl.h"
#include <map>
#include <string>
#include "event_recorder.h"
#include "tick_event.h"
#include "timing_event.h"
#include "zsim.h"

// add for trace 
#include <fstream>
#include <iostream>

#ifdef _WITH_DRAMSIM3_ //was compiled with dramsim3
//#include "memory_system.h"
#include "dramsim3.h"

using namespace std;

class DRAMsim3AccEvent : public TimingEvent
{
  private:
    DRAMsim3Memory *dram;
    bool write;
    Address addr;

  public:
    uint64_t sCycle;

    DRAMsim3AccEvent(DRAMsim3Memory *_dram, bool _write, Address _addr, int32_t domain) : TimingEvent(0, 0, domain), dram(_dram), write(_write), addr(_addr) {}

    bool isWrite() const
    {
        return write;
    }

    Address getAddr() const
    {
        return addr;
    }

    void simulate(uint64_t startCycle)
    {
        sCycle = startCycle;
        dram->enqueue(this, startCycle);
    }
};

DRAMsim3Memory::DRAMsim3Memory(std::string& ConfigName, std::string& OutputDir,
                               int cpuFreqMHz, uint32_t _domain, const g_string &_name)
{
    curCycle = 0;
    dramCycle = 0;
    dramPs = 0;
    cpuPs = 0;
    minLatency = 1;
    ACMAvailCycle = 0; // ACM@RowBuffer return cycle for Bound phase.
    ACMMinLatency = 0; // ACM@RowBuffer fix the interface for Bound-Weave model
    // NOTE: this will alloc DRAM on the heap and not the glob_heap, make sure only one process ever handles this
    callBackFn = std::bind(&DRAMsim3Memory::DRAM_read_return_cb, this, std::placeholders::_1);

    // For some reason you cannot "new" here because zsim seems to override this "new"
    // so we have to use the helper function to init the pointer
    dramCore = dramsim3::GetMemorySystem(ConfigName, OutputDir, callBackFn, callBackFn);
    double tCK = dramCore->GetTCK();
    channelMask = dramCore->GetChannelMask();
    rankMask = dramCore->GetRankMask();
    bankMask = dramCore->GetBankMask();
    rowMask = dramCore->GetRowMask();
    dramPsPerClk = static_cast<uint64_t>(tCK*1000);
    cpuPsPerClk = static_cast<uint64_t>(1000000. / cpuFreqMHz);
    assert(cpuPsPerClk < dramPsPerClk);
    domain = _domain;
    TickEvent<DRAMsim3Memory> *tickEv = new TickEvent<DRAMsim3Memory>(this, domain);
    tickEv->queue(0); // start the sim at time 0

    name = _name;
}

void DRAMsim3Memory::initStats(AggregateStat *parentStat)
{
    AggregateStat* memStats = new AggregateStat();
    memStats->init(name.c_str(), "Memory controller stats");
    profReads.init("rd", "Read requests"); memStats->append(&profReads);
    profWrites.init("wr", "Write requests"); memStats->append(&profWrites);
    profTotalRdLat.init("rdlat", "Total latency experienced by read requests"); memStats->append(&profTotalRdLat);
    profTotalWrLat.init("wrlat", "Total latency experienced by write requests"); memStats->append(&profTotalWrLat);
    parentStat->append(memStats);
}

// Get the cycles left in memory simulator for PIM operation to finish
uint64_t DRAMsim3Memory::wait_for_PIM_cycles() {
    // return 0;
    // cout << "dramsim3_mem_ctrl.cpp: " << dramCore->wait_for_PIM_cycles() << endl;
    return dramCore->wait_for_PIM_cycles();
}


// Enable sim config in DRAMsim3 side.
void DRAMsim3Memory::setDRAMsimConfiguration(uint32_t delayQueue)
{
    dramCore->setDelayQueue(delayQueue);
} 

// Enable ACM@RowBuffer in DRAMsim3 side.
void DRAMsim3Memory::setDRAMsimConfigurationWithACM(ACMInfo& acmInfo, uint32_t delayQueue)
{
    //printf("ACM is Enabled in DRAMsim3Cntrl at ZSim part\n");
    dramCore->EnableACMatRowBuffer(&acmInfo);
    setDRAMsimConfiguration(delayQueue);
}

void DRAMsim3Memory::setWriteAddressACM(Address startAddress) 
{
    // cout << "dramsim3_mem_ctrl setWriteAddressACM " << startAddress <<endl;
    dramCore->setWriteAddressACM(startAddress); 
}

void DRAMsim3Memory::setSortedReadAddressACM(uint64_t startAddress)
{
    // cout << "dramsim3_mem_ctrl setSortedReadAddressACM " << startAddress <<endl;
    dramCore->setSortedReadAddressACM(startAddress); 
}

void DRAMsim3Memory::setSizeOfACM(uint64_t numnerOfElement, uint64_t elementSize)
{
    // cout << "dramsim3_mem_ctrl setSizeOfACM numnerOfElement: " << numnerOfElement << "elementSize" << elementSize <<endl;
    dramCore->setSizeOfACM(numnerOfElement, elementSize); 
}

uint64_t DRAMsim3Memory::accessACM(MemReq &req)
{
    // NOTE so you basicall cannot access draoCore->*
    // in this function (or this phase I assume) otherwise
    // you break some weird memory and pin will try to kill you, like, what?
    // printf("accessACM in dramsim3 side\n");
    switch (req.type)
    {
    case PUTS:
    case PUTX:
        *req.state = I;
        break;
    case GETS:
        *req.state = req.is(MemReq::NOEXCL) ? S : E;
        break;
    case GETX:
        *req.state = M;
        break;

    default:
        panic("!?");
    }

    // TODO make this dynamic
    uint64_t respCycle = req.cycle ;// + minLatency;

    if ((req.type != PUTS /*discard clean writebacks*/) && zinfo->eventRecorders[req.srcId])
    {
        // line below may generate error be careful...!
        Address addr = req.lineAddr << lineBits; // exact address in dramsim3 side for ACM@RowBuffer
        uint64_t hexAddr = (uint64_t)addr;
        // DS3Request dramReq(hexAddr, req.cycle);
        // dramReq.channel = hexAddr & channelMask;
        // dramReq.rank = hexAddr & rankMask;
        // dramReq.bank = hexAddr & bankMask;
        // dramReq.row = hexAddr & rowMask;
        // if (requestQueues.count(dramReq.channel) == 0) {
        //     // BankQueue bankQ;
        //     g_vector<DS3Request> bankQ;
        //     bankQ.push_back(dramReq);
        //     RankQueue rankQueue;
        //     rankQueue.emplace(dramReq.bank, bankQ);
        //     ChannelQueue chanQueue;
        //     chanQueue.emplace(dramReq.rank, rankQueue);
        //     requestQueues.emplace(dramReq.channel, chanQueue);
        // }
        bool isWrite = (req.type == PUTX);
        DRAMsim3AccEvent *memEv = new (zinfo->eventRecorders[req.srcId]) DRAMsim3AccEvent(this, isWrite, addr, domain);
        memEv->setMinStartCycle(req.cycle);
        TimingRecord tr = {addr, req.cycle, respCycle, req.type, memEv, memEv};
        zinfo->eventRecorders[req.srcId]->pushRecord(tr);
    }


    // hack when in case we overflow the PIM engine in DRAMsim3
    if (dramCore->GetACMDelayforZSimBoundPhase()>10000)
    {
         // std::cout<<dramCore->GetACMDelayforZSimBoundPhase()<<std::endl;
         ACMMinLatency=50; // adding some latency to make sure the buffer is not full
    }
    else
    {
         ACMMinLatency=0;
    }
    
    
    return respCycle + ACMMinLatency;
}

uint64_t DRAMsim3Memory::access(MemReq &req)
{

    // NOTE so you basicall cannot access draoCore->*
    // in this function (or this phase I assume) otherwise
    // you break some weird memory and pin will try to kill you, like, what?
    switch (req.type)
    {
    case PUTS:
    case PUTX:
        *req.state = I;
        break;
    case GETS:
        *req.state = req.is(MemReq::NOEXCL) ? S : E;
        break;
    case GETX:
        *req.state = M;
        break;

    default:
        panic("!?");
    }

    // TODO make this dynamic
    uint64_t respCycle = req.cycle ;// + minLatency;

    if ((req.type != PUTS /*discard clean writebacks*/) && zinfo->eventRecorders[req.srcId])
    {
        Address addr = req.lineAddr << lineBits;
        uint64_t hexAddr = (uint64_t)addr;
        // DS3Request dramReq(hexAddr, req.cycle);
        // dramReq.channel = hexAddr & channelMask;
        // dramReq.rank = hexAddr & rankMask;
        // dramReq.bank = hexAddr & bankMask;
        // dramReq.row = hexAddr & rowMask;
        // if (requestQueues.count(dramReq.channel) == 0) {
        //     // BankQueue bankQ;
        //     g_vector<DS3Request> bankQ;
        //     bankQ.push_back(dramReq);
        //     RankQueue rankQueue;
        //     rankQueue.emplace(dramReq.bank, bankQ);
        //     ChannelQueue chanQueue;
        //     chanQueue.emplace(dramReq.rank, rankQueue);
        //     requestQueues.emplace(dramReq.channel, chanQueue);
        // }
        bool isWrite = (req.type == PUTX);
        DRAMsim3AccEvent *memEv = new (zinfo->eventRecorders[req.srcId]) DRAMsim3AccEvent(this, isWrite, addr, domain);
        memEv->setMinStartCycle(req.cycle);
        TimingRecord tr = {addr, req.cycle, respCycle, req.type, memEv, memEv};
        zinfo->eventRecorders[req.srcId]->pushRecord(tr);
    }

    return respCycle + minLatency;
}

uint32_t DRAMsim3Memory::tick(uint64_t cycle) {
    cpuPs += cpuPsPerClk;
    curCycle++;
    if (cpuPs > dramPs) {
        dramCore->ClockTick();
        dramPs += dramPsPerClk;
        dramCycle++;
    }
    if (cpuPs == dramPs) {  // reset to prevent overflow
        cpuPs = 0;
        dramPs = 0;
    }
    return 1;
}

void DRAMsim3Memory::enqueue(DRAMsim3AccEvent *ev, uint64_t cycle) {
    // info("[%s] %s access to %lx added at %ld, %ld inflight reqs", getName(), ev->isWrite()? "Write" : "Read", ev->getAddr(), cycle, inflightRequests.size());
    dramCore->AddTransaction(ev->getAddr(), ev->isWrite());
    inflightRequests.insert(std::pair<Address, DRAMsim3AccEvent *>(ev->getAddr(), ev));
    ev->hold();

}

void DRAMsim3Memory::DRAM_read_return_cb(uint64_t addr) {
    std::multimap<uint64_t, DRAMsim3AccEvent *>::iterator it = inflightRequests.find(addr);
    assert((it != inflightRequests.end()));
    DRAMsim3AccEvent *ev = it->second;

    uint32_t lat = curCycle + 1 - ev->sCycle;
    minLatency = lat;
    
    if (ev->isWrite()) {
        profWrites.inc();
        profTotalWrLat.inc(lat);
    } else {
        profReads.inc();
        profTotalRdLat.inc(lat);
    }

    ev->release();
    ev->done(curCycle + 1);
    inflightRequests.erase(it);
    // info("[%s] %s access to %lx DONE at %ld (%ld cycles), %ld inflight reqs", getName(), it->second->isWrite()? "Write" : "Read", it->second->getAddr(), curCycle, curCycle-it->second->sCycle, inflightRequests.size());
}

void DRAMsim3Memory::DRAM_write_return_cb(uint64_t addr)
{
    //Same as read for now
    DRAM_read_return_cb(addr);
}

#else //no dramsim3, have the class fail when constructed

using std::string;

DRAMsim3Memory::DRAMsim3Memory(std::string& ConfigName, std::string& OutputDir,
                               int cpuFreqMHz, uint32_t _domain, const g_string &_name)
{
    panic("Cannot use DRAMsim3Memory, zsim was not compiled with DRAMsim3");
}

void DRAMsim3Memory::initStats(AggregateStat *parentStat) { panic("???"); }
uint64_t DRAMsim3Memory::access(MemReq &req)
{
    panic("???");
    return 0;
}
uint32_t DRAMsim3Memory::tick(uint64_t cycle)
{
    panic("???");
    return 0;
}
void DRAMsim3Memory::enqueue(DRAMsim3AccEvent *ev, uint64_t cycle) { panic("???"); }
// void DRAMsim3Memory::DRAM_read_return_cb(uint64_t addr) { panic("???"); }
// void DRAMsim3Memory::DRAM_write_return_cb(uint64_t addr) { panic("???"); }

#endif
