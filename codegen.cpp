#define SIZE_8BIT	0
#define SIZE_16BIT	1
#define SIZE_32BIT	2
#define SIZE_64BIT	3

#define SIZE_BYTE	0
#define SIZE_WORD	1
#define SIZE_DWORD	2
#define SIZE_QWORD	3

enum Register : byte {
	REG_AL 		= 0,
	REG_CL 		= 1,
	REG_DL		= 2,
	REG_BL 		= 3,
	REG_AH 		= 4,
	REG_BH 		= 5,
	REG_CH		= 6,
	REG_DH		= 7,
	REG_R8B 	= 8,
	REG_R9B 	= 9,
	REG_R10B 	= 10,
	REG_R11B	= 11,
	REG_R12B 	= 12,
	REG_R13B 	= 13,
	REG_R14B 	= 14,
	REG_R15B	= 15,
	REG_AX 		= 16,
	REG_CX 		= 17,
	REG_DX		= 18,
	REG_BX 		= 19,
	REG_SP 		= 20,
	REG_BP 		= 21,
	REG_SI		= 22,
	REG_DI		= 23,
	REG_R8W 	= 24,
	REG_R9W 	= 25,
	REG_R10W	= 26,
	REG_R11W 	= 27,
	REG_R12W 	= 28,
	REG_R13W	= 29,
	REG_R14W	= 30,
	REG_R15W	= 31,
	REG_EAX 	= 32,
	REG_ECX 	= 33,
	REG_EDX 	= 34,
	REG_EBX 	= 35,
	REG_EBP 	= 36,
	REG_ESP 	= 37,
	REG_ESI 	= 38,
	REG_EDI 	= 39,
	REG_R8D 	= 40,
	REG_R9D 	= 41,
	REG_R10D 	= 42,
	REG_R11D	= 43,
	REG_R12D 	= 44,
	REG_R13D 	= 45,
	REG_R14D 	= 46,
	REG_R15D	= 47,
	REG_RAX 	= 48,
	REG_RCX 	= 49,
	REG_RDX		= 50,
	REG_RBX 	= 51,
	REG_RSP 	= 52,
	REG_RBP 	= 53,
	REG_RSI 	= 54,
	REG_RDI 	= 55,
	REG_R8		= 56,
	REG_R9		= 57,
	REG_R10 	= 58,
	REG_R11 	= 59,
	REG_R12 	= 60,
	REG_R13 	= 61,
	REG_R14 	= 62,
	REG_R15 	= 63,

	REG_count
};

#define X1 			0
#define X2 			1
#define X4 			2
#define X8 			3


enum MemoryMode : int32 {
	Mem_RelativeToReg = 0,
	Mem_RelativeToCurrent,
	Mem_AbsoluteAddressing,
	Mem_IndexedAddressing,
	Mem_IndexedAddressingNoBase
};

struct Memory {
	MemoryMode mode;
	int32 disp;

	byte regSize; // is32Bit
	byte base;  // Register without size
	byte index; // Register without size, not 4
	byte scale;
};

struct Immediate {
	bool sx;
	union {
		int64 s;
		uint64 u;
	};
};

#define RET 	0xC3
#define LEAVE 	0xC9
#define NOP 	0x90

void Emit(byte** ptr, byte b) {
	*(*ptr)++ = b;
}

void Emit16(byte** ptr, uint16 i) {
	*(uint16*)*ptr = i;
	*ptr += 2;
}

void Emit32(byte** ptr, uint32 i) {
	*(uint32*)*ptr = i;
	*ptr += 4;
}

void Emit64(byte** ptr, uint64 i) {
	*(uint64*)*ptr = i;
	*ptr += 8;
}

void EmitString(byte** ptr, String string) {
	*ptr += StringCopy(string, *ptr);
}

void EmitRexPrefix(byte** ptr, byte w, byte reg, byte inx, byte b) {
	ASSERT(w == 0 || w == 1);
	ASSERT(b == 0 || b == 1);

	byte r = (reg & 8) >> 3;
	byte x = (inx & 8) >> 3;
	Emit(ptr, 0x40 | (w << 3) | (r << 2) | (x << 1) | (b << 0));
}

void EmitRexPrefix(byte** ptr, byte w, byte reg) {
	ASSERT(w == 0 || w == 1);

	byte b = (reg & 8) >> 3;
	Emit(ptr, 0x40 | (w << 3) | b);
}

void EmitModRegRM(byte** ptr, byte mod, byte reg, byte r_m) {
	ASSERT(mod < 4);
	ASSERT(reg < 16);
	ASSERT(r_m < 16);

	Emit(ptr, (mod << 6) | ((reg & 7) << 3) | (r_m & 7) );
}

void EmitModRegRM(byte** ptr, byte reg, Memory mem) {
	ASSERT(mem.index != 4);

	byte mod, r_m;
	if (mem.mode == Mem_RelativeToCurrent) {
		r_m = 5;
		mod = 0;
	}
	else if (mem.mode == Mem_AbsoluteAddressing 
		|| mem.mode == Mem_IndexedAddressingNoBase) {
		r_m = 4;
		mod = 0;
	}
	else {
		r_m = mem.mode == Mem_IndexedAddressing ? 4 : mem.base;
		if (mem.disp == 0) {
			mod = (mem.base == 5 || mem.base == 13) ? 1 : 0;
		}
		else if (mem.disp <= 0xff) mod = 1;
		else mod = 2;
	}
	EmitModRegRM(ptr, mod, reg, r_m);
}

void EmitSIB(byte** ptr, Memory mem) {
	ASSERT(mem.index != 4);

	if (mem.mode == Mem_IndexedAddressing) {
		Emit(ptr, (mem.scale << 6) | ((mem.index & 7) << 3) | (mem.base & 7) );
	}
	else if (mem.mode == Mem_IndexedAddressingNoBase) {
		Emit(ptr, (mem.scale << 6) | ((mem.index & 7) << 3) | 5 );
	}
	else if (mem.mode == Mem_AbsoluteAddressing) {
		Emit(ptr, 0x25);
	}
	else if (mem.base == 4 || mem.base == 12) {
		Emit(ptr, 0x24);
	}
}

void EmitDisplacement(byte** ptr, Memory mem) {
	if (mem.mode == Mem_RelativeToCurrent
		|| mem.mode == Mem_AbsoluteAddressing
		|| mem.mode == Mem_IndexedAddressingNoBase
		|| mem.disp > 0xFF) {

		Emit32(ptr, (uint32)mem.disp);
	}
	else if (mem.disp > 0 || mem.base == 5 || mem.base == 13) {
		Emit(ptr, (byte)mem.disp);
	}
}


byte GetSignedSize(int64 imm) {
	if (-128 <= imm && imm <= 127) return SIZE_8BIT;
	if ((int64)MIN_INT16 <= imm && imm <= (int64)MAX_INT16) return SIZE_16BIT;
	if ((int64)MIN_INT32 <= imm && imm <= (int64)MAX_INT32) return SIZE_32BIT;
	return SIZE_64BIT;
}

