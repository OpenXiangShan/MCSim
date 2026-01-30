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

#include <iostream>

#include <iostream>
#include <cstdio>
#include <cstdarg>
#include <cstring>

#ifdef DEBUG
#define debug(fmt, ...) \
    do { \
        char buffer[1024]; \
        std::snprintf(buffer, sizeof(buffer), fmt, __VA_ARGS__); \
        std::cout << "[" << __FILE__ << ":" << __LINE__ << "] " << buffer << std::endl; \
    } while (0)
#else
#define debug(fmt, ...) (void)0
#endif
