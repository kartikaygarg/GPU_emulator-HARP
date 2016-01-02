///////////////////////////////////////////////////////
// For ECE 8823 : GPU Architecture
// by Kartikay Garg (GT iD: 903129139)
// Assignment 4 : GPU architecture : Warp Divergence & Multi lane parallel execution model
// Sub on: 3rd November 11:55 pm
//
//////////////////////////////////////////////////

/*
 * decoder.h
 *
 *  Created on: Oct 20, 2015
 *      Author: Kartikay
 */

#include <iostream>
#include <stdio.h>

#include "fetch.h"

#ifndef DECODER_H_
#define DECODER_H_

//Create pointers for each opearnd, and store only in the final register file of the execution lanes of the CPU

using namespace std;


typedef enum instr_type_ENUM {
    i_NONE,
	i_2REG,
	i_2IMM,
	i_3REG,
	i_3PREG,
	i_3IMM,
	i_3REGSRC,
	i_1IMM,
	i_1REG,
	i_3IMMSRC,
	i_PREG_REG,
	i_2PREG,
    NUM_INSTR_TYPES
} Instr_Type; 

typedef enum instr_opcode_ENUM {
    i_NOP,
	i_DI,
	i_EI,
	i_TLBADD,
	i_TLBFLUSH,
	i_NEG,
	i_NOT,
	i_AND,
	i_OR,
	i_XOR,
	i_ADD,
	i_SUB,
	i_MUL,
	i_DIV,
	i_MOD,
	i_SHL,
	i_SHR,
	i_ANDI,
	i_ORI,
	i_XORI,
	i_ADDI,
	i_SUBI,
	i_MULI,
	i_DIVI,
	i_MODI,
	i_SHLI,
	i_SHRI,
	i_JALI,
	i_JALR,
	i_JMPI,
	i_JMPR,
	i_CLONE,
	i_JALIS,
	i_JALRS,
	i_JMPRT,
	i_LD,
	i_ST,
	i_LDI,
	i_RTOP,
	i_ANDP,
	i_ORP,
	i_XORP,
	i_NOTP,
	i_ISNEG,
	i_ISZERO,
	i_HALT,
	i_TRAP,
	i_JMPRU,
	i_SKEP,
	i_RETI,
	i_TLBRM,
	i_ITOF,
	i_FTOI,
	i_FADD,
	i_FSUB,
	i_FMUL,
	i_FDIV,
	i_FNEG,
	i_WSPAWN,
	i_SPLIT,
	i_JOIN,
	i_BAR,	
    NUM_INSTR_OPCODE
} Instr_opcodes; 

class latch_dec {		//: public latch{				// LATER: maybe remove inheritance, no need to keep instr later on. Just keep addr from base class
	public:
	//uint64_t addr;
	instr_req fetch_req;
	int pred : 1;
	uint8_t pred_reg;
	uint8_t op_code;
	uint8_t reg1;
	uint8_t reg2;
	uint8_t reg3;
	uint64_t imm;
	Instr_Type instr_type;
	
	latch_dec();
	void init();
	void print_details();
	
};




class decode_stg{
	
	public:
	latch_dec op_decode;
	void parse(fetch_stg*);
		
};


#endif /* DECODER_H_ */
