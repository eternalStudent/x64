#include <winnt.h>

// TODO: I hate all of these types

struct Array32 {
	Arena* arena;
	union {
		int32** table;
		void* data;
	};
	int32 count;
	int32 capacity;
};

struct DllSymbol {
	String name;
	uint16 hint;
	Array32 calls;

	int32* jump;
	uint64* iat;
	uint64* ilt;
};

struct SymbolArray {
	Arena* arena;
	union {
		DllSymbol* table;
		void* data;
	};
	int32 count;
	int32 capacity;
};

uint64 GetRVA(byte* offset, byte* baseOffset, uint64 rvBaseOffset) {
	return offset - baseOffset + rvBaseOffset;
}

uint64 GetVA(byte* offset, byte* baseOffset, uint64 rvBaseOffset) {
	return GetRVA(offset, baseOffset, rvBaseOffset) + 0x000140000000;
}

// TODO:
// 1. list of dlls
// 2. dynamic size
// 3. reloc table
// 4. subsystem
void Link(Operation* ops, int32 opCount, int32 entryOpIndex, String* rodata_values, int32 roCount, String kernel, LPCWSTR filename) {

	// TODO: obviously don't alloate fixed size
	byte* base = (byte*)OSAllocate(0x800);
	byte* buffer;

// Headers
//----------
	buffer = base;

	// DOS Header
	IMAGE_DOS_HEADER* DOSHeader = (IMAGE_DOS_HEADER*)buffer;
	DOSHeader->e_magic = IMAGE_DOS_SIGNATURE;
	DOSHeader->e_lfanew = sizeof(IMAGE_DOS_HEADER);
	buffer += sizeof(IMAGE_DOS_HEADER);

	// Image NT Header
	IMAGE_NT_HEADERS* headers = (IMAGE_NT_HEADERS*)buffer;
	headers->Signature = IMAGE_NT_SIGNATURE;
	headers->FileHeader.Machine = IMAGE_FILE_MACHINE_AMD64;
	headers->FileHeader.NumberOfSections = 2;
	headers->FileHeader.SizeOfOptionalHeader = sizeof(IMAGE_OPTIONAL_HEADER);
	headers->FileHeader.Characteristics = 
		IMAGE_FILE_LARGE_ADDRESS_AWARE | 
		IMAGE_FILE_EXECUTABLE_IMAGE | 
		IMAGE_FILE_RELOCS_STRIPPED;
	headers->OptionalHeader.Magic = IMAGE_NT_OPTIONAL_HDR64_MAGIC;
	headers->OptionalHeader.MajorLinkerVersion = 0x0E;
	headers->OptionalHeader.MinorLinkerVersion = 0x22;
	headers->OptionalHeader.SizeOfInitializedData = 0x200;
	headers->OptionalHeader.BaseOfCode = 0x1000;
	headers->OptionalHeader.ImageBase = 0x000140000000;
	headers->OptionalHeader.SectionAlignment = 0x1000;
	headers->OptionalHeader.FileAlignment = 0x200;
	headers->OptionalHeader.MajorOperatingSystemVersion = 6;
	headers->OptionalHeader.MajorSubsystemVersion = 6;
	headers->OptionalHeader.SizeOfImage = 0x3000;
	headers->OptionalHeader.SizeOfHeaders = 0x200;
	headers->OptionalHeader.Subsystem = IMAGE_SUBSYSTEM_WINDOWS_CUI;
	headers->OptionalHeader.DllCharacteristics = 
		IMAGE_DLLCHARACTERISTICS_TERMINAL_SERVER_AWARE | // avoid the overhead of loading and running DLL that contains compatibility code.
		IMAGE_DLLCHARACTERISTICS_NX_COMPAT | 			 // The image is compatible with data execution prevention (DEP).
		IMAGE_DLLCHARACTERISTICS_DYNAMIC_BASE |			 // The DLL can be relocated at load time.
		IMAGE_DLLCHARACTERISTICS_HIGH_ENTROPY_VA;		 // ASLR with 64 bit address space.
	headers->OptionalHeader.SizeOfStackReserve = 0x100000;
	headers->OptionalHeader.SizeOfStackCommit = 0x1000;
	headers->OptionalHeader.SizeOfHeapReserve = 0x100000;
	headers->OptionalHeader.SizeOfHeapCommit = 0x1000;
	headers->OptionalHeader.NumberOfRvaAndSizes = 2;
	buffer += sizeof(IMAGE_NT_HEADERS);

	// Image Section Headers
	IMAGE_SECTION_HEADER* codeSection = (IMAGE_SECTION_HEADER*)buffer;
	codeSection->Name[0] = '.';
	codeSection->Name[1] = 't';
	codeSection->Name[2] = 'e';
	codeSection->Name[3] = 'x';
	codeSection->Name[4] = 't';
	codeSection->VirtualAddress = 0x1000;
	codeSection->PointerToRawData = 0x200;
	codeSection->Characteristics = 0x60000020;
	buffer += sizeof(IMAGE_SECTION_HEADER);

	IMAGE_SECTION_HEADER* dataSection = (IMAGE_SECTION_HEADER*)buffer;
	dataSection->Name[0] = '.';
	dataSection->Name[1] = 'r';
	dataSection->Name[2] = 'd';
	dataSection->Name[3] = 'a';
	dataSection->Name[4] = 't';
	dataSection->Name[5] = 'a';
	dataSection->VirtualAddress = 0x2000;
	dataSection->SizeOfRawData = 0x200;
	dataSection->Characteristics = 0x40000040;

// End of Headers
//----------------

// Start of Code
//---------------
	buffer = base + 0x200;

	// emit instructions
	for (int32 i = 0; i < opCount; i++) {
		Operation* op = ops + i;
		op->address = (int32)GetRVA(buffer, base + 0x200, 0x1000);
		EmitOperation(&buffer, op);
	}

	// fill in relative local jumps
	for (int32 i = 0; i < opCount; i++) {
		Operation* op = ops+i;
		if (op->operands[0].type == Od_JumpTarget) {
			// NOTE: relative jumps encoded as start_of_target - end_of_origin
			*(op->offset) = ops[op->operands[0].index].address - ((op+1)->address);
		}
	}

	// fill in address of entry point
	headers->OptionalHeader.AddressOfEntryPoint = (DWORD)ops[entryOpIndex].address;

//----------------------------------------------------------------------------------------

	/*
	 * TODO: replace with something better
	 *       this is all terrible and I hate this
	 */

#define sizeofitem(array)					sizeof(*(array)->table)

#define ARRAY_RESERVE(array, new_capacity)	do{if ((array)->table == NULL || (array)->capacity<(new_capacity)){ \
											(array)->data = ArenaReAlloc((array)->arena, (array)->table, (array)->capacity*sizeofitem(array), (new_capacity)*sizeofitem(array)); \
											(array)->capacity = (new_capacity);}}while(0)