byte GetSignedSize(uint64 imm) {
	if (imm <= 127) return SIZE_8BIT;
	if (imm <= MAX_INT16) return SIZE_16BIT;
	if (imm <= MAX_INT32) return SIZE_32BIT;
	return SIZE_64BIT;
}

byte GetSignedSize(Immediate imm) {
	return (imm.sx) 
		? GetSignedSize(imm.s)
		: GetSignedSize(imm.u);
}

byte GetUnsignedSize(uint64 imm) {
	if (imm <= 0xFF) return SIZE_8BIT;
	if (imm <= MAX_UINT16) return SIZE_16BIT;
	if (imm <= MAX_UINT32) return SIZE_32BIT;
	return SIZE_64BIT;
}

// MOV
//---------------------

void EmitMov(byte** ptr, Register _register, uint64 imm) {
	bool immIs64 = MAX_UINT32 < imm;

	byte reg_size = (_register & 0xF0) >> 4;
	byte reg = _register & 0x0F;

	if (reg_size == SIZE_16BIT) {
		Emit(ptr, 0x66);
	}

	if (immIs64 || reg > 7) {
		byte q = immIs64 ? 1 : 0;
		EmitRexPrefix(ptr, q, reg);
	}
	
	// Emit mov OpCode
	byte w = reg_size > SIZE_8BIT ? 0x08 : 0;
	Emit(ptr, 0xB0 | w | (reg & 7));

	// Emit Immediate
	if (immIs64) {
		Emit64(ptr, imm);
	}
	else if (reg_size >= SIZE_32BIT) {
		Emit32(ptr, (uint32)imm);
	}
	else if (reg_size == SIZE_16BIT) {
		Emit16(ptr, (uint16)imm);
	}
	else {
		Emit(ptr, (byte)imm);
	}
}

void EmitMov(byte** ptr, Register _register, int64 imm) {
	bool immIs64 = MAX_INT32 < imm || imm < MAX_INT32;

	if (imm >= 0) {
		EmitMov(ptr, _register, (uint64)imm);
		return;
	}

	byte reg_size = (_register & 0xF0) >> 4;
	byte reg = _register & 0x0F;

	if (reg_size == SIZE_16BIT) {
		Emit(ptr, 0x66);
	}

	if (reg > 7 || reg_size == SIZE_64BIT) {
		byte w = reg_size > SIZE_8BIT ? 1 : 0;
		EmitRexPrefix(ptr, w, reg);
	}

	if (immIs64) {
		Emit(ptr, 0xB8 | (reg & 7));
		Emit64(ptr, (uint64)imm);
	}
	else if (reg_size == SIZE_64BIT) {
		Emit(ptr, 0xC7);
		Emit(ptr, 0xC0 | (reg & 7));
		Emit32(ptr, (uint32)(int32)imm);
	}
	else if (reg_size == SIZE_32BIT) {
		Emit(ptr, 0xB8 | (reg & 7));
		Emit32(ptr, (uint32)(int32)imm);
	}
	else if (reg_size == SIZE_16BIT) {
		Emit(ptr, 0xB8 | (reg & 7));
		Emit16(ptr, (uint16)(int16)imm);
	}
	else {
		Emit(ptr, 0xB0 | (reg & 7));
		Emit(ptr, (byte)imm);
	}
}

void EmitMov(byte** ptr, Register _register, Immediate imm) {
	if (imm.sx)
		EmitMov(ptr, _register, imm.s);
	else
		EmitMov(ptr, _register, imm.u);
}

//--------------------

void Emit(byte** ptr, byte op1, byte op2, Register _register) {
	byte size = (_register & 0xF0) >> 4;
	byte reg = _register & 0x0F;

	if (size == SIZE_16BIT) {
		Emit(ptr, 0x66);
	}

	if (reg > 7 || size == SIZE_64BIT) {
		byte w = size == SIZE_64BIT || reg <= 7 ? 1 : 0;
		EmitRexPrefix(ptr, w, reg);
	}

	if (size == SIZE_8BIT)
		Emit(ptr, op1);
	else
		Emit(ptr, op1 + 1);
	EmitModRegRM(ptr, 3, op2, reg);
}

void Emit(byte** ptr, byte op1, byte op2, byte size, Memory mem) {
	if (size == SIZE_16BIT) {
		Emit(ptr, 0x66);
	}

	if (mem.regSize == SIZE_32BIT) {
		Emit(ptr, 0x67);
	}

	if (size == SIZE_64BIT || mem.base > 7 || mem.index > 7) {
		byte w = size == SIZE_64BIT ? 1 : 0;
		byte b = (mem.base & 8) >> 3;
		EmitRexPrefix(ptr, w, 0, mem.index, b);
	}

	if (size == SIZE_8BIT)
		Emit(ptr, op1);
	else
		Emit(ptr, op1 + 1);

	EmitModRegRM(ptr, op2, mem);
	EmitSIB(ptr, mem);
	EmitDisplacement(ptr, mem);
}

// used by test
void Emit(byte** ptr, byte op, Register _register, Immediate imm) {
	byte size = (_register & 0xF0) >> 4;
	byte reg = _register & 0x0F;

	if (size == SIZE_16BIT) {
		Emit(ptr, 0x66);
	}

	if (reg > 7 || size == SIZE_64BIT) {
		byte w = size == SIZE_64BIT ? 1 : 0;
		EmitRexPrefix(ptr, w, reg);
	}

	if (size == SIZE_8BIT)
		Emit(ptr, op);
	else
		Emit(ptr, op + 1);
	EmitModRegRM(ptr, 3, 0, reg);
	
	if (size == SIZE_8BIT)
		Emit(ptr, (byte)imm.u);
	else if (size == SIZE_16BIT)
		Emit16(ptr, (uint16)imm.u);
	else 
		Emit32(ptr, (uint32)imm.u);
}

void Emit(byte** ptr, byte op, Register dst, Register src) {
	byte size = dst >> 4;
	byte reg1 = dst & 0x0F;
	byte reg2 = src & 0x0F;

	if (size == SIZE_16BIT) {
		Emit(ptr, 0x66);
	}

	if (size == SIZE_64BIT || reg1 > 7 || reg2 > 7) {
		byte w = size == SIZE_64BIT ? 1 : 0;
		byte b = (reg1 & 8) >> 3;
		EmitRexPrefix(ptr, w, reg2, 0, b);
	}

	if (size == SIZE_8BIT)
		Emit(ptr, op);
	else
		Emit(ptr, op+1);
	EmitModRegRM(ptr, 3, reg2, reg1);
}

// TODO: at least 32bit? used for imul 
void Emit16(byte** ptr, uint16 op, Register dst, Register src) {
	byte size = dst >> 4;
	byte reg1 = dst & 0x0F;
	byte reg2 = src & 0x0F;

	if (size == SIZE_16BIT) {
		Emit(ptr, 0x66);
	}

	if (size == SIZE_64BIT || reg1 > 7 || reg2 > 7) {
		byte w = size == SIZE_64BIT ? 1 : 0;
		byte b = (reg1 & 8) >> 3;
		EmitRexPrefix(ptr, w, reg2, 0, b);
	}

	if (size == SIZE_8BIT)
		Emit16(ptr, op);
	else
		Emit16(ptr, op+1);
	EmitModRegRM(ptr, 3, reg2, reg1);
}

