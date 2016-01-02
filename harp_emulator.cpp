///////////////////////////////////////////////////////
// For ECE 8823 : GPU Architecture
// by Kartikay Garg (GT iD: 903129139)
// Assignment 4 : GPU architecture : Warp Divergence & Multi lane parallel execution model
// Sub on: 3rd November 11:55 pm
//
//////////////////////////////////////////////////

//#define debug_all 3
//#define debug_trace 1
//#define debug_prints 3

#ifdef debug_all
	#ifndef debug_trace
		#define debug_trace 3
	#endif
	#ifndef debug_prints
		#define debug_prints 3
	#endif
#endif

#include <iostream>
#include <fstream>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <cstdlib>
//#include <vector>
#include <iomanip>
using namespace std;

char inp_bin_file[50];
char out_file[50];
char dev_out_file[50];
char overshoot_file[50];
char mem_stream_file[50];
char asm_file[50];

ofstream asm_trace_file;
ofstream mem_stream_out;
ofstream ptr_out_file;
fstream overshoot_out_file;
ofstream ptr_dev_out_file;
ifstream ptr_inp_file;
//char halt_exec = 0;
char* halt_exec;
uint64_t global_mem_size;

uint8_t GPR_LANE=8;
uint8_t PRED_REG_LANE=8;
char ENCODE = 1;		//1: Word encoding || 0: byte encoding
//uint8_t LANES=1;		//Currently keeping 1 thread
uint8_t LANES=8;
//uint8_t WARPS=1;		//Currently keeping 1 warp/SM
uint8_t WARPS=8;	//no of SMs???
uint8_t BYTE_ADDR=4;

uint8_t CACHE_LINE=16;
uint32_t CACHE_SIZE=32768;

uint64_t* mem_access_addr;

#define INSTR_SIZE 8*BYTE_ADDR
#define ADDR_SIZE 8*BYTE_ADDR

//for 4w/8/8/8/8

//introduce mutex locks in order to lock the main memory in case of alteration by ultiple threads
//introduce concept of p threads for multi lane SIMDs
uint8_t* main_mem;

#include "execute.h"
vector<barrier> bar_list;
extern uint64_t instr_cnt;
//stack_pdom* top[8];
stack_pdom** top;

void die_message(const char *msg) {
    printf("%s. Exiting...\n", msg);
	ptr_inp_file.close();
	ptr_out_file.close();
	overshoot_out_file.close();
	ptr_dev_out_file.close();
	asm_trace_file.close();
    exit(1);
}

void die_usage() {
    printf("Usage : ./harp_emulator [-h] <program> <output_generated> [options]\n");
    printf("Binary Trace driven GPU Emulator\n\n");
    printf("   -h or -help      :  Usage symantics\n");
    printf("   program          :  Name of program binary to be emulated (Extn: *.bin\n");
    printf("   output_generated :  Name of output file(Default: out.log)\n");
    printf("Options\n");
	printf("   -lanes  <expr>  :  Parameterized architecture ID specs (Default: 8)\n");
    printf("                       Ranges from 1 to 64\n");
	printf("   -warps  <expr>  :  Parameterized architecture ID specs (Default: 1)\n");
    printf("                       Ranges from 1 to 64\n");
    //printf("   -archID  <expr>  :  Parameterized architecture ID specs (Default: 4w/8/8/8/8)\n");
    //printf("                       Ranges from 4w/8/8/1/1 to 8w/64/64/64/64\n");
}

streampos main_mem_init(ifstream* ptr_inp_file2, ofstream* ptr_out_file2){
	uint64_t i=0;
	streampos pos;
	ptr_inp_file2->seekg(0,ios::end);
	pos = ptr_inp_file2->tellg();
	global_mem_size = pos;
	#ifdef debug_prints
		if(debug_prints > 2){
			cout<<"\nRAM Memory size: "<<hex<<global_mem_size;
		}
	#endif
	main_mem = new uint8_t [pos];
	ptr_inp_file2->seekg(0,ios::beg);
	while(!(ptr_inp_file2->eof())){
		ptr_inp_file2->read((char*)(&main_mem[i]),BYTE_ADDR*sizeof(uint8_t));
		i+=BYTE_ADDR;
	}
	return pos;
}

