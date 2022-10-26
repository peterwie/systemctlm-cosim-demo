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

#include "catapult_device.h"
#include "slots_dma.h"

namespace Role 
{
    class HelloWorldRole : public sc_core::sc_module, public Catapult::CatapultRoleInterface
    {
        Catapult::CatapultShellInterface* _shell;
        Catapult::SlotsEngine _slots_engine;

    public:
        HelloWorldRole(sc_core::sc_module_name name, Catapult::CatapultShellInterface* shell) : 
            sc_module(name),
            _shell(shell),
            _slots_engine("SlotsEngine", 64, shell)
        {

        }

        virtual void reset() override;
        virtual bool    read_soft_register(uint64_t address, uint64_t& value) override;
        virtual bool   write_soft_register(uint64_t address, uint64_t  value) override;

    };
}