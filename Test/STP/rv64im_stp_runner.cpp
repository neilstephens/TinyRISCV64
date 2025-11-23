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
#include <deque>
#include <vector>
#include <string>
#include <fstream>
#include <regex>
#include <inttypes.h>

#include "../../TinyRISCV64.h"

int main(int argc, char** argv)
{
	if (argc < 3)
	{
		std::fprintf(stderr, "Usage: %s <asm_file> <bin_file> [all|failed]\n", argv[0]);
		return 1;
	}

	const char* asm_file = argv[1];
	const char* bin_file = argv[2];
	bool print_all = (argc > 3 && std::string(argv[3]) == "all");

	std::deque<uint64_t> stack_values;
	try
	{
		// Create VM with a modest stack (4 KiB)
		TinyRISCV64::VM vm(4096);
		vm.program_load(bin_file);

		//save the stack pointer
		auto sp_before = vm.register_get(2);

		vm.execute_program();

		//dump the stack
		while (vm.register_get(2) < sp_before)
			stack_values.push_front(vm.stack_pop<uint64_t>());
	}
	catch (const std::exception &e)
	{
		std::fprintf(stderr, "VM Exception: %s\n", e.what());
		return 1;
	}

	//Read and parse the ASM file into a std::vector<std::pair<std::string,uint64_t>>
	//Each std::pair is a TEST block from the asm, paired with the expected push value from the block

	std::ifstream file(asm_file);
	if (!file.is_open())
	{
		std::fprintf(stderr, "Failed to open asm file '%s'\n", asm_file);
		return 1;
	}
	std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
	file.close();

	// Use regex to extract blocks and expected values
	std::vector<std::pair<std::string, uint64_t>> test_cases;
	std::regex test_block_regex(R"((\# TEST:[\s\S]*?EXPECTED PUSH:\s*(0x[0-9A-Fa-f]+)[\s\S]*?)(?=\#\s*TEST|$$))");
	auto blocks_begin = std::sregex_iterator(content.begin(), content.end(), test_block_regex);
	auto blocks_end = std::sregex_iterator();
	for (std::sregex_iterator i = blocks_begin; i != blocks_end; ++i)
	{
		std::smatch match = *i;
		std::string block = match[1].str();
		std::string expected_str = match[2].str();
		uint64_t expected_value = std::stoull(expected_str, nullptr, 16);
		test_cases.emplace_back(block, expected_value);
	}

	//Compare results
	int passed = 0;
	int failed = 0;

	for (size_t i = 0; i < test_cases.size() && i < stack_values.size(); ++i)
	{
		uint64_t expected = test_cases[i].second;
		uint64_t actual = stack_values[i];
		bool pass = (expected == actual);

		if (pass)
			passed++;
		else
			failed++;

		if (!pass || print_all)
		{
			std::printf("%s Test %zu:\n", pass ? "PASS" : "FAIL", i + 1);
			std::printf("Expected: 0x%016" PRIX64 "\n", expected);
			std::printf("Actual:   0x%016" PRIX64 "\n", actual);
			if (!pass)
				std::printf("%s\n", test_cases[i].first.c_str());
			std::printf("\n");
		}
	}

	std::printf("Passed: %d, Failed: %d\n", passed, failed);

	//return the number of failed tests
	return failed;
}
