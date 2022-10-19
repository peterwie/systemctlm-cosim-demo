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

#include <ctime>
#include <chrono>

using namespace sc_core;
using namespace sc_dt;
using namespace std;

using namespace Catapult;

const char* tlm_commands[] = {
    "read  ",
    "write ",
    "ignore",
};

CatapultDevice::CatapultDevice(sc_core::sc_module_name name, const CatapultDeviceOptions& opts) :
    sc_module(name),
    tgt_socket("tgt-socket"),
    options(opts),
    _shell_regs("core")
{
    tgt_socket.register_b_transport(this, &CatapultDevice::b_transport);

    init_registers();
}

void CatapultDevice::b_transport(tlm::tlm_generic_payload& trans,
        sc_time& delay)
{
    // tlm::tlm_command cmd = trans.get_command();
    unsigned char *data = trans.get_data_ptr();
    size_t len = trans.get_data_length();
    uint64_t addr = trans.get_address();

    enum tlm::tlm_command cmd = trans.get_command();
    const char* cmd_name = cmd <= tlm::TLM_IGNORE_COMMAND ? tlm_commands[cmd] : "??????";

    auto log_inbound = [&]() -> std::ostream& {
        return (cout << "CatapultDevice: " << cmd_name << " cmd @ 0x" << std::hex << addr << " for 0x" << std::hex << len << " bytes");
    };

    if (len != 4 && len != 8)
    {
        log_inbound() << " - invalid length" << endl;
        trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
        return;
    }

    if (trans.get_byte_enable_ptr())
    {
        log_inbound() << " - byte_enable_ptr not supported" << endl;
        trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
        return;
    }

    auto reg_type = get_address_type(addr);

    size_t (CatapultDevice::* read_fn)(uint64_t, size_t, uint64_t&) =  &CatapultDevice::read_unimplemented_register;
    size_t (CatapultDevice::* write_fn)(uint64_t, size_t, uint64_t) = &CatapultDevice::write_unimplemented_register;

    switch (reg_type)
    {
        case external:
        {
            read_fn = &CatapultDevice::read_external_register;
            write_fn = &CatapultDevice::write_external_register;
            break;
        }

        case shell:
        {
            read_fn = &CatapultDevice::read_shell_register;
            write_fn = &CatapultDevice::write_shell_register;
            break;
        }

        case soft:
        case dma:
        {
            read_fn = &CatapultDevice::read_soft_register;
            write_fn = &CatapultDevice::write_soft_register;
            break;
        }

        case invalid:
        {
            read_fn =  &CatapultDevice::read_unimplemented_register;
            write_fn  = &CatapultDevice::write_unimplemented_register;
            break;
        }
    }


    if (trans.is_read())
    {
        uint64_t value = 0xdeadbeefdeadbeef;

        size_t bytes_read = (this->*read_fn)(addr, len, value);

        if (bytes_read == 0)
        {
            cout << "CatapultDevice: read completed with length " << bytes_read /* << " and value " << std::hex << value */ << endl;
        }
        else
        {
            memcpy(reinterpret_cast<void*>(data),
                reinterpret_cast<const void *>(&value),
                max(len, bytes_read));
        }
    }
    else if (trans.is_write())
    {
        uint64_t value = 0;
        memcpy(reinterpret_cast<void *>(&value),
               reinterpret_cast<void*>(data),
               min(len, sizeof(value)));
        size_t bytes_written = (this->*write_fn)(addr, len, value);
        cout << "CatapultDevice: write completed with length " << bytes_written << " and value " << std::hex << value << endl;
    }
}

size_t CatapultDevice::read_external_register(uint64_t address, size_t length, uint64_t& value)
{
    cout << "CatapultDevice: read " << std::showbase << std::hex << address << " past end of valid registers" << endl;
    return 0;
}

size_t CatapultDevice::write_external_register(uint64_t address, size_t, uint64_t)
{
    cout << "CatapultDevice: write " << std::showbase << std::hex << address << " past end of valid registers" << endl;
    return 0;
}

size_t CatapultDevice::read_unimplemented_register(uint64_t address, size_t, uint64_t&)
{
    cout << "CatapultDevice: read of unimplemented register 0x" << hex << address << endl;
    return 0;
}

