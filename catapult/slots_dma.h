/*
 * Definition of the slots DMA engine
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
#include <iomanip>
#include <ostream>
#include <utility>

#include "systemc.h"
#include "tlm_utils/simple_initiator_socket.h"
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

#include <vector>
#include <string>

// Catapult bedrock specific types and definitions
typedef uint32_t ULONG;
typedef uint64_t ULONGLONG;
#define DEFINE_GUID(...)   /* do nothing */
#include "CatapultShellInterface.h"

// #include "register_map.hpp"

namespace Catapult
{
    struct SlotsInputs
    {
        virtual uint64_t get_input_register(uint8_t slot_number) = 0;
        virtual uint64_t get_output_register(uint8_t slot_number) = 0;
        virtual uint64_t get_control_register(uint8_t slot_number) = 0;

        virtual uint64_t send_done_notification(uint8_t slot_number) = 0;
    };

    class SlotsEngine
    {
    public:
        static const uint64_t slots_magic_number    = SOFT_REG_MAPPING_SLOT_DMA_MAGIC_VALUE;
        static const unsigned int maximum_slot_count= 64;

        typedef RegisterMap<uint64_t>::Register RegisterT;

        enum DoorbellType { full = 0, done = 1 };

    private:
        // The number of slots the engine is running
        unsigned int _slot_count = maximum_slot_count;

        // And a register map for DMA registers
        RegisterMap<uint64_t> _dma_regs;

        void init_dma_registers(void);

        bool write_doorbell_register(RegisterT* reg,
                                     unsigned int slot_number,
                                     DoorbellType type,
                                     uint64_t new_value);

    public:

        SlotsEngine(unsigned int slot_count) : _slot_count(slot_count),
                                               _dma_regs("dma")
        {
            if (slot_count > maximum_slot_count)
            {
                throw logic_error("slot_count is larger than maximum allowed value (64)");
            }

            init_dma_registers();
        }

        void rst(void);

        // methods for reading and writing the slot DMA registers, if slots is enabled.
        uint64_t read_dma_register(uint32_t index, string& out_message);
        void write_dma_register(uint32_t index, uint64_t value, std::string& out_message);

        void print();
    };
}