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
#include <chrono>
#include <random>
#include <unistd.h>
#include <array>
#include <deque>
#include "AddrMap/AddrMap.h"
#include "top/Driver.h"
#include "top/Transaction.h"
#include "top/Controller.h"
#include "top/DepTracker.h"
#include "config.h"

std::mt19937_64 rng(std::chrono::steady_clock::now().time_since_epoch().count());

#ifndef GEM5_MODE
// ==================== Standalone mode (not for gem5 lib) ====================
static char *trace_file = NULL;
static int driver_type = 0;
static double read_ratio = -1.0;
static int limit = 1000000;
static const char *log_dir = DumpConfig::dump_log_dir_path;

static bool trace_ready(const SysTransaction& st, TIME_TYPE cycle)
{
	return st.timestamp * timestamp_x <= cycle;
}

static bool pending_empty(const std::array<std::deque<SysTransaction>, NUM_CHANNELS>& pending)
{
	for (const auto& queue : pending) {
		if (!queue.empty()) {
			return false;
		}
	}
	return true;
}

static bool pending_has_all_channels(const std::array<std::deque<SysTransaction>, NUM_CHANNELS>& pending)
{
	for (const auto& queue : pending) {
		if (queue.empty()) {
			return false;
		}
	}
	return true;
}

static void usage(const char *prog)
{
	std::cerr << "Usage:" << std::endl;
	std::cerr << "  " << prog << " -d 1 -t <trace_file> [-n <transactions>] [-l <log_dir>]" << std::endl;
	std::cerr << "  " << prog << " -d 2 -r <read_ratio> [-n <transactions>] [-l <log_dir>]" << std::endl;
	std::cerr << "  " << prog << " -h" << std::endl;
	exit(1);
}

static void parse_args(int argc, char **argv)
{
	int opt;
	while ((opt = getopt(argc, argv, "hd:r:t:n:l:")) != -1) {
		switch (opt) {
			case 'h':
				usage(argv[0]);
				break;
			case 'd':
				driver_type = atoi(optarg);
				break;
			case 'n':
				limit = atoi(optarg);
				break;
			case 'r':
				read_ratio = atof(optarg);
				break;
			case 't':
				trace_file = optarg;
				break;
			case 'l':
				log_dir = optarg;
				break;
			default:
				usage(argv[0]);
		}
	}
	if (driver_type != 1 && driver_type != 2) {
		std::cerr << "Invalid driver type" << std::endl;
		usage(argv[0]);
	}

	if (driver_type == 1 && trace_file == NULL) {
		std::cerr << "Trace file not specified" << std::endl;
		usage(argv[0]);
	}
	
	if (driver_type == 2 && (read_ratio < 0.0 || read_ratio > 1.0) || limit <= 0) {
		std::cerr << "Read ratio not specified" << std::endl;
		usage(argv[0]);
	}
}

void main_loop(Controller &controller, Driver *driver)
{
	std::array<std::deque<SysTransaction>, NUM_CHANNELS> pending;
	SysTransaction next_trace;
	bool has_next_trace = false;
	bool no_more_trace = false;
	int trans_count = 0;
	for (; ;) {
		controller.step();
		TIME_TYPE current_cycle = controller.get_cycles();

		for (;;) {
			if (!has_next_trace && !no_more_trace) {
				no_more_trace = !driver->next_transaction();
				if (!no_more_trace) {
					next_trace = driver->get_transaction();
					has_next_trace = true;
				}
			}

			if (!has_next_trace || !trace_ready(next_trace, current_cycle)) {
				break;
			}

			int ch = calculate_channel(next_trace.addr);
			assert(ch >= 0 && ch < NUM_CHANNELS);
			pending[ch].push_back(next_trace);
			has_next_trace = false;

			if (pending_has_all_channels(pending)) {
				break;
			}
		}

		for (int ch = 0; ch < NUM_CHANNELS; ++ch) {
			if (!pending[ch].empty() && controller.addTrans(pending[ch].front())) {
				pending[ch].pop_front();
				trans_count++;
				if (DumpConfig::DUMP_TRANS_COUNT) { std::cout << "\rtrans_count: " << trans_count << std::flush; }
			}
		}

		if (no_more_trace && pending_empty(pending)) {
			if (DumpConfig::DUMP_TRANS_COUNT) { std::cout << std::endl; }
			break;
		}
	}
	while (!controller.all_done()) {
		controller.step();
	}
}

// new trace with dependency
void dep_main_loop(Controller &controller, Driver *driver)
{
	bool add_ok = true;   // successfully added to dep_tracker
	bool no_more_trace = false;   // no more trace in driver
    int trans_count = 0;   // first loop is different from main_loop(), so init differently
	DepTracker *dep_tracker = new DepTracker();
	driver->dep_tracker = dep_tracker;
    
    for (; ;) {
        controller.step();
		
		// break if all traces are done
		if (no_more_trace) {
			if (dep_tracker->all_done()) {
				if (DumpConfig::DUMP_TRANS_COUNT) { std::cout << std::endl; }
				break;
			}
		}

		// add trace to dep_tracker, until dep_tracker full or all trace added
		for (; ;) {
			// break if dep_tracker full
			if (dep_tracker->full()) {
				break;
			}

			// get trace from driver
			if (!no_more_trace && add_ok) {
				no_more_trace = !driver->next_transaction();
				add_ok = false;
			}

			// break if no_more_trace
			if (no_more_trace) {
				break;
			}

			// add trace to dep_tracker
			SysTransaction st = driver->get_transaction();   // NOTICE: value copy
			st.dependency = USE_DEPENDENCY ? st.dependency : 0;
			add_ok = driver->dep_tracker->add_record(st);
		}

		// Each memory channel may accept one ready trace in the same controller cycle.
		TIME_TYPE current_cycle = controller.get_cycles();
		for (int ch = 0; ch < NUM_CHANNELS; ++ch) {
			SysTransaction trace;
			int record_pos = -1;
			bool get_success = driver->dep_tracker->get_trace(trace, current_cycle, ch, record_pos);
			if (get_success && controller.addTrans(trace)) {
				driver->dep_tracker->mark_sent(record_pos, current_cycle);
				trans_count++;
				if (DumpConfig::DUMP_TRANS_COUNT) { std::cout << "\rtrans_count: " << trans_count << std::flush; }
			}
		}
    }
	while (!controller.all_done()) {
        controller.step();
    }
}

int main(int argc, char **argv)
{
	parse_args(argc, argv);
	std::cout << "Hello DDR4SIM" << std::endl;
	std::ios::sync_with_stdio(false);
	Driver *driver;
	if (driver_type == 1) {
		assert(trace_file != NULL);
		driver = new TraceDriver(trace_file);
	} else if (driver_type == 2) {
		driver = new RandomDriver(read_ratio, limit);
	}
	driver->ia = new IdAllocator(driver);
	Controller controller(driver->ia, log_dir);
	driver->ia->controller = &controller;

	if (NEW_TRACE) {
		dep_main_loop(controller, driver);
		// debug: assert all entry in driver->dep_tracker is returned
		assert(driver->dep_tracker->all_done());
	} else {
		main_loop(controller, driver);
	}

	controller.statistics();
	std::cout << "Max rid: " << driver->stats.max_rid << std::endl;
	std::cout << "Done " << std::dec << controller.get_cycles() << " cycles" << std::endl;
	return 0;
}
#endif
