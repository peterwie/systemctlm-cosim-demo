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

#include <iomanip>
#include <ostream>

namespace Catapult
{
    struct out_write64b
    {
        const uint64_t value;
        const size_t  length;
        const size_t  offset;

        char fill = '0';
        char blank = 'x';

        out_write64b(uint64_t v, size_t l, size_t o) : value(v), length(l), offset(o)
        {
            assert((length == sizeof(uint64_t) && offset == 0) ||
                   (length == sizeof(uint32_t) && (offset == 0 || offset == sizeof(uint32_t))));
        }
    };

    struct out_read64b
    {
        const uint64_t value;
        const size_t  length;
        const size_t  offset;

        char fill = '0';
        char blank = 'x';

        out_read64b(uint64_t v, size_t l, size_t o) : value(v), length(l), offset(o)
        {
            assert((length == sizeof(uint64_t) && offset == 0) ||
                   (length == sizeof(uint32_t) && (offset == 0 || offset == sizeof(uint32_t))));
        }
    };

    struct out_hex
    {
        const uint64_t      _value;
        const unsigned int  _width;
        const bool          _show_base;

        out_hex(uint64_t value, size_t width = 16, bool show_base = true) : _value(value), _width(width), _show_base(show_base) { }
    };

    inline std::ostream& operator<<(std::ostream& o, const out_write64b& wb)
    {
        using namespace std;

        auto old_width = o.width(wb.length * 2);
        auto old_fill  = o.fill(wb.fill);
        auto old_fmt   = o.flags(ios::hex | ios::right);

        char blanks[9] = {0};

        if (wb.length == sizeof(uint64_t))
        {
            o << wb.value;
            return o;
        }

        for (int i = 0; i < 8; i += 1)
        {
            blanks[i] = wb.blank;
        }

        uint32_t v32 = uint32_t((wb.value >> (wb.offset * 8)) & 0xfffffffful);

        if (wb.offset == 0)
        {
            o << blanks;
        }

        o << setw(8) << v32;

        if (wb.offset == 4)
        {
            o << blanks;
        }

        o.flags(old_fmt);
        o.fill(old_fill);
        o.width(old_width);

        return o;
    }

    inline std::ostream& operator<<(std::ostream& o, const out_read64b& wb)
    {
        using namespace std;

        auto old_width = o.width(wb.length * 2);
        auto old_fill  = o.fill(wb.fill);
        auto old_fmt   = o.flags(ios::hex | ios::right);

        char blanks[9] = {0};

        if (wb.length == sizeof(uint64_t))
        {
            o << wb.value;
            return o;
        }

        for (int i = 0; i < 8; i += 1)
        {
            blanks[i] = wb.blank;
        }

        uint32_t v32 = uint32_t(wb.value & 0xfffffffful);

        if (wb.offset == 0)
        {
            o << blanks;
        }

        o << setw(8) << v32;

        if (wb.offset == 4)
        {
            o << blanks;
        }

        o.flags(old_fmt);
        o.fill(old_fill);
        o.width(old_width);

        return o;
    }

    inline std::ostream& operator<<(std::ostream& o, const out_hex& h)
    {
        using namespace std;

        auto old_width = o.width(0);
        auto old_fill  = o.fill('0');
        auto old_fmt   = o.flags(ios::hex | ios::right);

        if (h._show_base) { o << "0x"; }

        o << setw(h._width) << h._value;

        o.flags(old_fmt);
        o.fill(old_fill);
        o.width(old_width);

        return o;
    }


}