size_t CatapultDevice::write_unimplemented_register(uint64_t address, size_t, uint64_t)
{
    cout << "CatapultDevice: write of unimplemented register 0x" << hex << address << endl;
    return 0;
}

size_t CatapultDevice::read_soft_register(uint64_t address, size_t length, uint64_t& value)
{
    auto reg_type = get_address_type(address);

    if (reg_type == soft)
    {
        auto softreg = static_cast<uint32_t>((address & soft_reg_addr_num_mask) >> soft_reg_addr_num_shift);
        cout << "CatapultDevice: read 0x" << std::hex << address << " softshell register 0x" << hex << softreg << endl;
        value = softreg;
        return sizeof(uint64_t);
    }
    else // regtype is DMA
    {
        auto dmareg = static_cast<uint32_t>((address & dma_reg_addr_num_mask) >> soft_reg_addr_num_shift);
        cout << "CatapultDevice: read 0x" << std::hex << address << " DMA register 0x" << hex << dmareg << endl;
        value = 0;
        return sizeof(uint64_t);
    }
}

size_t CatapultDevice::write_soft_register(uint64_t address, size_t length, uint64_t value)
{
    auto reg_type = get_address_type(address);

    cout << "CatapultDevice: write of unimplemented "
         << (reg_type == soft ? "soft" : "dma")
         << " register 0x"
         << hex << address << endl;
    return 0;
}

size_t CatapultDevice::read_shell_register(uint64_t address, size_t length, uint64_t& value)
{
    uint32_t value32;

    // valid shell register addresses all end with 0x4 - something about trying
    // to block 64b reads of the shell registers.
    if ((address & 0x7) != 0x4)
    {
        value = 0;
        return length;
    }

    if (_shell_regs.read_register(address, sizeof(uint32_t), value32))
    {
        value = value32;
        return sizeof(uint32_t);
    }
    else
    {
        value = static_cast<uint64_t>(-1);
        return length;
    }
}

size_t CatapultDevice::write_shell_register(uint64_t address, size_t length, uint64_t value)
{
    if (_shell_regs.write_register(address, sizeof(uint32_t), value))
    {
        return sizeof(uint32_t);
    }
    else
    {
        return 0;
    }
}

