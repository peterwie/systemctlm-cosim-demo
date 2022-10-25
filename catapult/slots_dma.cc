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

#include "catapult_device.h"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <numeric>
#include <sstream>

using namespace sc_core;
using namespace sc_dt;
using namespace std;

using namespace Catapult;

void SlotsEngine::reset()
{
    _dma_regs.reset();
}

void SlotsEngine::init_dma_registers()
{
    // DMA register addresses are pre-shifted by the soft-regsiter r/w handlers
    // In absolute address terms, a DMA register has:
    //  [23:20] = 1001b (vs. 1000b for a soft register)
    //
    // then for individual registers
    //  [19:12] = 00000000b
    //  [11:3]  = register number
    //  [2:0]   = 0 (ignored)
    //
    // for the address registers
    //  [19:12] = 00000001b
    //  [11:5]  = 7b slot number
    //  [4:3]   = address type (input, output or control respectively)
    //
    // and for the doorbell registers
    //  [19]    = 1
    //  [18:12] = 7b slot number
    //  [11:3]  = 0 for full doorbells, 1 for done doorbells
    //
    // the softreg handlers strip off bit 23, and then right shift by 3.  So that
    // gives us 18b total
    //  [17] = 1   (0x2'0000)
    //
    // individual regs:     [16:9]  = 0
    //                      [8:0]   = register number
    //
    // address registers:   [16:9]  = 000000001b    (0x200)
    //                      [8:2]   = 7b slot number
    //                      [1:0]   = address type
    //
    // doorbell registers:  [16]    = 1b    (0x1'0000)
    //                      [15:9]  = 7b slot number
    //                      [8:0]   = 0 for full doorbells, 1 for done doorbells

    _dma_regs.add(0x1c7f0, "dma.000.magicvalue0",       slots_magic_number );
    _dma_regs.add(0x20000, "dma.000.magicvalue",        slots_magic_number );
    _dma_regs.add(0x20001, "dma.001.buffer_size",                        0 );
    _dma_regs.add(0x20002, "dma.002.num_buffers",               _slot_count);
    _dma_regs.add(0x20003, "dma.003.num_gp_registers",                 128 );
    _dma_regs.add(0x20004, "dma.004.merged_slots",                       0 );
    _dma_regs.add(0x20005, "dma.005.isr_rate_limit_threshold",           0 );
    _dma_regs.add(0x20006, "dma.006.isr_rate_limit_multiplier",          0 );
    _dma_regs.add(0x20007, "dma.007.unused",                             0 );
    _dma_regs.add(0x20008, "dma.008.slot_full_status0",                  0 );
    _dma_regs.add(0x20009, "dma.009.slot_full_status1",                  0 );
    _dma_regs.add(0x20010, "dma.010.slot_done_status0",                  0 );
    _dma_regs.add(0x20011, "dma.011.slot_done_status1",                  0 );
    _dma_regs.add(0x20012, "dma.012.slot_pend_status0",                  0 );
    _dma_regs.add(0x20013, "dma.013.slot_pend_status1",                  0 );
    _dma_regs.add(0x20016, "dma.016.health_diag_version",                0 );
    _dma_regs.add(0x20017, "dma.017.health_diag_full_status",            0 );
    _dma_regs.add(0x20018, "dma.018.health_diag_sos_cpu_to_fpga",        0 );
    _dma_regs.add(0x20019, "dma.019.health_diag_sos_fpga_to_cpu",        0 );
    _dma_regs.add(0x20020, "dma.020.health_diag_sos_interrupt_mode",     0 );
    _dma_regs.add(0x20021, "dma.021.timeout_interval_setting",           0 );
    _dma_regs.add(0x20022, "dma.022.timeout_count",                      0 );
    _dma_regs.add(0x20023, "dma.023.any_avail_slot_ctrl",                0 );
    _dma_regs.add(0x20024, "dma.024.any_avail_slot_test",                0 );

    array<const char*, 3> address_types = {"input", "output", "ctrl"};
    array<const char*, 2> doorbell_types = {"full", "done"};

    // add all the address registers.

    for (unsigned int slot_index = 0; slot_index < _slot_count; slot_index += 1)
    {
        for (unsigned int type_index = 0; type_index < address_types.size(); type_index += 1)
        {
            ostringstream name;

            uint64_t a = 0x20200 | (slot_index << 2) | type_index;

            name << "dma."
                 << dec << setw(3) << setfill('0') << _dma_regs.size()
                 << ".addr_" << address_types[type_index]
                 << "_slot"
                 << dec << setw(3) << setfill('0') << slot_index;

            _dma_regs.add(a, name.str().c_str(), 0);
        }
    }

    // add all the doorbell registers, with callbacks for register
    // writes.
    for (unsigned int slot_index = 0; slot_index < _slot_count; slot_index += 1)
    {
        for (unsigned int type_index = 0; type_index < doorbell_types.size(); type_index += 1)
        {
            uint64_t a = 0x30000 | (slot_index << 9) | type_index;

            ostringstream name;

            name << "dma."
                 << dec << setw(3) << setfill('0') << _dma_regs.size()
                 << ".doorbell_" << doorbell_types[type_index]
                 << "_slot"
                 << dec << setw(3) << setfill('0') << slot_index;

            auto r = RegisterT(
                        name.str().c_str(),
                        0,
                        [](uint64_t /* address */, uint64_t& output_value, RegisterT* reg)
                        {
                            output_value = reg->value;
                            return true;
                        },
                        [this, slot_index, type_index](uint64_t address, uint64_t new_value, RegisterT* reg)
                        {
                            return write_doorbell_register(reg,
                                                           slot_index,
                                                           DoorbellType(type_index),
                                                           new_value);
                        }
                        );

            _dma_regs.add(a, name.str().c_str(), 0);
        }
    }
}

uint64_t SlotsEngine::read_dma_register(uint32_t index, string& message)
{
    RegisterMap<uint64_t>::Register* reg = _dma_regs.find_register(index);

    if (reg == nullptr)
    {
        message = "NOT IMPLEMENTED";
        return 0;
    }

    uint64_t value = 0;

    if (reg->read(index, value) == 0)
    {
        message = "READ FAILED";
        return 0;
    }

    message = "OK";
    return value;
}


void SlotsEngine::write_dma_register(uint32_t index, uint64_t value, string& message)
{
    RegisterMap<uint64_t>::Register* reg = _dma_regs.find_register(index);

    if (reg == nullptr)
    {
        message = "NOT IMPLEMENTED";
        return;
    }

    if (reg->write(index, value) == 0)
    {
        message = "WRITE DROPPED";
    }
    else
    {
        message = "OK";
    }
}

void SlotsEngine::print()
{
    _dma_regs.print_register_table(
        "softreg number"// ,
        // [](uint64_t a) { return (0x800000 | (a << 3)); }
        );
}


bool SlotsEngine::write_doorbell_register(RegisterT* reg,
                                          unsigned int slot_number,
                                          DoorbellType type,
                                          uint64_t new_value)
{
    // each doorbell (full & done) has its own register

    // Check if the slot is implemented.  If not then drop the write and return.

    if (slot_number >= _slot_count)
    {
        cout << "SlotsEngine: slot " << slot_number
             << " not implemented - dropping " << ((type == done) ? "done" : "full")
             << " doorbell write" << endl;
    }

    return true;
}