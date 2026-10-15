/***************************************************************************************
* Copyright (c) 2021-2026 Beijing Institute of Open Source Chip (BOSC)
* Copyright (c) 2020-2026 Institute of Computing Technology, Chinese Academy of Sciences
*
* MCSim is licensed under Mulan PSL v2.
* You can use this software according to the terms and conditions of the Mulan PSL v2.
* You may obtain a copy of Mulan PSL v2 at:
*          http://license.coscl.org.cn/MulanPSL2
*
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
* EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
* MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
*
* See the Mulan PSL v2 for more details.
***************************************************************************************/

#include "mcsim_c_api.h"

#include "config.h"
#include "top/Controller.h"
#include "top/Driver.h"
#include "top/Transaction.h"

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <memory>
#include <random>
#include <unordered_map>

// MCSim's standalone executable defines this in main.cc. The library excludes main.cc.
std::mt19937_64 rng(std::chrono::steady_clock::now().time_since_epoch().count());

namespace {

constexpr uint64_t kResponseValid = uint64_t{1} << 32;

class VerilatorMCSim {
public:
    explicit VerilatorMCSim(uint32_t cpu_freq_mhz)
        : cpu_freq_mhz_(cpu_freq_mhz), id_allocator_(nullptr), controller_(&id_allocator_)
    {
        id_allocator_.controller = &controller_;
        id_allocator_.on_read_complete = [this](ID_TYPE id) {
            complete(id, read_ids_, read_responses_);
        };
        id_allocator_.on_write_complete = [this](ID_TYPE id) {
            complete(id, write_ids_, write_responses_);
        };
    }

    void cpu_tick()
    {
        mc_phase_mhz_ += freq;
        while (mc_phase_mhz_ >= cpu_freq_mhz_) {
            controller_.step();
            mc_phase_mhz_ -= cpu_freq_mhz_;
        }
    }

    bool request(uint64_t addr, uint32_t axi_id, bool is_write)
    {
        if (!controller_.willAccept(addr, is_write)) {
            return false;
        }

        ID_TYPE internal_id = is_write ? id_allocator_.allocate_wid()
                                       : id_allocator_.allocate_rid();
        auto &ids = is_write ? write_ids_ : read_ids_;
        const bool inserted = ids.emplace(internal_id, axi_id).second;
        if (!inserted) {
            std::abort();
        }

        SysTransaction transaction{};
        transaction.id = internal_id;
        transaction.is_read = !is_write;
        transaction.addr = addr;
        transaction.timestamp = controller_.get_cycles();

        // The adapter is single-threaded, so admission cannot change after willAccept().
        if (!controller_.addTrans(transaction)) {
            std::abort();
        }
        return true;
    }

    uint64_t response(bool is_write)
    {
        auto &responses = is_write ? write_responses_ : read_responses_;
        if (responses.empty()) {
            return 0;
        }
        const uint64_t response = kResponseValid | responses.front();
        responses.pop_front();
        return response;
    }

    void print_stats() const
    {
        controller_.statistics();
    }

private:
    static void complete(ID_TYPE internal_id,
                         std::unordered_map<ID_TYPE, uint32_t> &ids,
                         std::deque<uint32_t> &responses)
    {
        const auto it = ids.find(internal_id);
        if (it == ids.end()) {
            std::abort();
        }
        responses.push_back(it->second);
        ids.erase(it);
    }

    uint64_t cpu_freq_mhz_;
    uint64_t mc_phase_mhz_ = 0;
    IdAllocator id_allocator_;
    Controller controller_;
    std::unordered_map<ID_TYPE, uint32_t> read_ids_;
    std::unordered_map<ID_TYPE, uint32_t> write_ids_;
    std::deque<uint32_t> read_responses_;
    std::deque<uint32_t> write_responses_;
};

std::unique_ptr<VerilatorMCSim> instance;

} // namespace

extern "C" int mcsim_init(uint32_t cpu_freq_mhz)
{
    if (cpu_freq_mhz == 0) {
        return -1;
    }
    instance.reset();
    try {
        instance = std::make_unique<VerilatorMCSim>(cpu_freq_mhz);
    } catch (...) {
        instance.reset();
        return -1;
    }
    return 0;
}

extern "C" void mcsim_finish(void)
{
    instance.reset();
}

extern "C" void mcsim_cpu_tick(void)
{
    if (instance) {
        instance->cpu_tick();
    }
}

extern "C" uint8_t mcsim_request(uint64_t addr, uint32_t axi_id, uint8_t is_write)
{
    return instance && instance->request(addr, axi_id, is_write != 0);
}

extern "C" uint64_t mcsim_response(uint8_t is_write)
{
    return instance ? instance->response(is_write != 0) : 0;
}

extern "C" void mcsim_print_stats(void)
{
    if (instance) {
        instance->print_stats();
    }
}
