#pragma once

#include <cstdint>
#include <ostream>
#include <iomanip>


#include "manipulators.hpp"

namespace Catapult
{
    template<typename R, typename W>
    class RegisterWidthAdapter
    {
        uint64_t _addr = UINT64_MAX;
        uint32_t _data = 0;

        R _read;
        W _write;

        bool is_low_word(uint64_t a)  { return a % 8 == 0; }
        bool is_high_word(uint64_t a) { return a % 8 == 4; }

        bool is_write_in_progress() { return _addr != UINT64_MAX; }
        bool is_next_write(uint64_t a)
        {
            assert(is_write_in_progress() == true);
            return a == _addr + 4;
        }

    public:
        RegisterWidthAdapter() { reset(); }

        void set_read(const R& r)  { _read  = r; }
        void set_write(const W& w) { _write = w; }

        void reset()
        {
            _addr = UINT64_MAX;
            _data = 0;
        }

        size_t write(uint64_t address, size_t length, uint64_t value)
        {
            assert((length == 4 && (address % 4 == 0)) ||
                   (length == 8 && (address % 8 == 0)));

            // Check the cases where we don't write through to the role
            if (is_write_in_progress() == false && length == 4 && is_low_word(address))
            {
                // write to low word of a 64b address - cache and accept the write
                _addr = address;
                _data = value;
                // cout << "RegisterAdapter: lo-word write @ " << out_hex(address, 6) << " saved" << endl;
                return 4;
            }

            if (is_write_in_progress() == false && length == 4 && is_high_word(address))
            {
                // out-of-sequence 32b write to a new low-word.  Drop the previous write, stash the new one
                cout << "WARNING: OOS 32b write to " << out_hex(address) << " with no previous low-word write." << endl;
                cout << "         dropping write" << endl;
                return 0;
            }

            if (is_write_in_progress() == true && length == 4 && is_low_word(address))
            {
                // out-of-sequence 32b write to a new low-word.  Drop the previous write, stash the new one
                cout << "WARNING: OOS 32b write to " << out_hex(address) << " after partial write to " << out_hex(_addr) << endl;
                cout << "         dropping in-progress write, staging new write" << endl;
                return 0;
            }

            if (is_write_in_progress() == true && length == 4 && is_high_word(address) && is_next_write(address) == false)
            {
                // out-of-sequence 32b write to a different high-word than expected.  Drop both writes.
                cout << "WARNING: unaligned, OOS 32b write to " << out_hex(address) << " after partial write to " << out_hex(_addr) << endl;
                cout << "         dropping both in-progress write and unaligned write" << endl;
                return 0;
            }

            // Now check other error cases but where we will pass something through to the shell.

            if (is_write_in_progress() == true && length == 8)
            {
                // 64b write following a stashed 32b write.  Drop the old write, let the new one through.
                cout << "WARNING: OOS 64b write to " << out_hex(address) << " after partial write to " << out_hex(_addr) << endl;
                cout << "         dropping in-progress write, passing through new write" << endl;
                reset();
            }
            else if (is_write_in_progress() == true && is_next_write(address))
            {
                // cout << "RegisterAdapter: hi-word write @ " << out_hex(address, 6) << " detected" << endl;
                assert(length == 4);
                value = (value << 32) | _data;
                address = _addr;
                reset();
            }

            // cout << "RegisterAdapter: write " << out_hex(value, 8, true) << " @ " << out_hex(address, 6) << " posted" << endl;
            _write(address, value);
            return length;
        }

        size_t read(uint64_t address, size_t length, uint64_t& value)
        {
            assert((length == 4 && (address % 4 == 0)) ||
                   (length == 8 && (address % 8 == 0)));

            // Check the cases where we don't write through to the role
            if (is_write_in_progress() == true)
            {
                // out-of-sequence read - warn about potential tearing
                cout << "WARNING: read of " << out_hex(address) << " overlapping with pending "
                     << "write to " << out_hex(_addr) << " - may cause data tearing" << endl;
            }

            if (_read(address, value))
            {
                if (length == 8)
                {
                    return 8;
                }

                if (address % 8 == 4)
                {
                    value >>= 32;
                }

                value &= UINT32_MAX;

                return 4;
            }
            else
            {
                return 0;
            }
        }
    };
}
