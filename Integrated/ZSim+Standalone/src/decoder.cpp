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

#include "decoder.h"
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string.h>
#include <string>
#include <vector>
#include "core.h"
#include "locks.h"
#include "log.h"

extern "C" {
#include "xed-interface.h"
}

//XED expansion macros (enable us to type opcodes at a reasonable speed)
#define XC(cat) (XED_CATEGORY_##cat)
#define XO(opcode) (XED_ICLASS_##opcode)

#define INACCUOP ; //emitExecUop(0, 0, 0, 0, uops, 1, PORTS_015 | PORTS_23 | PORT_4);

void DynUop::clear() {
    memset(this, 0, sizeof(DynUop));  // NOTE: This may break if DynUop becomes non-POD
}

Decoder::Instr::Instr(INS _ins) : ins(_ins), numLoads(0), numInRegs(0), numOutRegs(0), numStores(0) {
    uint32_t numOperands = INS_OperandCount(ins);
    for (uint32_t op = 0; op < numOperands; op++) {
        bool read = INS_OperandRead(ins, op);
        bool write = INS_OperandWritten(ins, op);
        assert(read || write);
        if (INS_OperandIsMemory(ins, op)) {
            if (read) loadOps[numLoads++] = op;
            if (write) storeOps[numStores++] = op;
        } else if (INS_OperandIsReg(ins, op) && INS_OperandReg(ins, op)) { 
	//it's apparently possible to get INS_OperandIsReg to be true and an invalid reg ... WTF Pin?
            REG reg = INS_OperandReg(ins, op);
            assert(reg);  // can't be invalid
            reg = REG_FullRegName(reg);  // eax -> rax, etc; o/w we'd miss a bunch of deps!
            if (read) inRegs[numInRegs++] = reg;
            if (write) outRegs[numOutRegs++] = reg;
        } else if (INS_OperandIsAddressGenerator(ins, op)) {
            // address generators occur in LEAs & satisfy neither IsMemory nor,
            // more infuriatingly, IsReg. despite being one operand, address
            // generators may introduce dependencies on up to two registers: the
            // base and index registers
            assert(INS_OperandReadOnly(ins, op));

            // base register; optional
            REG reg = INS_OperandMemoryBaseReg(ins, op);
            if (REG_valid(reg)) inRegs[numInRegs++] = REG_FullRegName(reg);

            // index register; optional
            reg = INS_OperandMemoryIndexReg(ins, op);
            if (REG_valid(reg)) inRegs[numInRegs++] = REG_FullRegName(reg);
        } else if (INS_OperandIsImmediate(ins, op)
                   || INS_OperandIsBranchDisplacement(ins, op)) {
            // No need to do anything for immediate operands
        } else if (INS_OperandReg(ins, op) == REG_X87) {
            // We don't model x87 accurately in general
            //reportUnhandledCase(*this, "Instr");
        } else {
            assert(INS_OperandIsImplicit(ins, op));
            // Pin classifies the use and update of RSP in various stack
            // operations as "implicit" operands. Although they contribute to
            // OperandCount, OperandIsReg surprisingly returns false.
            // Let's not bother to add RSP to inRegs or outRegs here,
            // since we won't want to consider it as an ordinary register operand.
            // (See handling of stack operations in Decoder::decodeInstr.)
            //
            // Even more weirdly, the use and update of RSI and RDI in MOVSB
            // and similar string-handling instructions are considered
            // implicit operands for which OperandIsReg returns false.
            // Oh well, with ERMSB in Ivy Bridge and later,
            // who knows what's the right way to model these things anyway?

            // [victory] I wish these assertion weren't true, so we could
            //           cleanly check what the implicit register operand is.
            assert(!REG_valid(INS_OperandReg(ins, op)));
            assert(!REG_valid(INS_OperandMemoryBaseReg(ins, op)));
        }
        
    }

    //By convention, we move flags regs to the end
    reorderRegs(inRegs, numInRegs);
    reorderRegs(outRegs, numOutRegs);
}

static inline bool isFlagsReg(uint32_t reg) {
    return (reg == REG_EFLAGS || reg == REG_FLAGS || reg == REG_MXCSR);
}

void Decoder::Instr::reorderRegs(uint32_t* array, uint32_t regs) {
    if (regs == 0) return;
    //Unoptimized bubblesort -- when arrays are this short, regularity wins over O(n^2).
    uint32_t swaps;
    do {
        swaps = 0;
        for (uint32_t i = 0; i < regs-1; i++) {
            if (isFlagsReg(array[i]) && !isFlagsReg(array[i+1])) {
                std::swap(array[i], array[i+1]);
                swaps++;
            }
        }
    } while (swaps > 0);
}

//Helper function
static std::string regsToString(uint32_t* regs, uint32_t numRegs) {
    std::string str = ""; //if efficiency was a concern, we'd use a stringstream
    if (numRegs) {
        str += "(";
        for (uint32_t i = 0; i < numRegs - 1; i++) {
            str += REG_StringShort((REG)regs[i]) + ", ";
        }
        str += REG_StringShort((REG)regs[numRegs - 1]) + ")";
    }
    return str;
}

void Decoder::reportUnhandledCase(Instr& instr, const char* desc) {
    warn("Unhandled case: %s | %s | loads=%d stores=%d inRegs=%d %s outRegs=%d %s", desc, INS_Disassemble(instr.ins).c_str(),
            instr.numLoads, instr.numStores, instr.numInRegs, regsToString(instr.inRegs, instr.numInRegs).c_str(),
            instr.numOutRegs, regsToString(instr.outRegs, instr.numOutRegs).c_str());
}

void Decoder::emitLoad(Instr& instr, uint32_t idx, DynUopVec& uops, uint32_t destReg) {
    assert(idx < instr.numLoads);
    uint32_t op = instr.loadOps[idx];
    uint32_t baseReg = INS_OperandMemoryBaseReg(instr.ins, op);
    uint32_t indexReg = INS_OperandMemoryIndexReg(instr.ins, op);

    if (destReg == 0) destReg = REG_LOAD_TEMP + idx;
	 //if ( lat == 0  ) lat = 1;
	 //if ( portMask == 0 ) portMask = PORTS_015 ; 

    DynUop uop;
    uop.clear();
    uop.rs[0] = baseReg;
    uop.rs[1] = indexReg;
    uop.rd[0] = destReg;
    uop.type = UOP_LOAD;
	//REG_is_gr32((REG)baseReg) || REG_is_gr32((REG)indexReg) || REG_is_gr32((REG)destReg)  ||
    if ( REG_is_gr64((REG)baseReg) || REG_is_gr64((REG)indexReg) ||
		 REG_is_gr64((REG)destReg) ){
		 uop.extraLat = 1   ;
	 }

    uop.portMask = PORT_2 | PORT_3;
    uops.push_back(uop); //FIXME: The interface should support in-place grow...
}

void Decoder::emitStore(Instr& instr, uint32_t idx, DynUopVec& uops, uint32_t srcReg) {
    assert(idx < instr.numStores);
    uint32_t op = instr.storeOps[idx];
    uint32_t baseReg = INS_OperandMemoryBaseReg(instr.ins, op);
    uint32_t indexReg = INS_OperandMemoryIndexReg(instr.ins, op);

    if (srcReg == 0) srcReg = REG_STORE_TEMP + idx;
	 //if ( lat == 0  ) lat = 1;
	 //if ( portMask == 0 ) portMask = PORTS_23 ; 

    uint32_t addrReg;

    //Emit store address uop
    //NOTE: Although technically one uop would suffice with <=1 address register,
    //stores always generate 2 uops. The store address uop is especially important,
    //as in Nehalem loads don't issue after all prior store addresses have been resolved.
    addrReg = REG_STORE_ADDR_TEMP + idx;

    DynUop addrUop;
    addrUop.clear();
    addrUop.rs[0] = baseReg;
    addrUop.rs[1] = indexReg;
    addrUop.rd[0] = addrReg;
    addrUop.portMask = PORT_2 | PORT_3;
    addrUop.type = UOP_STORE_ADDR;

  //   if ( REG_is_gr64((REG)addrReg) || REG_is_gr64((REG)baseReg) || REG_is_gr64((REG)indexReg)  ) 
		// addrUop.extraLat = 1;
	
	addrUop.extraSlots = 1;

    uops.push_back(addrUop);

    //Emit store uop
    DynUop uop;
    uop.clear();
    uop.rs[0] = addrReg;
    uop.rs[1] = srcReg;
    uop.portMask = PORT_4;
    uop.type = UOP_STORE;

    
    if ( INS_MemoryDisplacement( instr.ins ))  
			uop.extraLat += 1 ;

    uops.push_back(uop);
}


void Decoder::emitLoads(Instr& instr, DynUopVec& uops) {
    for (uint32_t i = 0; i < instr.numLoads; i++) {
        emitLoad(instr, i, uops);
    }
}

void Decoder::emitStores(Instr& instr, DynUopVec& uops) {
    for (uint32_t i = 0; i < instr.numStores; i++) {
        emitStore(instr, i, uops);
    }
}

void Decoder::emitFence(DynUopVec& uops, uint32_t lat) {
    DynUop uop;
    uop.clear();
    uop.lat = lat;
    uop.portMask = PORT_4; //to the store queue
    uop.type = UOP_FENCE;
    uops.push_back(uop);
}

void Decoder::emitExecUop(uint32_t rs0, uint32_t rs1, uint32_t rd0, uint32_t rd1, DynUopVec& uops, uint32_t lat, uint8_t ports, uint8_t extraSlots) {
    DynUop uop;
    uop.clear();
    uop.rs[0] = rs0;
    uop.rs[1] = rs1;
    uop.rd[0] = rd0;
    uop.rd[1] = rd1;
    uop.lat = lat;
    uop.type = UOP_GENERAL;
    uop.portMask = ports;
    uop.extraSlots = extraSlots;
    uops.push_back(uop);
}

void Decoder::emitBasicMove(Instr& instr, DynUopVec& uops, uint32_t lat, uint8_t ports) {
	//warn("emitBasicMove Entry uops: %ld %d:%x", uops.size(), lat, ports );

    if (instr.numLoads + instr.numInRegs > 1 || instr.numStores + instr.numOutRegs != 1) {
        reportUnhandledCase(instr, "emitBasicMove");
    }
    //Note that we can have 0 loads and 0 input registers. In this case, we are loading from an immediate, and we set the input register to 0 so there is no dependence
    uint32_t inReg = (instr.numInRegs == 1)? instr.inRegs[0] : 0;
    if (!instr.numLoads && !instr.numStores) { //reg->reg
        emitExecUop(inReg, 0, instr.outRegs[0], 0, uops, lat, ports);
    } else if (instr.numLoads && !instr.numStores) { //mem->reg
        emitLoad(instr, 0, uops, instr.outRegs[0]);// , 3, PORTS_23);
    } else if (!instr.numLoads && instr.numStores) { //reg->mem
        emitStore(instr, 0, uops, inReg);
    } else { //mem->mem
        emitLoad(instr, 0, uops, 0);//, 3, PORTS_23);
        emitStore(instr, 0, uops, REG_LOAD_TEMP /*chain with load*/);
    }

	//warn("emitBasicMove Exit uops: %ld %d:%x", uops.size(), lat, ports );

}

void Decoder::emitXchg(Instr& instr, DynUopVec& uops) {
    if (instr.numLoads) { // mem <-> reg
        assert(instr.numLoads == 1 && instr.numStores == 1);
        assert(instr.numInRegs == 1 && instr.numOutRegs == 1);
        assert(instr.inRegs[0] == instr.outRegs[0]);

        emitLoad(instr, 0, uops);
        emitExecUop(instr.inRegs[0], 0, REG_EXEC_TEMP, 0, uops, 1, PORTS_0156); //r -> temp
        emitExecUop(REG_LOAD_TEMP, 0, instr.outRegs[0], 0, uops, 1, PORTS_0156); // load -> r
        emitStore(instr, 0, uops, REG_EXEC_TEMP); //temp -> out
        if (!INS_LockPrefix(instr.ins)) emitFence(uops, 14); //xchg has an implicit lock prefix (TODO: Check we don't introduce two fences...)
    } else { // reg <-> reg
        assert(instr.numInRegs == 2 && instr.numOutRegs == 2);
        assert(instr.inRegs[0] == instr.outRegs[0]);
        assert(instr.inRegs[1] == instr.outRegs[1]);

        emitExecUop(instr.inRegs[0], 0, REG_EXEC_TEMP, 0, uops, 1, PORTS_0156);
        emitExecUop(instr.inRegs[1], 0, instr.outRegs[0], 0, uops, 1, PORTS_0156);
        emitExecUop(REG_EXEC_TEMP, 0, instr.outRegs[1], 0, uops, 1, PORTS_0156);
    }
}


