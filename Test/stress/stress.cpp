/*
 * MIT License
 *
 * Copyright (c) 2025 Neil Stephens
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <vector>
#include <inttypes.h>

#include "../../TinyRISCV64.h"

extern "C"
{
//This is the code that was compiled and assembled into RV64IM. See stress.s for the assembly
//include it here so we can run it natively to compare results.
#include "stress.c"
}

int main(int argc, char** argv)
{
	try
	{
		//create a buffer to process
		constexpr size_t buf_size = 1024;
		std::vector<uint8_t> buf(buf_size);

		// Initialize buffer with deterministic 64-bit LCG-derived bytes
		uint64_t x = 0x0123456789abcdefULL;
		for (size_t i = 0; i < buf_size; ++i)
		{
			x = x * 6364136223846793005ULL + 1ULL;
			buf[i] = static_cast<uint8_t>(x >> 56);
		}

		//copy the buffer for comparison later
		std::vector<uint8_t> native_buf(buf);

		// Create VM with a modest stack (4 KiB), with memory mapped to our buffer
		TinyRISCV64::VM vm(4096);
		vm.program_load("stress.rv64im");
		auto data_addr_buf = vm.map_data_mem(buf.data(),buf.size());

		//the program implements get_addrs(u8*,sz,u64*,u64*)

		// make room on the stack for the results
		auto stack_addr_src = vm.stack_push<uint64_t>(0);
		auto stack_addr_dst = vm.stack_push<uint64_t>(0);

		//set the fn arg registers
		vm.register_set(10,data_addr_buf);
		vm.register_set(11,buf.size());
		vm.register_set(12,stack_addr_src);
		vm.register_set(13,stack_addr_dst);

		vm.execute_program();

		//Get the results
		auto res = vm.register_get(10);
		auto dst = vm.stack_pop<uint64_t>();
		auto src = vm.stack_pop<uint64_t>();
		std::printf("res = 0x%016" PRIx64 "\n", res);
		std::printf("src = 0x%016" PRIx64 "\n", src);
		std::printf("dst = 0x%016" PRIx64 "\n", dst);

		uint64_t native_src=0, native_dst=0;
		uint64_t native_res = get_addrs(native_buf.data(),native_buf.size(),&native_src,&native_dst);
		std::printf("native_res = 0x%016" PRIx64 "\n", native_res);
		std::printf("native_src = 0x%016" PRIx64 "\n", native_src);
		std::printf("native_dst = 0x%016" PRIx64 "\n", native_dst);

		std::cout<<"Buffer equal : "<<(native_buf==buf)<<std::endl;
		std::cout<<"Result equal : "<<(native_res==res)<<std::endl;
		std::cout<<"Source equal : "<<(native_src==src)<<std::endl;
		std::cout<<"Destin equal : "<<(native_dst==dst)<<std::endl;

	}
	catch (const std::exception &e)
	{
		std::fprintf(stderr, "error: %s\n", e.what());
		return 1;
	}
	return 0;
}