void Emit(byte** ptr, byte op, Register _register, Memory mem) {
	byte size = (_register & 0xF0) >> 4;
	byte reg = _register & 0x0F;

	if (mem.regSize == SIZE_32BIT) {
		Emit(ptr, 0x67);
	}

	if (size == SIZE_16BIT) {
		Emit(ptr, 0x66);
	}

	if (size == SIZE_64BIT || reg > 7 || mem.base > 7 || mem.index > 7) {
		byte w = size == SIZE_64BIT ? 1 : 0;
		byte b = (mem.base & 8) >> 3;
		EmitRexPrefix(ptr, w, reg, mem.index, b);
	}

	if (size == SIZE_8BIT)
		Emit(ptr, op);
	else
		Emit(ptr, op + 1);

	EmitModRegRM(ptr, reg, mem);
	EmitSIB(ptr, mem);
	EmitDisplacement(ptr, mem);
}

void Emit(byte** ptr, byte op, byte size, Memory mem, Immediate imm) {
	if (mem.regSize == SIZE_32BIT) {
		Emit(ptr, 0x67);
	}

	if (size == SIZE_16BIT) {
		Emit(ptr, 0x66);
	}

	if (size == SIZE_64BIT || mem.base > 7 || mem.index > 7) {
		byte w = size == SIZE_64BIT ? 1 : 0;
		byte b = (mem.base & 8) >> 3;
		EmitRexPrefix(ptr, w, 0, mem.index, b);
	}

	if (size == SIZE_8BIT)
		Emit(ptr, op);
	else
		Emit(ptr, op + 1);

	EmitModRegRM(ptr, 0, mem);
	EmitSIB(ptr, mem);
	EmitDisplacement(ptr, mem);

	if (size == SIZE_8BIT) {
		Emit(ptr, (byte)imm.u);
	}
	else if (size == SIZE_16BIT) {
		Emit16(ptr, (uint16)imm.u);
	}
	else {
		Emit32(ptr, (uint32)imm.u);
	}
}

void Emit(byte** ptr, byte op, Memory mem, Register _register) {
	byte size = (_register & 0xF0) >> 4;
	byte reg = _register & 0x0F;

	if (mem.regSize == SIZE_32BIT) {
		Emit(ptr, 0x67);
	}

	if (size == SIZE_16BIT) {
		Emit(ptr, 0x66);
	}

	if (size == SIZE_64BIT || reg > 7 || mem.base > 7 || mem.index > 7) {
		byte w = size == SIZE_64BIT ? 1 : 0;
		byte b = (mem.base & 8) >> 3;
		EmitRexPrefix(ptr, w, reg, mem.index, b);
	}

	if (size == SIZE_8BIT) {
		Emit(ptr, op);
	}
	else {
		Emit(ptr, op + 1);
	}

	EmitModRegRM(ptr, reg, mem);
	EmitSIB(ptr, mem);
	EmitDisplacement(ptr, mem);
}

void Emit(byte** ptr, byte op, Register register1, Register register2, Immediate imm) {
	byte imm_size = GetSignedSize(imm);
	Emit(ptr, op, register1, register2);

	if (imm_size == SIZE_8BIT) {
		Emit(ptr, (byte)imm.u);
	}
	else {
		Emit32(ptr, (uint32)imm.u);
	}
}

#define INC  		0
#define DEC  		1
#define CALL 		2
#define CALL_FAR 	3
#define JMP  		4
#define JMP_FAR 	5
#define PUSH 		6

#define ADD 0
#define OR  1
#define ADC 2
#define SBB 3
#define AND 4
#define SUB 5
#define XOR 6
#define CMP 7

void Emit_80(byte** ptr, byte op, Register _register, Immediate imm) {
	byte imm_size = GetSignedSize(imm);
	byte reg_size = (_register & 0xF0) >> 4;
	byte reg = _register & 0x0F;

	if (reg_size == SIZE_16BIT) {
		Emit(ptr, 0x66);
	}

	if (reg > 7 || reg_size == SIZE_64BIT) {
		byte w = reg_size == SIZE_64BIT ? 1 : 0;
		EmitRexPrefix(ptr, w, reg);
	}

	if (reg_size == SIZE_8BIT)
		Emit(ptr, 0x80);
	else if (imm_size == SIZE_8BIT)
		Emit(ptr, 0x83);
	else
		Emit(ptr, 0x81);

	EmitModRegRM(ptr, 3, op, reg);

	if (imm_size == SIZE_8BIT) {
		Emit(ptr, (byte)imm.u);
	}
	else if (reg_size == SIZE_16BIT) {
		Emit16(ptr, (uint16)imm.u);
	}
	else {
		Emit32(ptr, (uint32)imm.u);
	}
}

void Emit_80(byte** ptr, byte op, byte size, Memory mem, Immediate imm) {
	byte imm_size = GetSignedSize(imm);
	if (mem.regSize == SIZE_32BIT) {
		Emit(ptr, 0x67);
	}

	if (size == SIZE_16BIT) {
		Emit(ptr, 0x66);
	}

	if (size == SIZE_64BIT || mem.base > 7 || mem.index > 7) {
		byte w = size == SIZE_64BIT ? 1 : 0;
		byte b = (mem.base & 8) >> 3;
		EmitRexPrefix(ptr, w, 0, mem.index, b);
	}

	if (size == SIZE_8BIT)
		Emit(ptr, 0x80);
	else if (imm_size == SIZE_8BIT)
		Emit(ptr, 0x83);
	else
		Emit(ptr, 0x81);

	EmitModRegRM(ptr, op, mem);
	EmitSIB(ptr, mem);
	EmitDisplacement(ptr, mem);

	if (imm_size == SIZE_8BIT) {
		Emit(ptr, (byte)imm.u);
	}
	else if (size == SIZE_16BIT && imm_size == SIZE_16BIT) {
		Emit16(ptr, (uint16)imm.u);
	}
	else {
		Emit32(ptr, (uint32)imm.u);
	}
}

#define NOT  2
#define NEG  3
#define MUL  4
#define IMUL 5
#define DIV  6
#define IDIV 7

#define ROL 0
#define ROR 1
#define RCL 2
#define RCR 3
#define SHL 4
#define SHR 5
#define SAR 7

void Emit_C0(byte** ptr, byte op, Register _register, Immediate imm) {
	byte size = (_register & 0xF0) >> 4;
	byte reg = _register & 0x0F;

	if (size == SIZE_16BIT) {
		Emit(ptr, 0x66);
	}

	if (reg > 7 || size == SIZE_64BIT) {
		byte w = size == SIZE_64BIT ? 1 : 0;
		EmitRexPrefix(ptr, w, reg);
	}

	if (size == SIZE_8BIT)
		Emit(ptr, 0xC0);
	else
		Emit(ptr, 0xC1);
	EmitModRegRM(ptr, 3, op, reg);
	Emit(ptr, (byte)imm.u);
}

