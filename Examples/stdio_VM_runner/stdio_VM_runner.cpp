#include "../../TinyElfRISCV64.h"
#include <iostream>
#include <string>
#include <unistd.h>

/*
 * Example of loading an elf binary that uses virtual stdio.
 *   in this case we map the native stdio directly
 * Example also shows how to map some data (arg data) into VM memory and call the VM main() fn with args
 */

int main(int argc, char** argv)
{
	if (argc < 2)
	{
		std::fprintf(stderr, "Usage: %s <bin_file>\n", argv[0]);
		return 1;
	}
	const std::string vm_bin_filename(argv[1]);
	try
	{
		TinyRISCV64::ElfVM VM;
		auto entry_point = VM.program_load(vm_bin_filename);

		VM.map_fd(STDIN_FILENO,std::make_shared<std::iostream>(std::cin.rdbuf()));
		VM.map_fd(STDOUT_FILENO,std::make_shared<std::iostream>(std::cout.rdbuf()));
		VM.map_fd(STDERR_FILENO,std::make_shared<std::iostream>(std::cerr.rdbuf()));

		//copy the remaining args and map data into vm
		std::vector<TinyRISCV64::u8> vm_arg_data;
		std::vector<TinyRISCV64::u64> vm_argv;
		for(int i = 1; i < argc; i++)
		{
			auto n = std::strlen(argv[i]) + 1;
			auto offset = vm_arg_data.size();
			vm_arg_data.insert(vm_arg_data.end(), argv[i], argv[i] + n);
			vm_argv.push_back(offset);
		}
		auto virt_addr = VM.map_data_mem(vm_arg_data.data(),vm_arg_data.size());

		//add the base address to the offsets in vm argv
		for(auto& av : vm_argv)
			av += virt_addr;
		vm_argv.push_back(0);

		//push vm_argv onto the vm stack in reverse
		for(int i = vm_argv.size() - 1; i >= 0; i--)
			virt_addr = VM.stack_push(vm_argv[i]);

		//assign vm argc and argv to registers before calling vm main
		VM.register_set(10, argc - 1);  // a0 = argc
		VM.register_set(11, virt_addr); // a1 = argv

		//beware, this could block on reading stdin - if that's what the guest program does
		//  put it on another thread if that's not OK
		VM.execute_program(entry_point,100*1024UL*1024);
	}
	catch(const std::exception &e)
	{
		std::fprintf(stderr, "VM Exception: %s\n", e.what());
		return 1;
	}
	return 0;
}
