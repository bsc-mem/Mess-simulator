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

#ifndef FILTER_CACHE_H_
#define FILTER_CACHE_H_

#include "bithacks.h"
#include "cache.h"
#include "galloc.h"
#include "zsim.h"
#include "acm.h"

/* Extends Cache with an L0 direct-mapped cache, optimized to hell for hits
 *
 * L1 lookups are dominated by several kinds of overhead (grab the cache locks,
 * several virtual functions for the replacement policy, etc.). This
 * specialization of Cache solves these issues by having a filter array that
 * holds the most recently used line in each set. Accesses check the filter array,
 * and then go through the normal access path. Because there is one line per set,
 * it is fine to do this without grabbing a lock.
 */

class FilterCache : public Cache {
    private:
        struct FilterEntry {
            volatile Address rdAddr;
            volatile Address wrAddr;
            volatile uint64_t availCycle;

            void clear() {wrAddr = 0; rdAddr = 0; availCycle = 0;}
        };

        //Replicates the most accessed line of each set in the cache
        FilterEntry* filterArray;
        Address setMask;
        uint32_t numSets;
        uint32_t srcId; //should match the core
        uint32_t reqFlags;
        g_vector<MemObject*> ancestors; // ACM@rowBuffer (Bypass cache system with a pointer)
        lock_t filterLock;
        uint64_t fGETSHit, fGETXHit;

        bool ACMEnable; // ACM@rowBuffer Enable bit
        uint64_t ACMAllocatedSpace; // ACM@rowBuffer DATA-RAM memory size
        uint32_t ACMRecordSize;
        uint64_t ConventionalPerspectiveStartAddress;
        uint64_t SortedPerspectiveStartAddress;
       
    public:

        ACMInfo myACMInfo;

        FilterCache(uint32_t _numSets, uint32_t _numLines, CC* _cc, CacheArray* _array,
                ReplPolicy* _rp, uint32_t _accLat, uint32_t _invLat, g_string& _name)
            : Cache(_numLines, _cc, _array, _rp, _accLat, _invLat, _name)
        {
            numSets = _numSets;
            setMask = numSets - 1;
            filterArray = gm_memalign<FilterEntry>(CACHE_LINE_BYTES, numSets);
            for (uint32_t i = 0; i < numSets; i++) filterArray[i].clear();
            futex_init(&filterLock);
            fGETSHit = fGETXHit = 0;
            srcId = -1;
            reqFlags = 0;
            ACMEnable = 0;
            ACMAllocatedSpace=0;
            ConventionalPerspectiveStartAddress=0;
            SortedPerspectiveStartAddress=0;
        }

        // ACM@rowBuffer virtual address space
        int is_ACM_conventional_perspective_accessed(Address vAddr) {
            if((Address)ConventionalPerspectiveStartAddress <= vAddr && vAddr < (Address)ConventionalPerspectiveStartAddress+(Address)ACMAllocatedSpace)
                return 1;
            else
                return 0;
        }

        // ACM@rowBuffer virtual address space
        int is_ACM_sorted_perspective_accessed(Address vAddr) {
            if((Address)SortedPerspectiveStartAddress <= vAddr && vAddr < (Address)SortedPerspectiveStartAddress+(Address)ACMAllocatedSpace)
                return 1;
            else
                return 0;
        }

        uint64_t wait_for_PIM_cycles() {
            // cout << "filter_cache.h: " << endl;
            ancestors[0]->wait_for_PIM_cycles(); 
        }

        void setWriteAddressACM(Address startAddress) {
            // cout<< "filter cache of core " << srcId << " with start address of sortedWrite " << startAddress << endl;
            ConventionalPerspectiveStartAddress=startAddress;
            for (uint32_t p = 0; p < ancestors.size(); p++) {
                ancestors[p]->setWriteAddressACM(startAddress);
            }
        }

