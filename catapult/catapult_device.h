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

#include "manipulators.hpp"

#include "register_map.hpp"
#include "register_adapter.hpp"
#include "slots_dma.h"

namespace Catapult
{
    struct CatapultDeviceOptions
    {
        bool enable_slots_dma = true;
        bool dump_regs = false;
    };

    struct CatapultShellInterface
    {
        virtual void dma_read_from_host(uint64_t source_address, void* destination_address, uint64_t transfer_cb) = 0;
        virtual void dma_write_to_host(void* source_address, uint64_t destination_address, uint64_t transfer_cb) = 0;
    };

    class CatapultDevice : public sc_core::sc_module, CatapultShellInterface
    {
    public:
        // core addresses are the 16MB of memory defined in section 9 of the shell specifications
        static const uint64_t core_address_valid_mask= 0x0000000000ffffff;  // [23:00] allowed
        static const uint64_t core_address_zero_mask = ~core_address_valid_mask;         // [63:24] must be 0

        // core address bits to test for a shell or soft register (including DMA registers)
                                                    // high b  31 23 15 07
                                                    // low  b  24 16 08 00
        static const uint64_t core_address_type_mask = 0x0000000000f00000 | core_address_zero_mask;  // bits [23:20]
        static const uint64_t shell_reg_addr_test    = 0x0000000000000000;  // bits [23:20] = 0b0000
        static const uint64_t dma_alias_addr_test    = 0x0000000000700000;  // bits [23:20] = 0b0111 // i think this is an alias of the DMA space
        static const uint64_t soft_reg_addr_test     = 0x0000000000800000;  // bits [23:20] = 0b1000
        static const uint64_t dma_reg_addr_test      = 0x0000000000900000;  // bits [23:20] = 0b1001

        static const uint64_t soft_reg_addr_num_mask = 0x00000000001ffff8;  // bits [20:3]
        static const uint64_t dma_reg_addr_num_mask  = 0x00000000000ffff8;  // bits [19:3]
        static const uint64_t soft_reg_offset_mask   = 0x0000000000000007;  // bits [2:0]
        static const int      soft_reg_addr_num_shift= 3;

        static const uint32_t soft_reg_64b_support_magic_number = 0x50F750F7;

        static const uint64_t mmio_size              = core_address_valid_mask + 1;

        static const uint64_t mmio_bad_value         = 0xdeadbeefdeadbeef;

        // register type enum, as an encoded 16b value.
        // the top 4b are 0 if bits [63:24] of the address are 0, and 0001 otherwise
        // the next 4b are bits [23:20] of the address
        // The bottom 8b are the register size in bytes (4 or 8)
        //
        //
        enum CatapultRegisterType : uint16_t
        {
            invalid  = 0,
            external = 0x1008,
            shell    = 0x0004,
            soft     = 0x0808,
            dma      = 0x0908
        };

        CatapultRegisterType get_address_type(uint64_t address);

        size_t get_register_size(CatapultRegisterType type)
        {
            return static_cast<size_t>(type & 0x00ff);
        };

        // member variables
        tlm_utils::simple_target_socket<CatapultDevice> target_socket;
        tlm_utils::simple_initiator_socket<CatapultDevice> initiator_socket;

        CatapultDeviceOptions options;

        // Constructors
        CatapultDevice(sc_core::sc_module_name name, const CatapultDeviceOptions& options);

        void reset();

        virtual void dma_read_from_host(uint64_t source_address, void* destination_address, uint64_t transfer_cb) override;
        virtual void dma_write_to_host(void* source_address, uint64_t destination_address, uint64_t transfer_cb) override;

    private:

        // A register map for shell/legacy regs
        RegisterMap<uint32_t> _shell_regs;

        // A 32b-64b adapter for soft register writes
        // When a 32b write comes to offset 0 of a 64b soft register, the address and
        // data are stored in the two fields.  The next write should be a 32b write
        // at a 4B offset.  If that address is write_address + 4, then this composes
        // and commits a 64b write and commits it to the specified softreg.
        // If it's a write to any other address, this prints an error and discards
        // the previous 32b write.

        RegisterWidthAdapter<
            std::function<bool (uint64_t, uint64_t&)>,
            std::function<bool (uint64_t, uint64_t)>
            > _softreg_width_adapter;

        SlotsEngine _slots_engine;

        void init_registers(void);

        void init_shell_registers(void);

        virtual void b_transport(tlm::tlm_generic_payload& trans, sc_time& delay);

        // Reads a 32b shell register.  Returns false if the register address is
        // invalid or unimplemented, true if it's valid.  If valid, value contains
        // the result, otherwise it should be untouched.
        size_t  read_shell_register(uint64_t address, size_t size, uint64_t& value);
        size_t write_shell_register(uint64_t address, size_t size, uint64_t value);

        // reads a 64b soft register in the soft register range.  Unimplemented softregs return -1
        // this includes any potential DMA registers.  Note this goes through a
        // width adapter, which takes care of converting partial 64b r/w into complete
        // ones.
        bool    read_soft_register(uint64_t address, uint64_t& value);
        bool   write_soft_register(uint64_t address, uint64_t  value);

        // reads a register outside of the core registers.
        // returns true if the register is implemented, and stores the value to return in value
        // returns false and leaves value untouched otherwise.
        size_t  read_external_register(uint64_t address, size_t length, uint64_t& value);
        size_t write_external_register(uint64_t address, size_t length, uint64_t  value);

        size_t  read_unimplemented_register(uint64_t address, size_t size, uint64_t& value);
        size_t write_unimplemented_register(uint64_t address, size_t size, uint64_t value);

        // uses the simulation time to generate a 64b 100MHz counter and returns
        // either the low 32b or the high 32b (depending on low_part)
        uint64_t get_cycle_counter();

        // tests a masked address against an expected value and returns true if they match,
        // also  returns the value tested in 'value'
        // if 'name' is non-null, prints a testing message to the console.
        bool test_addr(const char* name, uint64_t address, uint64_t mask, uint64_t expected, uint64_t& value);
    };

    inline std::ostream& operator<<(CatapultDevice::CatapultRegisterType t, std::ostream& o)
    {
        switch (t)
        {
            case CatapultDevice::CatapultRegisterType::invalid:   { o << "invalid"; break; }
            case CatapultDevice::CatapultRegisterType::external:  { o << "external"; break; }
            case CatapultDevice::CatapultRegisterType::shell:     { o << "shell"; break; }
            case CatapultDevice::CatapultRegisterType::soft:      { o << "soft"; break; }
            case CatapultDevice::CatapultRegisterType::dma:       { o << "dma"; break; }
            default:        { o << "unknown(" << (int) t << ")"; break; }
        }
        return o;
    }

}