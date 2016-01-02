///////////////////////////////////////////////////////
// For ECE 8823 : GPU Architecture
// by Kartikay Garg (GT iD: 903129139)
// Assignment 4 : GPU architecture : Warp Divergence & Multi lane parallel execution model
// Sub on: 3rd November 11:55 pm
//
//////////////////////////////////////////////////

//#define debug_all 3
//#define debug_trace 2
//#define debug_prints 3

#define coal_mem 1		//0: no memory coalescing, but print memory trace file || 1: memory coalescing activated along with memory trace print
#define mem_remap 1		//0: no memory re-mapping, but print memory trace file || 1: memory mapping activated along with memory trace print

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
#include <cstdlib>				// for calloc
#include <string.h>
#include <string>
#include <sstream>
//#include <stdio.h>
//#include <stdlib.h>

#include "execute.h"

using namespace std;

extern uint8_t LANES;
extern uint8_t BYTE_ADDR;
extern uint8_t GPR_LANE;
extern uint8_t PRED_REG_LANE;
//extern char halt_execute;
extern char* halt_exec;
extern ofstream ptr_out_file;
extern ofstream mem_stream_out;
extern fstream overshoot_out_file;
extern ofstream ptr_dev_out_file;
extern ofstream asm_trace_file;
extern uint8_t* main_mem;
//extern uint8_t active_lanes;
extern uint64_t global_mem_size;
extern uint64_t* mem_access_addr;
extern vector<barrier> bar_list;
extern uint8_t CACHE_LINE;
extern uint16_t CACHE_SIZE;
//extern ifstream ptr_inp_file;

extern stack_pdom** top;

//stack_pdom* top[WARPS];

uint64_t instr_cnt;

bit_int::bit_int(){
	bit = 0;
}

void thread::init(){
	//status_execute = 0;
}

thread::thread(){
	regs = (uint64_t*)std::calloc(GPR_LANE,sizeof(uint64_t));			//manage if 32 bit size data registers		:: can make by	(uint32_t*)(regs[i])		& keep regs as uint64_t* type in execute.h
	pred_regs = (bit_int*)std::calloc(PRED_REG_LANE,sizeof(bit_int));			//manage if 32 bit size data registers
	//pred_regs = (uint64_t*)std::calloc(PRED_REG_LANE,sizeof(uint64_t));			//manage if 32 bit size data registers
	//warp_id = 0;
	init();
}

void thread::print_pred_regs(ofstream* out_file, uint8_t id){
	//*out_file<<"\nPredicate state: for Lane: "<<hex<<id;
	*out_file<<"\nPredicate state: for Lane: "<<(int)id;
	for(char ptr=0;ptr<PRED_REG_LANE;++ptr){
		*out_file<<"\n@p"<<(int)(ptr)<<": "<<(int)pred_regs[ptr].bit;
	}
}

void thread::print_regs(ofstream* out_file, uint8_t id){
	//*out_file<<"\nRegister state: for Lane: "<<hex<<id;
	*out_file<<"\nRegister state: for Lane: "<<(int)id;
	for(char ptr=0;ptr<GPR_LANE;++ptr){
		*out_file<<"\n%r"<<(int)(ptr)<<": "<<hex<<regs[ptr];
	}
	
}

void execute_stg::print_pred_reg_state(ofstream* out_file){
	*out_file<<"Predicate state: ";//"for Lane: "<<(int)id;
	for(char ptr=0;ptr<PRED_REG_LANE;++ptr){
		//*out_file<<"\nPredicate state: for Lane: "<<hex<<id;
		*out_file<<"\n@p"<<(int)(ptr)<<":";
		for(char lane_ptr=0;lane_ptr<LANES;++lane_ptr){
			*out_file<<"  "<<(int)(lanes[lane_ptr].pred_regs[ptr].bit);
		}
	}
	//cout<<"\t";		//Print Warp no.
	
}

void execute_stg::print_reg_state(ofstream* out_file){
	*out_file<<"Register state: ";//"for Lane: "<<(int)id;
	for(char ptr=0;ptr<GPR_LANE;++ptr){
		//*out_file<<"\nRegister state: for Lane: "<<hex<<id;
		*out_file<<"\n%r"<<(int)(ptr)<<":";
		for(char lane_ptr=0;lane_ptr<LANES;++lane_ptr){
			*out_file<<"  "<<hex<<lanes[lane_ptr].regs[ptr];
		}
	}
	//cout<<"\t";		//Print Warp no.
	
}

stack_pdom::stack_pdom(){
	int i;
	next_link = NULL;
	occupy_mask = new char [LANES+1];
	for(i=0;i<LANES;++i){
		occupy_mask[i] = 1;
	}
	occupy_mask[LANES] = '\0';
	reconv_pc = 0;
	next_pc = -1;
	
}

char push_in(uint64_t pc, char* act_mask, uint8_t wp_id){
	char stat = -1;
	stack_pdom* temp;
	temp = new stack_pdom ;
	if(temp == NULL){
		cout<<"\nFailed to push divergence details into stack. Exits.";
		return stat;
	}
	stat = 1;
	if(top[wp_id] == NULL){
		top[wp_id] = temp;
		temp->next_link = NULL;
		//temp->next_pc = pc;
		//strcpy(temp->occupy_mask,act_mask);
	}
	else{
		temp->next_link = top[wp_id];
		//temp->next_pc = pc;
		//strcpy(temp->occupy_mask,act_mask);
		top[wp_id]=temp;
	}
	temp->next_pc = pc;
	//strcpy(temp->occupy_mask,act_mask);
	for(int i=0;i<LANES;++i){
		temp->occupy_mask[i] = act_mask[i];
	}
	temp = NULL;
	return stat;
}

stack_pdom pop_top(uint8_t wp_id){
	//char success = 0;
	stack_pdom temp_ret;
	stack_pdom* del = NULL;
	if(top[wp_id] != NULL){
		temp_ret = *(top[wp_id]);
		del = top[wp_id];
		//success = 1;
		//top[warp_id] = *(top[warp_id])->next_link;
		top[wp_id] = (top[wp_id])->next_link;
		delete del;
	}
	
	return temp_ret;
}

barrier::barrier(){
	int i;
	wrp_cnt_req = 1;
	wrp_cnt = 0;
	cnt = 0;
	bar_id = 0;
	status_bar = 0;
	for(i=0;i<WARPS;++i){
		warp_ids[i] = WARPS;				//initialise warp ids required for this warp as invalid warp nos
	}
}

void insert(uint64_t id_bar, uint8_t warp_id_enter, uint64_t cnt_req, execute_stg* exec1){
	
	char flag = 0;
	vector<barrier>::iterator iter;
	barrier temp;
	iter = bar_list.begin();
	int cnter=0;
	for(iter = bar_list.begin(); iter!=bar_list.end(); ++iter){
		
		if(bar_list[cnter].bar_id == id_bar){			//barrier already existed	: so no need to establish its barrier id
			 bar_list[cnter].wrp_cnt_req = cnt_req;
			 bar_list[cnter].warp_ids[bar_list[cnter].cnt] = warp_id_enter;
			 bar_list[cnter].status_bar = 1; 
			 ++(bar_list[cnter].wrp_cnt);
			 ++(bar_list[cnter].cnt);
			 flag = 1;
			 bar_list[cnter].resolve(exec1);
			 break;
		}
		++cnter;
	}
	if(flag == 0){
		temp.bar_id = id_bar;
		temp.status_bar = 1;
		temp.wrp_cnt_req = cnt_req;
		temp.warp_ids[temp.cnt] = warp_id_enter;
		++(temp.cnt);
		++(temp.wrp_cnt);
		bar_list.push_back(temp);
		temp.resolve(exec1);
	}
	//bar_list.resolve(exec1);
}