        void setSortedReadAddressACM(uint64_t startAddress){
            // cout<< "filter cache of core " << srcId << " with start address of sortedRead " << startAddress << endl;
            SortedPerspectiveStartAddress=startAddress;
            for (uint32_t p = 0; p < ancestors.size(); p++) {
                ancestors[p]->setSortedReadAddressACM(startAddress);
            }
        }

        void setSizeOfACM(uint64_t numnerOfElement, uint64_t elementSize) {
            // cout<< "filter cache of core " << srcId << "  numnerOfElement  " << numnerOfElement << " elementSize " << elementSize << endl;
            ACMRecordSize=elementSize;
            ACMAllocatedSpace=numnerOfElement*elementSize;
            for (uint32_t p = 0; p < ancestors.size(); p++) {
                ancestors[p]->setSizeOfACM(numnerOfElement, elementSize);
            }
        }
        
        // ACM@rowBuffer (Bypass cache system with a pointer) and enable ACM
        void setAncestors(const g_vector<MemObject*>& _parents, uint32_t delayQueue, ACMInfo& acmInfo){
            ACMEnable = 1;
            myACMInfo = acmInfo;
            // ACMAllocatedSpace = acmInfo.ACMAllocatedSpace;
            // ACMRecordSize = acmInfo.recordSize;
            ancestors.resize(_parents.size());
            for (uint32_t p = 0; p < ancestors.size(); p++) {
                ancestors[p] = _parents[p];
                ancestors[p]->setDRAMsimConfigurationWithACM(acmInfo, delayQueue);
            }
        }

        // Configure No man's land delay (Rommel Sanchez et al)
        void setAncestors(const g_vector<MemObject*>& _parents, uint32_t delayQueue){
            ancestors.resize(_parents.size());
            for (uint32_t p = 0; p < ancestors.size(); p++) {
                ancestors[p] = _parents[p];
                ancestors[p]->setDRAMsimConfiguration(delayQueue);
            }
        }

        void setSourceId(uint32_t id) {
            srcId = id;
        }

        void setFlags(uint32_t flags) {
            reqFlags = flags;
        }

        void initStats(AggregateStat* parentStat) {
            AggregateStat* cacheStat = new AggregateStat();
            cacheStat->init(name.c_str(), "Filter cache stats");
            ProxyStat* fgetsStat = new ProxyStat();
            fgetsStat->init("fhGETS", "Filtered GETS hits", &fGETSHit);
            ProxyStat* fgetxStat = new ProxyStat();
            fgetxStat->init("fhGETX", "Filtered GETX hits", &fGETXHit);
            cacheStat->append(fgetsStat);
            cacheStat->append(fgetxStat);
            initCacheStats(cacheStat);
            parentStat->append(cacheStat);
        }

        inline uint64_t load(Address vAddr, uint64_t curCycle) {
            
            /*                     Start of ACM@rowBuffer                   */
            // study more the implications of Sorted Read for Records that are big 
            if(is_ACM_sorted_perspective_accessed(vAddr)&&vAddr%ACMRecordSize<64) {
                //ACMSortedReadAccess++;
                if(vAddr%ACMRecordSize)
                    return curCycle;
                Address vLineAddr = vAddr >> lineBits;
                uint32_t idx = vLineAddr & setMask;
                uint64_t results = replaceACM(vLineAddr, idx, true, curCycle); //+ L1D_LAT ; // later L1D_LAT should be change to some other constant...
                return results;
            }
            /*                     End of ACM@rowBuffer                   */

            Address vLineAddr = vAddr >> lineBits;
            uint32_t idx = vLineAddr & setMask;
            uint64_t availCycle = filterArray[idx].availCycle; //read before, careful with ordering to avoid timing races
            if (vLineAddr == filterArray[idx].rdAddr) {
                fGETSHit++;
                return MAX(curCycle, availCycle);
            } else {
                return replace(vLineAddr, idx, true, curCycle);
            }
        }