void Emit_C0(byte** ptr, byte op, byte size, Memory mem, Immediate imm) {
	if (mem.regSize == SIZE_32BIT) {
		Emit(ptr, 0x67);
	}

	if (size == SIZE_16BIT) {
		Emit(ptr, 0x66);
	}

	if (size == SIZE_64BIT || mem.base > 7 || mem.index > 7) {
		byte w = size == SIZE_64BIT ? 1 : 0;
		byte b = (mem.base & 8) >> 3;
		EmitRexPrefix(ptr, w, 0, mem.index, b);
	}

	if (size == SIZE_8BIT)
		Emit(ptr, 0xC0);
	else 
		Emit(ptr, 0xC1);

	EmitModRegRM(ptr, op, mem);
	EmitSIB(ptr, mem);
	EmitDisplacement(ptr, mem);

	Emit(ptr, (byte)imm.u);
}

//--------------------

int32* EmitIndirectJmp(byte** ptr) {
	Emit(ptr, 0xFF);
	Emit(ptr, 0x25);
	byte* offset = *ptr;
	*ptr += 4;
	return (int32*)offset;
}

// Pop
//------------------

void EmitPop(byte** ptr, byte size, Memory mem) {
	if (size == SIZE_16BIT) {
		Emit(ptr, 0x66);
	}

	if (mem.regSize == SIZE_32BIT) {
		Emit(ptr, 0x67);
	}

	if (mem.base > 7 || mem.index > 7) {
		byte w = size == SIZE_64BIT ? 1 : 0;
		byte b = (mem.base & 8) >> 3;
		EmitRexPrefix(ptr, w, 0, mem.index, b);
	}

	Emit(ptr, 0x8F);

	EmitModRegRM(ptr, 0, mem);
	EmitSIB(ptr, mem);
	EmitDisplacement(ptr, mem);
}

void EmitPop(byte** ptr, Register _register) {
	byte reg = _register & 0x0F;

	if (reg > 7) Emit(ptr, 0x41);
	Emit(ptr, 0x58 | (reg & 7));
}

// Conditional Jump
//------------------

#define JO   0x0
#define JNO  0x1
#define JB   0x2
#define JC   0x2
#define JAE  0x3
#define JNC  0x3
#define JE   0x4
#define JZ   0x4
#define JNE  0x5
#define JNZ  0x5
#define JBE  0x6
#define JNA  0x6
#define JA   0x7
#define JS   0x8
#define JNS  0x9
#define JPE  0xA
#define JPO  0xB
#define JL   0xC
#define JGE  0xD
#define JNL  0xD
#define JLE  0xE
#define JNG  0xE
#define JG   0xF

//-------------

enum XMM : byte {
	REG_XMM0,
	REG_XMM1,
	REG_XMM2,
	REG_XMM3,
	REG_XMM4,
	REG_XMM5,
	REG_XMM6,
	REG_XMM7,
	REG_XMM8,
	REG_XMM9,
	REG_XMM10,
	REG_XMM11,
	REG_XMM12,
	REG_XMM13,
	REG_XMM14,
	REG_XMM15,

	REG_XMM_count
};

void EmitCvtsi2ss(byte** ptr, XMM dst, Register src) {
	byte reg1 = dst & 0x0F;
	byte reg2 = src & 0x0F;

	Emit(ptr, 0xF3);

	if (reg1 > 7 || reg2 > 7) {
		byte b = (reg2 & 8) >> 3;
		EmitRexPrefix(ptr, 0, reg1, 0, b);
	}

	Emit(ptr, 0x0F);
	Emit(ptr, 0x2A);
	EmitModRegRM(ptr, 3, reg1, reg2);
}

void Emit(byte** ptr, byte op, XMM dst, XMM src, bool prefix) {
	byte reg1 = dst & 0x0F;
	byte reg2 = src & 0x0F;

	if (prefix)
		Emit(ptr, 0xF3);

	if (reg1 > 7 || reg2 > 7) {
		byte b = (reg2 & 8) >> 3;
		EmitRexPrefix(ptr, 0, reg1, 0, b);
	}

	Emit(ptr, 0x0F);
	Emit(ptr, op);
	EmitModRegRM(ptr, 3, reg1, reg2);
}

void EmitMovss(byte** ptr, XMM dst, XMM src) {
	byte reg1 = dst & 0x0F;
	byte reg2 = src & 0x0F;

	Emit(ptr, 0xF3);

	if (reg1 > 7 || reg2 > 7) {
		byte b = (reg2 & 8) >> 3;
		EmitRexPrefix(ptr, 0, reg1, 0, b);
	}

	Emit16(ptr, 0x100F);
	EmitModRegRM(ptr, 3, reg1, reg2);
}

void EmitMovss(byte** ptr, XMM xmm, Memory mem) {
	byte reg = xmm & 0x0F;

	if (mem.regSize == SIZE_32BIT) {
		Emit(ptr, 0x67);
	}

	Emit(ptr, 0xF3);

	if (reg > 7 || mem.base > 7 || mem.index > 7) {
		byte b = (mem.base & 8) >> 3;
		EmitRexPrefix(ptr, 0, reg, mem.index, b);
	}

	Emit16(ptr, 0x100F);

	EmitModRegRM(ptr, reg, mem);
	EmitSIB(ptr, mem);
	EmitDisplacement(ptr, mem);
}

void EmitMovss(byte** ptr, Memory mem, XMM xmm) {
	byte reg = xmm & 0x0F;

	if (mem.regSize == SIZE_32BIT) {
		Emit(ptr, 0x67);
	}

	Emit(ptr, 0xF3);

	if (reg > 7 || mem.base > 7 || mem.index > 7) {
		byte b = (mem.base & 8) >> 3;
		EmitRexPrefix(ptr, 0, reg, mem.index, b);
	}

	Emit16(ptr, 0x110F);

	EmitModRegRM(ptr, reg, mem);
	EmitSIB(ptr, mem);
	EmitDisplacement(ptr, mem);
}

void EmitCvttss2si(byte** ptr, Register dst, XMM src) {
	byte size = dst >> 4;
	byte reg1 = dst & 0x0F;
	byte reg2 = src & 0x0F;

	Emit(ptr, 0xF3);

	if (size == SIZE_64BIT || reg1 > 7 || reg2 > 7) {
		byte w = size == SIZE_64BIT ? 1 : 0;
		byte b = (reg2 & 8) >> 3;
		EmitRexPrefix(ptr, w, reg1, 0, b);
	}

	Emit16(ptr, 0x2C0F);
	EmitModRegRM(ptr, 3, reg1, reg2);
}

void EmitRoundss(byte** ptr, XMM dst, XMM src, Immediate imm) {
	byte reg1 = dst & 0x0F;
	byte reg2 = src & 0x0F;

	Emit(ptr, 0x66);

	if (reg1 > 7 || reg2 > 7) {
		byte b = (reg1 & 8) >> 3;
		EmitRexPrefix(ptr, 0, reg2, 0, b);
	}

	Emit16(ptr, 0x3A0F);
	Emit(ptr, 0x0A);
	EmitModRegRM(ptr, 3, reg2, reg1);
	Emit(ptr, (byte)imm.u);
}

//-------------

