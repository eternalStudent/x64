#include <winnt.h>

struct CallNode {
	int32* value;

	CallNode* prev;
	CallNode* next;
};

struct DllSymbolNode {
	String name;
	uint16 hint;
	struct {
		CallNode* first;
		CallNode* last;
	} calls;

	int32* jump;
	uint64* iat;
	uint64* ilt;

	DllSymbolNode* prev;
	DllSymbolNode* next;
};

struct SymbolList {
	DllSymbolNode* first;
	DllSymbolNode* last;
	int32 count;
};

uint64 GetRVA(byte* offset, byte* baseOffset, uint64 rvBaseOffset) {
	return offset - baseOffset + rvBaseOffset;
}

uint64 GetVA(byte* offset, byte* baseOffset, uint64 rvBaseOffset) {
	return GetRVA(offset, baseOffset, rvBaseOffset) + 0x000140000000;
}

bool Link(Operation* ops, int32 opCount, 
		  int32 entryOpIndex, 
		  String* rodata_values, int32 roCount, 
		  String* rwdata_values, int32 rwCount, 
		  String kernel,                        // TODO: replace with list of dlls
		  LPCWSTR filename) {

	bool success = true;
	BigBuffer buffer = CreateBigBuffer(KB(4));

// define variables on the stack so I can jump over them.
	File file;
	IMAGE_IMPORT_DESCRIPTOR* import_desc;
	uint64* mem_addresses;
	int32 sectionOffset, rawSize, virtualSize;
	Arena arena;
	SymbolList importedList; // TODO: one for each library

// Headers
//----------

	// DOS Header
	IMAGE_DOS_HEADER* DOSHeader = (IMAGE_DOS_HEADER*)buffer.pos;
	DOSHeader->e_magic = IMAGE_DOS_SIGNATURE;
	DOSHeader->e_lfanew = sizeof(IMAGE_DOS_HEADER);
	buffer.pos += sizeof(IMAGE_DOS_HEADER);

	// Image NT Header
	IMAGE_NT_HEADERS* headers = (IMAGE_NT_HEADERS*)buffer.pos;
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
	headers->OptionalHeader.NumberOfRvaAndSizes = IMAGE_NUMBEROF_DIRECTORY_ENTRIES;
	buffer.pos += sizeof(IMAGE_NT_HEADERS);

	// Image Section Headers
	IMAGE_SECTION_HEADER* codeSection = (IMAGE_SECTION_HEADER*)buffer.pos;
	codeSection->Name[0] = '.';
	codeSection->Name[1] = 't';
	codeSection->Name[2] = 'e';
	codeSection->Name[3] = 'x';
	codeSection->Name[4] = 't';
	codeSection->VirtualAddress = 0x1000;
	codeSection->Characteristics =     // 0x60000020
		IMAGE_SCN_MEM_READ |           // 0x40000000
		IMAGE_SCN_MEM_EXECUTE |        // 0x20000000
		IMAGE_SCN_CNT_CODE;            // 0x00000020
	buffer.pos += sizeof(IMAGE_SECTION_HEADER);

	IMAGE_SECTION_HEADER* rdataSection = (IMAGE_SECTION_HEADER*)buffer.pos;
	rdataSection->Name[0] = '.';
	rdataSection->Name[1] = 'r';
	rdataSection->Name[2] = 'd';
	rdataSection->Name[3] = 'a';
	rdataSection->Name[4] = 't';
	rdataSection->Name[5] = 'a';
	rdataSection->VirtualAddress = 0x2000;
	rdataSection->Characteristics =     // 0x40000040
		IMAGE_SCN_MEM_READ |            // 0x40000000
		IMAGE_SCN_CNT_INITIALIZED_DATA; // 0x00000040;
	buffer.pos += sizeof(IMAGE_SECTION_HEADER);

	IMAGE_SECTION_HEADER* dataSection = NULL;
	if (rwCount) {
		dataSection = (IMAGE_SECTION_HEADER*)buffer.pos;
		dataSection->Name[0] = '.';
		dataSection->Name[1] = 'd';
		dataSection->Name[2] = 'a';
		dataSection->Name[3] = 't';
		dataSection->Name[4] = 'a';
		dataSection->VirtualAddress = 0x3000; 
		dataSection->Characteristics =      // 0xC0000040
			IMAGE_SCN_MEM_READ |            // 0x40000000
			IMAGE_SCN_MEM_WRITE |           // 0x80000000
			IMAGE_SCN_CNT_INITIALIZED_DATA; // 0x00000040;

		headers->FileHeader.NumberOfSections++;
		headers->OptionalHeader.SizeOfImage += 0x1000;
	}

// End of Headers
//----------------

// Start of Code
//---------------
	sectionOffset = 0x200;
	codeSection->PointerToRawData = sectionOffset;
	buffer.pos = buffer.start + sectionOffset;

	// emit instructions
	for (int32 i = 0; i < opCount; i++) {
		Operation* op = ops + i;
		op->address = (int32)GetRVA(buffer.pos, buffer.start + 0x200, 0x1000);
		if (!EmitOperation(&buffer, op)) {
			success = false;
			goto clean;
		}
	}

	// fill in relative local jumps
	for (int32 i = 0; i < opCount; i++) {
		Operation* op = ops+i;
		if (op->operands[0].type == Od_JumpTarget) {
			// NOTE: relative jumps encoded as start_of_target - end_of_origin
			*(op->offset) = ops[op->operands[0].instruction.index].address - ((op+1)->address);
		}
	}

	// fill in address of entry point
	headers->OptionalHeader.AddressOfEntryPoint = (DWORD)ops[entryOpIndex].address;

	arena = CreateArena();
	importedList = {};
	for (int32 i = 0; i < opCount; i++) {
		Operation* op = ops+i;
		if (op->operands[0].type == Od_ImportedFunction) {

			ImportedFunction func = op->operands[0].func;
			// TODO: SymbolList importedList = GetSymbolList(func->library);
			DllSymbolNode* symbol = NULL;

			LINKEDLIST_FOREACH(&importedList, DllSymbolNode, imported) {
				if (StringEquals(imported->name, func.name)) {
					symbol = imported;
					break;	
				}
			}

			if (!symbol) {
				symbol = (DllSymbolNode*)ArenaAlloc(&arena, sizeof(DllSymbolNode));
				symbol->name = func.name;
				symbol->hint = (uint16)func.hint; //GetSymbolHint(func);
				LINKEDLIST_ADD(&importedList, symbol);
				importedList.count++;
			}

			CallNode* call = (CallNode*)ArenaAlloc(&arena, sizeof(CallNode));
			call->value = op->offset;
			LINKEDLIST_ADD(&symbol->calls, call);
		}
	}

	// emit jumps for each imported call and fill in relative calls
	LINKEDLIST_FOREACH(&importedList, DllSymbolNode, imported) {
		LINKEDLIST_FOREACH(&imported->calls, CallNode, call) {
			int32* ref = call->value;;
			*ref = (int32)(buffer.pos - (byte*)ref - 4);
		}
		imported->jump = EmitIndirectJmp(&buffer);
	}

	// end of code, fill in virtual size of code section...
	virtualSize = (int32)(buffer.pos - (buffer.start + 0x200));
	codeSection->Misc.VirtualSize = virtualSize;
	// .. and code size
	rawSize = RoundUpPowerOf2(virtualSize, 0x200);
	codeSection->SizeOfRawData = rawSize;
	headers->OptionalHeader.SizeOfCode = rawSize;

// End of Code
//-------------------------

// Start of Read-Only Data
//-------------------------
	sectionOffset += rawSize;
	rdataSection->PointerToRawData = sectionOffset;
	buffer.pos = buffer.start + sectionOffset;

	// IAT Directory
	headers->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IAT].VirtualAddress 
		= (DWORD)GetRVA(buffer.pos, buffer.start + sectionOffset, 0x2000);

	LINKEDLIST_FOREACH(&importedList, DllSymbolNode, import) {
		import->iat = (uint64*)buffer.pos;
		*import->jump = (int32)(GetRVA(buffer.pos, buffer.start + sectionOffset, 0x2000) - (GetRVA((byte*)import->jump, buffer.start + 0x200, 0x1000) + 4));
		buffer.pos += 8;
	}
	Emit64(&buffer, 0);

	// end of IAT, fill in IAT size
	headers->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IAT].Size 
		= (importedList.count + 1)*8;

	// emit read-only data, fill in absolute-address refrences
	mem_addresses = (uint64*)ArenaAlloc(&arena, roCount * 8);
	for (int32 i = 0; i < roCount; i++) {
		uint64 va = GetVA(buffer.pos, buffer.start + sectionOffset, 0x2000);
		mem_addresses[i] = va;
		BigBufferWrite(&buffer, rodata_values[i]);
	}

	for (int32 i = 0; i < opCount; i++) {
		Operation* op = ops + i;
		Operand operand = op->operands[1];
		if (operand.type == Od_StaticValue && operand.static_value.segment == 0) {
			*(op->offset) = (int32)(mem_addresses[operand.static_value.id] - 0x000140000000 - (op + 1)->address);
		}
		else if (operand.type == Od_MemoryAddress && operand.mem_addr.segment == 0) {
			*(op->mem_address) = mem_addresses[operand.mem_addr.id];
		}
	}

	// Image Import Descriptor
	import_desc = (IMAGE_IMPORT_DESCRIPTOR*)buffer.pos;
	import_desc->FirstThunk = headers->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IAT].VirtualAddress;
	buffer.pos += 2*sizeof(IMAGE_IMPORT_DESCRIPTOR);

	// Import Directory
	headers->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress 
		= (DWORD)GetRVA((byte*)import_desc, buffer.start + sectionOffset, 0x2000);
	headers->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].Size 
		= 2*sizeof(IMAGE_IMPORT_DESCRIPTOR);

	// fill in import descriptor first thunk
	import_desc->OriginalFirstThunk = (DWORD)GetRVA(buffer.pos, buffer.start + sectionOffset, 0x2000);

	// skip and store offsets of ilt to be filled in
	LINKEDLIST_FOREACH(&importedList, DllSymbolNode, import) {
		import->ilt = (uint64*)buffer.pos;
		buffer.pos += 8;
	}
	Emit64(&buffer, 0);

	// emit names and hints of imported function, fill in both ilt and iat with the relative address
	LINKEDLIST_FOREACH(&importedList, DllSymbolNode, import) {
		*import->ilt = GetRVA(buffer.pos, buffer.start + sectionOffset, 0x2000);
		*import->iat = *import->ilt;
		Emit16(&buffer, import->hint);
		BigBufferWrite(&buffer, import->name);
		Emit(&buffer, 0);
		if (import->name.length % 2) Emit(&buffer, 0);
	}

	// fill in relative address to library name
	import_desc->Name = (DWORD)GetRVA(buffer.pos, buffer.start + sectionOffset, 0x2000);
	BigBufferWrite(&buffer, kernel);
	Emit(&buffer, 0);
	Emit(&buffer, 0);

	// fill in size of read-only data section
	virtualSize = (int32)(buffer.pos - (buffer.start + sectionOffset));
	rdataSection->Misc.VirtualSize = virtualSize;
	rawSize = RoundUpPowerOf2(virtualSize, 0x200);
	rdataSection->SizeOfRawData = rawSize;

