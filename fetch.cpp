///////////////////////////////////////////////////////
// For ECE 8823 : GPU Architecture
// by Kartikay Garg (GT iD: 903129139)
// Assignment 4 : GPU architecture : Warp Divergence & Multi lane parallel execution model
// Sub on: 3rd November 11:55 pm
//
/////////////////////////////////////////////////

#include <inttypes.h>
#include <fstream>
#include <iostream>
#include <stdio.h>
#include <string.h>
#include <cstdlib>
#include "fetch.h"

using namespace std;

extern uint8_t GPR_LANE;
extern uint8_t PRED_REG_LANE;
extern uint8_t LANES;
extern uint8_t WARPS;
extern uint8_t BYTE_ADDR;
extern uint8_t* main_mem;
extern ofstream ptr_out_file;
extern ifstream ptr_inp_file;
extern ofstream asm_trace_file;
extern char* halt_exec;
//char halt_exec[WARPS];

extern void die_message(const char *);

uint8_t counter = 0;

instr_req::instr_req(){
	char ptr;
	addr = -1;
	//act_mask = (char*) malloc (sizeof(char)*(LANES+1));
	act_mask = new char [LANES+1];
	for(ptr=0;ptr<LANES;++ptr){
		act_mask[ptr] = 1;
	}
	act_mask[LANES] = '\0';
	active_lanes_cnt = 1;
	warp_id = 0;
}

latch::latch(){
	instr = 0;
	//addr=0;			//see invalid addrs
	//op_type=0;		//see invalid opcodes
	
	//fetch_req.instr_req();
}

//scheduler::scheduler(uint8_t warps){
scheduler::scheduler(){
	int ptr;
	fetch_start=0;
	fetch_end=-1;
	//MAX_FETCH_BUF = 3*warps;
	MAX_FETCH_BUF = 5*WARPS;
	fetch_req = new instr_req [MAX_FETCH_BUF];				//MAX_FETCH_BUF = 3*WARPS
}

uint8_t scheduler::fetch_req_push(uint64_t next_pc, char* activity_mask, uint8_t warp_id, uint8_t new_active_lanes_cnt){
//uint8_t scheduler::fetch_req_push(uint64_t next_pc, char activity_mask[9], uint8_t warp_id, uint8_t new_active_lanes_cnt){
	int i;
	//Introduce ARBITER here, which will sort out of all incoming next fetch push requests, in case of simultaneous parallel execution of multiple SMs/threads
		
	if(fetch_end == -1){					//Fetch FIFO was empty
		fetch_req[fetch_start].addr = next_pc;
		for(i=0;i<LANES;++i){
			fetch_req[fetch_start].act_mask[i] = activity_mask[i];
		}
		//strcpy(fetch_req[fetch_start].act_mask, activity_mask);
		fetch_req[fetch_start].warp_id = warp_id;
		fetch_req[fetch_start].active_lanes_cnt = new_active_lanes_cnt;
		fetch_end=0;
		return 1;
	}
	else if((fetch_end > fetch_start) && (fetch_end < MAX_FETCH_BUF-1)){		//can push into fetch_req FIFO untill fetch_end < MAX_FETCH_BUF
		++fetch_end;
		fetch_req[fetch_end].addr = next_pc;
		for(i=0;i<LANES;++i){
			fetch_req[fetch_end].act_mask[i] = activity_mask[i];
		}
		//strcpy(fetch_req[fetch_end].act_mask, activity_mask);
		fetch_req[fetch_end].warp_id = warp_id;
		fetch_req[fetch_end].active_lanes_cnt = new_active_lanes_cnt;
		return 1;
	}
	else if((fetch_end == MAX_FETCH_BUF-1) && (fetch_start > 0)){
		fetch_end = 0;
		fetch_req[fetch_end].addr = next_pc;
		for(i=0;i<LANES;++i){
			fetch_req[fetch_end].act_mask[i] = activity_mask[i];
		}
		//strcpy(fetch_req[fetch_end].act_mask, activity_mask);
		fetch_req[fetch_end].warp_id = warp_id;
		fetch_req[fetch_end].active_lanes_cnt = new_active_lanes_cnt;
		return 1;
	}
	else if(fetch_end < fetch_start-1){			
		++fetch_end;
		fetch_req[fetch_end].addr = next_pc;
		//strcpy(fetch_req[fetch_end].act_mask, activity_mask);
		for(i=0;i<LANES;++i){
			fetch_req[fetch_end].act_mask[i] = activity_mask[i];
		}
		fetch_req[fetch_end].warp_id = warp_id;
		fetch_req[fetch_end].active_lanes_cnt = new_active_lanes_cnt;
		return 1;
	}
	else if(fetch_start == fetch_end){
		++fetch_end;
		fetch_req[fetch_end].addr = next_pc;
		for(i=0;i<LANES;++i){
			fetch_req[fetch_end].act_mask[i] = activity_mask[i];
		}		
		//strcpy(fetch_req[fetch_end].act_mask, activity_mask);
		fetch_req[fetch_end].warp_id = warp_id;
		fetch_req[fetch_end].active_lanes_cnt = new_active_lanes_cnt;
		return 1;
	}
	else{
		
		die_message("\nHalting scheduler. Can't load more fetch requests. Halting emulator!!!\n");
		return -1;		//cannot load anymore. Fetch request buffer is full
	}
}