// TODO: enum?
#define Od_Immediate		1
#define Od_Register 		2
#define Od_Memory			3
#define Od_ImportedFunction 4
#define Od_StaticValue		5
#define Od_JumpTarget		6
#define Od_MemoryAddress	7
#define Od_PJumpTarget		8
#define Od_Xmm				9

enum OpCode : int32 {
	
	Op_Movsb = 0xA4,
	Op_Ret   = 0xC3,
	Op_Leave = 0xC9,
	Op_Nop   = 0x90,
	Op_Rep   = 0xF3,

	Op_Mov,
	Op_Lea,
	Op_Pop,
	Op_Test,

	Op_Inc      = 0x100,
	Op_Dec      = 0x101,
	Op_Call     = 0x102,
	Op_Call_Far = 0x103,
	Op_Jmp      = 0x104,
	Op_Jmp_Far  = 0x105,
	Op_Push     = 0x106,	

	Op_Add      = 0x200,
	Op_Or       = 0x201,
	Op_Adc      = 0x202,
	Op_Sbb      = 0x203,
	Op_And      = 0x204,
	Op_Sub      = 0x205,
	Op_Xor      = 0x206,
	Op_Cmp      = 0x207,

	Op_Not      = 0x302,
	Op_Neg      = 0x303,
	Op_Mul      = 0x304,
	Op_Imul     = 0x305,
	Op_Div      = 0x306,
	Op_Idiv     = 0x307,
	
	Op_Rol      = 0x400,
	Op_Ror      = 0x401,
	Op_Rcl      = 0x402,
	Op_Rcr      = 0x403,
	Op_Shl      = 0x404,
	Op_Shr      = 0x405,
	Op_Sar      = 0x407,

	Op_Jo       = 0x500,
	Op_Jno      = 0x500 | (1 << 1),
	Op_Jb       = 0x500 | (2 << 1),
	Op_Jc,
	Op_Jae      = 0x500 | (3 << 1),
	Op_Jnc,
	Op_Je       = 0x500 | (4 << 1),
	Op_Jz,
	Op_Jne      = 0x500 | (5 << 1),
	Op_Jnz,
	Op_Jbe      = 0x500 | (6 << 1),
	Op_Jna,
	Op_Ja       = 0x500 | (7 << 1),
	Op_Js       = 0x500 | (8 << 1),
	Op_Jns      = 0x500 | (9 << 1),
	Op_Jpe      = 0x500 | (10 << 1),
	Op_Jpo      = 0x500 | (11 << 1),
	Op_Jl       = 0x500 | (12 << 1),
	Op_Jge      = 0x500 | (13 << 1),
	Op_Jnl,
	Op_Jle      = 0x500 | (14 << 1),
	Op_Jng,
	Op_Jg       = 0x500 | (15 << 1),

	Op_Sqrtss   = 0x601,
	Op_Rsqrtss  = 0x602,
	Op_Rcpss    = 0x603,
	Op_Andps    = 0x604,
	Op_Andnps   = 0x605,
	Op_Orps     = 0x606,
	Op_Xorps    = 0x607,
	Op_Addss    = 0x608,
	Op_Mulss    = 0x609,
	Op_Cvtps2pd = 0x60A,
	Op_Cvtdq2ps = 0x60B,
	Op_Subss    = 0x60C,
	Op_Minss    = 0x60D,
	Op_Divss    = 0x60E,
	Op_Maxss    = 0x60F,

	Op_Ucomiss  = 0x70E,
	Op_Comiss   = 0x70F,

	Op_Movss,
	Op_Cvttss2si,
	Op_Cvtsi2ss,
};

void Emit(byte** ptr, OpCode opCode) {
	Emit(ptr, (byte)opCode);
}

void Emit(byte** ptr, OpCode opCode, Immediate imm) {
	switch (opCode) {
	case Op_Call: {
		Emit(ptr, 0xE8);
		Emit32(ptr, (uint32)imm.u);
	} break;
	case Op_Jmp: {
		Emit(ptr, 0xE9);
		Emit32(ptr, (uint32)imm.u);
	} break;
	case Op_Push: {
		byte size = GetSignedSize(imm);
		if (size == SIZE_8BIT) {
			Emit(ptr, 0x6A);
			Emit(ptr, (byte)imm.u);
		}
		else {
			Emit(ptr, 0x68);
			Emit32(ptr, (uint32)imm.u);
		}
	} break;
	case Op_Jo :
	case Op_Jno:
	case Op_Jb :
	case Op_Jc :
	case Op_Jae:
	case Op_Jnc:
	case Op_Je :
	case Op_Jz :
	case Op_Jne:
	case Op_Jnz:
	case Op_Jbe:
	case Op_Jna:
	case Op_Ja :
	case Op_Js :
	case Op_Jns:
	case Op_Jpe:
	case Op_Jpo:
	case Op_Jl :
	case Op_Jge:
	case Op_Jnl:
	case Op_Jle:
	case Op_Jng:
	case Op_Jg : {
		byte cc = ((byte)(opCode & 0x1E) >> 1);
		Emit(ptr, 0x0F);
		Emit(ptr, 0x80 | cc);
		Emit32(ptr, (uint32)imm.u);
	} break;
	default: {
		ASSERT(0);
	} break;
	}
}

void Emit(byte** ptr, OpCode opCode, Register reg) {
	switch (opCode) {
	case Op_Pop: {
		EmitPop(ptr, reg);
	} break;
	case Op_Inc:
	case Op_Dec:
	case Op_Call:
	case Op_Call_Far:
	case Op_Jmp:
	case Op_Jmp_Far: {
		byte op = (byte)(opCode & 7);
		Emit(ptr, 0xFE, op, reg);
	} break;
	case Op_Not:
	case Op_Neg:
	case Op_Mul:
	case Op_Imul:
	case Op_Div:
	case Op_Idiv: {
		byte op = (byte)(opCode & 7);
		Emit(ptr, 0xF6, op, reg);
	} break;
	case Op_Rol:
	case Op_Ror:
	case Op_Rcl:
	case Op_Rcr:
	case Op_Shl:
	case Op_Shr:
	case Op_Sar: {
		byte op = (byte)(opCode & 7);
		Emit(ptr, 0xD2, op, reg);
	} break;
	default: {
		ASSERT(0);
	} break;
	}
}

void Emit(byte** ptr, OpCode opCode, byte size, Memory mem) {
	switch (opCode) {
	case Op_Inc:
	case Op_Dec:
	case Op_Call:
	case Op_Call_Far:
	case Op_Jmp:
	case Op_Jmp_Far:
	case Op_Push: {
		byte op = (byte)(opCode & 7);
		Emit(ptr, 0xFE, op, size, mem);
	} break;
	case Op_Not:
	case Op_Neg:
	case Op_Mul:
	case Op_Imul:
	case Op_Div:
	case Op_Idiv: {
		byte op = (byte)(opCode & 7);
		Emit(ptr, 0xF6, op, size, mem);
	} break;
	case Op_Rol:
	case Op_Ror:
	case Op_Rcl:
	case Op_Rcr:
	case Op_Shl:
	case Op_Shr:
	case Op_Sar: {
		byte op = (byte)(opCode & 7);
		Emit(ptr, 0xD2, op, size, mem);
	} break;
	case Op_Pop: {
		EmitPop(ptr, size, mem);
	} break;
	default: {
		ASSERT(0);
	} break;
	}
}