void Decoder::emitConditionalMove(Instr& instr, DynUopVec& uops, uint32_t lat, uint8_t ports) {
    uint32_t initialUops = uops.size();
    assert(instr.numOutRegs == 1); //always move to reg
    assert(instr.numStores == 0);

    if (instr.numLoads) {
        assert(instr.numLoads == 1);
        assert(instr.numInRegs == 1);
        uint32_t flagsReg = instr.inRegs[0];
        emitExecUop(flagsReg, 0, REG_EXEC_TEMP, 0, uops, lat, ports);
        emitLoad(instr, 0, uops);
        uint32_t numUops = uops.size();
        assert(numUops - initialUops == 2);
        //We need to make the load depend on the result. This is quite crude, but works:
        uops[numUops - 2].rs[1] = uops[numUops - 1].rs[1]; //comparison uop gets source of load (possibly 0)
        uops[numUops - 1].rs[1] = REG_EXEC_TEMP; //load uop is made to depend on comparison uop
        //TODO: Make this follow codepath below + load
    } else {
        assert(instr.numInRegs == 2);
        assert(instr.numOutRegs == 1);
        uint32_t flagsReg = instr.inRegs[1];
        //Since this happens in 2 instructions, we'll assume we need to read the output register
        emitExecUop(flagsReg, instr.inRegs[0], REG_EXEC_TEMP, 0, uops, 1, ports);
        emitExecUop(instr.outRegs[0], REG_EXEC_TEMP, instr.outRegs[0], 0, uops, lat, ports);
    }
}

void Decoder::emitCompareAndExchange(Instr& instr, DynUopVec& uops) {
    emitLoads(instr, uops);

    uint32_t srcs = instr.numLoads + instr.numInRegs;
    uint32_t dsts = instr.numStores + instr.numOutRegs;

    uint32_t srcRegs[srcs + 2];
    uint32_t dstRegs[dsts + 2];
    populateRegArrays(instr, srcRegs, dstRegs);

    assert(srcs == 3);
    assert(dsts == 3);

    //reportUnhandledCase(instr, "XXXX");
    //info("%d %d %d | %d %d %d", srcRegs[0], srcRegs[1], srcRegs[2], dstRegs[0], dstRegs[1], dstRegs[2]);

    uint32_t rflags = dstRegs[2];
    uint32_t rax = dstRegs[1]; //note: can be EAX, etc
    assert(srcRegs[2] == rax); //if this fails, pin has changed the register orderings...

    //Compare destination (first operand) w/ RAX. If equal, copy source (second operand) into destination and set the zero flag; o/w copy destination into RAX
    if (!instr.numLoads) {
        //2 swaps, implemented in 2 stages: first, and all sources with rflags.zf; then or results pairwise. This is pure speculation, but matches uops required.
        emitExecUop(srcRegs[0], rax, REG_EXEC_TEMP, rflags, uops, 1, PORTS_015 | PORT_6); //includes compare
        emitExecUop(srcRegs[1], rflags, REG_EXEC_TEMP+1, 0, uops, 2, PORTS_015 | PORT_6);
        emitExecUop(srcRegs[2], rflags, REG_EXEC_TEMP+2, 0, uops, 2, PORTS_015 | PORT_6);

        emitExecUop(REG_EXEC_TEMP, REG_EXEC_TEMP+1, dstRegs[0], 0, uops, 2, PORTS_015 | PORT_6);
        emitExecUop(REG_EXEC_TEMP+1, REG_EXEC_TEMP+2, dstRegs[1] /*rax*/, 0, uops, 2, PORTS_015 | PORT_6);
    } else {
        //6 uops (so 3 exec), and critical path is 4 (for rax), GO FIGURE
        emitExecUop(srcRegs[0], rax, REG_EXEC_TEMP, rflags, uops, 2, PORTS_015 | PORT_6);
        emitExecUop(srcRegs[1], rflags, dstRegs[0], 0, uops, 2, PORTS_015 | PORT_6); //let's assume we can do a fancy conditional store
        emitExecUop(srcRegs[2], REG_EXEC_TEMP, dstRegs[1] /*rax*/, 0, uops, 2, PORTS_015 | PORT_6); //likewise
    }

    //NOTE: While conceptually srcRegs[0] == dstRegs[0], when it's a memory location they map to different temporary regs

    emitStores(instr, uops);
}



void Decoder::populateRegArrays(Instr& instr, uint32_t* srcRegs, uint32_t* dstRegs) {
    uint32_t curSource = 0;
    for (uint32_t i = 0; i < instr.numLoads; i++) {
        srcRegs[curSource++] = REG_LOAD_TEMP + i;
    }
    for (uint32_t i = 0; i < instr.numInRegs; i++) {
        srcRegs[curSource++] = instr.inRegs[i];
    }
    srcRegs[curSource++] = 0;
    srcRegs[curSource++] = 0;

    uint32_t curDest = 0;
    for (uint32_t i = 0; i < instr.numStores; i++) {
        dstRegs[curDest++] = REG_STORE_TEMP + i;
    }
    for (uint32_t i = 0; i < instr.numOutRegs; i++) {
        dstRegs[curDest++] = instr.outRegs[i];
    }
    dstRegs[curDest++] = 0;
    dstRegs[curDest++] = 0;
}

void Decoder::emitBasicOp(Instr& instr, DynUopVec& uops, uint32_t lat, uint8_t ports, uint8_t extraSlots, bool reportUnhandled) {
    // info("NumLoads %d NUmStores %d\xa", instr.numLoads, instr.numStores);
    emitLoads(instr, uops);

    uint32_t srcs = instr.numLoads + instr.numInRegs;
    uint32_t dsts = instr.numStores + instr.numOutRegs;

    uint32_t srcRegs[srcs + 2];
    uint32_t dstRegs[dsts + 2];
    populateRegArrays(instr, srcRegs, dstRegs);

    if (reportUnhandled && (srcs > 2 || dsts > 2)) 
	reportUnhandledCase(instr, "emitBasicOp"); //We're going to be ignoring some dependencies

    emitExecUop(srcRegs[0], srcRegs[1], dstRegs[0], dstRegs[1], uops, lat, ports, extraSlots);

    emitStores(instr, uops);
}

void Decoder::emitChainedOp(Instr& instr, DynUopVec& uops, uint32_t numUops, uint32_t* latArray, uint8_t* portsArray) {
    emitLoads(instr, uops);

    uint32_t srcs = instr.numLoads + instr.numInRegs;
    uint32_t dsts = instr.numStores + instr.numOutRegs;

    uint32_t srcRegs[srcs + 2];
    uint32_t dstRegs[dsts + 2];
    populateRegArrays(instr, srcRegs, dstRegs);

    assert(numUops > 1);
    //if (srcs != numUops + 1) reportUnhandledCase(instr, "emitChainedOps");
    assert(srcs + 2 >= numUops + 1); // note equality is not necessary in case one or more operands are immediates

    emitExecUop(srcRegs[0], srcRegs[1], REG_EXEC_TEMP, 0, uops, latArray[0], portsArray[0]);
    for (uint32_t i = 1; i < numUops-1; i++) {
        emitExecUop(REG_EXEC_TEMP, srcRegs[i+1], REG_EXEC_TEMP, 0, uops, latArray[i], portsArray[i]);
    }
    emitExecUop(REG_EXEC_TEMP, srcRegs[numUops-1], dstRegs[0], dstRegs[1], uops, latArray[numUops-1], portsArray[numUops-1]);

    emitStores(instr, uops);
}

//Some convert ops are implemented in 2 uops, even though they could just use one given src/dst reg constraints
void Decoder::emitConvert2Op(Instr& instr, DynUopVec& uops, uint32_t lat1, uint32_t lat2, uint8_t ports1, uint8_t ports2, uint8_t extraSlots1) {
    if (instr.numStores > 0 || instr.numLoads > 1 || instr.numOutRegs != 1 || instr.numLoads + instr.numInRegs != 1) {
        reportUnhandledCase(instr, "convert");
    } else {
        //May have single load, has single output
        uint32_t src;
        if (instr.numLoads) {
            emitLoads(instr, uops);
            src = REG_LOAD_TEMP;
        } else {
            src = instr.inRegs[0];
        }
        uint32_t dst = instr.outRegs[0];
        emitExecUop(src, 0, REG_EXEC_TEMP, 0, uops, lat1, ports1, extraSlots1);
        if(lat2!=0)
            emitExecUop(REG_EXEC_TEMP, 0, dst, 0, uops, lat2, ports2);
    }
}


void Decoder::emitMul(Instr& instr, DynUopVec& uops) {
    uint32_t dsts = instr.numStores + instr.numOutRegs;
    if (dsts == 3) {
        emitLoads(instr, uops);

        uint32_t srcs = instr.numLoads + instr.numInRegs;

        uint32_t srcRegs[srcs + 2];
        uint32_t dstRegs[dsts + 2];
        populateRegArrays(instr, srcRegs, dstRegs);

        assert(srcs <= 2);



        emitExecUop(srcRegs[0], srcRegs[1], dstRegs[0], REG_EXEC_TEMP, uops, 1, PORT_6 | PORT_0); //3 in both
        emitExecUop(srcRegs[0], srcRegs[1], dstRegs[1], REG_EXEC_TEMP+1, uops, 4, PORT_1,3);
        emitExecUop(REG_EXEC_TEMP, REG_EXEC_TEMP+1, dstRegs[2], 0, uops, 1, PORTS_0156);

        emitStores(instr, uops);
    } else {
        uint32_t width = INS_OperandWidth(instr.ins, 1);
        uint32_t lat = 0;
        uint32_t extraSlots = 0;
        switch (width) {
            case 8:
                //lat = 3;
                //break;
            case 16:
            case 32:
                //lat = 3;
                //extraSlots = 1;
                //break;
            case 64:
                lat = 4;
                // extraSlots = 3;
                break;
            default:
                panic("emitMul: Invalid reg size");
        }
        //printf("Good %d, %d\n",lat, extraSlots);
        emitBasicOp(instr, uops, lat, PORT_0|PORT_1|PORT_5|PORT_6, extraSlots);
    }
}

void Decoder::emitDiv(Instr& instr, DynUopVec& uops) {
    uint32_t srcs = instr.numLoads + instr.numInRegs;
    uint32_t dsts = instr.numStores + instr.numOutRegs;

    /* div and idiv are microsequenced, with a variable number of uops on all ports, and have fixed
     * input and output regs (rdx:rax is the input, rax is the quotient and rdx is the remainder).
     * Also, the number of uops and latency depends on the data. We approximate this with a 4-uop
     * sequence that sorta kinda emulates the typical latency.
     */

    uint32_t srcRegs[srcs + 2];
    uint32_t dstRegs[dsts + 2];
    populateRegArrays(instr, srcRegs, dstRegs);

    //assert(srcs == 3); //there is a variant of div that uses only 2 regs --> see below
    //assert(dsts == 3);
    assert(instr.numInRegs > 1);

    uint32_t width = INS_OperandWidth(instr.ins, 1);
    uint32_t lat = 0;
    switch (width) {
        case 8:
            lat = 24;//15, 
            break;
        case 16:
            lat = 24;//19
            break;
        case 32:
            lat = 50;//23
            break;
        case 64:
            lat = 50;//63
            break;
        default:
            panic("emitDiv: Invalid reg size");
    }
    
    if (srcs == 3 && dsts == 3) {
        emitLoads(instr, uops);

        emitExecUop(srcRegs[0], srcRegs[1], REG_EXEC_TEMP, 0, uops, lat/2.0, PORTS_0156);//, extraSlots);
        emitExecUop(srcRegs[0], srcRegs[2], REG_EXEC_TEMP+1, 0, uops, lat/2.0, PORTS_0156);//, extraSlots);
        emitExecUop(REG_EXEC_TEMP, REG_EXEC_TEMP+1, dstRegs[0], dstRegs[1], uops, 1, PORTS_0156); //quotient and remainder
        emitExecUop(REG_EXEC_TEMP, REG_EXEC_TEMP+1, dstRegs[2], 0, uops, 1, PORTS_0156); //flags

        emitStores(instr, uops);
    } else if (srcs <= 2 && dsts <= 2) {
        emitBasicOp(instr, uops, lat, PORTS_0156);
    } else {
        reportUnhandledCase(instr, "emitDiv");
    }
}

