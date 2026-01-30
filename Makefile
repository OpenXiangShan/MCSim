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
CXXFLAGS := $(CXXFLAGS) $(INC_FLAGS) -std=c++20 -O3 -g
# CXXFLAGS := $(CXXFLAGS) $(INC_FLAGS) -std=c++20 -O0 -g
#-fno-inline -fno-omit-frame-pointer
LDFLAGS =  -g
#-fno-inline -fno-omit-frame-pointer

SRC_DIR = src
BUILD_DIR = build

# subdir src
CC_SRCS = $(shell find $(SRC_DIR) -name "*.cc")
CC_OBJS = $(patsubst $(SRC_DIR)/%.cc, $(BUILD_DIR)/%.o, $(CC_SRCS))
CC_DEPS = $(CC_OBJS:.o=.d)

BUILD_SUBDIRS = $(sort $(dir $(CC_OBJS)))

# ECHO:
# 	echo $(CC_SRCS)
# 	echo $(CC_OBJS)

TARGET = mcsim
# TARGET = mcsim_folder/mcsim_cache
# TARGET = mcsim_folder/mcsim_nocache
# TARGET = mcsim_folder/mcsim_filter

$(TARGET): $(BUILD_SUBDIRS) $(CC_OBJS)
	$(CXX) $(LDFLAGS) $(CC_OBJS) -o $(TARGET)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cc
	$(CXX) $(CXXFLAGS) -MMD $< -c -o $@

$(BUILD_SUBDIRS):
	mkdir -p $@

-include $(CC_DEPS)

latency:
	python3 ./scripts/dump_latency.py -in logs/in_cycle -out logs/out_cycle -o logs/latency.csv

clean:
	rm -rfv $(BUILD_DIR)


.PHONY: clean run
