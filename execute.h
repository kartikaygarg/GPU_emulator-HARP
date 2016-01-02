///////////////////////////////////////////////////////
// For ECE 8823 : GPU Architecture
// by Kartikay Garg (GT iD: 903129139)
// Assignment 4 : GPU architecture : Warp Divergence & Multi lane parallel execution model
// Sub on: 3rd November 11:55 pm
//
//////////////////////////////////////////////////

#include <vector>
#include "decoder.h"
//#include "fetch.h"

//extern char ENCODE;		//1: Word encoding || 0: byte encoding
extern uint8_t LANES;
extern uint8_t WARPS;	//no of SMs???
extern uint8_t BYTE_ADDR;
extern char* halt_exec;

#define INSTR_SIZE 8*BYTE_ADDR

class bit_int{
	public:
	int bit : 1;
	bit_int();
};

class thread{
	public:			//see if keep public or protected (register space)
	//uint8_t warp_id;
	//Make/TYPECAST results from 64 bit values acc to the INSTR_SIZE : that is 32 bit registers or 64 bit registers
	uint64_t* regs;
	bit_int* pred_regs;
	//uint64_t* pred_regs;
	//latch_dec latch should be a 1 common latch keeping the fetched decoded instruction for all the threads
	//latch_dec thread_latch;			//this will keep a copy of the currently being processed instruction in its decoded manner
	//data struct for Thread context : thread ID etc
	
	void init();
	thread();
	void print_regs(ofstream*, uint8_t);
	void print_pred_regs(ofstream*, uint8_t);
	//latch_dec* decoded_latch;
};


//Execute stage has no of threads = no of LANES
// And the execute stage will run = no of Thread Block times i.e. xWARPS times
class execute_stg{
	public:
	//thread lanes[LANES];
	thread* lanes;
	latch_dec exec_latch;			//this will keep a copy of the currently being processed instruction in its decoded manner
	//char inst_str[100];
	string inst_str;
	uint64_t offset;
	uint8_t new_active_lanes_cnt;
	char status_execute;			//keeps a track of execution's stage	::	0: execute not start		1: execution underway		2: execution complete
	//data struct for warp context
	
	execute_stg();
	uint8_t new_warp_id;
	uint8_t warps_launched;		//keeps a track of no of warps launched by this warp
	char* new_activity_mask;			//for activity mask of new instruction that is to be fetched
	//char new_activity_mask[LANES+1];
	//shared memory block allocated here 
	//Transform and manupilate this also according to 32 bit or 64 bit
	uint64_t* shared_mem;
	//char execute(uint8_t, latch_dec*, scheduler*);
	//char execute(latch_dec*, scheduler*);
	char execute(latch_dec*, scheduler*, execute_stg*);
	void init(scheduler*);
	//void init(latch_dec* decoded_latch);
	
	void print_reg_state(ofstream*);
	void print_pred_reg_state(ofstream*);
};

class barrier{
	public:
	uint8_t wrp_cnt_req;				//No of required warps to reach the arrier before realeasing the control
	uint8_t wrp_cnt;
	//uint8_t warp_ids[WARPS];
	uint8_t warp_ids[8];
	uint8_t cnt;
	uint64_t bar_id;
	char status_bar;					//status : 0 means "not yet arrived" | 1: barrier pending to be resolved i.e. some entered, some still yet to arrive | 2: barrier resolved
	
	barrier();
	void resolve(execute_stg*);
};

void insert(uint64_t id_bar, uint8_t warp_id_enter, uint64_t cnt_req, execute_stg* exec1);


class stack_pdom{
	public:
	uint64_t next_pc;
	//uint64_t call_pc;
	uint64_t reconv_pc;
	char* occupy_mask;
	//char occupy_mask[LANES+1];
	stack_pdom* next_link;
	
	stack_pdom();
	//char push_in(uint64_t, char*);
	//stack_pdom pop_top();
	
	
	
};

char push_in(uint64_t, char*, uint8_t);
stack_pdom pop_top(uint8_t);