instr_req scheduler::fetch_req_pop(){						//keep poping on request till "ptr < fetch_end"
	//Delivers the next PC to fetch address to teh Fetch stage, on its invocation
	instr_req ret_pc;
	if(fetch_end == -1){
		die_message("\nProgram exceution finished.\nHalting scheduler. Halting emulator!!!\n");
		return ret_pc;
		//return instr_req::instr_req();	//if fetch_buf is empty. time to halt program execution.
		//return -1;	//if fetch_buf is empty. time to halt program execution.
	}
	else{
		//asm_trace_file<<"\nFetch pop from: "<<(int)fetch_start;
		//cout<<"\nFetch pop from: ft_start: "<<(int)fetch_start<<" fetch_end: "<<(int)fetch_end;
		ret_pc.warp_id = fetch_req[fetch_start].warp_id;
		ret_pc.active_lanes_cnt = fetch_req[fetch_start].active_lanes_cnt;
		//ret_pc.act_mask = fetch_req.act_mask;
		
		for(int i=0;i<LANES;++i){
			ret_pc.act_mask[i] = fetch_req[fetch_start].act_mask[i];
		}
		
		ret_pc.addr = fetch_req[fetch_start].addr;
		
	//else if(fetch_start<fetch_end){	
	if(fetch_start<fetch_end){
		//ret_pc = fetch_req[fetch_start];
		++fetch_start;
		//return ret_pc;
	}
	else if(fetch_end<fetch_start){
		//ret_pc = fetch_req[fetch_start];
		if(fetch_start == MAX_FETCH_BUF-1){
			fetch_start = 0;
		}
		else{
			++fetch_start;
		}
		//return ret_pc;
	}
	else if(fetch_start == fetch_end){
		//ret_pc = fetch_req[fetch_start];
		fetch_start = 0;
		fetch_end = -1;
		//return ret_pc;
	}
	//cout<<"\nFetch pop from: ft_start: "<<(int)fetch_start<<" fetch_end: "<<(int)fetch_end;
	return ret_pc;
	}
	/*
	die_message("\nProgram exceution finished.\nHalting scheduler. Halting emulator!!!\n");
	return -1;	//if fetch_buf is empty. time to halt program execution.
	*/
}


fetch_stg::fetch_stg(){
	//fetch_req.instr_req();
	//addr=0;
	instr=0;
	//op_fetch.latch();
}

void fetch_stg::fetch_next(scheduler* scheduler1){
	
	fetch_req = scheduler1->fetch_req_pop();
	//addr = scheduler1->fetch_req_pop();
	if(fetch_req.addr != -1){
		//ptr_inp_file.seekg(addr, ios::beg);
		//ptr_inp_file.read((char*)&instr, BYTE_ADDR*sizeof(uint8_t));
		memcpy(&(instr),&main_mem[fetch_req.addr],BYTE_ADDR*sizeof(uint8_t));
		
		if((fetch_req.addr != op_fetch.fetch_req.addr) || (fetch_req.warp_id != op_fetch.fetch_req.warp_id) || (strcmp(fetch_req.act_mask,op_fetch.fetch_req.act_mask) != 0) ){
		//if(fetch_req.addr != op_fetch.fetch_req.addr){
			//op_fetch.instr = instr;
			//op_fetch.fetch_req.addr = fetch_req.addr;
			
			halt_exec[fetch_req.warp_id] = 0;
			//status_execute = 0;
		}
		else{				//HALT if fetched is same as previous one. i.e. as addr == (pc+0x4) + (-0x4)		&& 	also the warpID is the same. i.e. it is being fetched for the same warp. Then only halt. otherwise let it pass by.
			//stop execution in  main
			
			halt_exec[fetch_req.warp_id] = 1;
			//status_execute = 1;
			//change the next instruction with HALT instruction
			
			//op_fetch.instr = instr;
			//op_fetch.fetch_req.addr = fetch_req.addr;
		}
		op_fetch.fetch_req.warp_id = fetch_req.warp_id;
		op_fetch.fetch_req.active_lanes_cnt = fetch_req.active_lanes_cnt;
		//op_fetch.fetch_req.act_mask = fetch_req.act_mask;
		
		for(int i=0;i<LANES;++i){
			op_fetch.fetch_req.act_mask[i] = fetch_req.act_mask[i];
		}
		
		op_fetch.instr = instr;
		op_fetch.fetch_req.addr = fetch_req.addr;
		//////////////////////// Temp processing on fetched instr /////////////////////////////////
		//ptr_out_file.seekp(fetch_req.addr, ios::beg);
		//ptr_out_file.write((char*)&instr, BYTE_ADDR*sizeof(uint8_t));		
		///////////////////////////////////////////////////////////////////////////////////////////
		
		
	}
	else{
		//Don't do this here. As this will stop the emulatpr when the last instr (Halt) has arrived in the fetch stage in the pipeline, even though earlier instructions have not yet been processed and are still in some stage in the pipeline and are still awaiting GPU's attenstion.
		
		die_message("\nProgram execution finished.\nHalting fetch stage. Halting emulator!!!\n");
	}
	
}
