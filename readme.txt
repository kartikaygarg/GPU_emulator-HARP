Author: 		Kartikay Garg
Course:			ECE 8823: GPU Architectures
Assignement 4: 	HARP emulator
Notes:            This emulator program contains following files:
                  1. harp_emulator.cpp                              main Function  & system configuration (LANES & WARPS)
                  2. fetch.cpp                                  definitions of fetch_stage & scheduler_stage for fetch_stage class  
                  3. decoder.cpp                               definitions of decoder stage for decode_stage class  
                  4. execute.cpp                                 definitions of excute stage for execute_stage class  
                  5. fetch.h                               fetch stage class
                  6. decoder.h                                decide stage class: opcode enums
                  7. execute.h                                   execute stage
                  8. Makefile
                  
                   Emulator can be compiled using gcc version 4.9 (PACE cluster)

                   To run the executable run the following command:
                   ./harp_emulator <input_binary_file> <output_file>

                   Emulator Supports Arch ID of type: 4w/8/8/X/X

                   To vary the number of lanes in the SM,
                   Vary the value of global variable "LANES" in "harp_emulator.cpp" | Line 35 or 36

                   To vary number of warps in the SM,
                   Vary the value of global variable "WARPS" in "harp_emulator.cpp" | Line 37 or 38

		   Example: ./harp_runs/mem_remap_coal/harp_emulator -warps 8 -lanes 8 -chsize 32768 -chlsize 16 assignment5_programs/bin/scan.8.bin tester_scan_8.log -mem ./results/mem_remap_coal/chl16/mem_trace_16_map_coal_scan_8.log
 