void Emit(byte** ptr, OpCode opCode, Register reg, Immediate imm) {
	switch (opCode) {
	case Op_Mov:	{
		EmitMov(ptr, reg, imm);
	} break;
	case Op_Add:
	case Op_Or :
	case Op_Adc:
	case Op_Sbb:
	case Op_And:
	case Op_Sub:
	case Op_Xor:
	case Op_Cmp: {
		byte op = (byte)(opCode & 7);

		Emit_80(ptr, op, reg, imm);
	} break;
	case Op_Test: {
		Emit(ptr, 0xF6, reg, imm);
	} break;
	case Op_Rol:
	case Op_Ror:
	case Op_Rcl:
	case Op_Rcr:
	case Op_Shl:
	case Op_Shr:
	case Op_Sar: {
		byte op = (byte)(opCode & 7);

		Emit_C0(ptr, op, reg, imm);
	} break;
	default: {
		ASSERT(0);
	} break;
	}
}

void Emit(byte** ptr, OpCode opCode, Register dst, Register src) {
	switch (opCode) {
	case Op_Mov:	{
		Emit(ptr, 0x88, dst, src);
	} break;
	case Op_Add:
	case Op_Or :
	case Op_Adc:
	case Op_Sbb:
	case Op_And:
	case Op_Sub:
	case Op_Xor:
	case Op_Cmp: {
		byte op = (byte)(opCode & 7);

		Emit(ptr, op << 3, dst, src);
	} break;
	case Op_Test: {
		Emit(ptr, 0x84, dst, src);
	} break;
	case Op_Rol:
	case Op_Ror:
	case Op_Rcl:
	case Op_Rcr:
	case Op_Shl:
	case Op_Shr:
	case Op_Sar: {
		ASSERT(src == REG_CL);
		byte op = (byte)(opCode & 7);
		Emit(ptr, 0xD2, op, dst);
	} break;
	case Op_Imul: {
		ASSERT(SIZE_32BIT <= (dst >> 4)); 
		Emit16(ptr, 0xAF0E, dst, src);
	} break;
	default: {
		ASSERT(0);
	} break;
	}
}

void Emit(byte** ptr, OpCode opCode, XMM dst, XMM src) {
	switch (opCode) {
	case Op_Movss: {
		EmitMovss(ptr, dst, src);
	} break;
	case Op_Sqrtss :
	case Op_Rsqrtss:
	case Op_Rcpss  :
	case Op_Addss  :
	case Op_Mulss  :
	case Op_Subss  :
	case Op_Minss  :
	case Op_Divss  :
	case Op_Maxss  : {
		byte op = (byte)(opCode & 15);
		Emit(ptr, 0x50 | op, dst, src, true);
	} break;
	case Op_Andps   :
	case Op_Andnps  :
	case Op_Orps    :
	case Op_Xorps   :
	case Op_Cvtps2pd:
	case Op_Cvtdq2ps: {
		byte op = (byte)(opCode & 15);
		Emit(ptr, 0x50 | op, dst, src, false);
	} break;
	case Op_Ucomiss:
	case Op_Comiss: {
		byte op = (byte)(opCode & 15);
		Emit(ptr, 0x20 | op, dst, src, false);
	} break;
	}
}

void Emit(byte** ptr, OpCode opCode, Register dst, XMM src) {
	EmitCvttss2si(ptr, dst, src);
}

void Emit(byte** ptr, OpCode opCode, XMM dst, Register src) {
	EmitCvtsi2ss(ptr, dst, src);
}

void Emit(byte** ptr, OpCode opCode, Register reg, Memory mem) {
	switch (opCode) {
	case Op_Mov: {
		Emit(ptr, 0x8A, reg, mem);
	} break;
	case Op_Lea: {
		Emit(ptr, 0x8C, reg, mem);
	} break;
	case Op_Add:
	case Op_Or :
	case Op_Adc:
	case Op_Sbb:
	case Op_And:
	case Op_Sub:
	case Op_Xor:
	case Op_Cmp: {
		byte op = (byte)(opCode & 7);

		Emit(ptr, (op<<3) | 2, reg, mem);
	} break;
	case Op_Test:{
		Emit(ptr, 0x84, reg, mem);
	} break;
	default: {
		ASSERT(0);
	} break;
	}
}

void Emit(byte** ptr, OpCode opCode, XMM xmm, Memory mem) {
	EmitMovss(ptr, xmm, mem);
}

void Emit(byte** ptr, OpCode opCode, byte size, Memory mem, Immediate imm) {
	switch (opCode) {
	case Op_Mov: {
		Emit(ptr, 0xC6, size, mem, imm);
	} break;
	case Op_Add:
	case Op_Or :
	case Op_Adc:
	case Op_Sbb:
	case Op_And:
	case Op_Sub:
	case Op_Xor:
	case Op_Cmp: {
		byte op = (byte)(opCode & 7);

		Emit_80(ptr, op, size, mem, imm);
	} break;
	case Op_Test: {
		Emit(ptr, 0xF6, size, mem, imm);
	} break;
	case Op_Rol:
	case Op_Ror:
	case Op_Rcl:
	case Op_Rcr:
	case Op_Shl:
	case Op_Shr:
	case Op_Sar: {
		byte op = (byte)(opCode & 7);
		Emit_C0(ptr, op, size, mem, imm);
	} break;
	default: {
		ASSERT(0);
	} break;
	}
}

void Emit(byte** ptr, OpCode opCode, Memory mem, Register reg) {
	switch (opCode) {
	case Op_Mov: {
		Emit(ptr, 0x88, mem, reg);
	} break;
	case Op_Add:
	case Op_Or :
	case Op_Adc:
	case Op_Sbb:
	case Op_And:
	case Op_Sub:
	case Op_Xor:
	case Op_Cmp: {
		byte op = (byte)(opCode & 7);

		Emit(ptr, op << 3, mem, reg);
	} break;
	case Op_Test: {
		Emit(ptr, 0x84, mem, reg);
	} break;
	default: {
		ASSERT(0);
	} break;
	}
}

void Emit(byte** ptr, OpCode opCode, Memory mem, XMM xmm) {
	EmitMovss(ptr, mem, xmm);
}

void Emit(byte** ptr, OpCode opCode, Register dst, Register src, Immediate imm) {
	switch (opCode) {
	case Op_Imul: {
		byte imm_size = GetSignedSize(imm);
		if (imm_size == SIZE_8BIT) {
			Emit(ptr, 0x6A, dst, src, imm);
		}
		else {
			Emit(ptr, 0x68, dst, src, imm);
		}
	} break;
	default: {
		ASSERT(0);
	} break;
	}
}

void Emit(byte** ptr, OpCode opCode, XMM dst, XMM src, Immediate imm) {
	EmitRoundss(ptr, dst, src, imm);
}

struct ImportedFunction {
	String library;
	String name;
	int32 hint;
};

struct Operand {
	uint32 type;
	union {
		Register reg;
		XMM xmm;
		Immediate imm;
		Memory mem;
		ImportedFunction func;
		int32 index; // for static values and for local functions
		int32* ptr;  // for indirect index
	};
};

