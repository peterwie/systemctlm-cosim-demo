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

#include "bedrock_shell.h"

#include <ctime>
#include <chrono>

using namespace sc_core;
using namespace sc_dt;
using namespace std;

const char* tlm_commands[] = {
    "read  ",
    "write ",
    "ignore",
};

BedrockShell::BedrockShell(sc_core::sc_module_name name) :
    sc_module(name),
    tgt_socket("tgt-socket"),
    _simple_regs(),
    _dynamic_regs()
{
    tgt_socket.register_b_transport(this, &BedrockShell::b_transport);

    init_registers();
}

void BedrockShell::b_transport(tlm::tlm_generic_payload& trans,
        sc_time& delay)
{
    // tlm::tlm_command cmd = trans.get_command();
    unsigned char *data = trans.get_data_ptr();
    size_t len = trans.get_data_length();
    uint64_t addr = trans.get_address();

    enum tlm::tlm_command cmd = trans.get_command();
    const char* cmd_name = cmd <= tlm::TLM_IGNORE_COMMAND ? tlm_commands[cmd] : "??????";

    uint64_t value = 0xdeadbeefdeadbeef;

    cout << "BedrockShell: " << cmd_name << " cmd @ 0x" << std::hex << addr << " for 0x" << std::hex << len << " bytes." << endl;

    if (len != 4 && len != 8)
    {
        cout << "BedrockShell: len " << len << " invalid." << endl;
        trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
        return;
    }

    if (trans.get_byte_enable_ptr())
    {
        cout << "BedrockShell: byte_enable_ptr not supported" << endl;
        trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
        return;
    }

    if (trans.is_read())
    {
        size_t bytes_read = read_register(addr, len, value);
        cout << "BedrockShell: read completed with length " << bytes_read << " and value " << std::hex << value << endl;
        memcpy(reinterpret_cast<void*>(data),
               reinterpret_cast<const void *>(&value),
               max(len, bytes_read));
    }
    else
    {
        cout << "BedrockShell: " << cmd_name << " ignored." << endl;
    }
}

size_t BedrockShell::read_register(uint64_t address, size_t length, uint64_t& value)
{
    for (const auto& fn = _simple_regs.find(address); 
         fn != _simple_regs.end();)
    {
        cout << "BedrockShell: matched 0x" << hex << address << " to sreg " << fn->second.first << endl;
        value = fn->second.second;
        return sizeof(uint32_t);
    }

    for (const auto& fn = _dynamic_regs.find(address); 
         fn != _dynamic_regs.end();)
    {
        cout << "BedrockShell: matched 0x" << hex << address << " to dreg " << fn->second.first << endl;
        return fn->second.second(address, length, value);
    }

    return length;
}

