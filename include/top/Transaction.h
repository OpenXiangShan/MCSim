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

#ifndef __TRANSACTION_H__
#define __TRANSACTION_H__
#include <cstdint>
#include <unordered_set>
#include <string>
#include <functional>
#include "utils/common.h"
struct SysTransaction {
   ID_TYPE id;
   bool is_read; 
   uint64_t addr;
   TIME_TYPE timestamp;
   uint64_t cycle;   // at which cycle controller commit this trace sucessfully
   friend std::istream& operator>>(std::istream& is, SysTransaction& trans);
};

struct AxiTransaction {
    ID_TYPE id;
    bool is_read;
    uint64_t addr;
    uint64_t cycle;
    TOKEN_TYPE token;   // offset in rdback_buffer
    AxiTransaction() {}
    AxiTransaction(const SysTransaction& st) : id(st.id), is_read(st.is_read), cycle(st.cycle), addr(st.addr) {}
};

struct FilterTransaction : AxiTransaction {
    bool is_to_cache;   // True: to cache, False: to scheduler
    bool is_prefetch;
    FilterTransaction() {}
    FilterTransaction(const AxiTransaction& at) : AxiTransaction(at) {}
    FilterTransaction(const AxiTransaction& at, const bool is_to_cache) : AxiTransaction(at), is_to_cache(is_to_cache) {}
};

struct MappedAddr {
    unsigned int rank: 1;
    unsigned int row: 16;
    unsigned int col: 10;
    unsigned int bank: 2;
    unsigned int bg  : 2;
    friend std::ostream& operator<<(std::ostream& os, const MappedAddr& addr);
};

struct MappedTransaction : FilterTransaction {
    MappedAddr mapped_addr;
    MappedTransaction() {}
    MappedTransaction(const FilterTransaction& ft) : FilterTransaction(ft) {}
    MappedTransaction(const FilterTransaction& ft, const MappedAddr& Maddr) : FilterTransaction(ft), mapped_addr(Maddr) {}
};

enum class DFI_TYPE {
    NONE,
    ACT,
    PRE,
    READ,
    WRITE
};
struct DFICommand {
    DFI_TYPE type;
    uint8_t rank;
    uint8_t bank;
    uint8_t bg;
    void assign(DFI_TYPE type, const MappedTransaction& trans) {
        this->type = type;
        this->rank = trans.mapped_addr.rank;
        this->bank = trans.mapped_addr.bank;
        this->bg = trans.mapped_addr.bg;
    }
};

static int row_bits[] = {31, 30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17};
static int col_bits[] = {16, 15, 14, 13, 12, 11, 10};
static int bank_bits[] = {9, 8};
static int bg_bits[] = {7, 6};

enum class CASFail {
    NONE,
    R2W,
    W2R,
    BG_CONFLICT,
    ROW_SWITCH,
    W2WDR,
    W2RDR,
    R2RDR,
    R2WDR
};

#endif // __TRANSACTION_H__