        inline uint64_t store(Address vAddr, uint64_t curCycle) {

            /*                     Start of ACM@rowBuffer                   */
            if(is_ACM_conventional_perspective_accessed(vAddr)&&vAddr%ACMRecordSize<64) {    
                cout << "current cycle cpu: " << curCycle << endl;            
                if(vAddr%ACMRecordSize)
                    return curCycle;
                Address vLineAddr = vAddr >> lineBits;
                uint32_t idx = vLineAddr & setMask;
                uint64_t results = replaceACM(vLineAddr, idx, false, curCycle); //+ L1D_LAT ; This latency miss with issue rate
            	return results;
            }
            /*                     End of ACM@rowBuffer                   */

            Address vLineAddr = vAddr >> lineBits;
            uint32_t idx = vLineAddr & setMask;
            uint64_t availCycle = filterArray[idx].availCycle; //read before, careful with ordering to avoid timing races
            if (vLineAddr == filterArray[idx].wrAddr) {
                fGETXHit++;
                //NOTE: Stores don't modify availCycle; we'll catch matches in the core
                //filterArray[idx].availCycle = curCycle; //do optimistic store-load forwarding
                return MAX(curCycle, availCycle);
            } else {
                return replace(vLineAddr, idx, false, curCycle);
            }
        }

        // ACM@rowBuffer (For sorted perspective accesses arrive here and bypass the cache hierarchy)
        uint64_t replaceACM(Address vLineAddr, uint32_t idx, bool isLoad, uint64_t curCycle) {
            Address pLineAddr = procMask | vLineAddr;
            MESIState dummyState = MESIState::I;
            uint64_t respCycle;
            futex_lock(&filterLock);
            MemReq req = {pLineAddr, isLoad? GETX : PUTX, 0, &dummyState, curCycle, &filterLock, dummyState, srcId, reqFlags};
            respCycle  = ancestors[0]->accessACM(req); 
            futex_unlock(&filterLock);
            return respCycle;
        }

        uint64_t replace(Address vLineAddr, uint32_t idx, bool isLoad, uint64_t curCycle) {
            Address pLineAddr = procMask | vLineAddr;
            MESIState dummyState = MESIState::I;
            futex_lock(&filterLock);
            MemReq req = {pLineAddr, isLoad? GETS : GETX, 0, &dummyState, curCycle, &filterLock, dummyState, srcId, reqFlags};
            uint64_t respCycle  = access(req);

            //Due to the way we do the locking, at this point the old address might be invalidated, but we have the new address guaranteed until we release the lock

            //Careful with this order
            Address oldAddr = filterArray[idx].rdAddr;
            filterArray[idx].wrAddr = isLoad? -1L : vLineAddr;
            filterArray[idx].rdAddr = vLineAddr;

            //For LSU simulation purposes, loads bypass stores even to the same line if there is no conflict,
            //(e.g., st to x, ld from x+8) and we implement store-load forwarding at the core.
            //So if this is a load, it always sets availCycle; if it is a store hit, it doesn't
            if (oldAddr != vLineAddr) filterArray[idx].availCycle = respCycle;

            futex_unlock(&filterLock);
            return respCycle;
        }

        uint64_t invalidate(const InvReq& req) {
            Cache::startInvalidate();  // grabs cache's downLock
            futex_lock(&filterLock);
            uint32_t idx = req.lineAddr & setMask; //works because of how virtual<->physical is done...
            if ((filterArray[idx].rdAddr | procMask) == req.lineAddr) { //FIXME: If another process calls invalidate(), procMask will not match even though we may be doing a capacity-induced invalidation!
                filterArray[idx].wrAddr = -1L;
                filterArray[idx].rdAddr = -1L;
            }
            uint64_t respCycle = Cache::finishInvalidate(req); // releases cache's downLock
            futex_unlock(&filterLock);
            return respCycle;
        }

        void contextSwitch() {
            futex_lock(&filterLock);
            for (uint32_t i = 0; i < numSets; i++) filterArray[i].clear();
            futex_unlock(&filterLock);
        }
};

#endif  // FILTER_CACHE_H_
