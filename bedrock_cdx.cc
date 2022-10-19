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

#define SC_INCLUDE_DYNAMIC_PROCESSES

#include <inttypes.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>

#include "systemc.h"
#include "tlm_utils/simple_initiator_socket.h"
#include "tlm_utils/simple_target_socket.h"
#include "tlm_utils/tlm_quantumkeeper.h"

#include <vector>
#include <string>

using namespace sc_core;
using namespace sc_dt;
using namespace std;

#include "trace.h"
#include "iconnect.h"
#include "debugdev.h"
#include "soc/xilinx/versal-net/xilinx-versal-net.h"
#include "soc/dma/xilinx-cdma.h"
#include "tlm-extensions/genattr.h"
#include "memory.h"

#include "catapult/catapult_device.h"

using namespace Catapult;

#define NR_MASTERS	2
#define NR_DEVICES	2

class SMIDdev : public sc_core::sc_module
{
public:
    tlm_utils::simple_target_socket<SMIDdev> tgt_socket;
    tlm_utils::simple_initiator_socket<SMIDdev> init_socket;

    SMIDdev(sc_core::sc_module_name name, uint32_t smid) :
        sc_module(name),
        tgt_socket("tgt-socket"),
        init_socket("init-socket"),
        m_smid(smid)
    {
        tgt_socket.register_b_transport(this, &SMIDdev::b_transport);
    }

private:
    virtual void b_transport(tlm::tlm_generic_payload& trans,
            sc_time& delay)
    {
        genattr_extension *genattr;

        trans.get_extension(genattr);
        if (!genattr) {
            genattr = new genattr_extension();
            trans.set_extension(genattr);
        }

        //
        // Setup the SMID (master_id)
        //
        genattr->set_master_id(m_smid);

        init_socket->b_transport(trans, delay);
    }

    uint32_t m_smid;
};

SC_MODULE(Top)
{
	iconnect<NR_MASTERS, NR_DEVICES> bus;
	xilinx_versal_net versal_net;

    CatapultDevice catapult_dev;
	SMIDdev smid_catapult_dev;

	sc_signal<bool> rst;

	SC_HAS_PROCESS(Top);

	void pull_reset(void) {
		/* Pull the reset signal.  */
		rst.write(true);
		wait(1, SC_US);
		rst.write(false);
	}

	Top(sc_module_name name, const char *sk_descr, sc_time quantum, CatapultDeviceOptions catapult_opts) :
		sc_module(name),
		bus("bus"),
		versal_net("versal-net", sk_descr),
        catapult_dev("catapult_dev", catapult_opts),
		smid_catapult_dev("smid-catapult_dev", 0x250),
		rst("rst")
	{
		m_qk.set_global_quantum(quantum);

		versal_net.rst(rst);

		//
		// Bus slave devices
		//
		// Address         Device
		// [0xe4000000] : debugdev
		// [0xe4020000] : CDMA 0 (SMID 0x250)
		// [0xe4030000] : CDMA 1 (SMID 0x251)
		// [0xe4040000] : address_repeater
		// [0xe4100000] : Memory 2 MB
		// [0xe4300000] : Memory 2 MB
		//
	    bus.memmap(0xe4000000ULL, CatapultDevice::mmio_size - 1,
			    ADDRMODE_RELATIVE, -1, catapult_dev.tgt_socket);
  		bus.memmap(0x0LL, UINT64_MAX,
  				ADDRMODE_RELATIVE, -1, *(versal_net.s_cpm));

		//
		// Bus masters
		//
		versal_net.m_cpm->bind(*(bus.t_sk[0]));
		smid_catapult_dev.init_socket(*(bus.t_sk[1]));

		// bind devices to their bus-masters
		catapult_dev.init_socket(smid_catapult_dev.tgt_socket);

		/* Connect the PL irqs to the irq_pl_to_ps wires.  */
		// debugdev_cpm.irq(versal_net.pl2ps_irq[0]);

		/* Tie off any remaining unconnected signals.  */
		versal_net.tie_off();

		SC_THREAD(pull_reset);
	}

private:
	tlm_utils::tlm_quantumkeeper m_qk;
};

void usage(const char* program_name)
{
	cout << program_name << " socket-path [sync-quantum-ns] [--] [options]" << endl;
	cout << "options include:" << endl;
	cout << "  --slots - enables slots DMA engine" << endl;
}

int sc_main(int argc, char* argv[])
{
	Top *top;
	uint64_t sync_quantum;
	sc_trace_file *trace_fp = NULL;

	const char* socket_path = NULL;
	const char* quantum_arg = NULL;

	CatapultDeviceOptions catapult_opts;

	// check for help
	int positional = 1;
	for (int i = 1; i < argc; i +=1) {
		if (strcmp(argv[i], "--help") == 0 ||
		    strcmp(argv[i], "-h") == 0 ||
			strcmp(argv[i], "-?") == 0) {

			usage(argv[0]);
			return -1;
		}

		const char* arg = argv[i];

		// handle the positional arguments
		if (arg[0] != '-')
		{
			if (positional == 1)
			{
				if (socket_path != nullptr)
				{
					cout << "socket_path argument provided mutliple times" << endl;
					usage(argv[0]);
					return -1;
				}

				socket_path = arg;
				positional += 1;
				continue;
			}
			else if (positional == 2)
			{
				if (quantum_arg != nullptr)
				{
					cout << "quantum_arg argument provided mutliple times" << endl;
					usage(argv[0]);
					return -1;
				}

				quantum_arg = arg;
				positional +=1;
				continue;
			}
			else
			{
				cout << "unrecognized positional argument '" << arg << "'" << endl;
				usage(argv[0]);
				return -1;
			}
		}
		else if (arg[1] == '1' && arg[2] == '\0')
		{
			// ignore -- argument
			continue;
		}

		// remove - and -- from the start of the argument
		arg += (arg[1] == '-' ? 2 : 1);

		if (strcasecmp("slots", arg) == 0) {
			cout << "catapult: enabling slots" << endl;
			catapult_opts.enable_slots_dma = true;
		}
	}

	if (socket_path == nullptr)
	{
		cout << "required socket_path parameter not provided" << endl;
		usage(argv[0]);
		return -1;
	}

	if (quantum_arg == nullptr) {
		sync_quantum = 10000;
	} else {
		sync_quantum = strtoull(quantum_arg, NULL, 10);
	}

	sc_set_time_resolution(1, SC_PS);

	top = new Top("top", argv[1], sc_time((double) sync_quantum, SC_NS), catapult_opts);

	if (argc < 3) {
		sc_start(1, SC_PS);
		sc_stop();
		usage(argv[0]);
		exit(EXIT_FAILURE);
	}

	trace_fp = sc_create_vcd_trace_file("trace");
	trace(trace_fp, *top, top->name());

	sc_start();
	if (trace_fp) {
		sc_close_vcd_trace_file(trace_fp);
	}
	return 0;
}