void barrier::resolve(execute_stg* exec1){
	int ptr;
	if(wrp_cnt == wrp_cnt_req){
		status_bar = 2;
		for(ptr=0;(ptr<WARPS) && (warp_ids[ptr] != WARPS);++ptr){
			exec1[warp_ids[ptr]].status_execute = 0;				//resume suspended warp
		}
		cnt = 0;
		wrp_cnt_req = 1;
		wrp_cnt = 0;
	}
}

void execute_stg::init(scheduler* scheduler1){
	char i;
	//uint8_t warp_id;
	char status=-1;
	
	////////////////// TEMPORARY //////////////////////////////
	// initialise all thread lanes as active and active lanes. But on basis of threadId or context registers, decide if activity mask for the respective lane is 1 or 0 for execution of the NEXT INSTRUCTION
	// Pass this info of activity mask along with the PC for the next instruction to the scheduler FIFO via the req_push() func of scheduler
	
	/*
	for(i=0;i<LANES;++i){
		lanes[i].init();				//initialise only the active threads i.e. the ones that are to be executed
		new_activity_mask[i] = 1;				//initialise all Actb=vity masks as 0 i.e. inactive
	}
	new_activity_mask[LANES] = '\0';
	*/
	///////////////////////////////////////////////////////////
		
	if (!(halt_exec[new_warp_id]) ){
		//		later execute stage's completion is gonna feed what instruction address to fetch next
		//next instr's address is fed by exec stage	| also supplementary like WARP_ID of next instrn and activity mask
		
		status = scheduler1->fetch_req_push(((exec_latch.fetch_req.addr+BYTE_ADDR+offset) & (0xFFFFFFFFFFFFFFFF>>(64-INSTR_SIZE))),new_activity_mask,new_warp_id,new_active_lanes_cnt);
		
		if(status == -1){			// i.e. fetch next instruction request was not fulfilled as the fetch buffer was full etc
			//halt or RE-DO this instruction, as the next fetch instruction's request was not put successfully
			cout<<"\nPushing of a fetch request to the fetch block failed. Perhaps the fetch buffer is full.\n";
		}
			
	}
}


execute_stg::execute_stg(){
	char i=0;
	lanes = new thread [LANES];
	new_activity_mask = new char [LANES+1];
	new_activity_mask[LANES] = '\0';
	new_active_lanes_cnt = 1;
	new_warp_id = 0;
	warps_launched = 0;
	status_execute = 2;
	inst_str = "";
	offset = 0;
	shared_mem = NULL;					//assign this according to kernel passing parameters at time of kernel launch
	
	//init();			//Can't put here, because otherwise will push in 0+4 into fetch_request queue at scheduler, before pushing in START=0 (at time of instantiation of object of execute_stg only)
}