#define ARRAY_ADD(array, item)				do{if ((array)->capacity == (array)->count) {ARRAY_RESERVE(array, ((array)->count ? (array)->count * 2 : 4));} \
											(array)->table[(array)->count] = item; (array)->count++;}while(0)

	Arena arena = CreateArena();
	uint64* mem_addresses = (uint64*)OSAllocate(roCount * 8);
	SymbolArray importedArr = {&arena}; // TODO: one for each library
	for (int32 i = 0; i < opCount; i++) {
		Operation* op = ops+i;
		if (op->operands[0].type == Od_ImportedFunction) {

			ImportedFunction func = op->operands[0].func;
			// TODO: SymbolArray importedArr = GetSymbolArr(func->library);
			Array32* calls = NULL;

			for (int32 j = 0; j < importedArr.count; j++) {
				DllSymbol* imported = importedArr.table + j;
				if (StringEquals(imported->name, func.name)) {
					calls = &imported->calls;
					break;	
				}
			}

			if (!calls) {
				DllSymbol symbol = {};
				symbol.name = func.name;
				symbol.hint = (uint16)func.hint; //GetSymbolHint(func);
				symbol.calls.arena = &arena;
				ARRAY_ADD(&importedArr, symbol);
				calls = &(importedArr.table[importedArr.count - 1].calls);
			}

			ARRAY_ADD(calls, op->offset);
		}
	}