void CatapultDevice::init_registers()
{
    _shell_regs.add(0x0034, "shell.000.control",           0x00000000 );
    _shell_regs.add(0x0134, "shell.001.unused",            0x00000000 );
    _shell_regs.add(0x0234, "shell.002.unused",            0x00000000 );
    _shell_regs.add(0x0334, "shell.003.unused",            0x00000000 );
    _shell_regs.add(0x0434, "shell.004.network_status",    0x00000000 );
    _shell_regs.add(0x0534, "shell.005.network_error",     0x00000000 );
    _shell_regs.add(0x0634, "shell.006.pcie0_tlp_error",   0x0000ff00 );
    _shell_regs.add(0x0734, "shell.007.pcie0_tlp_status",  0x00000000 );
    _shell_regs.add(0x0834, "shell.008.pcie1_tlp_error",   0x00000000 );
    _shell_regs.add(0x0934, "shell.009.pcie1_tlp_status",  0x00000000 );
    _shell_regs.add(0x0A34, "shell.010.unused",            0x00000000 );
    _shell_regs.add(0x0B34, "shell.011.unused",            0x00000000 );
    _shell_regs.add(0x0C34, "shell.012.unused",            0x00000000 );
    _shell_regs.add(0x0D34, "shell.013.unused",            0xaaaaaaaa );
    _shell_regs.add(0x0E34, "shell.014.unused",            0xaaaaaaaa );
    _shell_regs.add(0x0F34, "shell.015.unused",            0x00000000 );

    _shell_regs.add(0x1034, "shell.016.unused",            0xaaaaaaaa );
    _shell_regs.add(0x1134, "shell.017.unused",            0xaaaaaaaa );
    _shell_regs.add(0x1234, "shell.018.unused",            0x00000000 );
    _shell_regs.add(0x1334, "shell.019.tor_tx_pcounter",   0x00000000 );
    _shell_regs.add(0x1434, "shell.020.tor_rx_pcounter",   0x00000000 );
    _shell_regs.add(0x1534, "shell.021.tor_rxfcs_counter", 0x00000000 );
    _shell_regs.add(0x1634, "shell.022.tor_ldown_counter", 0x00000000 );
    _shell_regs.add(0x1734, "shell.023.tor_tx_pcounter",   0x00000000 );
    _shell_regs.add(0x1834, "shell.024.nic_rx_pcounter",   0x00000000 );
    _shell_regs.add(0x1934, "shell.025.nic_rxfcs_counter", 0x00000000 );
    _shell_regs.add(0x1A34, "shell.026.nic_ldown_counter", 0x00000000 );
    _shell_regs.add(0x1B34, "shell.027.nic_tor_debug0",    0x00000000 );
    _shell_regs.add(0x1C34, "shell.028.nic_tor_debug1",    0x00000000 );
    _shell_regs.add(0x1D34, "shell.029.nic_tor_debug2",    0x00e01400 );
    _shell_regs.add(0x1E34, "shell.030.tor_tx_psop_ctr",   0x00000000 );
    _shell_regs.add(0x1F34, "shell.031.tor_rx_psop_ctr",   0x00000000 );

    _shell_regs.add(0x2034, "shell.032.nic_tx_psop_ctr",   0x00000000 );
    _shell_regs.add(0x2134, "shell.033.nic_rx_psop_ctr",   0x00000000 );
    _shell_regs.add(0x2234, "shell.034.pcie_dma_health",   0x00000000 );
    _shell_regs.add(0x2334, "shell.035.tor_tx_fcsdrop_ctr",0x00000000 );
    _shell_regs.add(0x2434, "shell.036.nic_tx_fcsdrop_ctr",0x00000000 );
    _shell_regs.add(0x2534, "shell.037.tor_tx_errdrop_ctr",0x00000000 );
    _shell_regs.add(0x2634, "shell.038.nic_tx_errdrop_ctr",0x00000000 );
    _shell_regs.add(0x2734, "shell.039.unused",            0x00000000 );
    _shell_regs.add(0x2834, "shell.040.legacy_net_test0",  0xaaaaaaaa );
    _shell_regs.add(0x2934, "shell.041.legacy_net_test1",  0xaaaaaaaa );
    _shell_regs.add(0x2A34, "shell.042.nic_mac_health",    0x000c0000 );
    _shell_regs.add(0x2B34, "shell.043.tor_mac_health",    0x100c5002 );
    _shell_regs.add(0x2C34, "shell.044.qsfp_retimer_hlth", 0x00000000 );
    _shell_regs.add(0x2D34, "shell.045.slim40g_nic_health",0x00000000 );
    _shell_regs.add(0x2E34, "shell.046.slim40g_tor_health",0x00000000 );
    _shell_regs.add(0x2F34, "shell.047.slim40g_version",   0x00000000 );

    _shell_regs.add(0x3034, "shell.048.unused",            0x00000000 );
    _shell_regs.add(0x3134, "shell.049.tor_rx_pdrop_ctr",  0x00000000 );
    _shell_regs.add(0x3234, "shell.050.nic_rx_pdrop_ctr",  0x00000000 );
    _shell_regs.add(0x3334, "shell.051.pcie_telemetry",    0x00000000 );
    _shell_regs.add(0x3434, "shell.052.retimer_dbg_write", 0x00000000 );
    _shell_regs.add(0x3534, "shell.053.retimer_dbg_read",  0x00000000 );
    _shell_regs.add(0x3634, "shell.054.ddr_reset_ctrl_in", 0x00000000 );
    _shell_regs.add(0x3734, "shell.055.ddr_reset_ctrl_out",0x00000000 );
    _shell_regs.add(0x3834, "shell.056.board_revision",    0x00000000 );
    _shell_regs.add(0x3934, "shell.057.shl_patch_board_id",0x000d00d0 );   // making up delta shell revision & board ID
    _shell_regs.add(0x3A34, "shell.058.shell_release_ver", 0x00010001 );
    _shell_regs.add(0x3B34, "shell.059.build_info",
        [](uint64_t, uint32_t& v, decltype(_shell_regs)::Register*)
        {
            SHELL_BUILD_INFO_REGISTER r = { 0 };

            auto t = std::time(nullptr);
            auto tm = std::localtime(&t);

            r.u.verbump = 0;
            r.u.day = tm->tm_mday;
            r.u.month = tm->tm_mon;
            r.u.year = tm->tm_year - 2013;
            r.u.clean = 1;
            r.u.tfsbuild = 1;

            v = r.as_ulong;

            return true;
        }
        );
    _shell_regs.add(0x3C34, "shell.060.shell_src_version", 0xa311adcf );
    _shell_regs.add(0x3D34, "shell.061.asl_version",       0x00020000 );
    _shell_regs.add(0x3E34, "shell.062.chip_id0",          0x89abcdef );
    _shell_regs.add(0x3F34, "shell.063.chip_id1",          0x01234567 );

    _shell_regs.add(0x4034, "shell.064.shell_id",          0x00bed70c ); // 0x00de17a0 );
    _shell_regs.add(0x4134, "shell.065.role_version",      0xfacecafe );

    _shell_regs.add(0x4234, "shell.066.cycle_counter0",
        [this](uint64_t, uint32_t& v, decltype(_shell_regs)::Register*)
        {
            auto tmp = this->get_cycle_counter();
            v = static_cast<uint32_t>(tmp & 0x00000000fffffffful);
            return true;
        }
        );
    _shell_regs.add(0x4334, "shell.067.cycle_counter1",
        [this](uint64_t, uint32_t& v, decltype(_shell_regs)::Register*)
        {
            auto tmp = this->get_cycle_counter();
            v = static_cast<uint32_t>((tmp & 0xffffffff00000000) >> 32);
            return true;
        }
        );

    _shell_regs.add(0x4434, "shell.068.shell_status",
        []() {
            SHELL_STATUS_REGISTER r = { .as_ulong = 0 };
            r.DDRHealthy = 1;
            r.CorePLLLocked = 1;
            r.DDRPLLLocked = 1;
            return r.as_ulong;
        }());

    _shell_regs.add(0x4534, "shell.069.pcie_link_status",  0x00000000 );
    _shell_regs.add(0x4634, "shell.070.role_status",       0x01233210 );
    _shell_regs.add(0x4734, "shell.071.temperature_status",0x1c1a1b00 );

    _shell_regs.add(0x4834, "shell.072.capabilities",      []() {
        SHELL_CAPABILITIES_REGISTER r = { .as_ulong = 0 };
        r.DDRCoreEnabled = 0;
        r.NetworkPortNICEnabled = 0;
        r.NetworkPortTOREnabled = 0;
        r.PCIeHIP0Enabled = 0;
        r.PCIeHIP1Enabled = 0;
        r.SoftRegisters64BitEnabled = 1;
        r.NetworkServicesEnabled = 0;
        r.ExtendedASMIEnabled = 1;
        r.NetworkPortSOCEnabled = 0;
        r.NetworkPortRMKEnabled = 0;
        return r.as_ulong;
    }());

    _shell_regs.add(0x4934, "shell.073.ddr0_status",       0x00000001 );

    _shell_regs.add(0x4A34, "shell.074.ddr0_ecc_counter",  0x00000000 );
    _shell_regs.add(0x4B34, "shell.075.pcie_dma_engine",   [this]() {
        STREAM_DMA_ENGINE_ID_REGISTER r = { 0 };

        if (this->options.enable_slots_dma)
        {
            cout << "SLOTS ENABLED" << endl;
            r.HIP_0_engine_id = SLOTS_DMA_ENGINE_ID;
            r.HIP_1_engine_id = SLOTS_DMA_ENGINE_ID;
        }
        else
        {
            cout << "SLOTS NOT ENABLED" << endl;
        }

        return r.as_ulong;
    }());

    _shell_regs.add(0x4C34, "shell.076.pcie_0_version",    0x00010001 );
    _shell_regs.add(0x4D34, "shell.077.pcie_1_version",    0x00010001 );
    _shell_regs.add(0x4E34, "shell.078.ddr1_status",       0x00000001 );
    _shell_regs.add(0x4F34, "shell.079.ddr1_ecc_counter",  0x00000000 );

    _shell_regs.add(0x5034, "shell.080.qfsp_eeprom_hlth1", 0x00000000 );
    _shell_regs.add(0x5134, "shell.081.qsfp_eeprom_hlth2", 0x00000000 );
    _shell_regs.add(0x5234, "shell.082.qsfp_eeprom_hlth3", 0x00000000 );
    _shell_regs.add(0x5334, "shell.083.qsfp_eeprom_hlth4", 0x00000000 );
    _shell_regs.add(0x5434, "shell.084.qsfp_eeprom_hlth5", 0x00000000 );
    _shell_regs.add(0x5534, "shell.085.qsfp_eeprom_hlth6", 0x00000000 );
    _shell_regs.add(0x5634, "shell.086.qsfp_eeprom_hlth7", 0x00000000 );
    _shell_regs.add(0x5734, "shell.087.board_mon_addr",    0x00000000 );
    _shell_regs.add(0x5834, "shell.088.i2c_bus_addr",      0x00000000 );
    _shell_regs.add(0x5934, "shell.089.board_mon_read",    0x00000000 );
    _shell_regs.add(0x5A34, "shell.090.unused",            0x00000000 );
    _shell_regs.add(0x5B34, "shell.091.unused",            0x00000000 );
    _shell_regs.add(0x5C34, "shell.092.ddr2_status",       0x00000000 );
    _shell_regs.add(0x5D34, "shell.093.ddr2_ecc_counter",  0x00000000 );
    _shell_regs.add(0x5E34, "shell.094.ddr3_status",       0x00000000 );
    _shell_regs.add(0x5F34, "shell.095.ddr3_ecc_counter",  0x00000000 );

    _shell_regs.add(0x6034, "shell.096.soc_tx_psop_ctr",   0x00000000 );
    _shell_regs.add(0x6134, "shell.097.soc_rx_psop_ctr",   0x00000000 );
    _shell_regs.add(0x6234, "shell.098.soc_rx_pdrop_ctr",  0x00000000 );
    _shell_regs.add(0x6334, "shell.099.asl_identifier",    0x00009a55 );
    _shell_regs.add(0x6434, "shell.100.asl_status",        0x00000001 );
    _shell_regs.add(0x6534, "shell.101.role_id",           0x000D0FAC );
    _shell_regs.add(0x6634, "shell.102.fifo_status",       0x00000000 );
    _shell_regs.add(0x6734, "shell.103.soc_25g_mac_hlth",  0x00000000 );
    _shell_regs.add(0x6834, "shell.104.config_crc_error",  0x00000000 );
    _shell_regs.add(0x6934, "shell.105.i2c_version",       0x00000000 );
    _shell_regs.add(0x6A34, "shell.106.flight_data_rcdr",  0x000decaf );
    _shell_regs.add(0x6B34, "shell.107.soc_tx_pcounter",   0x00000000 );
    _shell_regs.add(0x6C34, "shell.108.soc_rx_pcounter",   0x00000000 );
    _shell_regs.add(0x6D34, "shell.109.soc_rxfcs_counter", 0x00000000 );
    _shell_regs.add(0x6E34, "shell.110.soc_ldown_counter", 0x00000000 );
    _shell_regs.add(0x6F34, "shell.111.avs_values",        0x00000000 );

    _shell_regs.add(0x7034, "shell.112.eeprom_mac_telem0", 0xddecabc7 );
    _shell_regs.add(0x7134, "shell.113.eeprom_mac_telem1", 0x0000000c );
    _shell_regs.add(0x7234, "shell.114.unused",            0x00000000 );
    _shell_regs.add(0x7334, "shell.115.unused",            0x00000000 );
    _shell_regs.add(0x7434, "shell.116.unused",            0x00000000 );
    _shell_regs.add(0x7534, "shell.117.unused",            0x00000000 );
    _shell_regs.add(0x7634, "shell.118.unused",            0x00000000 );
    _shell_regs.add(0x7734, "shell.119.unused",            0x00000000 );
    _shell_regs.add(0x7834, "shell.120.unused",            0x00000000 );
    _shell_regs.add(0x7934, "shell.121.unused",            0x00000000 );
    _shell_regs.add(0x7A34, "shell.122.unused",            0x00000000 );
    _shell_regs.add(0x7B34, "shell.123.unused",            0x00000000 );
    _shell_regs.add(0x7C34, "shell.124.unused",            0x00000000 );
    _shell_regs.add(0x7D34, "shell.125.unused",            0x00000000 );
    _shell_regs.add(0x7E34, "shell.126.unused",            0x00000000 );
    _shell_regs.add(0x7F34, "shell.127.unused",            0x00000000 );

    // Add ASMI registers
    _shell_regs.add(0x00A4, "asmi.000.flash_status",       0xffffffff );
    _shell_regs.add(0x01A4, "asmi.001.rdid_status",        0xffffffff );
    _shell_regs.add(0x02A4, "asmi.002.read_flash_address", 0xffffffff );
    _shell_regs.add(0x03A4, "asmi.003.enable_4_byte_mode", 0xffffffff );
    _shell_regs.add(0x04A4, "asmi.004.enable_protect",     0xffffffff );
    _shell_regs.add(0x05A4, "asmi.005.read_4_bytes",       0xffffffff );
    _shell_regs.add(0x06A4, "asmi.006.write_4_bytes",      0xffffffff );
    _shell_regs.add(0x07A4, "asmi.007.page_write",         0xffffffff );
    _shell_regs.add(0x08A4, "asmi.008.sector_erase",       0xffffffff );
    _shell_regs.add(0x09A4, "asmi.009.write_enable",       0xffffffff );
    _shell_regs.add(0x0AA4, "asmi.010.rsu_read_param",     0xffffffff );
    _shell_regs.add(0x0BA4, "asmi.011.rsu_write_param",    0xffffffff );
    _shell_regs.add(0x0CA4, "asmi.012.trigger_reconfig",   0xffffffff );
    _shell_regs.add(0x0DA4, "asmi.013.arm_reconfig",       0xffffffff );
    _shell_regs.add(0x0EA4, "asmi.014.asmi_fifo_level",    0xffffffff );
    _shell_regs.add(0x0FA4, "asmi.015.asmi_major_version", 0x80000000 );

    _shell_regs.add(0x10A4, "asmi.016.asmi_key",           0xffffffff );
    _shell_regs.add(0x11A4, "asmi.017.asmi_status",        0xffffffff );
    _shell_regs.add(0x12A4, "asmi.018.asmi_control",       0xffffffff );
    _shell_regs.add(0x13A4, "asmi.019.asmi_fifo_status",   0xffffffff );
    _shell_regs.add(0x14A4, "asmi.020.asmi_burst_sector",  0xffffffff );
    _shell_regs.add(0x15A4, "asmi.021.asmi_feature_enable",0xffffffff );
    _shell_regs.add(0x16A4, "asmi.022.asmi_rsu_status",    0xffffffff );
    _shell_regs.add(0x17A4, "asmi.023.asmi_rsu_ready",     0xffffffff );
    _shell_regs.add(0x18A4, "asmi.024.flash_slot_count",   0xffffffff );
    _shell_regs.add(0x19A4, "asmi.025.flash_slot_size0",   0xffffffff );
    _shell_regs.add(0x1AA4, "asmi.026.flash_slot_size1",   0xffffffff );
    _shell_regs.add(0x1BA4, "asmi.027.flash_slot_addr0",   0xffffffff );
    _shell_regs.add(0x1CA4, "asmi.028.flash_slot_addr1",   0xffffffff );
    _shell_regs.add(0x1DA4, "asmi.029.flash_slot_type",    0xffffffff );
    _shell_regs.add(0x1EA4, "asmi.030.flash_total_size0",  0x80000000 );
    _shell_regs.add(0x1FA4, "asmi.031.flash_total_size1",  0x80000000 );

    cout << "init_regs: shell_regs count = " << _shell_regs.size() << endl;
}

uint64_t CatapultDevice::get_cycle_counter()
{
    // get the current simulation timestamp.

    sc_time now = sc_time_stamp();

    chrono::duration<double> fsec(now.to_seconds());

    cout << "CatapultDevice: get_cycle_counter - fsec = " << fsec.count() << endl;

    auto usec = chrono::duration_cast<chrono::duration<uint64_t, std::micro>>(fsec);

    cout << "CatapultDevice: get_cycle_counter - usec = " << usec.count() << endl;

    uint64_t v = usec.count();

    cout << "CatapultDevice: get_cycle_counter - v    = 0x" << std::hex << v << endl;
    return v;
}

CatapultDevice::CatapultRegisterType CatapultDevice::get_address_type(uint64_t address)
{
    if (address & ~core_address_valid_mask)
    {
        return CatapultRegisterType::external;
    }

    address &= core_address_type_mask;

    switch (address)
    {
        case shell_reg_addr_test:   return CatapultRegisterType::shell;
        case soft_reg_addr_test:    return CatapultRegisterType::soft;
        case dma_reg_addr_test:     return CatapultRegisterType::dma;
        default:                    return CatapultRegisterType::invalid;
    }
}