void BedrockShell::init_registers()
{
    _simple_regs[0x0034] = make_pair("shell.000.control",           0x00000000 );
    _simple_regs[0x0134] = make_pair("shell.001.unused",            0x00000000 );
    _simple_regs[0x0234] = make_pair("shell.002.unused",            0x00000000 );
    _simple_regs[0x0334] = make_pair("shell.003.unused",            0x00000000 );
    _simple_regs[0x0434] = make_pair("shell.004.network_status",    0x00000000 );
    _simple_regs[0x0534] = make_pair("shell.005.network_error",     0x00000000 );
    _simple_regs[0x0634] = make_pair("shell.006.pcie0_tlp_error",   0x0000ff00 );
    _simple_regs[0x0734] = make_pair("shell.007.pcie0_tlp_status",  0x00000000 );
    _simple_regs[0x0834] = make_pair("shell.008.pcie1_tlp_error",   0x00000000 );
    _simple_regs[0x0934] = make_pair("shell.009.pcie1_tlp_status",  0x00000000 );
    _simple_regs[0x0A34] = make_pair("shell.010.unused",            0x00000000 );
    _simple_regs[0x0B34] = make_pair("shell.011.unused",            0x00000000 );
    _simple_regs[0x0C34] = make_pair("shell.012.unused",            0x00000000 );
    _simple_regs[0x0D34] = make_pair("shell.013.unused",            0xaaaaaaaa );
    _simple_regs[0x0E34] = make_pair("shell.014.unused",            0xaaaaaaaa );
    _simple_regs[0x0F34] = make_pair("shell.015.unused",            0x00000000 );
    _simple_regs[0x1034] = make_pair("shell.016.unused",            0xaaaaaaaa );
    _simple_regs[0x1134] = make_pair("shell.017.unused",            0xaaaaaaaa );
    _simple_regs[0x1234] = make_pair("shell.018.unused",            0x00000000 );
    _simple_regs[0x1334] = make_pair("shell.019.tor_tx_pcounter",   0x00000000 );
    _simple_regs[0x1434] = make_pair("shell.020.tor_rx_pcounter",   0x00000000 );
    _simple_regs[0x1534] = make_pair("shell.021.tor_rxfcs_counter", 0x00000000 );
    _simple_regs[0x1634] = make_pair("shell.022.tor_ldown_counter", 0x00000000 );
    _simple_regs[0x1734] = make_pair("shell.023.tor_tx_pcounter",   0x00000000 );
    _simple_regs[0x1834] = make_pair("shell.024.nic_rx_pcounter",   0x00000000 );
    _simple_regs[0x1934] = make_pair("shell.025.nic_rxfcs_counter", 0x00000000 );
    _simple_regs[0x1A34] = make_pair("shell.026.nic_ldown_counter", 0x00000000 );
    _simple_regs[0x1B34] = make_pair("shell.027.nic_tor_debug0",    0x00000000 );
    _simple_regs[0x1C34] = make_pair("shell.028.nic_tor_debug1",    0x00000000 );
    _simple_regs[0x1D34] = make_pair("shell.029.nic_tor_debug2",    0x00e01400 );

    _simple_regs[0x1E34] = make_pair("shell.030.tor_tx_psop_ctr",   0x00000000 );
    _simple_regs[0x1F34] = make_pair("shell.031.tor_rx_psop_ctr",   0x00000000 );
    _simple_regs[0x2034] = make_pair("shell.032.nic_tx_psop_ctr",   0x00000000 );
    _simple_regs[0x2134] = make_pair("shell.033.nic_rx_psop_ctr",   0x00000000 );
    _simple_regs[0x2234] = make_pair("shell.034.pcie_dma_health",   0x00000000 );
    _simple_regs[0x2334] = make_pair("shell.035.tor_tx_fcsdrop_ctr",0x00000000 );
    _simple_regs[0x2434] = make_pair("shell.036.nic_tx_fcsdrop_ctr",0x00000000 );
    _simple_regs[0x2534] = make_pair("shell.037.tor_tx_errdrop_ctr",0x00000000 );
    _simple_regs[0x2634] = make_pair("shell.038.nic_tx_errdrop_ctr",0x00000000 );
    _simple_regs[0x2734] = make_pair("shell.039.unused",            0x00000000 );
    _simple_regs[0x2834] = make_pair("shell.040.legacy_net_test0",  0xaaaaaaaa );
    _simple_regs[0x2934] = make_pair("shell.041.legacy_net_test1",  0xaaaaaaaa );

    _simple_regs[0x2A34] = make_pair("shell.042.nic_mac_health",    0x000c0000 );
    _simple_regs[0x2B34] = make_pair("shell.043.tor_mac_health",    0x100c5002 );

    _simple_regs[0x2C34] = make_pair("shell.044.qsfp_retimer_hlth", 0x00000000 );
    _simple_regs[0x2D34] = make_pair("shell.045.slim40g_nic_health",0x00000000 );
    _simple_regs[0x2E34] = make_pair("shell.046.slim40g_tor_health",0x00000000 );
    _simple_regs[0x2F34] = make_pair("shell.047.slim40g_version",   0x00000000 );
    _simple_regs[0x3034] = make_pair("shell.048.unused",            0x00000000 );
    _simple_regs[0x3134] = make_pair("shell.049.tor_rx_pdrop_ctr",  0x00000000 );
    _simple_regs[0x3234] = make_pair("shell.050.nic_rx_pdrop_ctr",  0x00000000 );
    _simple_regs[0x3334] = make_pair("shell.051.pcie_telemetry",    0x00000000 );
    _simple_regs[0x3434] = make_pair("shell.052.retimer_dbg_write", 0x00000000 );
    _simple_regs[0x3534] = make_pair("shell.053.retimer_dbg_read",  0x00000000 );
    _simple_regs[0x3634] = make_pair("shell.054.ddr_reset_ctrl_in", 0x00000000 );
    _simple_regs[0x3734] = make_pair("shell.055.ddr_reset_ctrl_out",0x00000000 );
    _simple_regs[0x3834] = make_pair("shell.056.board_revision",    0x00000000 );

    _simple_regs[0x3934] = make_pair("shell.057.shl_patch_board_id",0x000d00d0 );   // making up delta shell revision & board ID

    _simple_regs[0x3A34] = make_pair("shell.058.shell_release_ver", 0x00010001 );
    _simple_regs[0x3B34] = make_pair("shell.059.build_info",        []() {
        uint32_t v = 0;

        auto t = std::time(nullptr);
        auto tm = std::localtime(&t);

        v |= (((uint16_t) tm->tm_year - 2013) & 0x0f) << 12;
        v |= (((uint16_t) tm->tm_mon)         & 0x0f) << 8;
        v |= (((uint16_t) tm->tm_mday)        & 0x1f) << 2;

        return v;
    }());


    _simple_regs[0x3C34] = make_pair("shell.060.shell_src_version", 0xa311adcf );

    _simple_regs[0x3D34] = make_pair("shell.061.asl_version",       0x00020000 );
    _simple_regs[0x3E34] = make_pair("shell.062.chip_id0",          0x01234567 );
    _simple_regs[0x3F34] = make_pair("shell.063.chip_id1",          0x89abcdef );

    _simple_regs[0x4034] = make_pair("shell.064.shell_id",          0x00bed70c ); // 0x00de17a0 );

    _simple_regs[0x4134] = make_pair("shell.065.role_version",      0xfacecafe );

    _dynamic_regs[0x4234]= make_pair("shell.066.cycle_counter0",   
                                     [this](uint64_t, size_t, uint64_t& v) { 
                                        v = this->get_cycle_counter(0); 
                                        return sizeof(uint32_t); 
                                     });
    _dynamic_regs[0x4334]= make_pair("shell.067.cycle_counter1",   
                                     [this](uint64_t, size_t, uint64_t& v) { 
                                         v = this->get_cycle_counter(1); 
                                         return sizeof(uint32_t); 
                                     });

    _simple_regs[0x4434] = make_pair("shell.068.shell_status",      []() {
        SHELL_STATUS_REGISTER r = { .as_ulong = 0 };
        r.DDRHealthy = 1;
        r.CorePLLLocked = 1;
        r.DDRPLLLocked = 1;
        return r.as_ulong;
    }());

    _simple_regs[0x4534] = make_pair("shell.069.pcie_link_status",  0x00000000 );
    _simple_regs[0x4634] = make_pair("shell.070.role_status",       0x01233210 );
    _simple_regs[0x4734] = make_pair("shell.071.temperature_status",0x1c1a1b00 );

    _simple_regs[0x4834] = make_pair("shell.072.capabilities",      []() {
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

    _simple_regs[0x4934] = make_pair("shell.073.ddr0_status",       0x00000001 );

    _simple_regs[0x4A34] = make_pair("shell.074.ddr0_ecc_counter",  0x00000000 );
    _simple_regs[0x4B34] = make_pair("shell.075.pcie_dma_engine",   []() {
        STREAM_DMA_ENGINE_ID_REGISTER r = { 0 };
        r.HIP_0_engine_id = SLOTS_DMA_ENGINE_ID;
        r.HIP_1_engine_id = SLOTS_DMA_ENGINE_ID;
        return r.as_ulong;
    }());

    _simple_regs[0x4C34] = make_pair("shell.076.pcie_0_version",    0x00010001 );
    _simple_regs[0x4D34] = make_pair("shell.077.pcie_1_version",    0x00010001 );

    _simple_regs[0x4E34] = make_pair("shell.078.ddr1_status",       0x00000001 );
    _simple_regs[0x4F34] = make_pair("shell.079.ddr1_ecc_counter",  0x00000000 );

    _simple_regs[0x5034] = make_pair("shell.080.qfsp_eeprom_hlth1", 0x00000000 );
    _simple_regs[0x5134] = make_pair("shell.081.qsfp_eeprom_hlth2", 0x00000000 );
    _simple_regs[0x5234] = make_pair("shell.082.qsfp_eeprom_hlth3", 0x00000000 );
    _simple_regs[0x5334] = make_pair("shell.083.qsfp_eeprom_hlth4", 0x00000000 );
    _simple_regs[0x5434] = make_pair("shell.084.qsfp_eeprom_hlth5", 0x00000000 );
    _simple_regs[0x5534] = make_pair("shell.085.qsfp_eeprom_hlth6", 0x00000000 );
    _simple_regs[0x5634] = make_pair("shell.086.qsfp_eeprom_hlth7", 0x00000000 );
    _simple_regs[0x5734] = make_pair("shell.087.board_mon_addr",    0x00000000 );
    _simple_regs[0x5834] = make_pair("shell.088.i2c_bus_addr",      0x00000000 );
    _simple_regs[0x5934] = make_pair("shell.089.board_mon_read",    0x00000000 );
    _simple_regs[0x5A34] = make_pair("shell.090.unused",            0x00000000 );
    _simple_regs[0x5B34] = make_pair("shell.091.unused",            0x00000000 );
    _simple_regs[0x5C34] = make_pair("shell.092.ddr2_status",       0x00000000 );
    _simple_regs[0x5D34] = make_pair("shell.093.ddr2_ecc_counter",  0x00000000 );
    _simple_regs[0x5E34] = make_pair("shell.094.ddr3_status",       0x00000000 );
    _simple_regs[0x5F34] = make_pair("shell.095.ddr3_ecc_counter",  0x00000000 );

    _simple_regs[0x6034] = make_pair("shell.096.soc_tx_psop_ctr",   0x00000000 );
    _simple_regs[0x6134] = make_pair("shell.097.soc_rx_psop_ctr",   0x00000000 );
    _simple_regs[0x6234] = make_pair("shell.098.soc_rx_pdrop_ctr",  0x00000000 );
    _simple_regs[0x6334] = make_pair("shell.099.asl_identifier",    0x00009a55 );
    _simple_regs[0x6434] = make_pair("shell.100.asl_status",        0x00000001 );
    _simple_regs[0x6534] = make_pair("shell.101.role_id",           0x000D0FAC );
    _simple_regs[0x6634] = make_pair("shell.102.fifo_status",       0x00000000 );
    _simple_regs[0x6734] = make_pair("shell.103.soc_25g_mac_hlth",  0x00000000 );
    _simple_regs[0x6834] = make_pair("shell.104.config_crc_error",  0x00000000 );
    _simple_regs[0x6934] = make_pair("shell.105.i2c_version",       0x00000000 );
    _simple_regs[0x6A34] = make_pair("shell.106.flight_data_rcdr",  0x000decaf );
    _simple_regs[0x6B34] = make_pair("shell.107.soc_tx_pcounter",   0x00000000 );
    _simple_regs[0x6C34] = make_pair("shell.108.soc_rx_pcounter",   0x00000000 );
    _simple_regs[0x6D34] = make_pair("shell.109.soc_rxfcs_counter", 0x00000000 );
    _simple_regs[0x6E34] = make_pair("shell.110.soc_ldown_counter", 0x00000000 );
    _simple_regs[0x6F34] = make_pair("shell.111.avs_values",        0x00000000 );

    _simple_regs[0x7034] = make_pair("shell.112.eeprom_mac_telem0", 0xddecabc7 );
    _simple_regs[0x7134] = make_pair("shell.113.eeprom_mac_telem1", 0x0000000c );
    _simple_regs[0x7234] = make_pair("shell.114.unused",            0x00000000 );
    _simple_regs[0x7334] = make_pair("shell.115.unused",            0x00000000 );
    _simple_regs[0x7434] = make_pair("shell.116.unused",            0x00000000 );
    _simple_regs[0x7534] = make_pair("shell.117.unused",            0x00000000 );
    _simple_regs[0x7634] = make_pair("shell.118.unused",            0x00000000 );
    _simple_regs[0x7734] = make_pair("shell.119.unused",            0x00000000 );
    _simple_regs[0x7834] = make_pair("shell.120.unused",            0x00000000 );
    _simple_regs[0x7934] = make_pair("shell.121.unused",            0x00000000 );
    _simple_regs[0x7A34] = make_pair("shell.122.unused",            0x00000000 );
    _simple_regs[0x7B34] = make_pair("shell.123.unused",            0x00000000 );
    _simple_regs[0x7C34] = make_pair("shell.124.unused",            0x00000000 );
    _simple_regs[0x7D34] = make_pair("shell.125.unused",            0x00000000 );
    _simple_regs[0x7E34] = make_pair("shell.126.unused",            0x00000000 );
    _simple_regs[0x7F34] = make_pair("shell.127.unused",            0x00000000 );

    cout << "init_regs: simple_regs  length = " << _simple_regs.size() << endl;
    cout << "init_regs: dynamic_regs length = " << _dynamic_regs.size() << endl;
}

uint32_t BedrockShell::get_cycle_counter(bool low_part)
{
    // get the current simulation timestamp.

    sc_time now = sc_time_stamp();

    chrono::duration<double> fsec(now.to_seconds());

    cout << "BedrockShell: get_cycle_counter - fsec = " << fsec.count() << endl;

    auto usec = chrono::duration_cast<chrono::duration<uint64_t, std::micro>>(fsec);

    cout << "BedrockShell: get_cycle_counter - usec = " << usec.count() << endl;

    uint64_t v = usec.count();

    cout << "BedrockShell: get_cycle_counter - v    = 0x" << std::hex << v << endl;

    auto low  = static_cast<uint32_t>(v & 0x00000000ffffffff);
    auto high = static_cast<uint32_t>((v >> 32) & 0x00000000ffffffff);

    cout << "BedrockShell: get_cycle_counter - vlow = 0x" << std::hex << low << endl;
    cout << "BedrockShell: get_cycle_counter - vhigh= 0x" << std::hex << high << endl;

    return low_part ? low : high;
}
