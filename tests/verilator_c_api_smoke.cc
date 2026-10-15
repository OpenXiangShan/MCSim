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

#include <array>
#include <cstdint>
#include <cstdio>

namespace {

bool consume(uint64_t response, std::array<bool, 3> &seen)
{
    if (response == 0) {
        return true;
    }
    if ((response >> 32) != 1) {
        return false;
    }
    switch (static_cast<uint32_t>(response)) {
    case 7:
        seen[0] = true;
        return true;
    case 9:
        seen[1] = true;
        return true;
    case 11:
        seen[2] = true;
        return true;
    default:
        return false;
    }
}

} // namespace

int main()
{
    if (mcsim_init(3000) != 0) {
        std::fprintf(stderr, "mcsim_init failed\n");
        return 1;
    }

    // The two reads differ only in external channel-select bit 6.
    if (!mcsim_request(0x80000000, 7, 0) ||
        !mcsim_request(0x80000040, 9, 0) ||
        !mcsim_request(0x80001000, 11, 1)) {
        std::fprintf(stderr, "initial requests were not accepted\n");
        return 1;
    }

    std::array<bool, 3> seen{};
    for (int cpu_cycle = 0; cpu_cycle < 1000000; ++cpu_cycle) {
        mcsim_cpu_tick();
        if (!consume(mcsim_response(0), seen) ||
            !consume(mcsim_response(1), seen)) {
            std::fprintf(stderr, "invalid response encoding or AXI ID\n");
            return 1;
        }
        if (seen[0] && seen[1] && seen[2]) {
            mcsim_finish();
            return 0;
        }
    }

    std::fprintf(stderr, "timed out waiting for MCSim responses\n");
    mcsim_finish();
    return 1;
}
