/***************************************************************************************
* Copyright (c) 2021-2026 Beijing Institute of Open Source Chip (BOSC)
* Copyright (c) 2020-2026 Institute of Computing Technology, Chinese Academy of Sciences
*
* MCSim is licensed under Mulan PSL v2.
***************************************************************************************/

#ifndef MCSIM_C_API_H
#define MCSIM_C_API_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Returns zero on success. cpu_freq_mhz must be non-zero.
int mcsim_init(uint32_t cpu_freq_mhz);
void mcsim_finish(void);

// Advance the CPU-facing clock by one cycle. The adapter derives MC ticks from it.
void mcsim_cpu_tick(void);

// Returns one if the request was accepted, otherwise zero. is_write is a boolean byte.
uint8_t mcsim_request(uint64_t addr, uint32_t axi_id, uint8_t is_write);

// Returns zero if no response is ready, otherwise bit 32 is valid and bits [31:0] hold AXI ID.
uint64_t mcsim_response(uint8_t is_write);

void mcsim_print_stats(void);

#ifdef __cplusplus
}
#endif

#endif // MCSIM_C_API_H