struct Operation {
	OpCode opCode;
	int32 operandCount;
	Operand operands[5];
	byte size;

	int32 address;
	union {
		int32* offset;
		uint64* mem_address;
	};
};

void ConvertPJumptTargetToJumpTarget(Operation* op) {
	if (op->operands[0].type == Od_PJumpTarget) {
		int32 index = *op->operands[0].ptr;
		op->operands[0].type = Od_JumpTarget;
		op->operands[0].index = index;
	}
}

void EmitOperation(byte** ptr, Operation* op) {
	if (op->operandCount == 0) {
		Emit(ptr, op->opCode);
	}
	else if (op->operandCount == 1) {
		Operand operand = op->operands[0];
		if (operand.type == Od_Immediate) {
			Emit(ptr, op->opCode, operand.imm);
		}
		else if (operand.type == Od_Register) {
			Emit(ptr, op->opCode, operand.reg);
		}
		else if (operand.type == Od_Memory) {
			Emit(ptr, op->opCode, op->size, operand.mem);
		}
		else if (operand.type == Od_ImportedFunction ||
			operand.type == Od_JumpTarget) {
			Immediate imm = {};
			Emit(ptr, op->opCode, imm);
			op->offset = (int32*)(*ptr - 4);
		}
		else if (operand.type == Od_PJumpTarget) {
			ConvertPJumptTargetToJumpTarget(op);
			Immediate imm = {};
			Emit(ptr, op->opCode, imm);
			op->offset = (int32*)(*ptr - 4);
		}
		else {
			ASSERT(0);
		}
	}
	else if (op->operandCount == 2) {
		Operand dst = op->operands[0];
		Operand src = op->operands[1];

		if (dst.type == Od_Register) {
			if (src.type == Od_Immediate) {
				Emit(ptr, op->opCode, dst.reg, src.imm);
			}
			else if (src.type == Od_Register) {
				Emit(ptr, op->opCode, dst.reg, src.reg);
			}
			else if (src.type == Od_Memory) {
				Emit(ptr, op->opCode, dst.reg, src.mem);
			}
			else if (src.type == Od_Xmm) {
				Emit(ptr, op->opCode, dst.reg, src.xmm);
			}
			else if (src.type == Od_StaticValue) {
				Memory mem = {Mem_RelativeToCurrent};
				Emit(ptr, op->opCode, dst.reg, mem);
				op->offset = (int32*)(*ptr - 4);
			}
			else if (src.type == Od_MemoryAddress) {
				ASSERT(op->opCode == Op_Mov);
				Immediate imm = {};
				imm.u = 0x123456789; // force 64 bit size
				Emit(ptr, op->opCode, dst.reg, imm);
				op->mem_address = (uint64*)(*ptr - 8);
			}
			else {
				ASSERT(0);
			}
		}
		else if (dst.type == Od_Memory) {
			if (src.type == Od_Immediate) {
				Emit(ptr, op->opCode, op->size, dst.mem, src.imm);
			}
			else if (src.type == Od_Register) {
				Emit(ptr, op->opCode, dst.mem, src.reg);
			}
			else if (src.type == Od_Xmm) {
				Emit(ptr, op->opCode, dst.mem, src.xmm);
			}
			else {
				ASSERT(0);
			}
		}
		else if (dst.type == Od_Xmm) {
			if (src.type == Od_Xmm) {
				Emit(ptr, op->opCode, dst.xmm, src.xmm);
			}
			else if (src.type == Od_Register) {
				Emit(ptr, op->opCode, dst.xmm, src.reg);
			}
			else if (src.type == Od_Memory) {
				Emit(ptr, op->opCode, dst.xmm, src.mem);
			}
			else if (src.type == Od_StaticValue) {
				Memory mem = {Mem_RelativeToCurrent};
				Emit(ptr, op->opCode, dst.xmm, mem);
				op->offset = (int32*)(*ptr - 4);
			}
			else {
				ASSERT(0);
			}
		}
		else {
			ASSERT(0);
		}
	}
	else if (op->operandCount == 3) {
		ASSERT(op->operands[2].type == Od_Immediate);
		Operand dst = op->operands[0];
		Operand src = op->operands[1];
		Immediate imm = op->operands[2].imm;

		if (dst.type == Od_Register && src.type == Od_Register) {
			Emit(ptr, op->opCode, dst.reg, src.reg, imm);
		}
		else if (dst.type == Od_Xmm && src.type == Od_Xmm) {
			Emit(ptr, op->opCode, dst.xmm, src.xmm, imm);
		}
		else {
			ASSERT(0);
		}
	}
}

Memory _Memory_(Register _register, int32 disp) {
	Memory mem = {};
	mem.mode = Mem_RelativeToReg;
	mem.regSize = _register >> 4;
	mem.base = _register & 0x0F;
	mem.disp = disp;
	return mem;
}

Memory _Memory_(Register base, byte scale, Register index) {
	Memory mem = {};
	mem.mode = Mem_IndexedAddressing;
	mem.regSize = base >> 4;
	mem.base = base & 0x0F;
	mem.index = index & 0x0F;
	mem.scale = scale;
	return mem;
}

Memory _Memory_(byte scale, Register index) {
	Memory mem = {};
	mem.mode = Mem_IndexedAddressingNoBase;
	mem.regSize = index >> 4;
	mem.index = index & 0x0F;
	mem.scale = scale;
	return mem;
}

Operation CreateOperation(OpCode opCode, Register register1, Register register2, Immediate imm) {
	Operation op = {};
	op.opCode = opCode;
	op.operandCount = 3;
	op.operands[0].type = Od_Register;
	op.operands[0].reg = register1;
	op.operands[1].type = Od_Register;
	op.operands[1].reg = register2;
	op.operands[2].type = Od_Immediate;
	op.operands[2].imm = imm;

	return op;
}

Operation CreateOperation(OpCode opCode, XMM dst, XMM src, Immediate imm) {
	Operation op = {};
	op.opCode = opCode;
	op.operandCount = 3;
	op.operands[0].type = Od_Xmm;
	op.operands[0].xmm = dst;
	op.operands[1].type = Od_Xmm;
	op.operands[1].xmm = src;
	op.operands[2].type = Od_Immediate;
	op.operands[2].imm = imm;

	return op;
}

Operation CreateOperation(OpCode opCode, Register register1, Register register2, uint64 immu) {
	Immediate imm;
	imm.sx = false;
	imm.u = immu;

	return CreateOperation(opCode, register1, register2, imm);
}

Operation CreateOperation(OpCode opCode, Register _register, Immediate imm) {
	Operation op = {};
	op.opCode = opCode;
	op.operandCount = 2;
	op.operands[0].type = Od_Register;
	op.operands[0].reg = _register;
	op.operands[1].type = Od_Immediate;
	op.operands[1].imm = imm;

	return op;
}

Operation CreateOperation(OpCode opCode, Register _register, int64 imms) {
	Immediate imm;
	imm.sx = true;
	imm.s = imms;

	return CreateOperation(opCode, _register, imm);
}

