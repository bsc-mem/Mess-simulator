#ifndef WEAVE_MESS_MEM_CTRL_H_
#define WEAVE_MESS_MEM_CTRL_H_

#include <fstream>
#include <cmath> // for floor function on implement round

#include "timing_event.h"
#include "zsim.h"
#include "memory_hierarchy.h"
#include "g_std/g_string.h"
#include "pad.h"
#include "stats.h"
#include "mess_mem_ctrl.h"

using namespace std;

/* Implements a weave-phase memory controller based on the bandwidth--latency curves, returning the same
 * latencies as bandwidth--latency memory controller would return in the weave phase
 */


//Weave-phase event
class MessAccEvent : public TimingEvent {
    private:
        uint32_t lat;

    public:
        MessAccEvent(uint32_t _lat, int32_t domain, uint32_t preDelay, uint32_t postDelay) :  TimingEvent(preDelay, postDelay, domain), lat(_lat) {}

        void simulate(uint64_t startCycle) {
            done(startCycle + lat);
        }
};


// Actual controller
class WeaveMessMemCtrl : public MessMemCtrl {
    private:
        const uint32_t domain;

        uint32_t zeroLoadLatency;
        uint32_t boundLatency;
        uint32_t preDelay, postDelay;

    public:


        WeaveMessMemCtrl( string curveAddress, uint32_t curveWindowSize, double frequency, uint32_t _domain, g_string& _name) :
            MessMemCtrl(curveAddress, curveWindowSize, frequency, _domain, _name), domain(_domain)
        {
            zeroLoadLatency = getLeadOffLatency();
            boundLatency = zeroLoadLatency;
            preDelay = zeroLoadLatency/2;
            postDelay = zeroLoadLatency - preDelay;
        }

        uint64_t access(MemReq& req) {
            uint64_t realRespCycle = MessMemCtrl::access(req);
            uint32_t realLatency = realRespCycle - req.cycle;

            uint64_t respCycle = req.cycle + ((req.type == PUTS)? 0 : boundLatency);
            assert(realRespCycle >= respCycle);
            assert(req.type == PUTS || realLatency >= zeroLoadLatency);

            if ((req.type != PUTS) && zinfo->eventRecorders[req.srcId]) {
                MessAccEvent* memEv = new (zinfo->eventRecorders[req.srcId]) MessAccEvent(realLatency-zeroLoadLatency, domain, preDelay, postDelay);
                memEv->setMinStartCycle(req.cycle);
                TimingRecord tr = {req.lineAddr, req.cycle, respCycle, req.type, memEv, memEv};
                zinfo->eventRecorders[req.srcId]->pushRecord(tr);
            }

            // info("Access to %lx at %ld, %d lat, returning %d", req.lineAddr, req.cycle, realLatency, zeroLoadLatency);
            return respCycle;
        }
};



#endif  // WEAVE_MESS_MEM_CTRL_H_
