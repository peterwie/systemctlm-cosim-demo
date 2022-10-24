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

        typedef typename map<uint64_t, Register>::const_iterator const_iterator;
        typedef typename map<uint64_t, Register>::iterator iterator;
        typedef typename map<uint64_t, Register>::value_type value_type;

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

            Register(Register&& r)
            {
                name = std::move(r.name);
                is_readonly = r.is_readonly;
                value = std::move(r.value);
                readfn = std::move(r.readfn);
                writefn = std::move(r.writefn);
            }

            size_t name_width() const { return name.size(); }

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

        // the maximum width of any of the register names.  use to format output so that
        // the arrows for reads and writes line-up regardless of name length.
        size_t _max_name_width = 0;

    public:

        size_t size() const { return _map.size(); }
        bool test(uint64_t address) const { return _map.count(address) > 0; }

        size_t max_name_width() { return _max_name_width; }

        iterator begin()        { return _map.begin();  }
        iterator end()          { return _map.end();    }
        const_iterator cbegin() const { return _map.cbegin(); }
        const_iterator cend()   const { return _map.cend();   }

        const string& name() const { return _name; }

        Register& add(uint64_t address, const char* name, R value)
        {
            return add_register(address, Register(name, value));
        }

        Register& add(uint64_t address, const char* name, const ReadFnObj& rfn)
        {
            return add_register(address, Register(name, rfn));
        }

        Register& add_register(uint64_t address, Register&& r)
        {
            _max_name_width = std::max(_max_name_width, r.name_width());

            assert(_map.count(address) == 0);
            pair<RegisterMap::iterator, bool> i = _map.emplace(address, std::move(r));

            return i.first->second;
        }


        bool read_register(uint64_t address, size_t read_size, R& value)
        {
            // locate the address in the register map.
            const auto reg = _map.find(address);

            // if we did not find any match, return false.
            if (reg == _map.end())
            {
                cout << "CatapultDevice: registermap " << _name << " " << hex << address << " not found in map" << endl;
                return false;
            }


            // call the register read function
            bool result = reg->second.read(address, value);

            cout << "CatapultDevice: rmap " << _name << "  read "
                 << setw(6) << setfill('0') << hex << address << " ("
                 << setw(_max_name_width) << setfill(' ') << right << reg->second.name << ") => ";

            if (result)
            {
                cout << hex << value;
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
                cout << "CatapultDevice: registermap " << _name << " " << hex << address << " not found in map" << endl;
                return false;
            }

            // call the register write function
            bool result = reg->second.write(address, value);

            cout << "CatapultDevice: rmap " << _name << " write "
                << setw(6) << setfill('0') << hex << address
                << " (" << setw(_max_name_width) << setfill(' ') << right << reg->second.name << ") <= "
                << hex << value
                << (result ? " ok " : " err")
                << endl;

            return result;
        }

        void print_register_table(const string& address_name = "address", function<uint64_t (uint64_t)> address_transform = [](uint64_t a) { return a; })
        {
            size_t max_name_length = 0;

            int value_width = sizeof(R) * 2;
            int address_width = std::max(address_name.size(), size_t(6)); // 6 hex digits (24b)

            for (const auto& r : *this)
            {
                max_name_length = std::max(max_name_length, r.second.name.size());
            }

            cout << dec << "address_width = " << address_width << endl;
            cout << dec << name() << " register map contains " << size() << "entries:" << endl;

            cout << setw(address_width) << std::left << address_name
                << "   "
                << setw(max_name_length) << std::left << "name"
                << "   "
                << setw(value_width) << std::left << "value (hex)"
                << "   "
                << "protection"
                << endl;

            for (const auto& r : *this)
            {
                cout << hex << "0x"
                    << std::right << setfill('0') << setw(6)               << hex  << address_transform(r.first)
                    <<               setfill(' ') << setw(address_width - 8)       << "   " << "   "
                    << std::left  << setfill(' ') << setw(max_name_length)         << r.second.name
                    << " = "
                    << std::right << setfill(' ') << setw(value_width)     << hex  << r.second.value
                    << "   "
                    << std::left  << setfill(' ')                          << "(r/"<< (r.second.is_readonly ? 'o' : 'w') << ")"
                    << endl;
            }
        }

    };

}