//Helper function
static bool dropRegister(uint32_t targetReg, uint32_t* regs, uint32_t& numRegs) {
    for (uint32_t i = 0; i < numRegs; i++) {
        uint32_t reg = regs[i];
        if (reg == targetReg) {
            //Shift rest of regs
            for (uint32_t j = i; j < numRegs - 1; j++) regs[j] = regs[j+1];
            numRegs--;
            return true;
        }
    }
    return false;
}

void Decoder::dropStackRegister(Instr& instr) {
    bool dropIn = dropRegister(REG_RSP, instr.inRegs, instr.numInRegs);
    bool dropOut = dropRegister(REG_RSP, instr.outRegs, instr.numOutRegs);
    if (!dropIn && !dropOut) /*reportUnhandledCase(instr, "dropStackRegister (no RSP found)")*/;
    else reportUnhandledCase(instr, "dropStackRegister (RSP found)");
}


bool Decoder::decodeInstr(INS ins, DynUopVec& uops) {
    uint32_t initialUops = uops.size();
    bool inaccurate = false;
    xed_category_enum_t category = (xed_category_enum_t) INS_Category(ins);
    xed_iclass_enum_t opcode = (xed_iclass_enum_t) INS_Opcode(ins);

    Instr instr(ins);

    bool isLocked = false;
    if (INS_LockPrefix(instr.ins)) {
        isLocked = true;
        emitFence(uops, 0); //serialize the initial load w.r.t. all prior stores
    }

    /*//David, tester
    switch (opcode) {
        case XO(PXOR):
            printf("PXOR %d\n",category);
            //perror("Hit ANDNPD OUT");
            break;
        default:
            break;
    }*/


  //   if (opcode == XO(STR)){
  //       printf("STR %d\n",category); // 66
		// exit(0);
  //   }

	//reportUnhandledCase(instr, "Debugging");


    switch (category) {

        case 56: //apparently setB is here!
        {

            uint32_t opLat = 1;
            uint32_t extraSlots = 0;
            uint8_t ports = PORTS_015;

            switch (opcode) {

                case XO(SETB):
                case XO(SETBE): // *same as other but occupy more the ports. do not know how to simulate
                case XO(SETL):
                case XO(SETLE):
                case XO(SETNB):
                case XO(SETNBE)://*same as above
                case XO(SETNL):
                case XO(SETNLE):
                case XO(SETNO):
                case XO(SETNP):
                case XO(SETNS):
                case XO(SETNZ):
                case XO(SETO):
                case XO(SETP):
                case XO(SETS):
                case XO(SETZ):
                    opLat = 1;
                    ports = PORT_0 | PORT_5;
                    extraSlots = 1;
                    break;
                case XO(LZCNT):
                    opLat = 1;
                    ports = PORT_1;
                    break;
                    //TODO: EXTRQ, INSERTQ, 
                default: {} //BT, BTx, SETcc ops are 1 cycle
                

            }
            emitBasicOp(instr, uops, opLat, ports, extraSlots);

            break;

        }



        //NOPs are optimized out in the execution pipe, but they still grab a ROB entry
        case XC(NOP):
        case XC(WIDENOP):
            emitExecUop(0, 0, 0, 0, uops, 1, PORTS_0156); //David, NOP doesn't use any port. So i'm simulating any.
            break;

         /* Moves */
        case XC(DATAXFER):
            switch (opcode) {
                case XO(BSWAP):
                    emitBasicMove(instr, uops, 1, PORT_1 | PORT_5);
                    break;
		
		case XO(MOVQ): //David: ZSim 74 cycles?
			emitBasicMove(instr, uops, 1, PORTS_015);
			break;

                
                
                case XO(MOVQ2DQ):
                    { 
                        uint32_t latsc[] = {1, 1};
                        uint8_t portsc[] = {PORT_0, PORT_1|PORT_5};
                        emitChainedOp(instr, uops, 2, latsc, portsc);
                    }
                    break;


                
                case XO(MOVSX): 
                case XO(MOVSXD):
                case XO(MOVZX):
                case XO(MOV): //
                    emitBasicMove(instr, uops, 1, PORTS_0156);
                    break;
                case XO(MOVAPD): // the reason I seperate this catogory from above is that in reality it use less port but we need to maintaon wrong port in simulation to achieve right BW
                case XO(MOVAPS):
                case XO(MOVDQA): 
                case XO(MOVDQU):
                    emitBasicMove(instr, uops, 1, PORTS_0156);

                    break;
                case XO(MOVDQ2Q): 
                    { 
                        uint32_t latsc[] = {1, 1};
                        uint8_t portsc[] = {PORT_0|PORT_5, PORT_0|PORT_5};
                        emitChainedOp(instr, uops, 2, latsc, portsc);
                    }

                    break;
            
                case XO(MOVUPS): // approximate. 0 Uops 
                case XO(MOVUPD):
                    emitBasicMove(instr, uops, 1, PORTS_0156);
                    break;
                case XO(MOVSS):
                case XO(MOVSD): 	//Maybe crashes spec
                case XO(MOVSD_XMM):
                case XO(MOVHLPS): //
                case XO(MOVLHPS): //
                case XO(MOVSHDUP): //
                case XO(MOVSLDUP):
        			emitBasicMove(instr, uops, 1, PORT_5);
        			break;
		        case XO(MOVD):
                case XO(MOVDDUP):
                    emitBasicMove(instr, uops, 1, PORT_5);
                    break;
                case XO(MOVHPS):
                case XO(MOVHPD):
                case XO(MOVLPS):
                case XO(MOVLPD):
                    //A bit unclear... could be 2 or 3 cycles, and current microbenchmarks are not enough to tell
                    emitBasicOp(instr, uops, /*2*/ 3, PORT_1); //David, 1 Memory
                    break;
                case XO(MOVMSKPS):
                case XO(MOVMSKPD):
                    emitBasicMove(instr, uops, 1, PORT_0); //David, this should be 1
                    break;
                case XO(XCHG): //David, 3 uops 1.5 cycles, interesting how it is done.
                    emitXchg(instr, uops);
                    break;
                default:
                    //TODO: MASKMOVQ, MASKMOVDQ, MOVBE (Atom only), MOVNTxx variants (nontemporal), MOV_CR and MOV_DR (privileged?), VMOVxxxx variants (AVX)
                    inaccurate = true;
                    //David, inaccurate
                    //printf("Innacurate: instruction %d, category: %d\n", opcode, category);
                   emitBasicMove(instr, uops, 1, PORTS_015);
            }
            break;

        case XC(CMOV):
            emitConditionalMove(instr, uops, 1, PORT_0 | PORT_6);
            break;
        case XC(FCMOV):
		      inaccurate = true;
    		//Should be move, modeling as basicop
            	 //emitConditionalMove(instr, uops, 2, PORT_0 | PORT_5); //David 1,0
    		emitBasicOp(instr, uops, 1, PORT_0 | PORT_5);
		      //INACCUOP
        	break;

        /* Barebones arithmetic instructions */
        case XC(BINARY):
            {
                if (opcode == XO(ADC)){
                    emitBasicOp(instr, uops, 1, PORTS_06);
                }
                else if (opcode == XO(SBB)) {
                    emitBasicOp(instr, uops, 1, PORTS_06);
                } else if (opcode == XO(MUL) || opcode == XO(IMUL)) {
                    emitMul(instr, uops);
                } else if (opcode == XO(DIV) || opcode == XO(IDIV)) {
                    emitDiv(instr, uops);
                } else {
                    //ADD, SUB, NEG, CMP, DEC, INC are 1 cycle
                    emitBasicOp(instr, uops, 1, PORTS_0156);
                }
            }
            break;
        case XC(BITBYTE):
            {
                uint32_t opLat = 1;
                uint32_t extraSlots = 0;
                uint8_t ports = PORTS_015;
                switch (opcode) {
                    case XO(BSF):
                    case XO(BSR):
                        opLat = 3;
                        ports = PORT_1;
            			extraSlots = 2; 
                                    break;
                	case XO(BT):

                        opLat = 1;
                        ports = PORT_0 | PORT_6;
                        // extraSlots = 1;
                        break;

                    case XO(BTC):
                    case XO(BTR):
                    case XO(BTS):
                        opLat = 1;
                        ports = PORT_0 | PORT_6;
                        extraSlots = 1;
                        break;

                	case XO(SETB): //
                        opLat = 1;
                        ports = PORT_0 | PORT_6;
                        exit(0);
                        break;


                	case XO(SETBE):
                	case XO(SETL):
                	case XO(SETLE):
                	case XO(SETNB):
                	case XO(SETNBE):
                	case XO(SETNL):
                	case XO(SETNLE):
                	case XO(SETNO):
                	case XO(SETNP):
                	case XO(SETNS):
                	case XO(SETNZ):
                	case XO(SETO):
                	case XO(SETP):
                	case XO(SETS):
                	case XO(SETZ):
                		opLat = 1;
                        ports = PORT_0 | PORT_5;
                		extraSlots = 1;
                        break;
                    case XO(LZCNT):
                        opLat = 1;
                        ports = PORT_1;
                        break;
                        //TODO: EXTRQ, INSERTQ, 
                    default: {} //BT, BTx, SETcc ops are 1 cycle
                }


                emitBasicOp(instr, uops, opLat, ports, extraSlots);
            }
            break;
        case XC(LOGICAL):
            {
             switch (opcode) {
                //David, LOGICAL
                case XO(ANDNPD):
                case XO(ANDPD):
                case XO(ORPD):
                case XO(ORPS):
                case XO(XORPS):
                case XO(XORPD):
                case XO(PANDN):
                case XO(PXOR):
                    emitBasicOp(instr, uops, 1, PORTS_015);
                    break;
                case XO(PTEST):
                {
                    uint32_t lats[] = {1, 1};
                    uint8_t ports[] = {PORT_0, PORT_5};
                    emitChainedOp(instr, uops, 2, lats, ports);
                    break;
                }

                default:
                    //AND, OR, XOR, TEST are 1 cycle
                    //David: PAND, , POR,  too -- pandn used to be here
                    emitBasicOp(instr, uops, 1, PORTS_0156);
                    break;
                }
            }
            break;
        case XC(ROTATE):
            {
                uint32_t opLat = 1; //ROR, ROL 1 cycle //David: no 2 uops
                if (opcode == XO(RCR) || opcode == XO(RCL)){ //3 uops, missing 1
							uint32_t latsc[] = {1, 1};
							uint8_t portsc[] = {PORT_0, PORT_1|PORT_5};
							emitChainedOp(instr, uops, 2, latsc, portsc);
					}
					//opLat = 2; //David, simplistic, there are many cases.
				else
                { //David: 2 uops, 1 cycle latency
					
                    emitBasicOp(instr, uops, opLat, PORT_0 | PORT_6,1);


				}
            }
            break;
        case XC(SHIFT): // 59
            {
                if (opcode == XO(SHLD) || opcode == XO(SHRD) ) {//
                    // uint32_t lats[] = {2, 1}; //David: Check behavior when using flags. Chained?
                    // uint8_t ports[] = {PORT_2 , PORT_2};
                    // emitChainedOp(instr, uops, 2, lats, ports);
                    emitBasicOp(instr, uops, 3, PORT_2,2);
                } else {
				uint32_t opLat = 1; //(SHR) (SHL) (SAR) are 1 cycle, SHRD 1 and SHLD are 1 uop only, maybe non pipelined?
				emitBasicOp(instr, uops, opLat, PORT_0 | PORT_5);
                }
            }
            break;
        case XC(DECIMAL): //pack/unpack BCD, these seem super-deprecated
            {
                uint32_t opLat = 1;
                switch (opcode) {
                    case XO(AAA):
                    case XO(AAS):
                    case XO(DAA):
                    case XO(DAS):
                        opLat = 4; //David, 3
                        break;
                    case XO(AAD):
                        opLat = 2; //David, 15
                        break;
                    case XO(AAM):
                        opLat = 20;
                        break;
                    default:
			inaccurate = true;
			printf("Innacurate: instruction %d, category: %d\n", opcode, category);
                	// panic("Invalid opcode for this class");
                }
                emitBasicOp(instr, uops, opLat, PORTS_015);
            }
            break;
        case XC(FLAGOP):
            switch (opcode) {
                case XO(LAHF):
			        emitBasicOp(instr, uops, 2, PORT_0 | PORT_6,1);
			break;
                case XO(SAHF): 
                    emitBasicOp(instr, uops, 2, PORT_0 | PORT_5,3); // BW is important here
                    break;
                case XO(CLC):
			        emitExecUop(0, 0, 0, 0, uops, 1, PORTS_015 | PORTS_23 | PORT_4); //David, CLC doesn't use any port. So I'm simulating any.
			break;
                case XO(STC):
                case XO(CMC):
                    emitBasicOp(instr, uops, 1, PORTS_015 | PORT_6);
                    break;
                case XO(CLD): //David: I don't understand this, added reciprocal, 2uops, throughput? uop order? chained? PORT_5 only takes 1
		        case XO(STD):
                    emitExecUop(0, 0, REG_EXEC_TEMP, 0, uops, 1, PORT_0 | PORT_1 | PORT_5 | PORT_6, 15);
                    // emitExecUop(REG_EXEC_TEMP, 0, REG_RFLAGS, 0, uops, 1, PORT_0 | PORT_6);
                    break;
                default:
                    inaccurate = true;
                    //David, inaccurate
                    // printf("Innacurate: instruction %d, category: %d\n", opcode, category);
		    //emitBasicOp(instr, uops, 1, PORTS_015);	
		    emitExecUop(0, 0, 0, 0, uops, 1, PORTS_015 | PORTS_23 | PORT_4 | PORT_6 | PORT_7);
            }
            break;

        case XC(SEMAPHORE): //atomic ops, these must involve memory
            //reportUnhandledCase(instr, "SEM");
            //emitBasicOp(instr, uops, 1, PORTS_015);

            switch (opcode) {
                case XO(CMPXCHG):
                case XO(CMPXCHG8B):
                //case XO(CMPXCHG16B): //not tested...
                    emitCompareAndExchange(instr, uops);
                    break;
                case XO(XADD)://David: 3uops, 2 lat. Missing 1 uop
                    {
                        uint32_t lats[] = {1, 1};
                        uint8_t ports[] = {PORTS_0156, PORTS_0156};
                        emitChainedOp(instr, uops, 2, lats, ports);
                    }
                    break;
                default:
                    inaccurate = true;
                    //David, inaccurate
                    printf("Innacurate: instruction %d, category: %d\n", opcode, category);
		  //emitBasicOp(instr, uops, 1, PORTS_015);
		  INACCUOP
            }
            break;

        /* FP, SSE and other extensions */
		//David, not very accurate, lacks the related memory operations
        case /*XC(X)87_ALU*/ XC(X87_ALU):
			switch (opcode) {
                
                  

                case XO(FSUBP): // wrong 
                case XO(FSUBR): 
                    emitBasicOp(instr, uops, 3, PORT_6); //3, page 630 intel appendix c

                    break;
                case XO(FADDP):
				case XO(FADD):
                case XO(FSUB):
                
                    emitBasicOp(instr, uops, 3, PORT_5); //3, page 630 intel appendix c
                    break;
				case XO(FMUL):
				case XO(FMULP):
					emitBasicOp(instr, uops, 5, PORT_0); //5, page 630 intel appendix c
					break;
                
                    


				case XO(FDIV):
                case XO(FDIVP):
                case XO(FDIVRP):
				case XO(FDIVR):
                case XO(FSQRT):
					emitBasicOp(instr, uops, 14, PORT_0); //
					break;

                case XO(FCOM):
                case XO(FUCOM):
				case XO(FTST):
                    emitBasicOp(instr, uops, 1, PORT_5);
					break;

				case XO(FCOMI): 
                    emitBasicOp(instr, uops, 1, PORT_0);
                    break;

                case XO(FUCOMIP):
                case XO(FUCOMP):
                case XO(FUCOMPP):
                    emitBasicOp(instr, uops, 1, PORT_0); // not accuate

                    break;

                case XO(FUCOMI):
					{
                        emitBasicOp(instr, uops, 1, PORT_0);
						// uint32_t latsc[] = {1, 1, 1};
						// uint8_t portsc[] = {PORTS_015, PORTS_015, PORTS_015};
						// emitChainedOp(instr, uops, 3, latsc, portsc);
					}
					break;

                case XO(FSTP):
                    emitBasicOp(instr, uops, 1, PORT_0 | PORT_5,1);
                    break;

                case XO(FABS):
				case XO(FCHS):
				case XO(FST):
				
				case XO(FNOP):
				case XO(FFREE):
				case XO(FDECSTP): 
				case XO(FINCSTP):
				case XO(FWAIT):
					emitBasicOp(instr, uops, 1, PORT_0 | PORT_5);
					break;

                case XO(FLD):
                case XO(FNSTSW): //David: Complex ports 015, 2 uops
					emitBasicOp(instr, uops, 1, PORT_0);
					break;

                case XO(FFREEP): //David: 2uops
					{
						uint32_t latsc[] = {1, 1};
						uint8_t portsc[] = {PORT_5 | PORT_0, PORT_5 | PORT_0};
						emitChainedOp(instr, uops, 2, latsc, portsc);
					}
                    //emitBasicOp(instr, uops, 2, PORT_5, 1);
					break;

                case XO(FXAM): //David: Flag operation
					{
						uint32_t latsc[] = {1, 1};
						uint8_t portsc[] = {PORT_5, PORT_5};
						emitChainedOp(instr, uops, 2, latsc, portsc);
					}
                    //emitBasicOp(instr, uops, 2, PORT_1, 1);
					break;

                //Load constants
                case XO(FLD1): //No idea about latency
					emitBasicOp(instr, uops, 2, PORT_0 | PORT_1);
					break;
				case XO(FXCH): // very inaccurate implementation ignore Uops
					// inaccurate = true;
					emitBasicOp(instr, uops, 1, PORTS_0156);
					//INACCUOP
					break;

				case XO(FNCLEX): //David: Complex port 015
					emitBasicOp(instr, uops, 22, PORT_0, 21);
					break;
				case XO(FNINIT): //David: Ports 0,1,5,
					emitBasicOp(instr, uops, 1, PORT_6, 1);
                    // {
                    //     uint32_t latsc[] = {30, 45};
                    //     uint8_t portsc[] = {PORTS_0156, PORT_5};
                    //     emitChainedOp(instr, uops, 2, latsc, portsc);
                    // }

					break;

				default:
                    inaccurate = true;
                    //David, inaccurate
                    printf("Innacurate: instruction %d, category: %d\n", opcode, category);
						  //emitBasicOp(instr, uops, 1, PORT_0 | PORT_1|PORT_5);
						 INACCUOP
			}
            break;

        case XC(SSE): // sse is 61
            {
                //TODO: Multi-uop BLENDVXX, DPXX

                uint32_t lat = 2;
                uint8_t ports = PORTS_015;
                uint8_t extraSlots = 0;
			    bool simple = true;

                    switch (opcode) { // for now this is valid
                        case XO(PCMPESTRI):
                        emitBasicOp(instr, uops, 4, PORT_0, 3);
                        emitBasicOp(instr, uops, 4, PORT_5, 3);
                        emitBasicOp(instr, uops, 4, PORT_1, 1);
                        emitBasicOp(instr, uops, 4, PORT_6, 1);

                        break;
                    case XO(PCMPESTRM):
                        emitBasicOp(instr, uops, 4, PORT_0, 3);
                        emitBasicOp(instr, uops, 5, PORT_5, 4);
                        emitBasicOp(instr, uops, 4, PORT_1, 1);
                        emitBasicOp(instr, uops, 4, PORT_6, 1);
                        break;
                    case XO(PCMPISTRM):
                    case XO(PCMPISTRI):
                        emitBasicOp(instr, uops, 3, PORT_0, 2);
                        break;
                    


                    case XO(PAVGB):
                    case XO(PAVGW):
                        lat = 1;
                        ports = PORT_0 | PORT_1 ;
                        break;

                    case XO(PACKSSDW):
                    case XO(PACKSSWB):
                    case XO(PACKUSDW):
                    case XO(PACKUSWB):
                    case XO(INSERTPS):
                        lat = 1;
                        ports = PORT_5;
                        break;

                    
                    case XO(ADDPD):
                    case XO(ADDPS):
					case XO(ADDSD):
                    case XO(ADDSS):
                    case XO(SUBPD):
                    case XO(SUBPS):
                    case XO(SUBSD):
                    case XO(SUBSS):
                    case XO(ADDSUBPD):
                    case XO(ADDSUBPS):
						  // rsv: some agner in avg.. just to check if something change
							lat = 4;
							ports = PORTS_01;
							break;
					case XO(CMPPD):
                    case XO(CMPPS):
                    case XO(CMPSD):
                    case XO(CMPSS):
                    case XO(CMPSD_XMM):    //Seems to be the same, must verify

                        lat = 4;
                        ports = PORT_0 | PORT_1;
                        break;

					case XO(MAXPD): //
                    case XO(MAXPS): //
                    case XO(MAXSD): //
                    case XO(MAXSS): //
                    case XO(MINPD):
                    case XO(MINPS):
                    case XO(MINSD):
                    case XO(MINSS):
                        lat = 4;
                        ports = PORT_0 | PORT_1;
                        break;

                    case XO(ROUNDPD): //
                    case XO(ROUNDPS): //
                        extraSlots = 1;
                        lat = 8;
                        ports = PORT_0 | PORT_1;
                        break;

                    case XO(CRC32):
                        lat = 3;
                        ports = PORT_1;
                        break;


					case XO(POPCNT):
						lat = 3;
                        ports = PORT_1;
						extraSlots =  1; //David: Doesn't seem to be pipelined
                        break;

                    case XO(SHUFPS): //
                    case XO(SHUFPD): //
                    case XO(UNPCKHPD): //Matches doc
                    case XO(UNPCKHPS): //Matches doc
                    case XO(UNPCKLPD): //Matches doc
                    case XO(UNPCKLPS): //Matches doc
                        lat = 1;
                        ports = PORT_5;
                        break;

                    case XO(COMISD):
                    case XO(COMISS):
                    case XO(UCOMISD):
                    case XO(UCOMISS):
                        lat = 1;
                        ports = PORT_0;

                        /*lat = 1+1; //David: 1+2; writes rflags, always crossing xmm -> int domains
                        ports = PORT_0 | PORT_1; //David, 1
						extraSlots = 1;*/
						// {
						// 	simple = false;
						// 	uint32_t latsc[] = {1, 1};
						// 	uint8_t portsc[] = {PORT_0|PORT_1, PORT_0|PORT_1};
						// 	emitChainedOp(instr, uops, 2, latsc, portsc);
						// }
                        break;

                    case XO(DIVPS):
                    case XO(DIVSS):
                        lat = 11; //from mubench //rsv agner
                        ports = PORT_0; //rsv   //non-pipelined
                        //extraSlots = lat - 12;
                        break;

                    case XO(DIVPD):
                    case XO(DIVSD):
                        lat = 13; //rom mubench //rsv agner
                        ports = PORT_0; //rsv   //non-pipelined
                        //extraSlots = lat - 12;
                        break;

					case XO(ROUNDSD):
                    case XO(ROUNDSS):
						lat = 8;
						ports = PORT_0 | PORT_1;
						// extraSlots = (2*lat - 1); //David: Should be pipelined
						break;

					case XO(PHMINPOSUW):
                        lat = 1;
                        ports = PORT_0;
                        break;
		    case XO(MULSD): //Mulxx matches documentation
                    case XO(MULPD):
    		    case XO(MULSS):
                    case XO(MULPS):
						//rsv agner
						lat = 4;
			   		ports= PORTS_01;
                        //extraSlots= lat-1;
						break;

                    case XO(PSADBW):
                        lat = 3;
                        ports = PORT_5;

                        break;
                    case XO(PMULLD):
                        lat = 10;
                        ports = PORT_0 | PORT_1;
                        break;

                    case XO(RSQRTPS): //
					case XO(RCPPS): // 

                        lat = 4;
                        ports = PORT_0;
                        break;




					//, PMul
					case XO(PMULHW): //
					case XO(PMULLW): //
					case XO(PMULHUW): //
					case XO(PMULHRSW): //
					
					case XO(PMULDQ): //
					case XO(PMULUDQ): //
					case XO(PMADDWD): //
					case XO(PMADDUBSW): //

					

                        lat = 5;
                        ports = PORT_0 | PORT_1;
                        break;

				case XO(RSQRTSS): //
				case XO(RCPSS): // 
					    lat = 4;
                        ports = PORT_0;
					    extraSlots = lat - 1; //David: non pipelined
                        break;

                    case XO(SQRTPD):
                        lat = 5;
                        ports = PORT_0;
                        extraSlots = lat-1; //unpiped
                        break;

                    case XO(SQRTSD):
                        lat = 13;
                        ports = PORT_0;
                        extraSlots = lat-1; //unpiped
                        break;


                    case XO(SQRTSS):
                        lat = 12;
                        ports = PORT_0;
                        extraSlots = lat-1; //unpiped
                        break;


                    
                    case XO(SQRTPS): //
                    
                    
                        lat = 3; //from mubench
                        ports = PORT_0;
                        extraSlots = lat-1; //unpiped
                        break;

                    //Packed arith; these are rare, so I'm implementing only what I've seen used (and simple variants)
                    case XO(PADDB):
                    case XO(PADDD):
                    case XO(PADDQ):
                    case XO(PADDW):
                        lat = 1;
                        ports = PORT_0 | PORT_1 | PORT_5;
                        break;

                    case XO(PALIGNR):
                        lat = 1;
                        ports = PORT_5;
                        break;

                    case XO(PCMPEQB):
                    case XO(PCMPEQD):
                    case XO(PCMPEQQ):
                    case XO(PCMPEQW):
                    case XO(PCMPGTB):
                    case XO(PCMPGTD):
                    case XO(PCMPGTW):
                        lat = 1;
                        ports = PORT_0 | PORT_1;
                        break;

                    case XO(PMAXUW):
                    case XO(PMAXUD):
                    case XO(PMAXUB):
                    case XO(PMAXSD):
                    case XO(PMAXSB):
                    case XO(PMAXSW):
                    case XO(PMINUW):
                    case XO(PMINUD):
                    case XO(PMINUB):
                    case XO(PMINSD):
                    case XO(PMINSB):
                    case XO(PMINSW):
                    case XO(PSIGNB):
                    case XO(PSIGNW):
                    case XO(PSIGND):

                    case XO(PSUBSB): //
                    case XO(PSUBSW): //
                    case XO(PSUBUSB): //
                    case XO(PSUBUSW): //

                        ports = PORT_0 | PORT_1;
                        lat =1;
                        break;

                    case XO(PSUBB):
                    case XO(PSUBD):
                    case XO(PSUBQ):
                    case XO(PSUBW):
                        ports = PORT_0 | PORT_1 | PORT_5;
                        lat =1;

                        break;

                    case XO(PADDSB):
                    case XO(PADDSW):
                    case XO(PADDUSB):
                    case XO(PADDUSW):
                    
                    
                    
                    
                    

                    case XO(PUNPCKHBW): //
                    case XO(PUNPCKHDQ): //
                    case XO(PUNPCKHQDQ): //
                    case XO(PUNPCKHWD): //
                    case XO(PUNPCKLBW): //
                    case XO(PUNPCKLDQ): //
                    case XO(PUNPCKLQDQ): //
                    case XO(PUNPCKLWD): //

                    case XO(PSHUFB): //
                    case XO(PSHUFD): //
                    case XO(PSHUFHW): //
                    case XO(PSHUFLW): //
                    
					
					

					

					case XO(PABSB): //David: PABSx doesn't have dependencies
					case XO(PABSW):
					case XO(PABSD):
                    case XO(PBLENDW):
                        lat = 1;
                        ports = PORT_5;
                        break;

					case XO(PHADDW): //David: 3 uops, missing 1
					case XO(PHADDD):
                    case XO(PHADDSW):
					case XO(PHSUBW):
					case XO(PHSUBD):
                    case XO(PHSUBSW):
					case XO(PBLENDVB):
						// {
						// 	simple = false;
						// 	uint32_t latsc[] = {1, 1};
						// 	uint8_t portsc[] = {PORT_1|PORT_5, PORT_1|PORT_5};
						// 	emitChainedOp(instr, uops, 2, latsc, portsc);
						// }
						lat = 3;
                        ports = PORT_0 | PORT_1 | PORT_5;
                        break;

					case XO(PINSRB): //David: 2 uops, 1 cycle
					case XO(PINSRD):
                    case XO(PINSRQ):
					case XO(PINSRW):
                        lat = 2;
                        ports = PORT_5;
                        break;
					// {
     //                    uint32_t latsc[] = {1, 1};
     //                    uint8_t portsc[] = {PORT_5, PORT_5};
     //                    emitChainedOp(instr, uops, 2, latsc, portsc);
     //                }
                  break;

                case XO(PCMPGTQ): //weeeird, only packed comparison that's done differently
                        lat = 3;
                        ports = PORT_0 | PORT_1| PORT_5;
                        break;

                    case XO(PMOVSXBD):
                    case XO(PMOVSXBQ):
                    case XO(PMOVSXWD):
                    case XO(PMOVSXWQ):
                    case XO(PMOVSXDQ):
                    case XO(PMOVSXBW):
                    case XO(PMOVZXBW):
                    case XO(PMOVZXBD):
                    case XO(PMOVZXBQ):
                    case XO(PMOVZXWD):
                    case XO(PMOVZXWQ):
                    case XO(PMOVZXDQ):

                        lat =1;
                        ports = PORT_5;
                        break;


                    case XO(PSLLQ):
                    case XO(PSLLW):
                        // emitBasicOp(instr, uops, 1, PORT_0 | PORT_1);
                        lat =1;
                        ports = PORT_5;

                    // {
                    //         simple = false;
                    //         uint32_t latsc[] = {1, 1};
                    //         uint8_t portsc[] = {PORT_0 | PORT_1, PORT_5};
                    //         emitChainedOp(instr, uops, 2, latsc, portsc);
                    // }
                        break;


                    case XO(PMOVMSKB):
                        lat = 3; //David 2+2
                        ports = PORT_0;
                        break;

                    case XO(PSRLD):
                    case XO(PSRAW):
                    case XO(PSRAD):
                        lat = 1; //David 2+2
                        ports = PORT_5;

                        break;



                    //David, taken from stream. There are others.
                    case XO(PSLLD): //
                    
                    
                    case XO(PSRLDQ):
                    case XO(PSRLQ)://
                    case XO(PSRLW):
                        lat = 1;
                        ports = PORT_5;
                        break;
                    case XO(PSLLDQ):
                        lat = 1;
                        ports = PORT_5;
                        break;


                    //case XO(FXTRACT): //Benchmark fails
                    //    lat = 10;
                    //    ports = PORTS_015;
                    //    break;

                    //David: 3 uops
                    case XO(DPPD):
						// {
						// 	simple = false;
						// 	uint32_t latsc[] = {5, 3, 1};
						// 	uint8_t portsc[] = {PORT_0 | PORT_1, PORT_0 | PORT_1, PORT_5};
						// 	emitChainedOp(instr, uops, 3, latsc, portsc);
						// }
                        lat = 9;
                        ports = PORTS_015;
                        //extraSlots = 1;
						break;
                    case XO(DPPS): //4 uops, crashes as chained, order? Missing one uop
						// {
						// 	simple = false;
						// 	uint32_t latsc[] = {8, 4, 1};
						// 	uint8_t portsc[] = {PORT_0, PORT_1, PORT_5};
						// 	emitChainedOp(instr, uops, 3, latsc, portsc);
						// }
                        lat = 12;
						ports = PORTS_015;
                        // extraSlots = 1;
                        break;

					case XO(BLENDPS): //David: Seems right, but why 108m in real?
                    case XO(BLENDPD):
                    case XO(BLENDVPS): //David: 2 uops
                    case XO(BLENDVPD):
						lat = 1;
						ports = PORTS_015;
						break;
					case XO(EXTRACTPS):	//David: 2uops, 136m cycles?
						{
							simple = false;
							uint32_t latsc[] = {1, 1};
							uint8_t portsc[] = {PORT_0|PORT_5, PORT_0|PORT_5};
							emitChainedOp(instr, uops, 2, latsc, portsc);
						}
						break;
                    case XO(HADDPD): //David: 3 uops
                    case XO(HADDPS):
                    case XO(HSUBPD):
                    case XO(HSUBPS):
						{
							simple = false;
							uint32_t latsc[] = {2, 2, 2};
							uint8_t portsc[] = {PORTS_01, PORT_5, PORT_5};
							emitChainedOp(instr, uops, 3, latsc, portsc);
						}
                        break;

                    case XO(PEXTRB): //David: 2 uops, wrong latency
                    case XO(PEXTRD):
                    case XO(PEXTRQ):
                    case XO(PEXTRW):
						{
							simple = false;
							uint32_t latsc[] = {1, 1};
							uint8_t portsc[] = {PORT_0, PORT_5} ;
							emitChainedOp(instr, uops, 2, latsc, portsc);
						}
                        //lat = 1;
                        //ports = PORTS_015;
                        break;

					case XO(MPSADBW): // 2 uops
						{
							simple = false;
							uint32_t latsc[] = {2, 1};
							uint8_t portsc[] = {PORT_5, PORT_5};
							emitChainedOp(instr, uops, 2, latsc, portsc);
						}
                        //lat = 6;
                        //ports = PORTS_015;
                        break;

                    default:
                        inaccurate = true;
                        //David, inaccurate
                        printf("Innacurate: instruction %d, category: %d\n", opcode, category);
							   INACCUOP
                }
                if (simple) emitBasicOp(instr, uops, lat, ports, extraSlots);
            } // SSE 
            break;



        case XC(CONVERT): //part of SSE // Modified some function to contain extra slots.
            switch (opcode) {
                
                case XO(CVTTPD2DQ):
                case XO(CVTPS2PD):
                case XO(CVTPD2PI):
                case XO(CVTPD2PS):
                case XO(CVTPI2PD):
                case XO(CVTPD2DQ):
                case XO(CVTSD2SS): //David: Intel says 4...
                case XO(CVTDQ2PD):
                case XO(CVTTPD2PI):
                    emitConvert2Op(instr, uops, 2, 2, PORT_1|PORT_0, PORT_5);
                    break;

               
                case XO(CVTSD2SI): 
                case XO(CVTSS2SI):
                case XO(CVTTSD2SI):
                case XO(CVTTSS2SI):
                    emitConvert2Op(instr, uops, 2, 2, PORT_1|PORT_0, PORT_0);
				    break;
                
                
				case XO(CVTPS2PI):
                case XO(CVTTPS2PI):
            
						// rsv agner
                    emitConvert2Op(instr, uops, 1, 1, PORT_0, PORT_5); 

                    break;
                
				
             
                case XO(CVTDQ2PS):
                case XO(CVTPS2DQ): // skylake
                case XO(CVTTPS2DQ):
                    emitBasicOp(instr, uops, 1 , PORT_0 | PORT_1);
                    break;
                case XO(CVTPI2PS): //Agner's says trhoughput is 2
					emitConvert2Op(instr, uops, 5, 5, PORT_0, PORT_1|PORT_0,3);
					break;
                case XO(CVTSI2SD):
                case XO(CVTSI2SS): 
                case XO(CVTSS2SD):
                    emitConvert2Op(instr, uops, 5, 5, PORT_5, PORT_0 | PORT_1 ,3);
                    break;

                
				
				
				
                case XO(CBW): //David: Matches doc
                case XO(CWDE):
                case XO(CDQE): //David:Matches doc
                    emitBasicOp(instr, uops, 1, PORTS_015 | PORT_6);
                    break;
                case XO(CWD): // Two uops, latency goes down if we do so
					{
                    uint32_t lats[] = {2, 2};
                    uint8_t ports[] = { PORTS_015 | PORT_6, PORT_0 };
                    emitChainedOp(instr, uops, 2, lats, ports);
					}
                    // emitBasicOp(instr, uops, 1, PORT_0);
                    break;
				case XO(CDQ): //This should check the registers, it is expanding AX or EAX
                    emitBasicOp(instr, uops, 1, PORT_0 | PORT_6, 1);
                    break;
                case XO(CQO): //This should check the registers, it is expanding AX or EAX
                    emitBasicOp(instr, uops, 1, PORT_0 | PORT_6);
                    break;

                default: // AVX converts
                    inaccurate = true;
                    //David, inaccurate
                    printf("Innacurate: instruction %d, category: %d\n", opcode, category);
						  //emitBasicOp(instr, uops, 1, PORT_0|PORT_1|PORT_5);
						 INACCUOP
            }
            break;

		//David
        case XC(AES): //All David
			switch (opcode) {
                case XO(AESDEC): //All 2 uops
                case XO(AESDECLAST):
				case XO(AESENC):
				case XO(AESENCLAST):
                    //emitBasicOp(instr, uops, 8, PORTS_015, 3); //reciprocal is 4
					emitBasicOp(instr, uops, 4, PORT_0); // at least for now 
                    break;
				case XO(AESIMC): //David: Looks right
					{
                    uint32_t lats[] = {1, 1};
                    uint8_t ports[] = {PORT_0, PORT_0};
                    emitChainedOp(instr, uops, 2, lats, ports);
					}
                    break;
				case XO(AESKEYGENASSIST): //not working with multi uops- make bw and lat ok
					emitBasicOp(instr, uops, 12, PORT_5, 11); //reciprocal is 8
					{
					// inaccurate = true;
                    // uint32_t lats[] = {5, 7};
                    // uint8_t ports[] = {PORT_0 , PORT_5};
                    // emitChainedOp(instr, uops, 2, lats, ports);
					}
                    break;
                default:
                    inaccurate = true;
					//David, inaccurate
                    printf("Innacurate: instruction %d, category: %d\n", opcode, category);
            }
            break;

        case XC(PCLMULQDQ): //CLMUL extension (carryless multiply, generally related to AES-NI)
			//David 1 uops
				emitBasicOp(instr, uops, 7, PORT_5); //reciprocal is 8
            break;




        /* Control flow ops (branches, jumps) */
        case XC(COND_BR):
        case XC(UNCOND_BR):
            // We model all branches and jumps with a latency of 1. Far jumps are really expensive, but they should be exceedingly rare (from Intel's manual, they are used for call gates, task switches, etc.)
            emitBasicOp(instr, uops, 1, PORT_5); //David, not changed to 0 as doc.
            if (opcode == XO(JMP_FAR)) inaccurate = true;
            break;

        /* Stack operations */
        case XC(CALL):
        case XC(RET):
            /* Call and ret are both unconditional branches and stack operations; however, Pin does not list RSP as source or destination for them */
            //dropStackRegister(instr); //stack engine kills accesses to RSP
            emitBasicOp(instr, uops, 2, PORT_5); //David, orig 1
            if (opcode != XO(CALL_NEAR) && opcode != XO(RET_NEAR)) inaccurate = true; //far call/ret or irets are far more complex
            break;

        case XC(POP):
        case XC(PUSH):
            //Again, RSP is not included here, so no need to remove it.
            switch (opcode) {
                case XO(POP):
                    //David, different latency in sandy bridge
                    emitBasicMove(instr, uops, 2, PORTS_015); //1
                    break;
                case XO(PUSH):
                    //Basic PUSH/POP are just moves. They are always to/from memory, so PORTS is irrelevant
                    emitBasicMove(instr, uops, 2, PORT_2 | PORT_3 | PORT_4 | PORT_7); //1
                    break;
                case XO(POPF):
                case XO(POPFD):
                case XO(POPFQ):
                    //Java uses POPFx/PUSHFx variants. POPF is complicated, 8 uops... microsequenced
                    inaccurate = true;
                    emitBasicOp(instr, uops, 8, PORTS_015);
						  //INACCUOP
                    break;
                case XO(PUSHF):
                case XO(PUSHFD):
                case XO(PUSHFQ):
                    //This one we can handle... 2 exec uops + store and reciprocal thput of 1
                    {
                        uint32_t lats[] = {1, 1};
                        uint8_t ports[] = {PORTS_015, PORTS_015};
                        emitChainedOp(instr, uops, 2, lats, ports);
                    }
                    break;

                default:
                    inaccurate = true;
                    //David, inaccurate
                    printf("Innacurate: instruction %d, category: %d\n", opcode, category);
						  emitBasicOp(instr, uops, 1, PORTS_015);
						  //INACCUOP//
            }
            break;

        /* Prefetches */
        case XC(PREFETCH):
            //A prefetch is just a load that doesn't feed into any register (or REG_TEMP in this case)
            //NOTE: Not exactly, because this will serialize future loads under TSO
            emitLoads(instr, uops);
            break;

        case XC(AVX):
		case XC(AVX2):
		case XC(AVX2GATHER):
		case XC(BROADCAST): //part of AVX
            //TODO: Whatever, Nehalem has no AVX
		/* Stuff on the system side (some of these are privileged) */
        case XC(INTERRUPT):
        case XC(SYSCALL):
        case XC(SYSRET):
        case XC(IO):
		case XC(IOSTRINGOP):
            //TODO: These seem to make sense with REP, which Pin unfolds anyway. Are they used al all?
		case XED_CATEGORY_3DNOW:
        case XC(MMX): //MMX=40  //David: This should not be very important
			switch (opcode) {
                case XO(PSHUFW):  //
                    emitBasicOp(instr, uops, 1, PORT_5);
                    break;

                case XO(EMMS): //David: Write custom code
				    {
                        emitBasicOp(instr, uops, 5, PORT_0 | PORT_5);       
							// uint32_t latsc[] = {5,1};
							// uint8_t portsc[] = {PORT_0 | PORT_5, PORT_0| PORT_6};
							// emitChainedOp(instr, uops, 2, latsc, portsc);
					}
					// emitBasicOp(instr, uops, 6, PORT_5, 13); //Possibly chained
                    break;
				default:
					inaccurate = true;
                    //David, inaccurate
                    //printf("Innacurate: instruction %d, category: %d\n", opcode, category);
					//emitBasicOp(instr, uops, 1, PORTS_015);
				 INACCUOP
			}
			break;
		case XC(SEGOP):
            //TODO: These are privileged, right? They are expensive but rare anyhow
        case XC(VTX): //virtualization, hmmm
            //TODO
		case XC(STTNI): //SSE 4.2 //Complex  // dmubench show this should be moved into XC(SSE)
            switch (opcode) {
                case XO(PCMPESTRI):
                    emitBasicOp(instr, uops, 4, PORTS_015, 3);
                    break;
                case XO(PCMPESTRM):
                    emitBasicOp(instr, uops, 8, PORTS_015, 4);
                    break;
                case XO(PCMPISTRI):
                    emitBasicOp(instr, uops, 3, PORT_0, 1);
                    break;
                case XO(PCMPISTRM):
                    emitBasicOp(instr, uops, 8, PORTS_015, 3);
                    break;
                default:
                    break;
            }
            break;
		case XC(SYSTEM): // 66

            switch (opcode){

                case XO(STR):
                    emitBasicOp(instr, uops, 7, PORT_0 | PORT_5,1);
                    emitBasicOp(instr, uops, 7, PORT_1,1);
                    emitBasicOp(instr, uops, 7, PORT_6,6);
                    break;

                case XO(LAR):
                    emitBasicOp(instr, uops, 9, PORT_6,55);
                    break;


                case XO(SLDT): // cannot handle. sim got error 
                {   
                    uint32_t latsc[] = {4, 2};
                    uint8_t portsc[] = {PORT_6 , PORT_1};
                    emitChainedOp(instr, uops, 2, latsc, portsc);
                    break;
                }
                case XO(SMSW):
                    emitBasicOp(instr, uops, 3, PORTS_0156, 2);
                    emitBasicOp(instr, uops, 2, PORT_0 | PORT_6, 1);
                    emitBasicOp(instr, uops, 2, PORT_0 | PORT_6, 1);
                    emitBasicOp(instr, uops, 9, PORT_5, 8);

                    break;

                case XO(RDTSC):
                    emitBasicOp(instr, uops, 24, PORT_6,23);
                    break;

                default:
                    break;


            }
            break;
            //TODO: Privileged ops are not included
            /*switch(opcode) {
                case XO(RDTSC):
                case XO(RDTSCP):
                    opLat = 24;
                    break;
                case XO(RDPMC):
                    opLat = 40;
                    break;
                default: ;
            }*/




		case XC(XSAVE):
        case XC(XSAVEOPT): //hold your horses, it's optimized!! (AVX)

		//David
        case XC(LZCNT): //Not supported by Sandy bridge, runs as BSR
            emitBasicOp(instr, uops, 1, PORT_1); //
            break;
		case XC(BDW):
		case XC(BMI1): //Not supported by Sandy bridge, runs as BSF
			switch (opcode) {
                case XO(TZCNT):
                    emitBasicOp(instr, uops, 3, PORT_1); //See BSF
                    break;
				default:
                    break;
			}
		case XC(BMI2):
		case XC(FMA4):
		case XC(RDRAND):
		case XC(RDSEED):
		case XC(RDWRFSGS):
		case XC(SMAP):
		case XC(TBM):
		case XC(VFMA):
		case XC(XOP):
			inaccurate = true;
			//David, inaccurate
			//printf("Innacurate: instruction %d, category: %d\n", opcode, category);
			//emitBasicOp(instr, uops, 1, PORTS_015);
			INACCUOP
            break;

        /* String ops (I'm reading the manual and they seem just like others... wtf?) */
        case XC(STRINGOP):
            switch (opcode) {
                case XO(STOSB):
                case XO(STOSW):
                case XO(STOSD):
                case XO(STOSQ):
                    //mov [rdi] <- rax
                    //add rdi, 8
                    //emitBasicOp(instr, uops, 1, PORTS_015); //not really, this emits the store later and there's no dep (the load is direct to reg)
                    emitStore(instr, 0, uops, REG_RAX);
                    emitExecUop(REG_RDI, 0, REG_RDI, 0, uops, 1, PORTS_015);
                    break;
                case XO(LODSB):
                case XO(LODSW):
                case XO(LODSD):
                case XO(LODSQ):
                    //mov rax <- [rsi]
                    //add rsi, 8
                    emitLoad(instr, 0, uops, REG_RAX);
                    emitExecUop(REG_RSI, 0, REG_RSI, 0, uops, 1, PORTS_015);
                    break;
                case XO(MOVSB):
                case XO(MOVSW):
                case XO(MOVSD):
                case XO(MOVSQ):
                    //lodsX + stosX
                    emitLoad(instr, 0, uops, REG_RAX);
                    emitStore(instr, 0, uops, REG_RAX);
                    emitExecUop(REG_RSI, 0, REG_RSI, 0, uops, 1, PORT_0, 3);
                    emitExecUop(REG_RDI, 0, REG_RDI, 0, uops, 1, PORT_1 | PORT_5);
                    break;
                case XO(CMPSB):
                case XO(CMPSW):
                case XO(CMPSD):
                case XO(CMPSQ):
                    //load [rsi], [rdi], compare them, and add the other 2
                    //Agner's tables say all exec uops can go anywhere, but I'm betting the comp op only goes in port5
                    emitLoad(instr, 0, uops, REG_LOAD_TEMP);
                    emitLoad(instr, 0, uops, REG_LOAD_TEMP+1);
                    emitExecUop(REG_LOAD_TEMP, REG_LOAD_TEMP+1, REG_RFLAGS, 0, uops, 1, PORT_5);
                    emitExecUop(REG_RSI, 0, REG_RSI, 0, uops, 1, PORTS_015);
                    emitExecUop(REG_RDI, 0, REG_RDI, 0, uops, 1, PORTS_015);
                    break;
                default: //SCAS and other dragons I have not seen yet
                    inaccurate = true;
                    //David, inaccurate
                    //printf("Innacurate: instruction %d, category: %d\n", opcode, category);
							//emitBasicOp(instr, uops, 1, PORTS_015);
						INACCUOP
            }
            break;


        /* Stuff not even the Intel guys know how to classify :P */
        case XC(MISC):
			switch (opcode) {
				case XO(LEA):
					// Rommel  -> worst case 
					emitBasicOp(instr, uops, 1,  PORT_1 |PORT_5 );
					break;
				case XO(PAUSE):
					//David:lat 11, 7 uops; uops right, how to achieve latency?
					//Pause is weird. It takes 9 cycles, issues 5 uops (to be treated like a complex instruction and put a wrench on the decoder?),
					//and those uops are issued to PORT_015. No idea about how individual uops are sized, but in ubenchs I cannot put even an ADD
					//between pauses for free, so I'm assuming it's 9 solid cycles total.
					emitExecUop(0, 0, 0, 0, uops, 9, PORTS_015, 8); //9, longest first
                    emitExecUop(0, 0, 0, 0, uops, 5, PORTS_015, 4); //NOTE: latency does not matter
                    emitExecUop(0, 0, 0, 0, uops, 5, PORTS_015, 4);
                    emitExecUop(0, 0, 0, 0, uops, 4, PORTS_015, 3);
                    emitExecUop(0, 0, 0, 0, uops, 4, PORTS_015, 3);
					break;
				case XO(LFENCE): //David: Simple case. Weird 425m cycles and few uops
					emitBasicOp(instr, uops, 4, PORT_6, 3); //Throughput seems more important than latency
					break;
				case XO(MFENCE): //David: Simple case, is a memory instruction, 1 load and 1 store
					emitBasicOp(instr, uops, 1, PORT_2 | PORT_3, 33); //Throughput seems more important than latency
                    emitBasicOp(instr, uops, 1, PORT_4, 33);
					break;
				case XO(SFENCE): //David: Simple case, is a memory instruction, 1 load and 1 store
					// emitBasicOp(instr, uops, 1, PORTS_015, 5); //Throughput seems more important than latency
                    emitBasicOp(instr, uops, 1, PORT_2 | PORT_3, 5); //Throughput seems more important than latency
                    emitBasicOp(instr, uops, 1, PORT_4, 5);
					break;
                case XO(ENTER):
                    emitExecUop(0, 0, 0, 0, uops, 4, PORTS_0156, 3);
                    emitExecUop(0, 0, 0, 0, uops, 4, PORTS_0156, 3);
                    emitExecUop(0, 0, 0, 0, uops, 4, PORTS_0156, 3);
                    emitExecUop(0, 0, 0, 0, uops, 4, PORTS_0156, 3);
                    emitExecUop(0, 0, 0, 0, uops, 4, PORTS_015, 3);
                    emitExecUop(0, 0, 0, 0, uops, 4, PORTS_015, 3);
                    emitExecUop(0, 0, 0, 0, uops, 4, PORTS_015, 3);
                    emitExecUop(0, 0, 0, 0, uops, 4, PORT_2 | PORT_3, 3);
                    emitExecUop(0, 0, 0, 0, uops, 4, PORT_2 | PORT_3, 3);
                    emitExecUop(0, 0, 0, 0, uops, 4, PORT_0 | PORT_6, 3);
                    emitExecUop(0, 0, 0, 0, uops, 4, PORT_0 | PORT_6, 3);
                    emitExecUop(0, 0, 0, 0, uops, 4, PORT_4 , 3);
                    


                    
                    break;
				default:
					inaccurate = true;
					//David, inaccurate
					//printf("Innacurate: instruction %d, category: %d\n", opcode, category);
					//emitBasicOp(instr, uops, 1, PORTS_015);
					INACCUOP
			}
            // switch (opcode) {
            //     // case CPUID:
            //     // 
            //     // case LEAVE:
            //     // case LEA:
            //     // case LFENCE:
            //     // case MFENCE:
            //     // case SFENCE:
            //     // case MONITOR:
            //     // case MWAIT:
            //     // case UD2:
            //     // case XLAT:
            //     case ENTER:

            // }
            //TODO
            break;

        default: {}
            //panic("Invalid instruction category");
    }

    //Try to produce something approximate...
    if (uops.size() - initialUops == isLocked? 1 : 0) { //if it's locked, we have the initial fence for an empty instr
        emitBasicOp(instr, uops, 1, PORTS_015, 0, false /* don't report unhandled cases */);
        inaccurate = true;
    }

    //NOTE: REP instructions are unrolled by PIN, so they are accurately simulated (they are treated as predicated in Pin)
    //See section "Optimizing Instrumentation of REP Prefixed Instructions" on the Pin manual

    //Add ld/st fence to all locked instructions
    if (isLocked) {
        //inaccurate = true; //this is now fairly accurate
        emitFence(uops, 9); //locked ops introduce an additional uop and cache locking takes 14 cycles/instr per the perf counters; latencies match with 9 cycles of fence latency
    }

    assert(uops.size() - initialUops < MAX_UOPS_PER_INSTR);
    //assert_msg(uops.size() - initialUops < MAX_UOPS_PER_INSTR, "%ld -> %ld uops", initialUops, uops.size());
    return inaccurate;
}

