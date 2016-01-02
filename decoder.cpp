///////////////////////////////////////////////////////
// For ECE 8823 : GPU Architecture
// by Kartikay Garg (GT iD: 903129139)
// Assignment 4 : GPU architecture : Warp Divergence & Multi lane parallel execution model
// Sub on: 3rd November 11:55 pm
//
//////////////////////////////////////////////////

/*
 * decoder.cpp
 *
 *  Created on: Oct 20, 2015
 *      Author: Kartikay
 */

#include <iostream>
#include <math.h>
#include <string.h>
#include "decoder.h"

using namespace std;

extern char ENCODE;
extern uint8_t GPR_LANE;
extern uint8_t PRED_REG_LANE;
extern uint8_t LANES;
extern uint8_t WARPS;
extern uint8_t BYTE_ADDR;
extern uint8_t* main_mem;
extern char* halt_exec;

#define INSTR_SIZE 8*BYTE_ADDR

int pred_size = log(PRED_REG_LANE) + 1;
int reg_size = log(GPR_LANE) + 1;

latch_dec::latch_dec(){
	init();
}

void latch_dec::init(){
	pred=0;
	pred_reg = -1;
	reg1=-1;
	reg2=-1;
	reg3=-1;
	imm=0;
	instr_type = NUM_INSTR_TYPES;
	//fetch_req.instr_req();
}

void latch_dec::print_details(){
	char ptr;
	cout<<endl;
	cout<<"Addr: "<<fetch_req.addr;
	
	cout<<"\tWarp Id: "<<hex<<fetch_req.warp_id;
	//printf("\tWarp Id: %d",fetch_req.warp_id);
	//cout<<"\tNo. of Active lanes: "<<hex<<fetch_req.active_lanes_cnt;
	cout<<"\tAct Mask: ";
	//puts(fetch_req.act_mask);
	
	for(ptr=0;ptr<LANES;++ptr){
		printf(" %d",fetch_req.act_mask[ptr]);			//check if prints the character for 0/1 ASCII value or 0/1 only
	}
	//fetch_req.act_mask[LANES] = '\0';
	
	printf("\tPred bit: %d",pred&0x1);
	printf("\tPred_reg: %d",pred_reg);
	printf("\tOp_Code: %x",op_code);
	printf("\tReg1: %d",reg1);
	printf("\tReg2: %d",reg2);
	printf("\tReg3: %d",reg3);
	cout<<"\tImm: "<<hex<<imm;
}

