# Copyright (c) 2021-2026 Beijing Institute of Open Source Chip (BOSC)
# Copyright (c) 2020-2026 Institute of Computing Technology, Chinese Academy of Sciences
#
# MCSim is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#          http://license.coscl.org.cn/MulanPSL2
#
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
# EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
# MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
#
# See the Mulan PSL v2 for more details.

CC = gcc
CXX = g++
INC_FLAGS = -I./include
# 正常编译参数（C++20）
CXXFLAGS := $(CXXFLAGS) $(INC_FLAGS) -std=c++20 -O3 -g
# gem5 共享库编译参数（C++17 + concepts + fPIC）
LIB_CXXFLAGS := $(INC_FLAGS) -std=c++17 -fconcepts -O3 -g -fPIC -DGEM5_MODE
LDFLAGS = -g

SRC_DIR = src
BUILD_DIR = build
LIB_BUILD_DIR = build-lib
VERILATOR_LIB_BUILD_DIR = build-verilator-lib

GEM5_DIR = /THE/PATH/TO/YOUR/GEM5/HOME

CC_SRCS = $(shell find $(SRC_DIR) -name "*.cc")
CC_OBJS = $(patsubst $(SRC_DIR)/%.cc, $(BUILD_DIR)/%.o, $(CC_SRCS))
CC_DEPS = $(CC_OBJS:.o=.d)
LIB_CC_OBJS = $(patsubst $(SRC_DIR)/%.cc, $(LIB_BUILD_DIR)/%.o, $(CC_SRCS))
LIB_CC_DEPS = $(LIB_CC_OBJS:.o=.d)
VERILATOR_CORE_SRCS = $(filter-out $(SRC_DIR)/main.cc,$(CC_SRCS))
VERILATOR_ADAPTER_SRCS = $(shell find adapter/verilator -name "*.cc")
VERILATOR_CORE_OBJS = $(patsubst $(SRC_DIR)/%.cc,$(VERILATOR_LIB_BUILD_DIR)/src/%.o,$(VERILATOR_CORE_SRCS))
VERILATOR_ADAPTER_OBJS = $(patsubst adapter/verilator/%.cc,$(VERILATOR_LIB_BUILD_DIR)/adapter/verilator/%.o,$(VERILATOR_ADAPTER_SRCS))
VERILATOR_LIB_OBJS = $(VERILATOR_CORE_OBJS) $(VERILATOR_ADAPTER_OBJS)
VERILATOR_LIB_DEPS = $(VERILATOR_LIB_OBJS:.o=.d)

BUILD_SUBDIRS = $(sort $(dir $(CC_OBJS)))
LIB_BUILD_SUBDIRS = $(sort $(dir $(LIB_CC_OBJS)))

TARGET = mcsim
LIB_TARGET = $(GEM5_DIR)/ext/mcsim/libmcsim.so
VERILATOR_SHARED_TARGET = $(VERILATOR_LIB_BUILD_DIR)/libmcsim.so
VERILATOR_STATIC_TARGET = $(VERILATOR_LIB_BUILD_DIR)/libmcsim.a
VERILATOR_SMOKE_TARGET = $(VERILATOR_LIB_BUILD_DIR)/mcsim_c_api_smoke
VERILATOR_LIB_CXXFLAGS := $(INC_FLAGS) -std=c++20 -O3 -g -fPIC -DGEM5_MODE

$(TARGET): $(BUILD_SUBDIRS) $(CC_OBJS)
	$(CXX) $(LDFLAGS) $(CC_OBJS) -o $(TARGET)

# 生成共享库给 gem5 链接使用
$(LIB_TARGET): $(LIB_BUILD_SUBDIRS) $(LIB_CC_OBJS)
	$(CXX) -shared $(LDFLAGS) $(LIB_CC_OBJS) -o $(LIB_TARGET)

lib: $(LIB_TARGET)

$(VERILATOR_SHARED_TARGET): $(VERILATOR_LIB_OBJS)
	$(CXX) -shared $(LDFLAGS) $^ -o $@

$(VERILATOR_STATIC_TARGET): $(VERILATOR_LIB_OBJS)
	$(AR) rcs $@ $^

verilator-lib: $(VERILATOR_SHARED_TARGET) $(VERILATOR_STATIC_TARGET)

$(VERILATOR_SMOKE_TARGET): tests/verilator_c_api_smoke.cc $(VERILATOR_STATIC_TARGET)
	$(CXX) $(INC_FLAGS) -std=c++20 -O3 -g $< $(VERILATOR_STATIC_TARGET) -o $@

verilator-lib-smoke: $(VERILATOR_SMOKE_TARGET)
	$(VERILATOR_SMOKE_TARGET)

# 正常编译规则（C++20）
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cc
	$(CXX) $(CXXFLAGS) -MMD $< -c -o $@

# lib 编译规则（C++17 + concepts + fPIC）
$(LIB_BUILD_DIR)/%.o: $(SRC_DIR)/%.cc
	$(CXX) $(LIB_CXXFLAGS) -MMD $< -c -o $@

$(VERILATOR_LIB_BUILD_DIR)/src/%.o: $(SRC_DIR)/%.cc
	mkdir -p $(dir $@)
	$(CXX) $(VERILATOR_LIB_CXXFLAGS) -MMD $< -c -o $@

$(VERILATOR_LIB_BUILD_DIR)/adapter/verilator/%.o: adapter/verilator/%.cc
	mkdir -p $(dir $@)
	$(CXX) $(VERILATOR_LIB_CXXFLAGS) -MMD $< -c -o $@

$(BUILD_SUBDIRS):
	mkdir -p $@

$(LIB_BUILD_SUBDIRS):
	mkdir -p $@

-include $(CC_DEPS) $(LIB_CC_DEPS) $(VERILATOR_LIB_DEPS)

latency:
	python3 ./scripts/dump_latency.py -in logs/in_cycle -out logs/out_cycle -o logs/latency.csv

clean:
	rm -rfv $(BUILD_DIR) $(LIB_BUILD_DIR) $(VERILATOR_LIB_BUILD_DIR)

.PHONY: clean run lib verilator-lib verilator-lib-smoke
