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

#include <functional>
#include <iomanip>
#include <map>
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

// A support structure for a map of registers of size R
namespace Catapult
{
    using namespace std;

    template<typename R>
    class RegisterMap
    {
    public:
        struct Register;

        typedef bool (ReadFn)(uint64_t address,  R& output_value, Register* reg);
        typedef bool (WriteFn)(uint64_t address, R new_value, Register* reg);

        typedef function<ReadFn>  ReadFnObj;
        typedef function<WriteFn> WriteFnObj;

        const class WriteableRegisterT { const uint32_t _dummy = 0; } WriteableRegister;

        // a register pair provides both access methods and storage for a register.
        // by default a register is read-only with a stored value of (presumably integer) type R.
        // the read() function reads the stored value.
        // if writable() the write function updates the stored value
        // optionally the creator can provide read and write callable objects of their own
        // which read() and write() will invoke internally instead of blindly using the stored value.
        // if the creator only provides a write callback, the callback should update the stored
        // value and the read() function will return it.
        //
        // the read callback can return true to indicate when a read is somehow invalid, in which
        // case the read function will also return false.
        struct Register
        {
            Register(const Register& r) :
                name(r.name),
                is_readonly(r.is_readonly),
                value(r.value),
                readfn(r.readfn),
                writefn(r.writefn)
            {

            }

            string name;
            bool is_readonly = true;

            R value = 0;

            ReadFnObj  readfn;
            WriteFnObj writefn;

            Register()                                        { }
            Register(const char* n)                         : name(n) { }
            Register(const char* n, R v)                    : name(n) { value = v; }
            Register(const char* n, WriteableRegisterT)     : name(n), is_readonly(false) { }
            Register(const char* n, R v, WriteableRegisterT): name(n), is_readonly(false) { value = v; }

            Register(const char* n, const ReadFnObj& rfn)   : name(n) { readfn = rfn; }
            Register(const char* n, const WriteFnObj& wfn)  : name(n), is_readonly(false) { writefn = wfn; }

            Register(const char* n, const ReadFnObj& rfn, const WriteFnObj& wfn) : name(n), is_readonly(false)
            {
                readfn = rfn;
                writefn = wfn;
            }

            Register(const char* n, R v, const ReadFnObj& rfn) : name(n), is_readonly(true) { value = v; readfn = rfn; }
            Register(const char* n,
                    R v,
                    const ReadFnObj& rfn,
                    const WriteFnObj& wfn)
                : name(n), is_readonly(false)
            {
                readfn = rfn;
                writefn = wfn;
                return;
            }

            bool read(uint64_t address, R& output_value)
            {
                if (readfn)
                {
                    return readfn(address, output_value, this);
                }
                else
                {
                    output_value = value;
                    return true;
                }
            }

            bool write(uint64_t address, R new_value)
            {
                if (writefn)
                {
                    return writefn(address, new_value, this);
                }
                else if (is_readonly == false)
                {
                    value = new_value;
                }

                return true;
            }
        };

        RegisterMap(const string& map_name) : _name(map_name) { }

    private:
        string _name;

        // Simple register reads ... map contains a static 32b value for the register.
        map<uint64_t, Register> _map;

    public:

        size_t size() const { return _map.size(); }
        bool test(uint64_t address) { return _map.count(address) > 0; }

        Register& add(uint64_t address, const char* name, R value)
        {
            return _map[address] = Register(name, value);
        }

        Register& add(uint64_t address, const char* name, const ReadFnObj& rfn)
        {
            return _map[address] = Register(name, rfn);
        }

        bool read_register(uint64_t address, size_t read_size, R& value)
        {
            // locate the address in the register map.
            const auto reg = _map.find(address);

            // if we did not find any match, return false.
            if (reg == _map.end())
            {
                cout << "CatapultDevice: registermap " << _name << " " << showbase << hex << address << " not found in map" << endl;
                return false;
            }


            // call the register read function
            bool result = reg->second.read(address, value);

            cout << "CatapultDevice: rmap " << _name << "  read "
                 << showbase << setw(6) << setfill('0') << hex << address
                 << " (" << reg->second.name << ") => ";

            if (result)
            {
                cout << showbase << hex << value;
            }
            else
            {
                cout << "(no data)";
            }

            cout << endl;
            return result;
        }

        Register* find_register(uint64_t address)
        {
            // locate the address in the register map.
            const auto reg = _map.find(address);

            if (reg == _map.end())
            {
                return nullptr;
            }
            else
            {
                return &(reg->second);
            }
        }

        bool write_register(uint64_t address, size_t read_size, R value)
        {
            // locate the address in the register map.
            const auto reg = _map.find(address);

            if (reg == _map.end())
            {
                cout << "CatapultDevice: registermap " << _name << " " << showbase << hex << address << " not found in map" << endl;
                return false;
            }

            // call the register write function
            bool result = reg->second.write(address, value);

            cout << "CatapultDevice: rmap " << _name << " write "
                << showbase << setw(6) << setfill('0') << hex << address
                << " (" << reg->second.name << ") <= "
                << showbase << hex << value
                << (result ? " ok " : " err")
                << endl;

            return result;
        }
    };

}