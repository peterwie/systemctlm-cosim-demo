/*
 * Top level of the Versal Net CDx stub cosim example.
 *
 * Copyright (c) 2022 Xilinx Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#pragma once

#define SC_INCLUDE_DYNAMIC_PROCESSES

#include <inttypes.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>

#include <map>
#include <functional>
#include <utility>

#include "systemc.h"
// #include "tlm_utils/simple_initiator_socket.h"
#include "tlm_utils/simple_target_socket.h"
// #include "tlm_utils/tlm_quantumkeeper.h"
// 
// #include "trace.h"
// #include "iconnect.h"
// #include "debugdev.h"
// #include "soc/xilinx/versal-net/xilinx-versal-net.h"
// #include "soc/dma/xilinx-cdma.h"
// #include "tlm-extensions/genattr.h"
// #include "memory.h"

// Catapult bedrock specific types and definitions
typedef uint32_t ULONG;
typedef uint64_t ULONGLONG;
#define DEFINE_GUID(...)   /* do nothing */
#include "CatapultShellInterface.h"

#define CATAPULT_MMIO_MAX (16 * 1024 * 1024)

class BedrockShell : public sc_core::sc_module
{
public:

    typedef size_t (BedrockShellReadFn)(uint64_t address, size_t length, uint64_t& value);

	tlm_utils::simple_target_socket<BedrockShell> tgt_socket;

	BedrockShell(sc_core::sc_module_name name);

private:

    // Simple register reads ... map contains a static 32b value for the register.
    std::map<uint64_t, std::pair<const char*, uint32_t>> _simple_regs;

    // For more complex registers, we allow a lambda or function pointer
    std::map<uint64_t, std::pair<const char*, std::function<BedrockShellReadFn>>> _dynamic_regs;
    
    void init_registers(void);

	virtual void b_transport(tlm::tlm_generic_payload& trans, sc_time& delay);

    // Reads a shell register based on address, returns the read length.
    // Returns 0 if the read is invalid ... for example if the caller 
    // attempts a 64b read of a 32b register (address ends in 4)
    size_t read_register(uint64_t address, size_t length, uint64_t& value);

    uint32_t get_cycle_counter(bool low_part);
};