// End of Read-Only Data
//-----------------------

// Start of Read/Write Data
//-------------------------

	if (rwCount) {
		sectionOffset += rawSize;
		dataSection->PointerToRawData = sectionOffset;
		buffer.pos = buffer.start + sectionOffset;

		mem_addresses = (uint64*)ArenaAlloc(&arena, rwCount * 8);
		for (int32 i = 0; i < rwCount; i++) {
			uint64 va = GetVA(buffer.pos, buffer.start + sectionOffset, 0x3000);
			mem_addresses[i] = va;
			BigBufferWrite(&buffer, rwdata_values[i]);
		}

		for (int32 i = 0; i < opCount; i++) {
			Operation* op = ops + i;
			Operand operand = op->operands[1];
			if (operand.type == Od_StaticValue && operand.static_value.segment == 1) {
				*(op->offset) = (int32)(mem_addresses[operand.static_value.id] - 0x000140000000 - (op + 1)->address);
			}
			else if (operand.type == Od_MemoryAddress && operand.mem_addr.segment == 1) {
				*(op->mem_address) = mem_addresses[operand.mem_addr.id];
			}
		}

		virtualSize = (int32)(buffer.pos - (buffer.start + sectionOffset));
		dataSection->Misc.VirtualSize = virtualSize;
		rawSize = RoundUpPowerOf2(virtualSize, 0x200);
		dataSection->SizeOfRawData = rawSize;
	}

// End of Read/Write Data
//-------------------------

	sectionOffset += rawSize;

	file = OSCreateFile(filename);
	if (file == FILE_ERROR) {
		success = false;
		goto clean;
	}

	OSWriteFile(file, buffer.start, sectionOffset);
	OSCloseFile(file);

clean:
	DestroyArena(&arena);
	DestroyBigBuffer(&buffer);

	return success;
}