Operation CreateOperation(OpCode opCode, Register dst_register, Register src_register) {
	Operation op = {};
	op.opCode = opCode;
	op.operandCount = 2;
	op.operands[0].type = Od_Register;
	op.operands[0].reg = dst_register;
	op.operands[1].type = Od_Register;
	op.operands[1].reg = src_register;

	return op;
}

Operation CreateOperation(OpCode opCode, byte size, Memory mem, Register _register) {
	Operation op = {};
	op.opCode = opCode;
	op.operandCount = 2;
	op.size = size;
	op.operands[0].type = Od_Memory;
	op.operands[0].mem = mem;
	op.operands[1].type = Od_Register;
	op.operands[1].reg = _register;

	return op;
}

Operation CreateOperation(OpCode opCode, byte size, Memory mem, Immediate imm) {
	Operation op = {};
	op.opCode = opCode;
	op.operandCount = 2;
	op.size = size;
	op.operands[0].type = Od_Memory;
	op.operands[0].mem = mem;
	op.operands[1].type = Od_Immediate;
	op.operands[1].imm = imm;

	return op;
}

Operation CreateOperation(OpCode opCode, byte size, Memory mem, int32 imms) {
	Immediate imm;
	imm.sx = true;
	imm.s = imms;
	return CreateOperation(opCode, size, mem, imm);
}

Operation CreateOperation(OpCode opCode, Register _register, Memory mem) {
	Operation op = {};
	op.opCode = opCode;
	op.operandCount = 2;
	op.operands[0].type = Od_Register;
	op.operands[0].reg = _register;
	op.operands[1].type = Od_Memory;
	op.operands[1].mem = mem;

	return op;
}

Operation CreateOperation(OpCode opCode, Memory mem, Register _register) {
	Operation op = {};
	op.opCode = opCode;
	op.operandCount = 2;
	op.operands[0].type = Od_Memory;
	op.operands[0].mem = mem;
	op.operands[1].type = Od_Register;
	op.operands[1].reg = _register;

	return op;
}

Operation CreateOperation(OpCode opCode, XMM dst, XMM src) {
	Operation op = {};
	op.opCode = opCode;
	op.operandCount = 2;
	op.operands[0].type = Od_Xmm;
	op.operands[0].xmm = dst;
	op.operands[1].type = Od_Xmm;
	op.operands[1].xmm = src;

	return op;
}

Operation CreateOperation(OpCode opCode, Register dst, XMM src) {
	Operation op = {};
	op.opCode = opCode;
	op.operandCount = 2;
	op.operands[0].type = Od_Register;
	op.operands[0].reg = dst;
	op.operands[1].type = Od_Xmm;
	op.operands[1].xmm = src;

	return op;
}

Operation CreateOperation(OpCode opCode, XMM dst, Register src) {
	Operation op = {};
	op.opCode = opCode;
	op.operandCount = 2;
	op.operands[0].type = Od_Xmm;
	op.operands[0].xmm = dst;
	op.operands[1].type = Od_Register;
	op.operands[1].reg = src;

	return op;
}

Operation CreateOperation(OpCode opCode, XMM xmm, Memory mem) {
	Operation op = {};
	op.opCode = opCode;
	op.operandCount = 2;
	op.operands[0].type = Od_Xmm;
	op.operands[0].xmm = xmm;
	op.operands[1].type = Od_Memory;
	op.operands[1].mem = mem;

	return op;
}

Operation CreateOperation(OpCode opCode, Memory mem, XMM xmm) {
	Operation op = {};
	op.opCode = opCode;
	op.operandCount = 2;
	op.operands[0].type = Od_Memory;
	op.operands[0].mem = mem;
	op.operands[1].type = Od_Xmm;
	op.operands[1].xmm = xmm;

	return op;
}

struct StaticValue {
	int32 id;
};

struct MemoryAddress {
	int32 id;
};

Operation CreateOperation(OpCode opCode, Register reg, StaticValue value) {
	Operation op = {};
	op.opCode = opCode;
	op.operandCount = 2;
	op.operands[0].type = Od_Register;
	op.operands[0].reg = reg;
	op.operands[1].type = Od_StaticValue;
	op.operands[1].index = value.id;

	return op;
}

Operation CreateOperation(OpCode opCode, XMM xmm, StaticValue value) {
	Operation op = {};
	op.opCode = opCode;
	op.operandCount = 2;
	op.operands[0].type = Od_Xmm;
	op.operands[0].xmm = xmm;
	op.operands[1].type = Od_StaticValue;
	op.operands[1].index = value.id;

	return op;
}

Operation CreateOperation(OpCode opCode, Register reg, MemoryAddress value) {
	Operation op = {};
	op.opCode = opCode;
	op.operandCount = 2;
	op.operands[0].type = Od_Register;
	op.operands[0].reg = reg;
	op.operands[1].type = Od_MemoryAddress;
	op.operands[1].index = value.id;

	return op;
}

Operation CreateOperation(OpCode opCode, Immediate imm) {
	Operation op = {};
	op.opCode = opCode;
	op.operandCount = 1;
	op.operands[0].type = Od_Immediate;
	op.operands[0].imm = imm;

	return op;
}

Operation CreateOperation(OpCode opCode, int32 imms) {
	Immediate imm;
	imm.sx = true;
	imm.s = imms;

	return CreateOperation(opCode, imm);
}

Operation CreateOperation(OpCode opCode, Register reg) {
	Operation op = {};
	op.opCode = opCode;
	op.operandCount = 1;
	op.operands[0].type = Od_Register;
	op.operands[0].reg = reg;

	return op;
}

Operation CreateOperation(OpCode opCode, Memory mem) {
	Operation op = {};
	op.opCode = opCode;
	op.operandCount = 1;
	op.operands[0].type = Od_Memory;
	op.operands[0].mem = mem;

	return op;
}

struct Instruction {
	int32 index;
};

Operation CreateOperation(OpCode opCode, Instruction instruction) {
	Operation op = {};
	op.opCode = opCode;
	op.operandCount = 1;
	op.operands[0].type = Od_JumpTarget;
	op.operands[0].index = instruction.index;

	return op;
}

Operation CreateOperation(OpCode opCode, int32* ptr) {
	Operation op = {};
	op.opCode = opCode;
	op.operandCount = 1;
	op.operands[0].type = Od_PJumpTarget;
	op.operands[0].ptr = ptr;

	return op;
}

Operation CreateOperation(OpCode opCode, ImportedFunction func) {
	Operation op = {};
	op.opCode = opCode;
	op.operandCount = 1;
	op.operands[0].type = Od_ImportedFunction;
	op.operands[0].func = func;

	return op;
}

Operation CreateOperation(OpCode opCode) {
	Operation op = {};
	op.opCode = opCode;
	op.operandCount = 0;

	return op;
}







/*
int32 GetSymbolHint(ImportedFunction func) {
	int32 low = 0;
	int32 high = func.dll->count - 1;

	while (low <= high) {
		int32 mid = low + (high - low)/2;
		int cmp = StringCompare(func.dll->symbols[mid], func.name);

		if (cmp == 0)
			return mid;
		else if (cmp < 0)
			low = mid + 1;
		else
			high = mid - 1;
	}

	return -1;
}
*/