//Original fuser
/*// See Agner Fog's uarch doc, macro-op fusion for Core 2 / Nehalem
bool Decoder::canFuse(INS ins) {
    xed_iclass_enum_t opcode = (xed_iclass_enum_t) INS_Opcode(ins);
    if (!(opcode == XO(CMP) || opcode == XO(TEST))) return false;
    //Discard if immediate
    for (uint32_t op = 0; op < INS_OperandCount(ins); op++) if (INS_OperandIsImmediate(ins, op)) return false;

    //OK so far, let's check the branch
    INS nextIns = INS_Next(ins);
    if (!INS_Valid(nextIns)) return false;
    xed_iclass_enum_t nextOpcode = (xed_iclass_enum_t) INS_Opcode(nextIns);
    xed_category_enum_t nextCategory = (xed_category_enum_t) INS_Category(nextIns);
    if (nextCategory != XC(COND_BR)) return false;
    if (!INS_IsDirectBranch(nextIns)) return false; //according to PIN's API, this s only true for PC-rel near branches

    switch (nextOpcode) {
        case XO(JZ):  //or JZ
        case XO(JNZ): //or JNE
        case XO(JB):
        case XO(JBE):
        case XO(JNBE): //or JA
        case XO(JNB):  //or JAE
        case XO(JL):
        case XO(JLE):
        case XO(JNLE): //or JG
        case XO(JNL):  //or JGE
            return true;
        case XO(JO):
        case XO(JNO):
        case XO(JP):
        case XO(JNP):
        case XO(JS):
        case XO(JNS):
            return opcode == XO(TEST); //CMP cannot fuse with these
        default:
            return false; //other instrs like LOOP don't fuse
    }
}*/


