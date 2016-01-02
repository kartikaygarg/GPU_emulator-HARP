///////////////////////////////////////////////////////
// For ECE 8823 : GPU Architecture
// by Kartikay Garg (GT iD: 903129139)
// Assignment 4 : GPU architecture : Warp Divergence & Multi lane parallel execution model
// Sub on: 3rd November 11:55 pm
//
//////////////////////////////////////////////////

#include <inttypes.h>
//using namespace std;

//extern uint8_t BYTE_ADDR;
//extern uint8_t LANES;
extern uint8_t WARPS;

extern char* halt_exec;		//[WARPS];

class instr_req{
	public:
	uint64_t addr;
	uint8_t warp_id;
	//char act_mask[LANES];
	char* act_mask;
	uint8_t active_lanes_cnt; 
	instr_req();
};

class latch{
	public:
	uint32_t instr;				//later on change to handle 64 bit instructions as well
	instr_req fetch_req;
	//uint64_t instr;
	//uint64_t addr;
	//uint8_t op_type;
	//reg1
	//reg2
	//dest
	latch();
	
	
};

class scheduler{
	protected:
	instr_req* fetch_req;
    int fetch_start;
	int fetch_end;
	uint8_t MAX_FETCH_BUF;
	
	public:
	//scheduler();
	uint8_t fetch_req_push(uint64_t, char*, uint8_t, uint8_t);
	//scheduler(uint8_t);
	scheduler();
	instr_req fetch_req_pop();			//return -1 if fetch queue is empty
										//stop execution when next_pc == current PC, i.e. END statement OR next_pc in fetch_req FIFO are all same
	
};

class fetch_stg{
	//protected:
	//uint64_t addr;
	//uint64_t instr;
	public:
	uint64_t instr;
	instr_req fetch_req;
	void fetch_next(scheduler*);
	fetch_stg();
	latch op_fetch;
	
};