char execute_stg::execute(latch_dec* decoded_latch, scheduler* scheduler1, execute_stg* exec1){
	uint8_t i,id;
	//uint8_t new_active_lanes;
	char flag_pc_alter = 0;
	char asm_process = 0;
	stack_pdom popped;
	char taken_act_mask[LANES+1];
	char not_taken_act_mask[LANES+1];
	//char new_warp_act_mask[LANES+1];
	char warp_spawn_act_mask[LANES+1];
	char push_stat,status;
	uint64_t access_start=-1;
	
	
	#ifndef debug_trace
		asm_process=1;
	#endif
	
	
	exec_latch = *decoded_latch;
	offset = 0;
	uint64_t index=0;
	ostringstream reg_char;//,prints2;
	inst_str = "";			//Can put this in init
	reg_char.str(std::string());
	
	new_active_lanes_cnt = exec_latch.fetch_req.active_lanes_cnt;
	new_warp_id = exec_latch.fetch_req.warp_id;
	++instr_cnt;
	for(i=0;i<LANES;++i){
	//for(i=0;i<exec_latch.fetch_req.active_lanes_cnt;++i){
		new_activity_mask[i] = exec_latch.fetch_req.act_mask[i];
		taken_act_mask[i]=0;
		not_taken_act_mask[i]=0;
		warp_spawn_act_mask[i]=1;
	}
	//new_warp_act_mask[0]=1;
	taken_act_mask[LANES] = '\0';
	not_taken_act_mask[LANES] = '\0';
	//new_warp_act_mask[LANES] = '\0';
	warp_spawn_act_mask[LANES] = '\0';
	
	
	#ifdef debug_prints
		if(debug_prints>2){
			cout<<"\nActive lanes count: "<<(int)exec_latch.fetch_req.active_lanes_cnt<<" in warp: "<<(int)exec_latch.fetch_req.warp_id;
		}
	#endif
	
	inst_str += "\nNow ";
	reg_char<<(int)exec_latch.fetch_req.active_lanes_cnt;
	inst_str = inst_str + reg_char.str();
	reg_char.str(std::string());
	inst_str += " active threads in warp: ";
	reg_char<<(int)exec_latch.fetch_req.warp_id;
	inst_str = inst_str + reg_char.str();
	reg_char.str(std::string());
	#ifdef debug_trace
		if(debug_trace >= 1){
			asm_trace_file<<inst_str;
		}
	#endif
	inst_str = "";
	
	if(exec_latch.pred != 0){
		inst_str += "@p";
		reg_char<<(int)exec_latch.pred_reg;
		inst_str = inst_str + reg_char.str();
		reg_char.str(std::string());
		inst_str += " ? ";
	}
	
	if((exec_latch.op_code == i_SPLIT) || (exec_latch.op_code == i_JOIN)){
		if(exec_latch.op_code == i_SPLIT){	
			inst_str += " split";
			#ifdef debug_trace
				if(debug_trace >= 1){
					asm_trace_file<<"\nAddr: 0x"<<hex<<exec_latch.fetch_req.addr<<"\thas Instr: "<<inst_str;
				}
			#endif
			
			//for(i=0;i<exec_latch.fetch_req.active_lanes_cnt;++i){
			for(i=0;i<LANES;++i){
				if(lanes[i].pred_regs[exec_latch.pred_reg].bit != 0){			// if this predicate bit is 1, so then this is taken branch
					//taken_act_mask[i] = lanes[i].pred_regs[exec_latch.pred_reg].bit & exec_latch.fetch_req.act_mask[i];
					taken_act_mask[i] = ((bool)(lanes[i].pred_regs[exec_latch.pred_reg].bit) & ((bool)(exec_latch.fetch_req.act_mask[i]))) & (bool)(exec_latch.pred);
					 
				}
				else{			//if this predicate bit is 0, so then this is NOT TAKEN branch
					//not_taken_act_mask[i] = (!(lanes[i].pred_regs[exec_latch.pred_reg].bit))  & exec_latch.fetch_req.act_mask[i];;
					not_taken_act_mask[i] = ((bool)(!(lanes[i].pred_regs[exec_latch.pred_reg].bit)) | ((bool)(!(exec_latch.pred))) ) & (bool)exec_latch.fetch_req.act_mask[i];
				}
				if(!(exec_latch.pred)){
					#ifdef debug_trace
						if(debug_trace > 2){
							asm_trace_file<<"\nPredicate bit was low: 0";
						}
					#endif
				}				
			}
			
			#ifdef debug_trace
				if(debug_trace >= 2){			
					asm_trace_file<<"  Orig Activity: ";
					for(i=0;i<LANES;++i){
						asm_trace_file<<(int)(exec_latch.fetch_req.act_mask[i]);
					}
					asm_trace_file<<"\t Taken Activity: ";
					for(i=0;i<LANES;++i){
						asm_trace_file<<(int)(taken_act_mask[i]);
					}
					asm_trace_file<<"\t Not-Taken Activity: ";
					for(i=0;i<LANES;++i){
						asm_trace_file<<(int)(not_taken_act_mask[i]);
					}
				}
			#endif
			
			//push_stat = push_in(1, exec_latch.fetch_req.act_mask, exec_latch.fetch_req.warp_id);
			push_stat = push_in(0, exec_latch.fetch_req.act_mask, exec_latch.fetch_req.warp_id);				//reconv_pc i.e. next_pc=reconv_pc = unknown = 1
			
			if (push_stat != 1){
				cout<<"\nError in stack push. Exiting.....";
				exit(0);
			}
			
			push_stat = push_in(exec_latch.fetch_req.addr, not_taken_act_mask, exec_latch.fetch_req.warp_id);
			
			if (push_stat != 1){
				cout<<"\nError in stack push. Exiting.....";
				exit(0);
			}
			for(i=0;i<LANES;++i){
				new_activity_mask[i] = taken_act_mask[i];
			}
			new_activity_mask[LANES] = '\0';
	
			#ifdef debug_trace
				if(debug_trace >= 1){
					asm_trace_file<<"\nExecute TAKEN branch.";
				}
			#endif
		}
		else{		//opcode is JOIN : obvio
			inst_str += " join";
			#ifdef debug_trace
				if(debug_trace >= 1){
					asm_trace_file<<"\nAddr: 0x"<<hex<<exec_latch.fetch_req.addr<<"\thas Instr: "<<inst_str;
				}
			#endif
			popped = pop_top(exec_latch.fetch_req.warp_id);
			
			if(popped.next_pc == -1){
				#ifdef debug_trace
					if(debug_trace >= 1){
						asm_trace_file<<"\nEMPTY STACK. Keep moving on to join+4 instruction address.\n";
					}
				#endif
				#ifdef debug_prints
					if(debug_prints >= 1){
						cout<<"\nEMPTY STACK. Keep moving on to join+4 instruction address.";
					}
				#endif
			}
			else{
				if(popped.next_pc == 0){		//reconvergence PC
					//strcpy(new_activity_mask,popped.occupy_mask);
					for(i=0;i<LANES;++i){
						new_activity_mask[i] = popped.occupy_mask[i];
					}
					new_activity_mask[LANES] = '\0';
					
					//Keep moving on to join+4 instruction address.
					#ifdef debug_trace
						if(debug_trace >= 1){
							asm_trace_file<<"\nReconverge. Keep moving on to join+4 instruction address.";
						}
					#endif
				}
				else{							//NOT taken branch
					//strcpy(new_activity_mask,popped.occupy_mask);
					for(i=0;i<LANES;++i){
						new_activity_mask[i] = popped.occupy_mask[i];
					}
					new_activity_mask[LANES] = '\0';					
					offset = ((-1)*exec_latch.fetch_req.addr) + popped.next_pc;
					#ifdef debug_trace
						if(debug_trace >= 1){
							asm_trace_file<<"\nExecute NOT TAKEN branch.";
						}
					#endif
				}
				
			}
			
		}
		asm_process = 1;
	}
	else{
	
	mem_access_addr = new uint64_t [exec_latch.fetch_req.active_lanes_cnt];
	
  for(id=0;id<exec_latch.fetch_req.active_lanes_cnt;++id){		//initialise only the active threads i.e. the ones that are to be executed
	//new_activity_mask[id] = exec_latch.fetch_req.act_mask[id];
	mem_access_addr[id] = -1;
	if(exec_latch.fetch_req.act_mask[id]){
	  if( (exec_latch.pred == 0) || ( (exec_latch.pred != 0) && (lanes[id].pred_regs[exec_latch.pred_reg].bit != 0) ) ) {				//In order to execute an instruction on a lane, ensure 2 things: 1) Activity mask for the lane is 1 ;	2) Either the predicate bit of the instruction is 0 or else if the predicate bit is 1, then the prdicate register which is specified should be 1		
		switch(exec_latch.op_code){
			case i_NOP:	if(!(asm_process)){		
							inst_str += "nop";
						}
						break;
						
			case i_DI:	
						break;
			case i_EI:
						break;
			case i_TLBADD:
							break;
			case i_TLBFLUSH:
							break;
							
			case i_NEG:	if(!(asm_process)){		
							inst_str += "neg %r";
							reg_char<<(int)exec_latch.reg1;
							inst_str = inst_str + reg_char.str();
							reg_char.str(std::string());
							inst_str += " %r";
							reg_char<<(int)exec_latch.reg2;
							inst_str += reg_char.str();
							reg_char.str(std::string());
						}
						lanes[id].regs[exec_latch.reg1] = ((~(lanes[id].regs[exec_latch.reg2] )) + 1) & (0xFFFFFFFFFFFFFFFF>>(64-INSTR_SIZE));
						break;			
						
			case i_NOT: if(!(asm_process)){		
							inst_str += "not %r";
							reg_char<<(int)exec_latch.reg1;
							inst_str += reg_char.str();
							reg_char.str(std::string());
							inst_str += " %r";
							reg_char<<(int)exec_latch.reg2;
							inst_str += reg_char.str();
							reg_char.str(std::string());
						}
						lanes[id].regs[exec_latch.reg1] = (~(lanes[id].regs[exec_latch.reg2])) & (0xFFFFFFFFFFFFFFFF>>(64-INSTR_SIZE));
						break;
						
			case i_AND:	
						if(!(asm_process)){		
							inst_str += "and %r";
							reg_char<<(int)exec_latch.reg1;
							inst_str += reg_char.str();
							reg_char.str(std::string());
							inst_str += " %r";
							reg_char<<(int)exec_latch.reg2;
							inst_str += reg_char.str();
							reg_char.str(std::string());
							inst_str += " %r";
							reg_char<<(int)exec_latch.reg3;
							inst_str += reg_char.str();
							reg_char.str(std::string());
						}
						lanes[id].regs[exec_latch.reg1] = (lanes[id].regs[exec_latch.reg2] & lanes[id].regs[exec_latch.reg3]) & (0xFFFFFFFFFFFFFFFF>>(64-INSTR_SIZE));
						break;
						
			case i_OR:	if(!(asm_process)){		
							inst_str += "or %r";
							reg_char<<(int)exec_latch.reg1;
							inst_str += reg_char.str();
							reg_char.str(std::string());
							inst_str += " %r";
							reg_char<<(int)exec_latch.reg2;
							inst_str += reg_char.str();
							reg_char.str(std::string());
							inst_str += " %r";
							reg_char<<(int)exec_latch.reg3;
							inst_str += reg_char.str();
							reg_char.str(std::string());
						}
						lanes[id].regs[exec_latch.reg1] = (lanes[id].regs[exec_latch.reg2] | lanes[id].regs[exec_latch.reg3]) & (0xFFFFFFFFFFFFFFFF>>(64-INSTR_SIZE));
						break;
						
			case i_XOR: if(!(asm_process)){		
							inst_str += "xor %r";
						reg_char<<(int)exec_latch.reg1;
						inst_str += reg_char.str();
						reg_char.str(std::string());
						inst_str += " %r";
						reg_char<<(int)exec_latch.reg2;
						inst_str += reg_char.str();
						reg_char.str(std::string());
						inst_str += " %r";
						reg_char<<(int)exec_latch.reg3;
						inst_str += reg_char.str();
						reg_char.str(std::string());
						}
						lanes[id].regs[exec_latch.reg1] = (lanes[id].regs[exec_latch.reg2] ^ lanes[id].regs[exec_latch.reg3]) & (0xFFFFFFFFFFFFFFFF>>(64-INSTR_SIZE));
						break;
						
			case i_ADD: if(!(asm_process)){		
							inst_str += "add %r";
						reg_char<<(int)exec_latch.reg1;
						inst_str += reg_char.str();
						reg_char.str(std::string());
						inst_str += " %r";
						reg_char<<(int)exec_latch.reg2;
						inst_str += reg_char.str();
						reg_char.str(std::string());
						inst_str += " %r";
						reg_char<<(int)exec_latch.reg3;
						inst_str += reg_char.str();
						reg_char.str(std::string());
						}
						lanes[id].regs[exec_latch.reg1] = (lanes[id].regs[exec_latch.reg2] + lanes[id].regs[exec_latch.reg3]) & (0xFFFFFFFFFFFFFFFF>>(64-INSTR_SIZE));
						break;
						
			case i_SUB: if(!(asm_process)){		
							inst_str += "sub %r";
						reg_char<<(int)exec_latch.reg1;
						inst_str += reg_char.str();
						reg_char.str(std::string());
						inst_str += " %r";
						reg_char<<(int)exec_latch.reg2;
						inst_str += reg_char.str();
						reg_char.str(std::string());
						inst_str += " %r";
						reg_char<<(int)exec_latch.reg3;
						inst_str += reg_char.str();
						reg_char.str(std::string());
						}
						lanes[id].regs[exec_latch.reg1] = (lanes[id].regs[exec_latch.reg2] - lanes[id].regs[exec_latch.reg3]) & (0xFFFFFFFFFFFFFFFF>>(64-INSTR_SIZE));
						break;
						
			case i_MUL: if(!(asm_process)){		
							inst_str += "mul %r";
						reg_char<<(int)exec_latch.reg1;
						inst_str += reg_char.str();
						reg_char.str(std::string());
						inst_str += " %r";
						reg_char<<(int)exec_latch.reg2;
						inst_str += reg_char.str();
						reg_char.str(std::string());
						inst_str += " %r";
						reg_char<<(int)exec_latch.reg3;
						inst_str += reg_char.str();
						reg_char.str(std::string());
						}
						lanes[id].regs[exec_latch.reg1] = (lanes[id].regs[exec_latch.reg2] * lanes[id].regs[exec_latch.reg3]) & (0xFFFFFFFFFFFFFFFF>>(64-INSTR_SIZE));
						break;
						
			case i_DIV: if(!(asm_process)){		
							inst_str += "div %r";
						reg_char<<(int)exec_latch.reg1;
						inst_str += reg_char.str();
						reg_char.str(std::string());
						inst_str += " %r";
						reg_char<<(int)exec_latch.reg2;
						inst_str += reg_char.str();
						reg_char.str(std::string());
						inst_str += " %r";
						reg_char<<(int)exec_latch.reg3;
						inst_str += reg_char.str();
						reg_char.str(std::string());
						}
						lanes[id].regs[exec_latch.reg1] = (lanes[id].regs[exec_latch.reg2] / lanes[id].regs[exec_latch.reg3]) & (0xFFFFFFFFFFFFFFFF>>(64-INSTR_SIZE));
						break;
						
			case i_MOD: if(!(asm_process)){		
							inst_str += "mod %r";
						reg_char<<(int)exec_latch.reg1;
						inst_str += reg_char.str();
						reg_char.str(std::string());
						inst_str += " %r";
						reg_char<<(int)exec_latch.reg2;
						inst_str += reg_char.str();
						reg_char.str(std::string());
						inst_str += " %r";
						reg_char<<(int)exec_latch.reg3;
						inst_str += reg_char.str();
						reg_char.str(std::string());
						}
						lanes[id].regs[exec_latch.reg1] = (lanes[id].regs[exec_latch.reg2] % lanes[id].regs[exec_latch.reg3]) & (0xFFFFFFFFFFFFFFFF>>(64-INSTR_SIZE));
						break;
						
			case i_SHL: if(!(asm_process)){		
							inst_str += "shl %r";
						reg_char<<(int)exec_latch.reg1;
						inst_str += reg_char.str();
						reg_char.str(std::string());
						inst_str += " %r";
						reg_char<<(int)exec_latch.reg2;
						inst_str += reg_char.str();
						reg_char.str(std::string());
						inst_str += " %r";
						reg_char<<(int)exec_latch.reg3;
						inst_str += reg_char.str();
						reg_char.str(std::string());
						}
						lanes[id].regs[exec_latch.reg1] = (lanes[id].regs[exec_latch.reg2]<<lanes[id].regs[exec_latch.reg3]) & (0xFFFFFFFFFFFFFFFF>>(64-INSTR_SIZE));
						break;
						
			case i_SHR: if(!(asm_process)){		
							inst_str += "shr %r";
						reg_char<<(int)exec_latch.reg1;
						inst_str += reg_char.str();
						reg_char.str(std::string());
						inst_str += " %r";
						reg_char<<(int)exec_latch.reg2;
						inst_str += reg_char.str();
						reg_char.str(std::string());
						inst_str += " %r";
						reg_char<<(int)exec_latch.reg3;
						inst_str += reg_char.str();
						reg_char.str(std::string());
						}
						lanes[id].regs[exec_latch.reg1] = (lanes[id].regs[exec_latch.reg2]>>lanes[id].regs[exec_latch.reg3]) & (0xFFFFFFFFFFFFFFFF>>(64-INSTR_SIZE));
						break;
						
			case i_ANDI:if(!(asm_process)){		
							inst_str += "andi %r";
						reg_char<<(int)exec_latch.reg1;
						inst_str += reg_char.str();
						reg_char.str(std::string());
						inst_str += " %r";
						reg_char<<(int)exec_latch.reg2;
						inst_str += reg_char.str();
						reg_char.str(std::string());
						inst_str += " #0x";
						reg_char<<hex<<exec_latch.imm;
						inst_str += reg_char.str();
						reg_char.str(std::string());
						}
						lanes[id].regs[exec_latch.reg1] = (lanes[id].regs[exec_latch.reg2] & exec_latch.imm) & (0xFFFFFFFFFFFFFFFF>>(64-INSTR_SIZE));
						break;
							
			case i_ORI:	if(!(asm_process)){		
							inst_str += "ori %r";
						reg_char<<(int)exec_latch.reg1;
						inst_str += reg_char.str();
						reg_char.str(std::string());
						inst_str += " %r";
						reg_char<<(int)exec_latch.reg2;
						inst_str += reg_char.str();
						reg_char.str(std::string());
						inst_str += " #0x";
						reg_char<<hex<<exec_latch.imm;
						inst_str += reg_char.str();
						reg_char.str(std::string());
						}
						lanes[id].regs[exec_latch.reg1] = (lanes[id].regs[exec_latch.reg2] | exec_latch.imm) & (0xFFFFFFFFFFFFFFFF>>(64-INSTR_SIZE));
						break;
						
			case i_XORI:if(!(asm_process)){		
							inst_str += "xori %r";
						reg_char<<(int)exec_latch.reg1;
						inst_str += reg_char.str();
						reg_char.str(std::string());
						inst_str += " %r";
						reg_char<<(int)exec_latch.reg2;
						inst_str += reg_char.str();
						reg_char.str(std::string());
						inst_str += " #0x";
						reg_char<<hex<<exec_latch.imm;
						inst_str += reg_char.str();
						reg_char.str(std::string());
						}
						lanes[id].regs[exec_latch.reg1] = (lanes[id].regs[exec_latch.reg2] ^ exec_latch.imm) & (0xFFFFFFFFFFFFFFFF>>(64-INSTR_SIZE));
						break;
						
			case i_ADDI:if(!(asm_process)){		
							inst_str += "addi %r";
						reg_char<<(int)exec_latch.reg1;
						inst_str += reg_char.str();
						reg_char.str(std::string());
						inst_str += " %r";
						reg_char<<(int)exec_latch.reg2;
						inst_str += reg_char.str();
						reg_char.str(std::string());
						inst_str += " #0x";
						reg_char<<hex<<exec_latch.imm;
						inst_str += reg_char.str();
						reg_char.str(std::string());
						}
						lanes[id].regs[exec_latch.reg1] = (lanes[id].regs[exec_latch.reg2] + exec_latch.imm) & (0xFFFFFFFFFFFFFFFF>>(64-INSTR_SIZE));
						break;
						
			case i_SUBI:if(!(asm_process)){		
							inst_str += "subi %r";
						reg_char<<(int)exec_latch.reg1;
						inst_str += reg_char.str();
						reg_char.str(std::string());
						inst_str += " %r";
						reg_char<<(int)exec_latch.reg2;
						inst_str += reg_char.str();
						reg_char.str(std::string());
						inst_str += " #0x";
						reg_char<<hex<<exec_latch.imm;
						inst_str += reg_char.str();
						reg_char.str(std::string());
						}
						lanes[id].regs[exec_latch.reg1] = (lanes[id].regs[exec_latch.reg2] - exec_latch.imm) & (0xFFFFFFFFFFFFFFFF>>(64-INSTR_SIZE));
						break;
						
			case i_MULI:if(!(asm_process)){		
							inst_str += "muli %r";
						reg_char<<(int)exec_latch.reg1;
						inst_str += reg_char.str();
						reg_char.str(std::string());
						inst_str += " %r";
						reg_char<<(int)exec_latch.reg2;
						inst_str += reg_char.str();
						reg_char.str(std::string());
						inst_str += " #0x";
						reg_char<<hex<<exec_latch.imm;
						inst_str += reg_char.str();
						reg_char.str(std::string());
						}
						lanes[id].regs[exec_latch.reg1] = (lanes[id].regs[exec_latch.reg2] * exec_latch.imm) & (0xFFFFFFFFFFFFFFFF>>(64-INSTR_SIZE));
						break;
						
			case i_DIVI:if(!(asm_process)){		
							inst_str += "divi %r";
						reg_char<<(int)exec_latch.reg1;
						inst_str += reg_char.str();
						reg_char.str(std::string());
						inst_str += " %r";
						reg_char<<(int)exec_latch.reg2;
						inst_str += reg_char.str();
						reg_char.str(std::string());
						inst_str += " #0x";
						reg_char<<hex<<exec_latch.imm;
						inst_str += reg_char.str();
						reg_char.str(std::string());
						}
						lanes[id].regs[exec_latch.reg1] = (lanes[id].regs[exec_latch.reg2] / exec_latch.imm) & (0xFFFFFFFFFFFFFFFF>>(64-INSTR_SIZE));
						break;			
						
			case i_MODI:if(!(asm_process)){		
							inst_str += "modi %r";
						reg_char<<(int)exec_latch.reg1;
						inst_str += reg_char.str();
						reg_char.str(std::string());
						inst_str += " %r";
						reg_char<<(int)exec_latch.reg2;
						inst_str += reg_char.str();
						reg_char.str(std::string());
						inst_str += " #0x";
						reg_char<<hex<<exec_latch.imm;
						inst_str += reg_char.str();
						reg_char.str(std::string());
						}
						lanes[id].regs[exec_latch.reg1] = (lanes[id].regs[exec_latch.reg2] % exec_latch.imm) & (0xFFFFFFFFFFFFFFFF>>(64-INSTR_SIZE));
						break;
						
			case i_SHLI:if(!(asm_process)){		
							inst_str += "shli %r";
							reg_char<<(int)exec_latch.reg1;
							inst_str += reg_char.str();
							reg_char.str(std::string());
							inst_str += " %r";
							reg_char<<(int)exec_latch.reg2;
							inst_str += reg_char.str();
							reg_char.str(std::string());
							inst_str += ", #0x";
							reg_char<<hex<<(long long unsigned int)exec_latch.imm;
							inst_str += reg_char.str();
							reg_char.str(std::string());
						}
						lanes[id].regs[exec_latch.reg1] = (lanes[id].regs[exec_latch.reg2] << exec_latch.imm) & (0xFFFFFFFFFFFFFFFF>>(64-INSTR_SIZE));

						break;
						
			case i_SHRI:
						if(!(asm_process)){		
							inst_str += "shri %r";
						reg_char<<(int)exec_latch.reg1;
						inst_str += reg_char.str();
						reg_char.str(std::string());
						inst_str += " %r";
						reg_char<<(int)exec_latch.reg2;
						inst_str += reg_char.str();
						reg_char.str(std::string());
						inst_str += ", #0x";
						reg_char<<hex<<exec_latch.imm;
						inst_str += reg_char.str();
						reg_char.str(std::string());
						}
						lanes[id].regs[exec_latch.reg1] = (lanes[id].regs[exec_latch.reg2] >> exec_latch.imm) & (0xFFFFFFFFFFFFFFFF>>(64-INSTR_SIZE));
						break;
						
			case i_JALI:if(!(asm_process)){		
							inst_str += "jali %r";
							reg_char<<(int)exec_latch.reg1;
							inst_str += reg_char.str();
							reg_char.str(std::string());
							inst_str += " #0x";
							reg_char<<hex<<exec_latch.imm;
							inst_str += reg_char.str();
							reg_char.str(std::string());
						}
						lanes[id].regs[exec_latch.reg1] = exec_latch.fetch_req.addr+BYTE_ADDR;
						if(!(flag_pc_alter)){			
							offset = exec_latch.imm;
							flag_pc_alter = 1;
						}
						break;
						
			case i_JALR:if(!(asm_process)){		
							inst_str += "jalr %r";
						reg_char<<(int)exec_latch.reg1;
						inst_str += reg_char.str();
						reg_char.str(std::string());
						inst_str += " %r";
						reg_char<<(int)exec_latch.reg2;
						inst_str += reg_char.str();
						reg_char.str(std::string());
						}
						lanes[id].regs[exec_latch.reg1] = exec_latch.fetch_req.addr+BYTE_ADDR;
						if(!(flag_pc_alter)){			
							offset = lanes[id].regs[exec_latch.reg2] + (-1*(exec_latch.fetch_req.addr+BYTE_ADDR));
							flag_pc_alter = 1;
						}
						break;
						
			case i_JMPI:if(!(asm_process)){		
							inst_str += "jmpi ";
							inst_str += " #0x";
							reg_char<<hex<<exec_latch.imm;
							inst_str += reg_char.str();
							reg_char.str(std::string());
						}
						if(!(flag_pc_alter)){		
							offset = exec_latch.imm;
							flag_pc_alter = 1;
						}
						break;
						
			case i_JMPR:if(!(asm_process)){		
							inst_str += "jmpr %r";
							reg_char<<(int)exec_latch.reg1;
							inst_str += reg_char.str();
							reg_char.str(std::string());
						}
						if(!(flag_pc_alter)){			
							offset = lanes[id].regs[exec_latch.reg1] + (-1*(exec_latch.fetch_req.addr+BYTE_ADDR));
							flag_pc_alter = 1;
						}
						break;
						
			case i_CLONE:if(!(asm_process)){		
							inst_str += "clone %r";
							reg_char<<(int)exec_latch.reg1;
							inst_str += reg_char.str();
							reg_char.str(std::string());
						}
						//lanes[exec_latch.reg1] = lanes[id];			//All register space (GPR as well as PRED_REG)
						//*(lanes[exec_latch.reg1].regs) = *(lanes[id].regs);
						//(lanes[exec_latch.reg1].regs) = (lanes[id].regs);				//Shallow copy
						memcpy(lanes[lanes[id].regs[exec_latch.reg1]].regs,lanes[id].regs,(sizeof(uint64_t)*GPR_LANE));		//Deep copy
						break;
						
			case i_JALIS:if(!(asm_process)){		
							inst_str += "jalis %r";
							reg_char<<(int)exec_latch.reg1;
							inst_str += reg_char.str();
							reg_char.str(std::string());
							inst_str += " %r";
							reg_char<<(int)exec_latch.reg2;
							inst_str += reg_char.str();
							reg_char.str(std::string());
							inst_str += " #0x";
							reg_char<<hex<<exec_latch.imm;
							inst_str += reg_char.str();
							reg_char.str(std::string());
						}
						cout<<"\nid: "<<(int)id<<" exec_latch.reg1 : "<<(int)exec_latch.reg1<<" exec_latch.fetch_req.addr : "<<(int)exec_latch.fetch_req.addr;
						asm_trace_file<<"\nid: "<<(int)id<<" exec_latch.reg1 : "<<(int)exec_latch.reg1<<" exec_latch.fetch_req.addr : "<<(int)exec_latch.fetch_req.addr;
						//lanes[id].regs[exec_latch.reg1] = exec_latch.fetch_req.addr+BYTE_ADDR;			//ASK: if this value/register is updated only for the original lane or all the newly spawned lanes as well?
						if(!(flag_pc_alter)){			
							lanes[id].regs[exec_latch.reg1] = exec_latch.fetch_req.addr+BYTE_ADDR;			//ASK: if this value/register is updated only for the original lane or all the newly spawned lanes as well?
							new_active_lanes_cnt = (int)(lanes[id].regs[exec_latch.reg2]);
							offset = exec_latch.imm;
							flag_pc_alter = 1;
						}
						
						break;
						
			case i_JALRS:if(!(asm_process)){		
							inst_str += "jalrs %r";
							reg_char<<(int)exec_latch.reg1;
							inst_str += reg_char.str();
							reg_char.str(std::string());
							inst_str += " %r";
							reg_char<<(int)exec_latch.reg2;
							inst_str += reg_char.str();
							reg_char.str(std::string());
							inst_str += " %r";
							reg_char<<(int)exec_latch.reg3;
							inst_str += reg_char.str();
							reg_char.str(std::string());
						}
						lanes[id].regs[exec_latch.reg1] = exec_latch.fetch_req.addr+BYTE_ADDR;
						if(!(flag_pc_alter)){
							
							new_active_lanes_cnt = (int)(lanes[id].regs[exec_latch.reg2]);
							//asm_trace_file<<"\nActive: "<<(int)new_active_lanes_cnt<< "\t : "<<hex<<lanes[id].regs[exec_latch.reg2];
							offset = lanes[id].regs[exec_latch.reg3] + (-1*(exec_latch.fetch_req.addr+BYTE_ADDR));
							flag_pc_alter = 1;
						}
						
						break;
						
			case i_JMPRT:if(!(asm_process)){		
							inst_str += "jmprt %r";
							reg_char<<(int)exec_latch.reg1;
							inst_str += reg_char.str();
							reg_char.str(std::string());
						}
						
						if(!(flag_pc_alter)){			
							/*
							for(i=0;i<LANES;++i){			//ASK: 1) loop starts from "i=0" or from "i=id" ; and uptill where? "lanes[id].regs[exec_latch.reg2]" OR "lanes[id].regs[exec_latch.reg2] + id"
								new_activity_mask[i] = 0;
							}
							*/
							//new_activity_mask[0] = 1;		//
							//new_activity_mask[id] = 1;		//ASK: so we terminate rest all the lanes, but keep the 1st original active lane, or we simply diable all the lanes, and activate only the 1st lane (lane 0 ) by our will?
							new_active_lanes_cnt = 1;
							offset = lanes[id].regs[exec_latch.reg1] + (-1*(exec_latch.fetch_req.addr+BYTE_ADDR));
							flag_pc_alter = 1;
						}
						
						break;
						
			case i_LD:	if(!(asm_process)){		
							inst_str += "ld %r";
						reg_char<<(int)exec_latch.reg1;
						inst_str += reg_char.str();
						reg_char.str(std::string());
						inst_str += " %r";
						reg_char<<(int)exec_latch.reg2;
						inst_str += reg_char.str();
						reg_char.str(std::string());
						inst_str += " #0x";
						reg_char<<hex<<(long long unsigned int)exec_latch.imm;
						inst_str += reg_char.str();
						reg_char.str(std::string());
						}
						
						index = ( (lanes[id].regs[exec_latch.reg2] + exec_latch.imm) & (0xFFFFFFFFFFFFFFFF>>(64-INSTR_SIZE)) );
						//mem_stream_out<<"r 0x"<<hex<<index<<" "<<(int)BYTE_ADDR<<endl;
						mem_access_addr[id] = index;
						if( index == 0x80000000 ){
							mem_access_addr[id] = -1;
						}
						if(index <= global_mem_size){
						//if(index <= global_mem_size -4){
							memcpy(&(lanes[id].regs[exec_latch.reg1]), &(main_mem[index]), BYTE_ADDR*sizeof(uint8_t));
						}
						else{
								index -= 0x8001000;
								overshoot_out_file.seekg(index, ios::beg);
								overshoot_out_file.read((char*)&(lanes[id].regs[exec_latch.reg1]),BYTE_ADDR*sizeof(uint8_t));
								//overshoot_out_file.read((char*)&(lanes[id].regs[exec_latch.reg1]), BYTE_ADDR*sizeof(uint8_t));
						}
						break;
						
			case i_ST:	if(!(asm_process)){		
							inst_str += "st %r";
						reg_char<<(int)exec_latch.reg1;
						inst_str += reg_char.str();
						reg_char.str(std::string());
						inst_str += " %r";
						reg_char<<(int)exec_latch.reg2;
						inst_str += reg_char.str();
						reg_char.str(std::string());
						inst_str += " #0x";
						//reg_char<<hex<<(long long unsigned int)exec_latch.imm;
						reg_char<<hex<<exec_latch.imm;
						inst_str += reg_char.str();
						reg_char.str(std::string());
						}
						index = ((lanes[id].regs[exec_latch.reg2] + exec_latch.imm) & (0xFFFFFFFFFFFFFFFF>>(64-INSTR_SIZE)));
						mem_access_addr[id] = index;
						//mem_stream_out<<"w 0x"<<hex<<index<<" "<<(int)BYTE_ADDR<<endl;
						if( index == 0x80000000 ){
							ptr_dev_out_file<<(char)(lanes[id].regs[exec_latch.reg1]);
							mem_access_addr[id] = -1;
						}
							if(index <= (global_mem_size)){
							//if(index <= (global_mem_size-4)){
								memcpy(&(main_mem[((lanes[id].regs[exec_latch.reg2] + exec_latch.imm) & (0xFFFFFFFFFFFFFFFF>>(64-INSTR_SIZE)))]) , &(lanes[id].regs[exec_latch.reg1]) , BYTE_ADDR*sizeof(uint8_t) );
							}
							else{
								index -= 0x8001000;
								overshoot_out_file.seekp(index, ios::beg);
								overshoot_out_file.write((char*)&(lanes[id].regs[exec_latch.reg1]), BYTE_ADDR*sizeof(uint8_t));
								//overshoot_out_file<<"\n";
							}
						
						break;
						
			case i_LDI:if(!(asm_process)){		
							inst_str += "ldi %r";
						reg_char<<(int)exec_latch.reg1;
						inst_str += reg_char.str();
						reg_char.str(std::string());
						inst_str += ", #0x";
						reg_char<<hex<<exec_latch.imm;
						inst_str += reg_char.str();
						reg_char.str(std::string());
						}
						
						lanes[id].regs[exec_latch.reg1] = exec_latch.imm;						
						break;
						
			case i_RTOP:if(!(asm_process)){		
							inst_str += "rtop @p";
						reg_char<<(int)exec_latch.reg1;
						inst_str += reg_char.str();
						reg_char.str(std::string());
						inst_str += " %r";
						reg_char<<(int)exec_latch.reg2;
						inst_str += reg_char.str();
						reg_char.str(std::string());
						}
						
						if(lanes[id].regs[exec_latch.reg2] != 0){
							lanes[id].pred_regs[exec_latch.reg1].bit = 1;
						}
						else{
							lanes[id].pred_regs[exec_latch.reg1].bit = 0;
						}
						
						break;
						
			case i_ANDP:if(!(asm_process)){		
							inst_str += "andp @p";
						reg_char<<(int)exec_latch.reg1;
						inst_str += reg_char.str();
						reg_char.str(std::string());
						inst_str += " @p";
						reg_char<<(int)exec_latch.reg2;
						inst_str += reg_char.str();
						reg_char.str(std::string());
						inst_str += " @p";
						reg_char<<(int)exec_latch.reg3;
						inst_str += reg_char.str();
						reg_char.str(std::string());
						}
						
						lanes[id].pred_regs[exec_latch.reg1].bit = lanes[id].pred_regs[exec_latch.reg2].bit && lanes[id].pred_regs[exec_latch.reg3].bit;
						break;
						
			case i_ORP:	if(!(asm_process)){		
							inst_str += "orp @p";
						reg_char<<(int)exec_latch.reg1;
						inst_str += reg_char.str();
						reg_char.str(std::string());
						inst_str += " @p";
						reg_char<<(int)exec_latch.reg2;
						inst_str += reg_char.str();
						reg_char.str(std::string());
						inst_str += " @p";
						reg_char<<(int)exec_latch.reg3;
						inst_str += reg_char.str();
						reg_char.str(std::string());
						
						}
						
						lanes[id].pred_regs[exec_latch.reg1].bit = lanes[id].pred_regs[exec_latch.reg2].bit || lanes[id].pred_regs[exec_latch.reg3].bit;
						break;
						
			case i_XORP:if(!(asm_process)){		
							inst_str += "xor @p";
						reg_char<<(int)exec_latch.reg1;
						inst_str += reg_char.str();
						reg_char.str(std::string());
						inst_str += " @p";
						reg_char<<(int)exec_latch.reg2;
						inst_str += reg_char.str();
						reg_char.str(std::string());
						inst_str += " @p";
						reg_char<<(int)exec_latch.reg3;
						inst_str += reg_char.str();
						reg_char.str(std::string());
						}
						lanes[id].pred_regs[exec_latch.reg1].bit = (lanes[id].pred_regs[exec_latch.reg2].bit) ? (!(lanes[id].pred_regs[exec_latch.reg3].bit)) : (lanes[id].pred_regs[exec_latch.reg3].bit);
						break;
						
			case i_NOTP:if(!(asm_process)){		
							inst_str += "notp @p";
							reg_char<<(int)exec_latch.reg1;
							inst_str += reg_char.str();
							reg_char.str(std::string());
							inst_str += " @p";
							reg_char<<(int)exec_latch.reg2;
							inst_str += reg_char.str();
							reg_char.str(std::string());
						}
						lanes[id].pred_regs[exec_latch.reg1].bit = !(lanes[id].pred_regs[exec_latch.reg2].bit);
						break;
						
			case i_ISNEG:if(!(asm_process)){		
							inst_str += "isneg @p";
							reg_char<<(int)exec_latch.reg1;
							inst_str += reg_char.str();
							reg_char.str(std::string());
							inst_str += " %r";
							reg_char<<(int)exec_latch.reg2;
							inst_str += reg_char.str();
							reg_char.str(std::string());
						}
						if( ((lanes[id].regs[exec_latch.reg2] & (0xFFFFFFFFFFFFFFFF>>(64-INSTR_SIZE)))>>(INSTR_SIZE-1)) > 0) {
							lanes[id].pred_regs[exec_latch.reg1].bit = 1;
						}
						else{
							lanes[id].pred_regs[exec_latch.reg1].bit = 0;
						}
						break;
						
			case i_ISZERO:	if(!(asm_process)){		
								inst_str += "iszero @p";
								reg_char<<(int)exec_latch.reg1;
								inst_str += reg_char.str();
								reg_char.str(std::string());
								inst_str += " %r";
								reg_char<<(int)exec_latch.reg2;
								inst_str += reg_char.str();
								reg_char.str(std::string());
							}	
							if( lanes[id].regs[exec_latch.reg2] == 0) {
								lanes[id].pred_regs[exec_latch.reg1].bit = 1;
							}
							else{
								lanes[id].pred_regs[exec_latch.reg1].bit = 0;
							}
							break;
						
			case i_HALT:halt_exec[exec_latch.fetch_req.warp_id] = 2;
						if(!(flag_pc_alter)){		
							cout<<"\n\n Emulation complete. All instructions have been processed. Dumping out device output to a file.";
							cout<<"\n Instructions processed: "<<inst_str<<"\t0x"<<hex<<instr_cnt<<"\n";
							status_execute = 2;	
							flag_pc_alter = 1;
						}		
						#ifdef debug_trace
							if(debug_trace >= 1){
								if(!(asm_process)){	
									inst_str += "HALT...";
									asm_trace_file<<"\nAddr: 0x"<<hex<<exec_latch.fetch_req.addr<<"\thas Instr: "<<inst_str;
									asm_process = 1;
								}
							}
						#endif
						return asm_process;
						break;
						
			case i_TRAP:
						break;
			case i_JMPRU:
						break;
			case i_SKEP:
						break;
			case i_RETI:
						break;
			case i_TLBRM:
						break;
						
			case i_ITOF:
						break;
						
			case i_FTOI:
						break;
						
			case i_FADD:
						break;
						
			case i_FSUB:
						break;
						
			case i_FMUL:
						break;
						
			case i_FDIV:if(!(asm_process)){		
								
						}
						
						
						break;
						
			case i_FNEG:if(!(asm_process)){		
							
						}
						
						
						break;
						
			case i_WSPAWN:	if(!(asm_process)){			
								inst_str += "wspawn %r";
								reg_char<<(int)exec_latch.reg1;
								inst_str += reg_char.str();
								reg_char.str(std::string());
								inst_str += " %r";
								reg_char<<(int)exec_latch.reg2;
								inst_str += reg_char.str();
								reg_char.str(std::string());
								inst_str += " %r";
								reg_char<<(int)exec_latch.reg3;
								inst_str += reg_char.str();
								reg_char.str(std::string());
							}
								
							//lanes[id].regs[exec_latch.reg1] = lanes[id].regs[exec_latch.reg3]	
							//new_warp_id = exec_latch.fetch_req.warp_id+1;
								
							if(!(flag_pc_alter)){	
								new_warp_id += warps_launched + 1;
								//new_warp_id = new_warp_id % 8;
								warps_launched += 1;
								
								if(warps_launched == 7){
									warps_launched = 0;
								}
								
								status = scheduler1->fetch_req_push(((lanes[id].regs[exec_latch.reg2]) & (0xFFFFFFFFFFFFFFFF>>(64-INSTR_SIZE))),warp_spawn_act_mask,new_warp_id,1);		//New warp spawned always has initially only 1 active lane : "new_active_lanes_cnt = 1" for new warp
								//status = (*(scheduler1-(exec_latch.fetch_req.warp_id)+new_warp_id))->fetch_req_push(((lanes[id].regs[exec_latch.reg2]) & (0xFFFFFFFFFFFFFFFF>>(64-INSTR_SIZE))),warp_spawn_act_mask,new_warp_id,1);		//New warp spawned always has initially only 1 active lane : "new_active_lanes_cnt = 1" for new warp
								exec1[new_warp_id].status_execute = 0;
								exec1[new_warp_id].lanes[0].regs[exec_latch.reg1] = lanes[id].regs[exec_latch.reg3];
								if(status == -1){			// i.e. fetch next instruction request was not fulfilled as the fetch buffer was full etc
									//halt or RE-DO this instruction, as the next fetch instruction's request was not put successfully
									cout<<"\nPushing of a fetch request to the fetch block failed. Perhaps the fetch buffer is full.\n";
								}
								
								flag_pc_alter = 1;
							}							
						break;
			case i_BAR:	if(!(asm_process)){						
							inst_str += "bar %r";
							reg_char<<(int)exec_latch.reg1;
							inst_str += reg_char.str();
							reg_char.str(std::string());
							inst_str += " %r";
							reg_char<<(int)exec_latch.reg2;
							inst_str += reg_char.str();
							reg_char.str(std::string());
						}
						
						if(WARPS != 1){				//Process barrier instructions only if no of warps allowed is greater than 1 i.e. 8
							
						
						if(!(flag_pc_alter)){			
							insert(lanes[id].regs[exec_latch.reg1], exec_latch.fetch_req.warp_id, lanes[id].regs[exec_latch.reg2], exec1);
							//resolve(exec1);
							//status_execute = 1;
							flag_pc_alter = 1;
						}	

						}
						break;						
						
			default: cout<<"\nInvalid instruction encountered during execution.";
		}
		
		//lanes[id].status_execute = 1;		
		
		//if(no barrier){}
		//lanes[id].status_execute = 2;		
	
		//Print Instruction executed at Thread 0 for warp 0 ; make sure the print in file happens only once
		#ifdef debug_trace
			if(debug_trace >= 1){
				if(!(asm_process)){			
					// print ASm output to file only once for all the active threads
					asm_trace_file<<"\nAddr: 0x"<<hex<<exec_latch.fetch_req.addr<<"\thas Instr: "<<inst_str;
					asm_process = 1;
				}
			}
		#endif
		
		#ifdef debug_trace
			if(debug_trace > 3){
				//lanes[id].print_regs(&asm_trace_file,id);
				//lanes[id].print_pred_regs(&asm_trace_file,id);
			}
		#endif
		//---------------------------------------- NOTE --------------------------------------------
		//Put this block inside the switch case, whereever we are changing the PC value, so that any 1 thread (need not be thread 0 always) is the only one that changes the PC.
		//if(!(flag_pc_alter)){			//so this was a (jalis,jalrs,jmprt,clone) PC altering/split/join instructin, soonly 1 active thread needs to operate on these.
			// PC/activity mask/terminate execution of other lanes etc manupilations instructions here.
			
			//but the register copying (or entire Rf copying) etc will be done explicitly outside this block for each thread on its own.
			
		//	flag_pc_alter = 1;
		//}
		//-------------------------------------------------------------------------------------------
		
	  }
	  else{		//active thread, but the predicate bit probits us from executing this thread/instruction.
			
	  }
		
			
	}
	else{			//nothing to do. This is an inactive thread
		#ifdef debug_trace
			if(debug_trace >= 1){
				if(!(asm_process)){
					asm_trace_file<<"\nAddr: 0x"<<hex<<exec_latch.fetch_req.addr<<" not executed. Instruction Skipped.";
					asm_process = 1;
				}
			}
		#endif
		
	}
  }
  
	uint64_t swap,swap_nib_4,swap_nib_1;
	uint8_t byte_fetch;
	#ifdef mem_remap
	if(mem_remap){						//Actively do memory-remapping & then print memory trace
		//Interchange the lowermost and the 4th nibble
		if((exec_latch.op_code == i_LD) || (exec_latch.op_code == i_ST)){	
			for(id=0;id<(exec_latch.fetch_req.active_lanes_cnt);++id){		
				if(mem_access_addr[id] != -1){
					swap = mem_access_addr[id];
					swap_nib_4 = swap & 0x000000000000f000;
					swap_nib_1 = swap & 0x000000000000000f;
					swap_nib_4 = swap_nib_4>>12;
					swap_nib_1 = swap_nib_1<<12;
					swap = mem_access_addr[id] & 0xffffffffffff0ff0;
					swap = swap | swap_nib_1;
					swap = swap | swap_nib_4;
					mem_access_addr[id] = swap;
				}				
			}
		
		}

	}
	#ifndef coal_mem
		#define coal_mem 0
	#endif
	
	#endif
	
	#ifdef coal_mem
	if(coal_mem){
	
	if((exec_latch.op_code == i_LD) || (exec_latch.op_code == i_ST)){	
		for(id=0;id<(exec_latch.fetch_req.active_lanes_cnt-1);++id){			//Bubble sort to get addresses in ascending order first for an easier coalescing
			for(i=0;i<(exec_latch.fetch_req.active_lanes_cnt-id-1);++i){
				if (mem_access_addr[i] > mem_access_addr[i+1]) /* For decreasing order use < */
				{
					swap = mem_access_addr[i];
					mem_access_addr[i] = mem_access_addr[i+1];
					mem_access_addr[i+1] = swap;
				}
			}
		}
		for(id=0;id<exec_latch.fetch_req.active_lanes_cnt;++id){				//Memory Coalescing
			if(mem_access_addr[id] != -1){
				access_start = mem_access_addr[id];
				byte_fetch = BYTE_ADDR;
				for(i=id+1;i<exec_latch.fetch_req.active_lanes_cnt;++i){
					if(mem_access_addr[i] != -1){
						if((mem_access_addr[i] <= (access_start+CACHE_LINE-BYTE_ADDR)) && (mem_access_addr[i] > access_start)){
							mem_access_addr[i] = -1;
							byte_fetch += BYTE_ADDR;
						}
						else if(mem_access_addr[i] == access_start){
							mem_access_addr[i] = -1;
						}
					}

				}
				if(exec_latch.op_code == i_LD){
					mem_stream_out<<"r 0x"<<hex<<access_start<<" "<<(int)byte_fetch<<endl;
				}
				else if(exec_latch.op_code == i_ST){
					mem_stream_out<<"w 0x"<<hex<<access_start<<" "<<(int)byte_fetch<<endl;
				}
			}
		}
	}
	}
	else if(coal_mem == 0){
		
		for(id=0;id<exec_latch.fetch_req.active_lanes_cnt;++id){
			if(mem_access_addr[id] != -1){
				if(exec_latch.op_code == i_LD){
					mem_stream_out<<"r 0x"<<hex<<mem_access_addr[id]<<" "<<(int)BYTE_ADDR<<endl;
				}
				else if(exec_latch.op_code == i_ST){
					mem_stream_out<<"w 0x"<<hex<<mem_access_addr[id]<<" "<<(int)BYTE_ADDR<<endl;
				}
			}
		}
		
	}		
	#endif
  
	delete[] mem_access_addr;
	mem_access_addr = NULL;
  
  }
    #ifdef debug_trace
		if(debug_trace >= 1){
			if(!(asm_process)){
				asm_trace_file<<"\nAddr: 0x"<<hex<<exec_latch.fetch_req.addr<<" not executed. Instruction Skipped.\n";
			}
		}
	#endif
	new_warp_id = exec_latch.fetch_req.warp_id;
	init(scheduler1);		
	
	return asm_process;
}
