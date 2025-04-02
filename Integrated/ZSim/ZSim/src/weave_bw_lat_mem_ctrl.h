/*
 * Copyright (c) 2024, Barcelona Supercomputing Center
 * Contact: mess             [at] bsc [dot] es
 *          pouya.esmaili    [at] bsc [dot] es
 *          petar.radojkovic [at] bsc [dot] es
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *     * Redistributions of source code must retain the above copyright notice,
 *       this list of conditions and the following disclaimer.
 *
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *
 *     * Neither the name of the copyright holder nor the names
 *       of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#ifndef WEAVE_BW_LAT_MEM_CTRL_H_
#define WEAVE_BW_LAT_MEM_CTRL_H_

#include <fstream>
#include <cmath> // for floor function on implement round

#include "timing_event.h"
#include "zsim.h"
#include "memory_hierarchy.h"
#include "g_std/g_string.h"
#include "pad.h"
#include "stats.h"
#include "bw_lat_mem_ctrl.h"

using namespace std;

/* Implements a weave-phase memory controller based on the bandwidth--latency curves, returning the same
 * latencies as bandwidth--latency memory controller would return in the weave phase
 */



//Weave-phase event
class BwLatAccEvent : public TimingEvent {
    private:
        uint32_t lat;

    public:
        BwLatAccEvent(uint32_t _lat, int32_t domain, uint32_t preDelay, uint32_t postDelay) :  TimingEvent(preDelay, postDelay, domain), lat(_lat) {}

        void simulate(uint64_t startCycle) {
            done(startCycle + lat);
        }
};


// Actual controller
class WeaveBwLatMemCtrl : public BwLatMemCtrl {
    private:
    	const uint32_t domain;

        uint32_t zeroLoadLatency;
        uint32_t boundLatency;
        uint32_t preDelay, postDelay;

    public:



        WeaveBwLatMemCtrl( string curveAddress, uint32_t curveWindowSize, double MaxTheoreticalBW, uint32_t _domain, g_string& _name) :
            BwLatMemCtrl(curveAddress, curveWindowSize, MaxTheoreticalBW, _domain, _name), domain(_domain)
        {
        	zeroLoadLatency = getLeadOffLatency();
        	boundLatency = zeroLoadLatency;
            preDelay = zeroLoadLatency/2;
            postDelay = zeroLoadLatency - preDelay;
        }

        uint64_t access(MemReq& req) {
            uint64_t realRespCycle = BwLatMemCtrl::access(req);
            uint32_t realLatency = realRespCycle - req.cycle;

            uint64_t respCycle = req.cycle + ((req.type == PUTS)? 0 : boundLatency);
            assert(realRespCycle >= respCycle);
            assert(req.type == PUTS || realLatency >= zeroLoadLatency);

            if ((req.type != PUTS) && zinfo->eventRecorders[req.srcId]) {
                WeaveMemAccEvent* memEv = new (zinfo->eventRecorders[req.srcId]) WeaveMemAccEvent(realLatency-zeroLoadLatency, domain, preDelay, postDelay);
                memEv->setMinStartCycle(req.cycle);
                TimingRecord tr = {req.lineAddr, req.cycle, respCycle, req.type, memEv, memEv};
                zinfo->eventRecorders[req.srcId]->pushRecord(tr);
            }

            // info("Access to %lx at %ld, %d lat, returning %d", req.lineAddr, req.cycle, realLatency, zeroLoadLatency);
            return respCycle;
        }
};



#endif  // WEAVE_BW_LAT_MEM_CTRL_H_