// See Agner Fog's uarch doc, macro-op fusion for Core 2 / Nehalem
//David, enhanced for Sandy bridge
bool Decoder::canFuse(INS ins) {
    xed_iclass_enum_t opcode = (xed_iclass_enum_t) INS_Opcode(ins);
    //if (!(opcode == XO(CMP) || opcode == XO(TEST))) return false;
    switch (opcode) {
        case(XO(CMP)):
        case(XO(TEST)):
        case(XO(ADD)):
        case(XO(SUB)):
        case(XO(INC)):
        case(XO(DEC)):
        case(XO(AND)):
            break;
        default:
            return false;
    }
    //Discard if immediate
    for (uint32_t op = 0; op < INS_OperandCount(ins); op++) if (INS_OperandIsImmediate(ins, op)) return false;

    //OK so far, let's check the branch
    INS nextIns = INS_Next(ins);
    if (!INS_Valid(nextIns)) return false;
    xed_iclass_enum_t nextOpcode = (xed_iclass_enum_t) INS_Opcode(nextIns);
    xed_category_enum_t nextCategory = (xed_category_enum_t) INS_Category(nextIns);
    if (nextCategory != XC(COND_BR)) return false;
    if (!INS_IsDirectBranch(nextIns)) return false; //according to PIN's API, this s only true for PC-rel near branches

    switch (nextOpcode) {
        //David
        case XO(JZ):  //or JZ
        case XO(JNZ): //or JNE
        case XO(JL):
        case XO(JLE):
        case XO(JNLE): //or JG
        case XO(JNL):  //or JGE
            return true; //Common jumps that can be fused
        case XO(JS):
        case XO(JNS):
        case XO(JP):
        case XO(JNP):
        case XO(JO):
        case XO(JNO):
            return opcode == XO(TEST) || opcode == XO(AND); //Only supported with TEST and AND
        //Weird, JC doesn't appear in the pin instruction list...
        case XO(JB):
        case XO(JBE):
        case XO(JNBE): //or JA
        case XO(JNB):  //or JAE
            return opcode == XO(TEST) || opcode == XO(AND) || opcode == XO(CMP) || opcode == XO(ADD) || opcode == XO(SUB); //Not supported with INC and DEC
        default:
            return false; //other instrs like LOOP don't fuse
    }
}

