#include "../common/basic/basic.cpp"
#include "../common/basic/array.cpp"
#include "codegen.cpp"
#include "kernelnames.cpp"
#include "pe.cpp"


int main() {
	String libName = STR("KERNEL32.dll");
	
	String rodata[] = {
		STR("Hello World!")
	};

	Operation ops[] = {
		// enter function
		/* 0*/ CreateOperation(Op_Sub, REG_RSP, 48),
		/* 1*/ CreateOperation(Op_Mov, _Memory_(REG_RSP, 56), REG_RCX), // message
		/* 2*/ CreateOperation(Op_Mov, _Memory_(REG_RSP, 64), REG_RDX), // length

		// get handle
		/* 3*/ CreateOperation(Op_Mov, REG_RCX, -11ll),
		/* 4*/ CreateOperation(Op_Call, {libName, STR("GetStdHandle"), 746}),

		// write file
		/* 5*/ CreateOperation(Op_Mov, SIZE_QWORD, _Memory_(REG_RSP, 32), 0),
		/* 6*/ CreateOperation(Op_Lea, REG_R9, _Memory_(REG_RSP, 40)),
		/* 7*/ CreateOperation(Op_Mov, REG_R8, _Memory_(REG_RSP, 64)), // length
		/* 8*/ CreateOperation(Op_Mov, REG_RDX, _Memory_(REG_RSP, 56)), // message
		/* 9*/ CreateOperation(Op_Mov, REG_RCX, REG_RAX),
		/*10*/ CreateOperation(Op_Call, {libName, STR("WriteFile"), 1597}),

		// exit function
		/*11*/ CreateOperation(Op_Add, REG_RSP, 48),
		/*12*/ CreateOperation(Op_Ret),

		// UnsignedToDecimal
		/*13*/ CreateOperation(Op_Mov, REG_RAX, 10000000000),
        /*14*/ CreateOperation(Op_Cmp, REG_RCX, REG_RAX),
        /*15*/ CreateOperation(Op_Jae, Instruction{49}), // more than 10 digits

		// at most 10 digits
        /*16*/ CreateOperation(Op_Cmp, REG_RCX, 100000),
        /*17*/ CreateOperation(Op_Jae, Instruction{34}), // 6 to 10 digits

		// at most 5 digits
        /*18*/ CreateOperation(Op_Cmp, REG_RCX, 1000),
        /*19*/ CreateOperation(Op_Jae, Instruction{30}), // 4 or 5 digits

		// at most 3 digits
        /*20*/ CreateOperation(Op_Cmp, REG_RCX, 10),
        /*21*/ CreateOperation(Op_Jae, Instruction{26}), // 2 or 3_digits

		// 1 digit
        /*22*/ CreateOperation(Op_Mov, REG_RAX, 1),
        /*23*/ CreateOperation(Op_Add, REG_RCX, 48),
        /*24*/ CreateOperation(Op_Mov, _Memory_(REG_R8, 0), REG_RCX),
        /*25*/ CreateOperation(Op_Ret),

		// 2 or 3 digits
        /*26*/ CreateOperation(Op_Mov, REG_RAX, 3),
        /*27*/ CreateOperation(Op_Cmp, REG_RCX, 100),
        /*28*/ CreateOperation(Op_Sbb, REG_RAX, 0),
        /*29*/ CreateOperation(Op_Jmp, Instruction{82}), // pre-loop

		// 4 or 5_digits
		/*30*/ CreateOperation(Op_Mov, REG_RAX, 5),
        /*31*/ CreateOperation(Op_Cmp, REG_RCX, 10000),
        /*32*/ CreateOperation(Op_Sbb, REG_RAX, 0),
        /*33*/ CreateOperation(Op_Jmp, Instruction{82}), // pre-loop

		// 6 to 10 digits
        /*34*/ CreateOperation(Op_Cmp, REG_RCX, 100000000),
        /*35*/ CreateOperation(Op_Jae, Instruction{44}), // 9 or 10 digits

		// 6 to 8 digits
        /*36*/ CreateOperation(Op_Cmp, REG_RCX, 10000000),
        /*37*/ CreateOperation(Op_Jae, Instruction{40}), // 7 or 8 digits

		// 6 digits
        /*38*/ CreateOperation(Op_Mov, REG_RAX, 6),
        /*39*/ CreateOperation(Op_Jmp, Instruction{82}), // pre-loop

		// 7 or 8 digits
        /*40*/ CreateOperation(Op_Mov, REG_RAX, 8),
        /*41*/ CreateOperation(Op_Cmp, REG_RCX, 10000000),
        /*42*/ CreateOperation(Op_Sbb, REG_RAX, 0),
        /*43*/ CreateOperation(Op_Jmp, Instruction{82}), // pre-loop

		// 9 or 10 digits
		/*44*/ CreateOperation(Op_Mov, REG_RAX, 10),
        /*45*/ CreateOperation(Op_Mov, REG_RDX, 1000000000),
        /*46*/ CreateOperation(Op_Cmp, REG_RCX, REG_RDX),
        /*47*/ CreateOperation(Op_Sbb, REG_RAX, 0),
        /*48*/ CreateOperation(Op_Jmp, Instruction{82}), // pre-loop

		// more than 10 digits
        /*49*/ CreateOperation(Op_Mov, REG_RAX, 1000000000000000),
        /*50*/ CreateOperation(Op_Cmp, REG_RCX, REG_RAX),
        /*51*/ CreateOperation(Op_Jae, Instruction{70}), // more than 15 digits

		// 11 to 15 digits
        /*52*/ CreateOperation(Op_Mov, REG_RAX, 10000000000000),
        /*53*/ CreateOperation(Op_Cmp, REG_RCX, REG_RAX),
        /*54*/ CreateOperation(Op_Jae, Instruction{65}), // 14 or 15 digits

		// 11 to 13 digits
        /*55*/ CreateOperation(Op_Mov, REG_RAX, 100000000000),
        /*56*/ CreateOperation(Op_Cmp, REG_RCX, REG_RAX),
        /*57*/ CreateOperation(Op_Jae, Instruction{60}), // 12 or 13 digits

		// 11 digits
        /*58*/ CreateOperation(Op_Mov, REG_RAX, 11),
        /*59*/ CreateOperation(Op_Jmp, Instruction{82}), // pre-loop

		// 12 or 13 digits
        /*60*/ CreateOperation(Op_Mov, REG_RAX, 13),
        /*61*/ CreateOperation(Op_Mov, REG_RDX, 1000000000000),
        /*62*/ CreateOperation(Op_Cmp, REG_RCX, REG_RDX),
        /*63*/ CreateOperation(Op_Sbb, REG_RAX, 0),
        /*64*/ CreateOperation(Op_Jmp, Instruction{82}), // pre-loop

		// 14 or 15 digits
        /*65*/ CreateOperation(Op_Mov, REG_RAX, 15),
        /*66*/ CreateOperation(Op_Mov, REG_RDX, 100000000000000),
        /*67*/ CreateOperation(Op_Cmp, REG_RCX, REG_RDX),
        /*68*/ CreateOperation(Op_Sbb, REG_RAX, 0),
        /*69*/ CreateOperation(Op_Jmp, Instruction{82}), // pre-loop

		// more than 15_digits
        /*70*/ CreateOperation(Op_Mov, REG_RAX, 100000000000000000),
        /*71*/ CreateOperation(Op_Cmp, REG_RCX, REG_RAX),
        /*72*/ CreateOperation(Op_Jae, Instruction{78}), // 18 or 19 digits

		// 16 or 17 digits
		/*73*/ CreateOperation(Op_Mov, REG_RAX, 17),
        /*74*/ CreateOperation(Op_Mov, REG_RDX, 10000000000000000),
        /*75*/ CreateOperation(Op_Cmp, REG_RCX, REG_RDX),
        /*76*/ CreateOperation(Op_Sbb, REG_RAX, 0),
        /*77*/ CreateOperation(Op_Jmp, Instruction{82}), // pre-loop

		// 18 or 19 digits
		/*78*/ CreateOperation(Op_Mov, REG_RAX, 19),
        /*79*/ CreateOperation(Op_Mov, REG_RDX, 1000000000000000000),
        /*80*/ CreateOperation(Op_Cmp, REG_RCX, REG_RDX),
        /*81*/ CreateOperation(Op_Sbb, REG_RAX, 0),
        
		// pre-loop:
        /*82*/ CreateOperation(Op_Mov, REG_R9, REG_RAX),
        /*83*/ CreateOperation(Op_Mov, REG_R11, REG_RAX),

		// loop
		/*84*/ CreateOperation(Op_Dec, REG_R9),

		// rdx := rcx/10
		/*85*/ CreateOperation(Op_Mov, REG_RAX, 0xcccccccccccccccd),
        /*86*/ CreateOperation(Op_Mul, REG_RCX),
        /*87*/ CreateOperation(Op_Shr, REG_RDX, 3),

        // r10 := rdx*10
        /*88*/ CreateOperation(Op_Lea, REG_R10, _Memory_(REG_RDX, X4, REG_RDX)),
        /*89*/ CreateOperation(Op_Add, REG_R10, REG_R10),

        // rcx := rcx - r10 == rcx%10
        /*90*/ CreateOperation(Op_Sub, REG_RCX, REG_R10),

        // rcx += '0'
        /*91*/ CreateOperation(Op_Add, REG_RCX, 48), 

        // [r8+r9] = cl
        /*92*/ CreateOperation(Op_Mov, _Memory_(REG_R8, X1, REG_R9), REG_CL),

        // rcx := rdx
        /*93*/ CreateOperation(Op_Mov, REG_RCX, REG_RDX),

        // jump if (r9 != 0)
        /*94*/ CreateOperation(Op_Test, REG_R9, REG_R9),
        /*95*/ CreateOperation(Op_Jne, Instruction{84}),
        
        /*96*/ CreateOperation(Op_Mov, REG_RAX, REG_R11),
        /*97*/ CreateOperation(Op_Ret),

		// main
		/*98*/ CreateOperation(Op_Sub, REG_RSP, 40),

		// call UnsignedToDecimal
		/*99*/ CreateOperation(Op_Mov, REG_RCX, 1234),
        /*100*/ CreateOperation(Op_Lea, REG_R8, _Memory_(REG_RSP, 32)),
        /*101*/ CreateOperation(Op_Call, Instruction{13}),
        
		// print 1234
        /*102*/ CreateOperation(Op_Lea, REG_RCX, _Memory_(REG_RSP, 32)),
        /*103*/ CreateOperation(Op_Mov, REG_RDX, REG_RAX),
        /*104*/ CreateOperation(Op_Call, Instruction{0}),

		/*105*/ CreateOperation(Op_Add, REG_RSP, 40),
		/*106*/ CreateOperation(Op_Ret)
	};

	Link(ops, sizeof(ops)/sizeof(Operation), 98, rodata, 1, libName, L"hello.exe");

	return 0;
}

extern "C" void mainCRTStartup() {
	ExitProcess(main());
}