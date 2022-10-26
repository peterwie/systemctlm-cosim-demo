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
    struct CatapultShellInterface;

    static_assert(sizeof(DMA_ISO_CONTROL_RESULT_COMBINED) == 128, "DMA_ISO_CONTROL_RESULT_COMBINED size incorrect");

    struct SlotInputConfig
    {
        vector<uint8_t>* buffer;
        uint64_t valid_length = 0;
        sc_core::sc_event* signal;

        void set_data(uint64_t length)
        {
            valid_length = length;
            if (signal)
            {
                signal->notify(SC_ZERO_TIME);
            }
        }

        void clear()
        {
            if (buffer)
            {
                buffer.clear();
            }

            valid_length = 0;
        }
    };

    class SlotsEngine : public sc_core::sc_module
    {
        SC_HAS_PROCESS(SlotsEngine);

    public:
        static const uint64_t slots_magic_number    = SOFT_REG_MAPPING_SLOT_DMA_MAGIC_VALUE;
        static const unsigned int maximum_slot_count= 64;

        static const size_t dma_block_size = (128 / 8);  // DMA is in 128b blocks, or 16B

        typedef RegisterMap<uint64_t>::Register RegisterT;

        enum AddressType  { input = 0, output = 1, control = 2 };
        enum DoorbellType { full = 0, done = 1 };

    private:

        // The number of slots the engine is running
        unsigned int _slot_count = 0;

        // back pointer to the catapult shell, through which the engine will initiate
        // DMA operations
        CatapultShellInterface* _shell = nullptr;

        // Role input buffers for each slot
        vector<SlotInputConfig*> _slot_config;

        // And a register map for DMA registers
        RegisterMap<uint64_t> _dma_regs;

        sc_core::sc_event _dma_doorbell_write;

        void init_dma_registers(void);

        bool write_doorbell_register(RegisterT* reg,
                                     unsigned int slot_number,
                                     DoorbellType type,
                                     uint64_t new_value);

        void dma_thread();

        static constexpr uint64_t get_doorbell_regnum(int slot, DoorbellType type)
        {
            return 0x30000ull | (uint64_t(slot) << 9) | uint64_t(type);
        }

        static constexpr uint64_t get_address_regnum(int slot, AddressType type)
        {
            return 0x20200ull | (uint64_t(slot) << 2) | uint64_t(type);
        }

        static constexpr uint64_t get_control_full_status_address(uint64_t control_address)
        {
            return reinterpret_cast<uint64_t>(&(reinterpret_cast<DMA_ISO_CONTROL_RESULT_COMBINED*>(control_address))->control_buffer.full_status);
        }

        static constexpr uint64_t get_control_done_status_address(uint64_t control_address)
        {
            return reinterpret_cast<uint64_t>(&(reinterpret_cast<DMA_ISO_CONTROL_RESULT_COMBINED*>(control_address))->control_buffer.done_status);
        }


        template<typename A, typename B, typename C> static constexpr A add_wrap(A a, B b, C max)
        {
            return ((a + b) % max);
        }

        uint64_t& get_doorbell_register(unsigned int slot, DoorbellType type)
        {
            return _dma_regs[get_doorbell_regnum(slot, type)];
        }

        uint64_t& get_address_register(unsigned int slot, AddressType type)
        {
            return _dma_regs[get_address_regnum(slot, type)];
        }

        bool find_next_full_doorbell(unsigned int& hint, uint64_t& db_value);

    public:

        SlotsEngine(sc_module_name module_name, unsigned int slot_count, CatapultShellInterface* shell);

        void reset(void);

        // attaches an input buffer and an event to a slot.
        void set_slot_config(unsigned int slot_number, SlotInputConfig* config);

        // methods for reading and writing the slot DMA registers, if slots is enabled.
        uint64_t read_dma_register(uint32_t index, string& out_message);
        void write_dma_register(uint32_t index, uint64_t value, std::string& out_message);

        void print();
    };
}