bool Decoder::decodeFusedInstrs(INS ins, DynUopVec& uops) {
    //assert(canFuse(ins)); //this better be true :)

    Instr instr(ins);
    Instr branch(INS_Next(ins));

    //instr should have 2 inputs (regs/mem), and 1 output (rflags), and branch should have 2 inputs (rip, rflags) and 1 output (rip)

    if (instr.numOutRegs != 1  || instr.outRegs[0] != REG_RFLAGS ||
        branch.numOutRegs != 1 || branch.outRegs[0] != REG_RIP)
    {
        //reportUnhandledCase(instr, "decodeFusedInstrs");
        //reportUnhandledCase(branch, "decodeFusedInstrs");
		;
    } else {
        instr.outRegs[1] = REG_RIP;
        instr.numOutRegs++;
    }

    emitBasicOp(instr, uops, 1, PORT_5);
    return false; //accurate
}


#ifdef BBL_PROFILING

//All is static for now...
#define MAX_BBLS (1<<24) //16M

static lock_t bblIdxLock = 0;
static uint64_t bblIdx = 0;

static uint64_t bblCount[MAX_BBLS];
static std::vector<uint32_t>* bblApproxOpcodes[MAX_BBLS];

ProfileInstruction * Decoder::instruction_profiling_init () { 
	uint16_t max_opcodes; 
	ProfileInstruction *ptr ;

	max_opcodes = xed_iform_enum_t_last() +1 ;

	//info("max_opcodes: %d\xa", max_opcodes);
	
   ptr =  (ProfileInstruction * ) 
		calloc( max_opcodes,  sizeof(struct instruction_profile_t) );

	//ptr[0].count=max_opcodes;

	/*
	info("max_opcodes: %d\xa", max_opcodes);
	
	for  (i = 0 ; i <  max_opcodes; i++ ){ 
		memset(ptr[i], 0, sizeof(struct instruction_profile_t));
	} 
	*/
	return ptr;
}