int main(int argc, char** argv){

	int ptr;
	uint8_t lane_id;			//to keep track of currently being executed lane
	uint8_t check=0;
	uint64_t mem_size,ii;
	char stats=0;
	
	strcpy(overshoot_file, "Overshoot.log");
	strcpy(dev_out_file, "bin_processed_out.log");
	strcpy(asm_file, "asm_trace_out.log");
	strcpy(mem_stream_file, "Memstream.log");
	//mem_stream_out = NULL;
	mem_access_addr = NULL;
	if(argc < 1) {
    	die_message("Must Provide a Trace File"); 
    }
	instr_cnt = 0;	
    //--------------------------------------------------------------------
    // -- Get params from command line 
    //--------------------------------------------------------------------    
    for ( ptr = 1; ptr < argc; ptr++) {
		if (argv[ptr][0] == '-') {
			if (!strcmp(argv[ptr], "-h") || !strcmp(argv[ptr], "-help")) {
				die_usage();
			}
			else if (!strcmp(argv[ptr], "-lanes")) {
				LANES = atoi(argv[++ptr]);
				//cout<<"Lanes: "<<(int)LANES<<endl;
				/*
				if (ptr < argc - 1) { 
		    		//params = atoi(argv[ptr+1]);
		    		ptr += 1;
				}
				*/
			}
			else if (!strcmp(argv[ptr], "-mem")) {
				strcpy(mem_stream_file, argv[++ptr]);
				mem_stream_out.open(mem_stream_file,ios::out);
			}
			else if (!strcmp(argv[ptr], "-chsize")) {			//l1dsize 32768 l1dbsize 16 l1dassoc 4 l1dwalloc n l1dccc
				CACHE_SIZE = atoi(argv[++ptr]);
				//cout<<"Cache Size: "<<(int)CACHE_SIZE<<endl;
			}
			else if (!strcmp(argv[ptr], "-chlsize")) {
				CACHE_LINE = atoi(argv[++ptr]);
				//cout<<"Cache Line Width: "<<(int)CACHE_LINE<<endl;
			}
			else if (!strcmp(argv[ptr], "-warps")) {
				WARPS = atoi(argv[++ptr]);
				//cout<<"Warps: "<<(int)WARPS<<endl;
				/*
				if (ptr < argc - 1) { 
		    		//params = atoi(argv[ptr+1]);
		    		ptr += 1;
				}
				*/
			}
			else if (!strcmp(argv[ptr], "-archID")) {
				if (ptr < argc - 1) { 
		    		//params = atoi(argv[ptr+1]);
		    		ptr += 1;
				}
			}
		}
		else {
	  		//strcpy(inp_bin_file, argv[1]);
			strcpy(inp_bin_file, argv[ptr]);
	  		//strcpy(out_file, argv[2]);
			strcpy(out_file, argv[++ptr]);
	  		ptr_inp_file.open(inp_bin_file, ios::binary | ios::in);
	  		if(!ptr_inp_file.is_open()){
	  			die_message("Invalid Trace File. Unable to open.\n");
	  		}
			ptr_dev_out_file.open(out_file,ios::out);
			ptr_out_file.open(dev_out_file,ios::out | ios::binary);
			overshoot_out_file.open(overshoot_file,ios::out | ios::in | ios::binary);
			asm_trace_file.open(asm_file,ios::out);
	  		//++ptr;
		}
    }
	
	///////////////////////////////////
	//Systems State initialisation
	/////////////////////////////////
	
	uint64_t START=0;		//starting PC
	char actv_msk[LANES+1];
	halt_exec = new char [WARPS];
	top = new stack_pdom* [WARPS];
	for(ptr=0;ptr<LANES;++ptr){
		actv_msk[ptr] = 1;
	}
	for(ptr=0;ptr<WARPS;++ptr){
		halt_exec[ptr] = 0;
		top[ptr] = NULL;
	}
	//active_lanes =  1;
	actv_msk[LANES] = '\0';				//start with only 1 active thread
	fetch_stg fetch_stg1; 	
	//fetch_stg fetch_stg1[WARPS]; 	
	scheduler scheduler1;
	//scheduler scheduler1[WARPS];
	decode_stg decoder1;
	//decode_stg decoder1[WARPS];
	//execute_stg exec1;
	execute_stg exec1[WARPS];
	
	
	
	//////////////////// Main Memory init ///////////////////////
	mem_size = main_mem_init(&ptr_inp_file, &ptr_out_file);	//copy the inout code binary in a commmon global memory space (byte addressable)
    //cout<<"\nAssuming a fetch request buffer of size: "<<3*WARPS<<" having 3 slots per warp.";		//keeping 3 instruction request slots per SM
	//Push the starting fetching address into the fetch request queue
	
	check = scheduler1.fetch_req_push(START, actv_msk, 0, 1);		//start with just 1 active lane
	//check = scheduler1[warp_id].fetch_req_push(START, actv_msk, 0, 1);		//start with just 1 active lane
    exec1[0].status_execute = 0;			//Turn on just 1 warp initially
	if(check == -1){
		die_message("Unable to load into Fetch buffer.\n");
	}
	
	
	//Proposals for multiple SMs(threadblocks) AND multiple lanes (barriers across them) & the (pc+4)-4 states
	//halt_exec: denotes, occurance of pc+4-4 state: for a thread : so the threads & warp of that state exit. Loop this over no of SMs/no of TBs number ;; then make halt_exec = 2 and thus stop the overall execution. Whereas halt_exec is set to '1' during the scheduler/fetcher(pref, as scheduler/fetcher might schedule/fetch the same instrn for multiple warps) ;; 	So set halt_exec = 1 ; to signal stop fetching & hint other modules to stop their processing
	//status_execute within the threads class can be used as a parameter for barriers.		;; 		when a barrier is encountered, the status is changed to '1' and thus the warp does not finish, but the thread finishes. and starts the execution of other threads. Thus in a sense waits for letting other threads to finish. Then when every thread reaches barrier, i.e. the inner loop expires over LANES value, all threads are '1' and thus now the combined read/write happen on the main memory acc to the threadIds. Thus now mark status_execute as '2'. this would mark the resuming of execution of thread processing further (while resetting the thread's status_execute as '0').
	//  At the final execution completion/merging of the warps (STILL NEED TO FIGURE OUT HOW), we will merge the threads and retire the warp when status_execute = '2'
	// So for now, when there is no warp_divergence we will directly make status_execute as '2'
	
	//DECIDE: CHECK & IMPLEMET://
	//BETTER app: When do we retire a warp, first execute all TBs i.e. all warps for an instruction? And retire all the warps (TBs) and then fetch the next instruction?
	//Alternate: Execute all instructions of a thread, and then retire the thread. And then work on all the other threads of same warp, one by one. And then process all warps, one by one in same fashion. So in this case, the "thread" will be retired, on encountering the HALT instruction.
	
	//ii=0;
	//while((ii<200) && (halt_exec != 2)){
	
	int warp_id = 0;
	int warp_complete = 0;
	//Run the same concept across multiple warps/SMs/Thread Blocks.
	//for(int warp_id=0;warp_id<WARPS;++warp_id){
	while(warp_id<WARPS){	

	//while(halt_exec != 2){
	//while(exec[warp_id].status_execute != 2){	
	while(exec1[warp_id].status_execute == 0){	
		warp_complete = 0;
		stats=0;
		
		fetch_stg1.fetch_next(&scheduler1);
		//fetch_stg1[warp_id].fetch_next(&(scheduler1[warp_id]));
		
		decoder1.parse(&fetch_stg1);
		//decoder1[warp_id].parse(&(fetch_stg1[warp_id]));
		
		//Generate objects for Thread Groups/SMs according to archID specified equal to multiple objects
		
		//stats = exec1[warp_id].execute(&(decoder1[warp_id].op_decode),&(scheduler1[warp_id]));			//Run execute stage, which will in turn, internally run all the threads
		
		//stats = exec1[decoder1.op_decode.fetch_req.warp_id].execute(&(decoder1.op_decode),&(scheduler1));			//Run execute stage, which will in turn, internally run all the threads
		stats = exec1[decoder1.op_decode.fetch_req.warp_id].execute(&(decoder1.op_decode),&(scheduler1),exec1);			//Run execute stage, which will in turn, internally run all the threads
		
		//Handle data collisions among different threads here. As multipe threads may simultaneous write onto a same meory location, and thus after emulation of all the threads belonging to 1 SM, we handle the collisions and memory coalesce common memory accesses.
		//Global memory shared across threads of a SM is thus kept coherent and sane.
		
		#ifdef debug_trace
			if(debug_trace>=1){
				if(stats){
					asm_trace_file<<"\n";
					//exec1[warp_id].print_reg_state(&asm_trace_file);
					exec1[decoder1.op_decode.fetch_req.warp_id].print_reg_state(&asm_trace_file);
					asm_trace_file<<"\n";
					//exec1[warp_id].print_pred_reg_state(&asm_trace_file);
					exec1[decoder1.op_decode.fetch_req.warp_id].print_pred_reg_state(&asm_trace_file);
					asm_trace_file<<"\n";
				}
			}
		#endif
		
		#ifdef debug_trace
			if(debug_trace>=1){
				asm_trace_file<<"\nActivity Mask: ";
			}
		#endif
		#ifdef debug_prints
			if(debug_prints>=1){
				cout<<"\nActivity Mask: ";
			}
		#endif
		//asm_trace_file<<stoi(decoder1.op_decode.fetch_req.act_mask);
		//cout<<atoi(decoder1.op_decode.fetch_req.act_mask)<<endl;
		
		#if defined(debug_trace) || defined(debug_prints)
		for(ptr=0;ptr<LANES;++ptr){
			#ifdef debug_trace
			if(debug_trace>=1){
				asm_trace_file<<(int)(decoder1.op_decode.fetch_req.act_mask[ptr]);
			}
			#endif
			#ifdef debug_prints
			if(debug_prints>=1){
				cout<<(int)(decoder1.op_decode.fetch_req.act_mask[ptr]);
			}
			#endif
		}
		#endif
		
	}
	++warp_complete;			//Mark the warp as complete
	warp_id += 1;
	if(warp_complete == WARPS){				//Stop scheduling warps when all finish execution i.e. no of completed warps is: 8
		break;
	}
	if(warp_id == WARPS){				//To loop over the scheduling of warps.
		warp_id = 0;
	}	
	
	}			
	//Closing loop of warp_id. Pass the warp_id just as the thread_id to the fetcher/decoder/execute stage blocks.
	//Each core/SM has a separate independent fetcher/decoder/execute stage module, which is in turn shared among all the threads belonging to 1 SM
	//This pipelining will help track dependencies in later on assignment & implementation models
    
	ptr_inp_file.close();
	ptr_out_file.close();
	overshoot_out_file.close();
	ptr_dev_out_file.close();
	mem_stream_out.close();
	asm_trace_file.close();

}
