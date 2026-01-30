// /***************************************************************************************
// * Copyright (c) 2021-2026 Beijing Institute of Open Source Chip (BOSC)
// * Copyright (c) 2020-2026 Institute of Computing Technology, Chinese Academy of Sciences
// *
// * MCSim is licensed under Mulan PSL v2.
// * You can use this software according to the terms and conditions of the Mulan PSL v2.
// * You may obtain a copy of Mulan PSL v2 at:
// *          http://license.coscl.org.cn/MulanPSL2
// *
// * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
// * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
// * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
// *
// * See the Mulan PSL v2 for more details.
// ***************************************************************************************/

// #ifndef DFI_PHASE_FILL_H
// #define DFI_PHASE_FILL_H
// #include "utils/FixedQueue.h"
// #include "top/Transaction.h"
// #include <cassert>
// class AXI2UI;
// class DFIPhaseFill {
// public:
//     void step();
//     void update(const MappedTransaction *act, const MappedTransaction *pre, const MappedTransaction *cas);
//     DFIPhaseFill();
//     void set_axi2ui(AXI2UI *axi2ui);
//     DFICommand phase0, phase1;
// private:
//     const MappedTransaction *act, *pre, *cas;
//     FixedQueue<MappedTransaction> casQ;
//     AXI2UI *axi2ui;
// };
// #endif // DFI_PHASE_FILL_H