#endif


BblInfo* Decoder::decodeBbl(BBL bbl, bool oooDecoding) {
    uint32_t instrs = BBL_NumIns(bbl);
    uint32_t bytes = BBL_Size(bbl);
    BblInfo* bblInfo;

#ifdef BBL_PROFILING
		  ProfileInstruction *profIns = instruction_profiling_init();
#endif

    if (oooDecoding) {
        //Decode BBL
        uint32_t approxInstrs = 0;
        uint32_t curIns = 0;
        DynUopVec uopVec;

#ifdef BBL_PROFILING
        std::vector<uint32_t> approxOpcodes;
		  size_t insBytes;
		  uint8_t buf[16];
		  uint16_t actualOpcode;
        //XED decoder init
        xed_state_t dstate;
        xed_decoded_inst_t xedd;
        xed_state_zero(&dstate);
        xed_state_init(&dstate, XED_MACHINE_MODE_LONG_64, XED_ADDRESS_WIDTH_64b, XED_ADDRESS_WIDTH_64b);
        xed_decoded_inst_zero_set_mode(&xedd, &dstate);
#endif

        //Gather some info about instructions needed to model decode stalls
        std::vector<ADDRINT> instrAddr;
        std::vector<uint32_t> instrBytes;
        std::vector<uint32_t> instrUops;
        std::vector<INS> instrDesc;
        std::vector<bool> instrApprox;


        //Decode
        for (INS ins = BBL_InsHead(bbl); INS_Valid(ins); ins = INS_Next(ins)) {
            //David, let's check instructions
			Instr instr(ins);
			//("inst address: %lx",INS_Address(ins));
			warn("instruction: %s", INS_Mnemonic(ins).c_str());
			//reportUnhandledCase(instr, "Debugging");
			bool inaccurate = false;
            uint32_t prevUops = uopVec.size();

#ifdef BBL_PROFILING
			xed_decoded_inst_zero_keep_mode(&xedd);
			ADDRINT this_add = INS_Address(ins);
			
         insBytes = PIN_SafeCopy(buf, (VOID *) this_add, 15);
         xed_error_enum_t err = xed_decode(&xedd, buf, insBytes);
         if (err != XED_ERROR_NONE) {
            info("xed_decode failed: %s", xed_error_enum_t2str(err));
		   }		
			actualOpcode =	xed_decoded_inst_get_iform_enum(&xedd) ;

			//if ( actualOpcode == XED_IFORM_INVALID ) info("wooops.. :");

			profIns[actualOpcode].count++;
#endif
            if (Decoder::canFuse(ins)) {
                inaccurate = Decoder::decodeFusedInstrs(ins, uopVec);
                instrAddr.push_back(INS_Address(ins));
                instrBytes.push_back(INS_Size(ins));
                instrUops.push_back(uopVec.size() - prevUops);
                instrDesc.push_back(ins);

                ins = INS_Next(ins); //skip the JMP

                instrAddr.push_back(INS_Address(ins));
                instrBytes.push_back(INS_Size(ins));
                instrUops.push_back(0);
                instrDesc.push_back(ins);

                curIns+=2;
            } else {
                inaccurate = Decoder::decodeInstr(ins, uopVec);

                instrAddr.push_back(INS_Address(ins));
                instrBytes.push_back(INS_Size(ins));
                instrUops.push_back(uopVec.size() - prevUops);
                instrDesc.push_back(ins);

                curIns++;
            }
#ifdef PROFILE_ALL_INSTRS
           // inaccurate = true; //uncomment to profile everything
#endif

            if (inaccurate) {
                approxInstrs++;
#ifdef BBL_PROFILING

					 profIns[actualOpcode].approx = true;

                xed_decoded_inst_zero_keep_mode(&xedd); //need to do this per instruction
                xed_iform_enum_t iform = XED_IFORM_INVALID;
                //uint8_t buf[16];
                //Using safecopy, we bypass pagefault uglyness due to out-of-bounds accesses
					 ADDRINT this_add = INS_Address(ins);
                insBytes = PIN_SafeCopy(buf, (VOID *) this_add, 15);
                xed_error_enum_t err = xed_decode(&xedd, buf, insBytes);
                if (err != XED_ERROR_NONE) {
                    panic("xed_decode failed: %s", xed_error_enum_t2str(err));
                } else {
                    iform = xed_decoded_inst_get_iform_enum(&xedd);
                }
                approxOpcodes.push_back((uint32_t)iform);
#endif
                //info("Approx decoding: %s", INS_Disassemble(ins).c_str());
            }
				instrApprox.push_back(inaccurate);
        }
        assert(curIns == instrs);

        //Instr predecoder and decode stage modeling; we assume clean slate between BBLs, which is typical because
        //optimizing compilers 16B-align most branch targets (and if it doesn't happen, the error introduced is fairly small)

        //1. Predecoding
        uint32_t predecCycle[instrs];
        uint32_t pcyc = 0;
        uint32_t psz = 0;
        uint32_t pcnt = 0;
        uint32_t pblk = 0;

        ADDRINT startAddr = (INS_Address(instrDesc[0]) >> 4) << 4;

        for (uint32_t i = 0; i < instrs; i++) {
            INS ins = instrDesc[i];
            ADDRINT addr = INS_Address(ins);
            uint32_t bytes = INS_Size(ins);
            uint32_t block = (addr - startAddr) >> 4;
            psz += bytes;
            pcnt++;
            if (psz > 16 /*leftover*/|| pcnt > 6 /*max predecs*/|| block > pblk /*block switch*/) {
                psz = bytes;
                pcnt = 1;
                pblk = block;
                pcyc++;
            }

            //Length-changing prefix introduce a 6-cycle penalty regardless;
            //In 64-bit mode, only operand size prefixes are LCPs; addr size prefixes are fine
            // UPDATE (dsm): This was introducing significant errors in some benchmarks (e.g., astar)
            // Turns out, only SOME LCPs (false LCPs) cause this delay
            // see http://www.jaist.ac.jp/iscenter-new/mpc/altix/altixdata/opt/intel/vtune/doc/users_guide/mergedProjects/analyzer_ec/mergedProjects/reference_olh/pentiumm_hh/pentiummy_hh/lipsmy/instructions_that_require_slow_decoding.htm
            // At this point I'm going to assume that gcc is smart enough to not produce these
            //if (INS_OperandSizePrefix(ins)) pcyc += 6;

            predecCycle[i] = pcyc;
            //info("PREDEC %2d: 0x%08lx %2d %d %d %d", i, instrAddr[i], instrBytes[i], instrUops[i], block, predecCycle[i]);
        }

        //2. Decoding
        //4-1-1-1 rules: Small decoders can only take instructions that produce 1 uop AND are at most 7 bytes long
        uint32_t uopIdx = 0;

        uint32_t dcyc = 0;
        uint32_t dsimple = 0;
		  // rsv uint32_t ddual = 0;
        uint32_t dcomplex = 0;

        for (uint32_t i = 0; i < instrs; i++) {
            if (instrUops[i] == 0) continue; //fused branch

            uint32_t pcyc = predecCycle[i]; //David: No clue about predecoder
            if (pcyc > dcyc) {
                dcyc = pcyc;
                dsimple = 0;
					 //ddual = 0;
                dcomplex = 0;
            }

            bool simple = (instrUops[i] == 1) && (instrBytes[i] < 8); //David: Not sure about the length of the instruction
			 // bool dual = (instrUops[i] == 2) && (instrBytes[i] < 8);

            if ((simple && dsimple + dcomplex == 4) || (!simple && dcomplex == 1)) { // Do: (!simple /*&& dcomplex == 1*/) to be conservative?
                dcyc++;
                dsimple = 0;
					//ddual = 0;
               //dcomplex = 0;
            }

            if (simple) dsimple++;
            else dcomplex++;

            //info("   DEC %2d: 0x%08lx %2d %d %d %d (%d %d)", i, instrAddr[i], instrBytes[i], instrUops[i], simple, dcyc, dcomplex, dsimple);

            for (uint32_t j = 0; j < instrUops[i]; j++) {
                uopVec[uopIdx + j].decCycle = dcyc; //David: Test with 0
            }

            uopIdx += instrUops[i];
        }

        assert(uopIdx == uopVec.size());

        //Allocate
        uint32_t objBytes = offsetof(BblInfo, oooBbl) + DynBbl::bytes(uopVec.size());
        bblInfo = static_cast<BblInfo*>(gm_malloc(objBytes));  // can't use type-safe interface

        //Initialize ooo part
        DynBbl& dynBbl = bblInfo->oooBbl[0];
        dynBbl.addr = BBL_Address(bbl);
        dynBbl.uops = uopVec.size();
        dynBbl.approxInstrs = approxInstrs;
        for (uint32_t i = 0; i < dynBbl.uops; i++) dynBbl.uop[i] = uopVec[i];

#ifdef BBL_PROFILING
        futex_lock(&bblIdxLock);

        dynBbl.bblIdx = bblIdx++;
        assert(dynBbl.bblIdx < MAX_BBLS);
        if (approxInstrs) {
            bblApproxOpcodes[dynBbl.bblIdx] = new std::vector<uint32_t>(approxOpcodes);  // copy
        }
        //info("DECODED BBL IDX %d", bblIdx);

        futex_unlock(&bblIdxLock);
#endif
    } else {
        bblInfo = gm_malloc<BblInfo>();
    }

#ifdef BBL_PROFILING
bblInfo->exIns = profIns;
#endif


    //Initialize generic part
    bblInfo->instrs = instrs;
    bblInfo->bytes = bytes;


    return bblInfo;
}