//----------------------------------------------------------------------------------------

	// emit jumps for each imported call and fill in relative calls
	for (int16 i = 0; i < importedArr.count; i++) {
		DllSymbol* imported = importedArr.table + i;
		for (int32 j = 0; j < imported->calls.count; j++) {
			int32* ref = imported->calls.table[j];
			*ref = (int32)(buffer - (byte*)ref - 4);
		}
		imported->jump = EmitIndirectJmp(&buffer);
	}

	// end of code, fill in virtual size of code section...
	int32 virtualSize = (int32)(buffer - (base + 0x200));
	codeSection->Misc.VirtualSize = virtualSize;
	// .. and code size
	int32 codeSize = ((virtualSize / 0x200) + 1)*0x200;
	codeSection->SizeOfRawData = codeSize;
	headers->OptionalHeader.SizeOfCode = codeSize;

// End of Code
//-------------------------

// Start of Read-Only Data
//-------------------------
	int32 dataSectionOffset = 0x200 + codeSize;
	dataSection->PointerToRawData = dataSectionOffset;
	buffer = base + dataSectionOffset;

	// IAT Directory
	headers->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IAT].VirtualAddress 
		= (DWORD)GetRVA(buffer, base + dataSectionOffset, 0x2000);
	for (int16 i = 0; i < importedArr.count; i++) {
		DllSymbol* import = importedArr.table + i;
		import->iat = (uint64*)buffer;
		*import->jump = (int32)(GetRVA(buffer, base + dataSectionOffset, 0x2000) - (GetRVA((byte*)import->jump, base + 0x200, 0x1000) + 4));
		buffer += 8;
	}
	Emit64(&buffer, 0);

	// end of IAT, fill in IAT size
	headers->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IAT].Size 
		= (importedArr.count + 1)*8;

	// emit read-only data, fill in absolute-address refrences
	for (int32 i = 0; i < roCount; i++) {
		uint64 va = GetVA(buffer, base + dataSectionOffset, 0x2000);
		mem_addresses[i] = va;
		EmitString(&buffer, rodata_values[i]);
	}

	for (int32 i = 0; i < opCount; i++) {
		Operation* op = ops + i;
		Operand operand = op->operands[1];
		if (operand.type == Od_StaticValue) {
			*(op->offset) = (int32)(mem_addresses[operand.index] - 0x000140000000 - (op + 1)->address);
		}
		else if (operand.type == Od_MemoryAddress) {
			*(op->mem_address) = mem_addresses[operand.index];
		}
	}

	// Image Import Descriptor
	IMAGE_IMPORT_DESCRIPTOR* import_desc = (IMAGE_IMPORT_DESCRIPTOR*)buffer;
	import_desc->FirstThunk = headers->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IAT].VirtualAddress;
	buffer += 2*sizeof(IMAGE_IMPORT_DESCRIPTOR);

	// Import Directory
	headers->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress 
		= (DWORD)GetRVA((byte*)import_desc, base + dataSectionOffset, 0x2000);
	headers->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].Size 
		= 2*sizeof(IMAGE_IMPORT_DESCRIPTOR);

	// fill in import descriptor first thunk
	import_desc->OriginalFirstThunk = (DWORD)GetRVA(buffer, base + dataSectionOffset, 0x2000);

	// skip and store offsets of ilt to be filled in
	for (int16 i = 0; i < importedArr.count; i++) {
		DllSymbol* import = importedArr.table + i;
		import->ilt = (uint64*)buffer;
		buffer += 8;
	}
	Emit64(&buffer, 0);

	// emit names and hints of imported function, fill in both ilt and iat with the relative address
	for (int16 i = 0; i < importedArr.count; i++) {
		DllSymbol* import = importedArr.table + i;
		*import->ilt = GetRVA(buffer, base + dataSectionOffset, 0x2000);
		*import->iat = *import->ilt;
		Emit16(&buffer, import->hint);
		EmitString(&buffer, import->name);
		Emit(&buffer, 0);
		if (import->name.length % 2) Emit(&buffer, 0);
	}

	// fill in relative address to library name
	import_desc->Name = (DWORD)GetRVA(buffer, base + dataSectionOffset, 0x2000);
	EmitString(&buffer, kernel);
	Emit(&buffer, 0);
	Emit(&buffer, 0);

	// fill in size of read-only data section
	dataSection->Misc.VirtualSize = (int32)(buffer - (base + dataSectionOffset));

// End of Read-Only Data
//-----------------------

	File file = OSCreateFile(filename);
	OSWriteFile(file, base, 0x400 + codeSize);
	OSCloseFile(file);

	DestroyArena(&arena);
	OSFree(base, 0x800);
}