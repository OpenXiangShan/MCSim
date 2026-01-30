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
#include "top/Driver.h"
#include "top/Transaction.h"
#include "top/Controller.h"
#include "config.h"

std::mt19937_64 rng(std::chrono::steady_clock::now().time_since_epoch().count());

static char *trace_file = NULL;
static int driver_type = 0;
static double read_ratio = -1.0;
static int limit = 1000000;
static void usage(char *prog)
{
	std::cerr << "Usage: " << prog << " -d <1|2> -t <trace_file> -r read_ratio -n trasanctions" << std::endl;
	exit(1);
}

static void parse_args(int argc, char **argv)
{
	int opt;
	while ((opt = getopt(argc, argv, "hd:r:t:n:")) != -1) {
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
	driver->ia = new IdAllocator();
	Controller controller(driver->ia);
	bool trans_ok = true;
	int trans_count = -1;
	for (; ;) {
		controller.step();
		if (trans_ok) {
			trans_count++;
			if (DumpConfig::DUMP_TRANS_COUNT) { std::cout << "\rtrans_count: " << trans_count << std::flush; }
			if (!driver->next_transaction()) {
				if (DumpConfig::DUMP_TRANS_COUNT) { std::cout << std::endl; }
				break;
			}
		}
		SysTransaction& st = driver->get_transaction();
		trans_ok = false;
		if (st.timestamp * timestamp_x <= controller.get_cycles()) {
			trans_ok = controller.addTrans(st);
		}
	}
	while (!controller.all_done()) {
		controller.step();
	}
	controller.statistics();
	std::cout << "Max rid: " << driver->ia->next_rid << std::endl;
	std::cout << "Done " << std::dec << controller.get_cycles() << " cycles" << std::endl;
	return 0;
}