#ifdef BBL_PROFILING
void Decoder::addUp_andfree_profileInstructions(ProfileInstruction *gblProf, ProfileInstruction *fromBbl) {
		
	uint16_t i,max_opcodes; 

	max_opcodes =  xed_iform_enum_t_last() +1 ;

	if ( gblProf  && fromBbl ) { 
	for ( i = 0 ; i < max_opcodes ; i++ ) {
		if ( fromBbl[i].count != 0 ) {
			gblProf[i].count +=  fromBbl[i].count;
			gblProf[i].approx =  fromBbl[i].approx ;
		}
	}

	free(fromBbl);
	}
}

void Decoder::profileBbl(uint64_t bblIdx) {
    assert(bblIdx < MAX_BBLS);
    __sync_fetch_and_add(&bblCount[bblIdx], 1);
}

void Decoder::dumpBblProfile() {
    uint32_t numOpcodes = xed_iform_enum_t_last() + 1;
    uint64_t approxOpcodeCount[numOpcodes];
    for (uint32_t i = 0; i < numOpcodes; i++) approxOpcodeCount[i] = 0;
    for (uint32_t i = 0; i < bblIdx; i++) {
        if (bblApproxOpcodes[i]) for (uint32_t& j : *bblApproxOpcodes[i]) approxOpcodeCount[j] += bblCount[i];
    }

    std::ofstream out("approx_instrs.stats");
    out << std::setw(16) << "Category" << std::setw(16) << "Iclass" << std::setw(32) << "Iform" << std::setw(16) << "Count" << std::endl;
    for (uint32_t i = 0; i < numOpcodes; i++) {
       //if (approxOpcodeCount[i]) {
            out << xed_iclass_enum_t2str((xed_iclass_enum_t)i) << "\t " << approxOpcodeCount[i] << std::endl;
            xed_iform_enum_t iform = (xed_iform_enum_t)i;
            xed_category_enum_t cat = xed_iform_to_category(iform);
            xed_iclass_enum_t iclass = xed_iform_to_iclass(iform);

            out << std::setw(16) << xed_category_enum_t2str(cat) << std::setw(16) << xed_iclass_enum_t2str(iclass) << std::setw(32) << xed_iform_enum_t2str(iform) << std::setw(16) << approxOpcodeCount[i] << std::endl;
     //  }
    }

    //Uncomment to dump a bbl profile
    //for (uint32_t i = 0; i < bblIdx; i++) out << std::setw(8) << i << std::setw(8) <<  bblCount[i] <<  std::endl;

    out << std::setw(16) << "Opcopde" << std::setw(16) << "IClass" << std::setw(8) <<  "count" << std::endl;
} 

void Decoder::dumpGlobalProfile(ProfileInstruction *executedInstructions) {
	uint32_t numOpcodes = xed_iform_enum_t_last() + 1;
	 std::ofstream out("profile_instrs.stats");

    out << std::setw(16) << "Opcopde" << std::setw(16) << "IClass" << std::setw(8) <<  "count" << std::endl;

	 for (uint32_t i = 0; i < numOpcodes; i++) {
			  if (  executedInstructions[i].count  != XED_ICLASS_INVALID ) {
				 out <<  std::setw(16) <<
			 	 xed_iclass_enum_t2str(    xed_iform_to_iclass( (xed_iform_enum_t)  i)) <<  std::setw(16) << 
			 	 xed_isa_set_enum_t2str(  xed_iform_to_isa_set( (xed_iform_enum_t)  i))  <<  std::setw(8)  << 
				 executedInstructions[i].count  << std::endl;
				}
	 }

    out.close();
}

#endif
// 