void decode_stg::parse(fetch_stg* fetcher){				// so if build multiple instances of fetcher and decode stage, just pass the respective fetchers to the respective decoders
	if((op_decode.fetch_req.addr != fetcher->op_fetch.fetch_req.addr)){
		this->op_decode.init();
	}
	uint8_t imm_sign_msb;
	uint64_t imm_sign_mask;
	//op_decode.instr = fetcher->op_fetch.instr;
	//op_decode.fetch_req.addr = fetcher->op_fetch.fetch_req.addr;
	
	if(ENCODE){		//word encoding
		
		//if pc = (pc+4) - 4 instruction is encountered, so replace opcode with that of HALT
		//Prevents multiple different warps with same instruction address to be replaced with HALT opcode by matching the warp_id as well
		
		if((op_decode.fetch_req.addr == fetcher->op_fetch.fetch_req.addr) && (halt_exec[fetcher->op_fetch.fetch_req.warp_id]) && (op_decode.fetch_req.warp_id == fetcher->op_fetch.fetch_req.warp_id) && (strcmp(op_decode.fetch_req.act_mask,fetcher->op_fetch.fetch_req.act_mask) == 0) ){		
		//if((op_decode.fetch_req.addr == fetcher->op_fetch.fetch_req.addr) && (status_execute) && (op_decode.fetch_req.warp_id == fetcher->op_fetch.fetch_req.warp_id) && (strcmp(op_decode.fetch_req.act_mask,fetcher->op_fetch.fetch_req.act_mask) == 0) ){		
			op_decode.fetch_req = fetcher->op_fetch.fetch_req;
			op_decode.op_code = 0x2d;
			op_decode.instr_type = i_NONE;
			return;
		}
		
		op_decode.fetch_req = fetcher->op_fetch.fetch_req;
		op_decode.pred = fetcher->op_fetch.instr >>(INSTR_SIZE-1);
		op_decode.pred_reg = (fetcher->op_fetch.instr<<1)>>(INSTR_SIZE - pred_size);
		op_decode.op_code = (fetcher->op_fetch.instr<<(1+pred_size))>>(INSTR_SIZE-6);
		
		switch(op_decode.op_code){
			case 0x00:
			case 0x01:
			case 0x02:
			case 0x04:
			case 0x2d:
			case 0x2e:
			case 0x31:
			case 0x3b:
			case 0x3c:	
						op_decode.instr_type = i_NONE;
						break;
			case 0x03:  
						op_decode.instr_type = i_3REGSRC;
						op_decode.reg1 = (fetcher->op_fetch.instr<<(1+pred_size+6))>>(INSTR_SIZE-reg_size);
						op_decode.reg2 = (fetcher->op_fetch.instr<<(1+pred_size+6+reg_size))>>(INSTR_SIZE-reg_size);
						op_decode.reg3 = (fetcher->op_fetch.instr<<(1+pred_size+6+reg_size+reg_size))>>(INSTR_SIZE-reg_size);
						break;
			case 0x05:
			case 0x06:
			case 0x1c:
			case 0x33:
			case 0x34:
			case 0x39:
			case 0x3d:	
						op_decode.reg1 = (fetcher->op_fetch.instr<<(1+pred_size+0x6))>>(INSTR_SIZE-reg_size);
						op_decode.reg2 = (fetcher->op_fetch.instr<<(1+pred_size+0x6+reg_size))>>(INSTR_SIZE-reg_size);
						op_decode.instr_type = i_2REG;
						break;
			case 0x07:
			case 0x08:
			case 0x09:
			case 0x0a:
			case 0x0b:
			case 0x0c:	
			case 0x0d:
			case 0x0e:
			case 0x0f:
			case 0x10:
			case 0x21:
			case 0x35:
			case 0x36:
			case 0x37:
			case 0x38:
			case 0x3a:
						op_decode.reg1 = (fetcher->op_fetch.instr<<(1+pred_size+6))>>(INSTR_SIZE-reg_size);
						op_decode.reg2 = (fetcher->op_fetch.instr<<(1+pred_size+6+reg_size))>>(INSTR_SIZE-reg_size);
						op_decode.reg3 = (fetcher->op_fetch.instr<<(1+pred_size+6+reg_size+reg_size))>>(INSTR_SIZE-reg_size);
						op_decode.instr_type = i_3REG;
						break;
			case 0x11:
			case 0x12:
			case 0x13:
			case 0x14:
			case 0x15:
			case 0x16:	
			case 0x17:
			case 0x18:
			case 0x19:
			case 0x1a:
			case 0x20:
			case 0x23:
						op_decode.reg1 = (fetcher->op_fetch.instr<<(1+pred_size+6))>>(INSTR_SIZE-reg_size);
						op_decode.reg2 = (fetcher->op_fetch.instr<<(1+pred_size+6+reg_size))>>(INSTR_SIZE-reg_size);
						imm_sign_msb = (fetcher->op_fetch.instr<<(1+pred_size+6+reg_size+reg_size))>>31;
						if(imm_sign_msb){
							imm_sign_mask = (0xFFFFFFFFFFFFFFFF<<(INSTR_SIZE-(1+pred_size+6+reg_size+reg_size)));
							op_decode.imm = imm_sign_mask | fetcher->op_fetch.instr;
							op_decode.imm = (op_decode.imm & (0xFFFFFFFFFFFFFFFF>>(64-INSTR_SIZE)));
						}
						else{
							op_decode.imm = (fetcher->op_fetch.instr<<(1+pred_size+6+reg_size+reg_size))>>(1+pred_size+6+reg_size+reg_size);
						}
						op_decode.instr_type = i_3IMM;
						break;
			case 0x1b:
			case 0x25:  
						//temp = (fetcher->op_fetch.instr<<(1+pred_size+6));
						//temp2 = (fetcher->op_fetch.instr<<(1+pred_size+6))>>(INSTR_SIZE-reg_size);
						op_decode.reg1 = (fetcher->op_fetch.instr<<(1+pred_size+6))>>(INSTR_SIZE-reg_size);
						imm_sign_msb = (fetcher->op_fetch.instr<<(1+pred_size+6+reg_size))>>31;
						if(imm_sign_msb){
							imm_sign_mask = (0xFFFFFFFFFFFFFFFF<<(INSTR_SIZE-(1+pred_size+6+reg_size)));
							op_decode.imm = imm_sign_mask | fetcher->op_fetch.instr;
							op_decode.imm = (op_decode.imm & (0xFFFFFFFFFFFFFFFF>>(64-INSTR_SIZE)));
						}
						else{
							op_decode.imm = (fetcher->op_fetch.instr<<(1+pred_size+6+reg_size))>>(1+pred_size+6+reg_size);
						}
						op_decode.instr_type = i_2IMM;
						break;
			case 0x1d:	//Sign extend
						//find the MSB of the immediate no
						//check if it is 1 or 0
						//Then according to the size of the immediate field bits/no of bits that were ignored or removed uptill now and then shift 0xFFFFFFFF / 0xFFFFFFFFFFFFFFFF left by those many bytes & OR with the masked data
						imm_sign_msb = (fetcher->op_fetch.instr<<(1+pred_size+6))>>31;
						if(imm_sign_msb){
							imm_sign_mask = (0xFFFFFFFFFFFFFFFF<<(INSTR_SIZE-(1+pred_size+6)));
							op_decode.imm = imm_sign_mask | fetcher->op_fetch.instr;
							op_decode.imm = (op_decode.imm & (0xFFFFFFFFFFFFFFFF>>(64-INSTR_SIZE)));
						}
						else{
							op_decode.imm = (fetcher->op_fetch.instr<<(1+pred_size+6))>>(1+pred_size+6);
						}
						
						op_decode.instr_type = i_1IMM;
						break;
			case 0x1e:
			case 0x1f:
			case 0x22:
			case 0x2f:
			case 0x30:
			case 0x32:	
						op_decode.reg1 = (fetcher->op_fetch.instr<<(1+pred_size+6))>>(INSTR_SIZE-reg_size);
						op_decode.instr_type = i_1REG;
						break;
			case 0x24:
						op_decode.reg1 = (fetcher->op_fetch.instr<<(1+pred_size+6))>>(INSTR_SIZE-reg_size);
						op_decode.reg2 = (fetcher->op_fetch.instr<<(1+pred_size+6+reg_size))>>(INSTR_SIZE-reg_size);
						imm_sign_msb = (fetcher->op_fetch.instr<<(1+pred_size+6+reg_size+reg_size))>>31;
						if(imm_sign_msb){
							imm_sign_mask = (0xFFFFFFFFFFFFFFFF<<(INSTR_SIZE-(1+pred_size+6+reg_size+reg_size)));
							op_decode.imm = imm_sign_mask | fetcher->op_fetch.instr;
							op_decode.imm = (op_decode.imm & (0xFFFFFFFFFFFFFFFF>>(64-INSTR_SIZE)));
						}
						else{
							op_decode.imm = (fetcher->op_fetch.instr<<(1+pred_size+6+reg_size+reg_size))>>(1+pred_size+6+reg_size+reg_size);
						}
						op_decode.instr_type = i_3IMMSRC;
						break;
			case 0x26:
			case 0x2b:
			case 0x2c:
						op_decode.reg1 = (fetcher->op_fetch.instr<<(1+pred_size+6))>>(INSTR_SIZE-pred_size);
						op_decode.reg2 = (fetcher->op_fetch.instr<<(1+pred_size+6+pred_size))>>(INSTR_SIZE-reg_size);		
						op_decode.instr_type = i_PREG_REG;
						break;
			case 0x27:
			case 0x28:
			case 0x29: 
						op_decode.reg1 = (fetcher->op_fetch.instr<<(1+pred_size+6))>>(INSTR_SIZE-pred_size);		//check: pred_size
						op_decode.reg2 = (fetcher->op_fetch.instr<<(1+pred_size+6+pred_size))>>(INSTR_SIZE-pred_size);
						op_decode.reg3 = (fetcher->op_fetch.instr<<(1+pred_size+6+pred_size+pred_size))>>(INSTR_SIZE-pred_size);
						op_decode.instr_type = i_3PREG;
						break;
			case 0x2a:
						op_decode.reg1 = (fetcher->op_fetch.instr<<(1+pred_size+6))>>(INSTR_SIZE-pred_size);		//check: pred_size
						op_decode.reg2 = (fetcher->op_fetch.instr<<(1+pred_size+6+pred_size))>>(INSTR_SIZE-pred_size);
						op_decode.instr_type = i_2PREG;
						break;
			default:	cout<<"\nInvalid Opcode fetched. fetched opcode vale is: "<<op_decode.op_code<<" at address: "<<op_decode.fetch_req.addr;		//op_decode.addr;
						break;
			
		}
	}
	else{			//BYTE encoding
		
		//do later
	}
	
	
}
