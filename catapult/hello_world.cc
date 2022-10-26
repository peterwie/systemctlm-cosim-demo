#include "hello_world.h"

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
using namespace Role;

bool HelloWorldRole::read_soft_register(uint64_t address, uint64_t& value)
{
    auto reg_type = CatapultDevice::get_address_type(address);

    uint32_t reg_index = static_cast<uint32_t>((address & CatapultDevice::soft_reg_addr_num_mask) >> CatapultDevice::soft_reg_addr_num_shift);

    if (reg_type == CatapultRegisterType::soft)
    {
        value = ((uint64_t) reg_index << 32) |  reg_index;
        cout << "HelloWorldRole: r " << std::hex << address << " softshell register 0x" << hex << reg_index << endl;
        return sizeof(uint64_t);
    }
    else // regtype is DMA
    {
        string message;
        cout << "HelloWorldRole: r " << out_hex(address, 6, false)
             << " dma register " << out_hex(reg_index, 6, false)
             << " => ";
        value =  _slots_engine.read_dma_register(reg_index, message);
        cout << out_hex(value, 16, true)
             << " [" << message << "]" << endl;

        return true;
    }
}

bool HelloWorldRole::write_soft_register(uint64_t address, uint64_t value)
{
    auto reg_type = CatapultDevice::get_address_type(address);
    uint32_t reg_index = static_cast<uint32_t>((address & CatapultDevice::soft_reg_addr_num_mask) >> CatapultDevice::soft_reg_addr_num_shift);

    if (reg_type == soft)
    {
        cout << "HelloWorldRole: write of unimplemented "
            << (reg_type == soft ? "soft" : "dma")
            << " register " << out_hex(address, 6) << endl;
    }
    else // regtype is DMA
    {
        cout << "HelloWorldRole: w " << out_hex(address, 6, false)
             << " dma register " << out_hex(reg_index, 6, false)
             << " <= " << out_hex(value, 16, true);

        string message;
        _slots_engine.write_dma_register(reg_index, value, message);

        cout << " [" << message << "]" << endl;
    }

    return true;
}
