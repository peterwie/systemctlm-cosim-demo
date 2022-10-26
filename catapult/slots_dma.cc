#include "catapult_device.h"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <numeric>
#include <sstream>
#include <vector>

using namespace sc_core;
using namespace sc_dt;
using namespace std;

using namespace Catapult;

SlotsEngine::SlotsEngine(sc_module_name module_name, unsigned int slot_count, CatapultShellInterface* shell) :
    sc_module(module_name),
    _slot_count(slot_count),
    _shell(shell),
    _dma_regs("dma")
{
    if (slot_count > maximum_slot_count)
    {
        throw logic_error("slot_count is larger than maximum allowed value (64)");
    }

    init_dma_registers();

    SC_THREAD(dma_thread);
}

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
        for (unsigned int type_index = AddressType::input; type_index <= AddressType::control; type_index += 1)
        {
            ostringstream name;

            uint64_t a = get_address_regnum(slot_index, AddressType(type_index));

            name << "dma."
                 << dec << setw(3) << setfill('0') << _dma_regs.size()
                 << ".addr_" << address_types[type_index]
                 << "_slot"
                 << dec << setw(3) << setfill('0') << slot_index;

            cout << "adding DMA address register " << name.str() << " at " << out_hex(a, 16, true) << endl;

            _dma_regs.add(a, name.str().c_str(), 0);
        }
    }

    // add all the doorbell registers, with callbacks for register
    // writes.
    for (unsigned int slot_index = 0; slot_index < _slot_count; slot_index += 1)
    {
        for (unsigned int type_index = 0; type_index < doorbell_types.size(); type_index += 1)
        {
            uint64_t a = get_doorbell_regnum(slot_index, DoorbellType(type_index));

            ostringstream name;

            name << "dma."
                 << dec << setw(3) << setfill('0') << _dma_regs.size()
                 << ".doorbell_" << doorbell_types[type_index]
                 << "_slot"
                 << dec << setw(3) << setfill('0') << slot_index;

            auto r = RegisterT(
                        name.str().c_str(),
                        0,
                        nullptr,    // readfn
                        [this, slot_index, type_index](uint64_t address, uint64_t new_value, RegisterT* reg) // writefn
                        {
                            return write_doorbell_register(reg,
                                                           slot_index,
                                                           DoorbellType(type_index),
                                                           new_value);
                        }
                        );

            assert(bool(r.writefn) == true);
            assert(bool(r.readfn) == false);

            auto tmp = _dma_regs.add_register(a, std::move(r));

            assert(bool(tmp.writefn) == true);
            assert(bool(tmp.readfn) == false);
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

    ostringstream m;

    if (reg->write(index, value) == 0)
    {
        m << "WRITE DROPPED";
    }
    else
    {
        m << "OK";

        if (reg->is_readonly == false)
        {
            string m2;
            // double check that the write stuck
            auto v2 = read_dma_register(index, m2);

            assert(v2 == value);
        }
    }

    m << " ptr: " << out_hex(reg, 16, true);
    message = m.str();
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
        return false;
    }

    if (reg->value != 0)
    {
        cout << "WARNING: host overwrite pending doorbell for slot " << slot_number
             << " (old value = " << out_hex(reg->value, 16, true)
             << ") with " << out_hex(reg->value, 16, true)
             << endl;
    }

    reg->value = new_value;

    if (new_value != 0)
    {
        // write the doorbell event to wake up the DMA thread.
        _dma_doorbell_write.notify(SC_ZERO_TIME);
    }

    return true;
}

// scans each slot's full-doorbell looking for a non-zero one, starting
// with after db_num, and wrapping back around to hint.
// set db_num to 0 to restart from the beginning.
// from the beginning.  If this finds a set doorbell, it clears the
// stored value.
// * returns true when it finds a slot, with db_num set to the index, and
//   db_value set to the doorbell register value.
// * returns false when it doesn't find a slot, with db_num and db_value
//   unchanged.
bool SlotsEngine::find_next_full_doorbell(unsigned int &db_num, uint64_t& db_value)
{
    assert(db_num >= 0 && db_num < _slot_count);

    // start with the slot after db_num
    unsigned int i = add_wrap(db_num, 1, _slot_count);

    // check each slot, increment the slot number, and tail-check when
    // we've looped back around to db_num.
    bool done;
    do
    {
        // i starts at db_num + 1 and wraps around.  we're done after
        // it has wrapped around to db_num AND we've checked db_num
        // once.
        done = (i == db_num);

        uint64_t dbn = get_doorbell_regnum(i, DoorbellType::full);
        uint64_t& dbv = _dma_regs[dbn];

        if (dbv != 0)
        {
            db_num = i;
            db_value = dbv;
            return true;
        }

        i = add_wrap(i, 1, _slot_count);
    }
    while(!done);

    return false;
}

void SlotsEngine::dma_thread()
{
    // index of the last busy doorbell OR the last
    // doorbell that was checked.  Setting it to
    // 63 ensures that we start the first pass looking
    // at doorbell 0.
    unsigned int slot_number = _slot_count - 1;

    while (true)
    {
        uint64_t read_count_blocks = 0;

        // TODO: check done doorbells to start any pending tx from the role
        // Scan for a non-zero doorbell, starting after the last doorbell
        // checked

        cout << "SlotsEngine: DMA engine scanning for full doorbell, starting @ " << slot_number << endl;
        if (find_next_full_doorbell(slot_number, read_count_blocks))
        {
            uint64_t read_cb  = read_count_blocks * dma_block_size;

            cout << "SlotsEngine: full db for slot " << slot_number << " detected - reading " << read_cb << "B from host" << endl;

            vector<uint32_t> input_data(read_cb / sizeof(uint32_t));

            // uint64_t input_address   = get_address_register(slot_number, AddressType::input);

            auto ian = get_address_regnum(slot_number, AddressType::input);
            auto iar = _dma_regs.find_register(ian);

            cout << "input_address register object is " << out_hex(uint64_t(iar), 16, true) << endl;
            if (iar != nullptr)
            {
                cout << "input_address register value is "  << out_hex(iar->value, 16, true) << endl;
            }

            uint64_t input_address = iar->value;

            assert(input_address != 0);

            uint64_t control_address = get_address_register(slot_number, AddressType::control);

            // start a DMA transaction
            _shell->dma_read_from_host(input_address,  input_data.data(), read_cb);
            wait(SC_ZERO_TIME);

            // clear the doorbell register.
            cout << "SlotsEngine: clearing slot << " << slot_number << " full db" << endl;
            get_doorbell_register(slot_number, full) = 0;

            // clear the full bit in the control register
            cout << "SlotsEngine: clearing slot " << slot_number << " full control bit" << endl;
            uint64_t zero;
            _shell->dma_write_to_host(&zero,
                                      get_control_full_status_address(control_address),
                                      sizeof(zero));
            wait(SC_ZERO_TIME);

            // Dump the output buffer
            const size_t line_dwords = 8;

            for (size_t dwi = 0; dwi < input_data.size(); dwi += line_dwords)
            {
                if (dwi % line_dwords == 0)
                {
                    cout << out_hex(dwi * 4, 8) << ": ";
                }

                for (size_t dwj = dwi; dwj < (dwi + line_dwords) && dwj < input_data.size(); dwj += 1)
                {
                    cout << out_hex(input_data[dwj], 8, false) << " ";
                }

                cout << endl;
            }

            cout << endl;
        }
        else
        {
            cout << "SlotsEngine: sleeping (next db scan starts with " << slot_number << ")" << endl;
            wait(_dma_doorbell_write);
        }
    }
}
