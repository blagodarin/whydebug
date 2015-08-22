#pragma once

#include <cstdint>

#pragma pack(push, 1)

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//
using RVA = uint32_t;

//
using RVA64 = uint64_t;

//
struct MINIDUMP_LOCATION_DESCRIPTOR
{
	uint32_t DataSize; //
	RVA      Rva;      //
};

//
struct MINIDUMP_LOCATION_DESCRIPTOR64
{
	uint64_t DataSize; //
	RVA64    Rva;      //
};

//
struct MINIDUMP_MEMORY_DESCRIPTOR
{
	uint64_t                     StartOfMemoryRange; //
	MINIDUMP_LOCATION_DESCRIPTOR Memory;             //
};

//
struct MINIDUMP_MEMORY_DESCRIPTOR64
{
	uint64_t StartOfMemoryRange; //
	uint64_t DataSize;           //
};

// UTF-16 string header.
struct MINIDUMP_STRING
{
	uint32_t Length; // Size of the string in *bytes*, excluding 0-terminator.
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//
enum MINIDUMP_TYPE : uint32_t
{
	MiniDumpNormal                         = 0x00000000, //
	MiniDumpWithDataSegs                   = 0x00000001, //
	MiniDumpWithFullMemory                 = 0x00000002, //
	MiniDumpWithHandleData                 = 0x00000004, //
	MiniDumpFilterMemory                   = 0x00000008, //
	MiniDumpScanMemory                     = 0x00000010, //
	MiniDumpWithUnloadedModules            = 0x00000020, //
	MiniDumpWithIndirectlyReferencedMemory = 0x00000040, //
	MiniDumpFilterModulePaths              = 0x00000080, //
	MiniDumpWithProcessThreadData          = 0x00000100, //
	MiniDumpWithPrivateReadWriteMemory     = 0x00000200, //
	MiniDumpWithoutOptionalData            = 0x00000400, //
	MiniDumpWithFullMemoryInfo             = 0x00000800, //
	MiniDumpWithThreadInfo                 = 0x00001000, //
	MiniDumpWithCodeSegs                   = 0x00002000, //
	MiniDumpWithoutAuxiliaryState          = 0x00004000, //
	MiniDumpWithFullAuxiliaryState         = 0x00008000, //
	MiniDumpWithPrivateWriteCopyMemory     = 0x00010000, //
	MiniDumpIgnoreInaccessibleMemory       = 0x00020000, //
	MiniDumpWithTokenInformation           = 0x00040000, //
	MiniDumpWithModuleHeaders              = 0x00080000, //
	MiniDumpFilterTriage                   = 0x00100000, //
	MiniDumpValidTypeFlags                 = 0x001fffff, //
};

//
struct MINIDUMP_HEADER
{
	uint32_t Signature;          //
	uint32_t Version;            // & 0xffff == 0xa793.
	uint32_t NumberOfStreams;    //
	RVA      StreamDirectoryRva; //
	uint32_t CheckSum;           //
	uint32_t TimeDateStamp;      //
	uint64_t Flags;              //

	static constexpr uint32_t SIGNATURE    = 0x504d444d; // "MDMP".
	static constexpr uint32_t VERSION_MASK = 0x0000ffff; //
	static constexpr uint32_t VERSION      = 0x0000a793; //
};

//
enum MINIDUMP_STREAM_TYPE : uint32_t
{
	UnusedStream              = 0,      // Reserved.
	ReservedStream0           = 1,      // Reserved.
	ReservedStream1           = 2,      // Reserved.
	ThreadListStream          = 3,      // Thread information (MINIDUMP_THREAD_LIST).
	ModuleListStream          = 4,      // Module information (MINIDUMP_MODULE_LIST).
	MemoryListStream          = 5,      // Memory allocation information (MINIDUMP_MEMORY_LIST).
	ExceptionStream           = 6,      // Exception information (MINIDUMP_EXCEPTION_STREAM).
	SystemInfoStream          = 7,      // General system information (MINIDUMP_SYSTEM_INFO).
	ThreadExListStream        = 8,      // Extended thread information (MINIDUMP_THREAD_EX_LIST).
	Memory64ListStream        = 9,      // Memory allocation information (MINIDUMP_MEMORY64_LIST).
	CommentStreamA            = 10,     // ANSI string used for documentation purposes.
	CommentStreamW            = 11,     // Unicode string used for documentation purposes.
	HandleDataStream          = 12,     // High-level information about the active operating system handles (MINIDUMP_HANDLE_DATA_STREAM).
	FunctionTableStream       = 13,     // Function table information (MINIDUMP_FUNCTION_TABLE_STREAM).
	UnloadedModuleListStream  = 14,     // Module information for the unloaded modules (MINIDUMP_UNLOADED_MODULE_LIST).
	MiscInfoStream            = 15,     // Miscellaneous information (MINIDUMP_MISC_INFO or MINIDUMP_MISC_INFO_2).
	MemoryInfoListStream      = 16,     // Memory region description information (MINIDUMP_MEMORY_INFO_LIST).
	ThreadInfoListStream      = 17,     // Thread state information (MINIDUMP_THREAD_INFO_LIST).
	HandleOperationListStream = 18,     // Operation list information (MINIDUMP_HANDLE_OPERATION_LIST).
	LastReservedStream        = 0xffff, // Any value greater than this value will not be used by the system (MINIDUMP_USER_STREAM).
};

//
struct MINIDUMP_DIRECTORY
{
	MINIDUMP_STREAM_TYPE         StreamType; //
	MINIDUMP_LOCATION_DESCRIPTOR Location;   //
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//
struct MINIDUMP_THREAD
{
	uint32_t                     ThreadId;      //
	uint32_t                     SuspendCount;  //
	uint32_t                     PriorityClass; //
	uint32_t                     Priority;      //
	uint64_t                     Teb;           //
	MINIDUMP_MEMORY_DESCRIPTOR   Stack;         //
	MINIDUMP_LOCATION_DESCRIPTOR ThreadContext; //
};

//
struct MINIDUMP_THREAD_LIST
{
	uint32_t NumberOfThreads; //
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//
enum : uint32_t
{
	VS_FF_DEBUG        = 0x00000001, //
	VS_FF_PRERELEASE   = 0x00000002, //
	VS_FF_PATCHED      = 0x00000004, //
	VS_FF_PRIVATEBUILD = 0x00000008, //
	VS_FF_INFOINFERRED = 0x00000010, //
	VS_FF_SPECIALBUILD = 0x00000020, //
};

// The operating system for which the file was designed.
enum : uint32_t
{
	VOS_UNKNOWN       = 0x00000000, // Unknown.
	VOS__WINDOWS16    = 0x00000001, // 16-bit Windows.
	VOS__PM16         = 0x00000002, // 16-bit Presentation Manager.
	VOS__PM32         = 0x00000003, // 32-bit Presentation Manager.
	VOS__WINDOWS32    = 0x00000004, // 32-bit Windows.
	VOS_DOS           = 0x00010000, // MS-DOS.
	VOS_DOS_WINDOWS16 = 0x00010001, // 16-bit Windows running on MS-DOS.
	VOS_DOS_WINDOWS32 = 0x00010004, // 32-bit Windows running on MS-DOS.
	VOS_OS216         = 0x00020000, // 16-bit OS/2.
	VOS_OS216_PM16    = 0x00020002, // 16-bit Presentation Manager running on 16-bit OS/2.
	VOS_OS232         = 0x00030000, // 32-bit OS/2.
	VOS_OS232_PM32    = 0x00030003, // 32-bit Presentation Manager running on 32-bit OS/2.
	VOS_NT            = 0x00040000, // Windows NT.
	VOS_NT_WINDOWS32  = 0x00040004, // Windows NT.
};

//
enum : uint32_t
{
	VFT_UNKNOWN    = 0x00000000, // Unknown.
	VFT_APP        = 0x00000001, // Application.
	VFT_DLL        = 0x00000002, // DLL.
	VFT_DRV        = 0x00000003, // Device driver.
	VFT_FONT       = 0x00000004, // Font.
	VFT_VXD        = 0x00000005, // Virtual device.
	VFT_STATIC_LIB = 0x00000007, // Static-link library.
};

//
enum : uint32_t
{
	VFT2_UNKNOWN               = 0x00000000, // Unknown.

	VFT2_DRV_PRINTER           = 0x00000001, //
	VFT2_DRV_KEYBOARD          = 0x00000002, //
	VFT2_DRV_LANGUAGE          = 0x00000003, //
	VFT2_DRV_DISPLAY           = 0x00000004, //
	VFT2_DRV_MOUSE             = 0x00000005, //
	VFT2_DRV_NETWORK           = 0x00000006, //
	VFT2_DRV_SYSTEM            = 0x00000007, //
	VFT2_DRV_INSTALLABLE       = 0x00000008, //
	VFT2_DRV_SOUND             = 0x00000009, //
	VFT2_DRV_COMM              = 0x0000000A, //
	VFT2_DRV_VERSIONED_PRINTER = 0x0000000C, //

	VFT2_FONT_RASTER           = 0x00000001, //
	VFT2_FONT_VECTOR           = 0x00000002, //
	VFT2_FONT_TRUETYPE         = 0x00000003, //
};

//
struct VS_FIXEDFILEINFO
{
	uint32_t dwSignature;        // == 0xfeef04bd.
	uint32_t dwStrucVersion;     //
	uint32_t dwFileVersionMS;    //
	uint32_t dwFileVersionLS;    //
	uint32_t dwProductVersionMS; //
	uint32_t dwProductVersionLS; //
	uint32_t dwFileFlagsMask;    //
	uint32_t dwFileFlags;        // VS_FF_*.
	uint32_t dwFileOS;           // VOS_*.
	uint32_t dwFileType;         // VFT_*.
	uint32_t dwFileSubtype;      // VFT2_*.
	uint32_t dwFileDateMS;       //
	uint32_t dwFileDateLS;       //
};

//
struct CodeViewRecordPDB70
{
	uint32_t signature;    //
	uint8_t  pdb_guid[16]; //
	uint32_t pdb_age;      //
	char     pdb_name[];   //

	static constexpr uint32_t MinSize = 24;
	static constexpr uint32_t Signature = 0x53445352; // "RSDS".
};

//
struct MINIDUMP_MODULE
{
	uint64_t                     BaseOfImage;   //
	uint32_t                     SizeOfImage;   //
	uint32_t                     CheckSum;      //
	uint32_t                     TimeDateStamp; //
	RVA                          ModuleNameRva; //
	VS_FIXEDFILEINFO             VersionInfo;   //
	MINIDUMP_LOCATION_DESCRIPTOR CvRecord;      //
	MINIDUMP_LOCATION_DESCRIPTOR MiscRecord;    //
	uint32_t                     Reserved[4];   //
};

//
struct MINIDUMP_MODULE_LIST
{
	uint32_t NumberOfModules; //
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//
struct MINIDUMP_MEMORY_LIST
{
	uint32_t                   NumberOfMemoryRanges; //
	MINIDUMP_MEMORY_DESCRIPTOR MemoryRanges[];       //
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

enum
{
	EXCEPTION_MAXIMUM_PARAMETERS = 15, //
};

//
struct MINIDUMP_EXCEPTION
{
	uint32_t ExceptionCode;
	uint32_t ExceptionFlags;
	uint64_t ExceptionRecord;
	uint64_t ExceptionAddress;
	uint32_t NumberParameters;
	uint32_t __unusedAlignment;
	uint64_t ExceptionInformation[EXCEPTION_MAXIMUM_PARAMETERS];
};

//
struct MINIDUMP_EXCEPTION_STREAM
{
	uint32_t                     ThreadId;        //
	uint32_t                     __alignment;     //
	MINIDUMP_EXCEPTION           ExceptionRecord; //
	MINIDUMP_LOCATION_DESCRIPTOR ThreadContext;   //
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//
enum : uint16_t
{
	PROCESSOR_ARCHITECTURE_INTEL   = 0,
	PROCESSOR_ARCHITECTURE_ARM     = 5,
	PROCESSOR_ARCHITECTURE_IA64    = 6,
	PROCESSOR_ARCHITECTURE_AMD64   = 9,
	PROCESSOR_ARCHITECTURE_UNKNOWN = 0xffff,
};

//
struct MINIDUMP_SYSTEM_INFO
{
	uint16_t ProcessorArchitecture;
	uint16_t ProcessorLevel;
	uint16_t ProcessorRevision;
	union
	{
		uint16_t Reserved0;
		struct
		{
			uint8_t NumberOfProcessors;
			uint8_t ProductType;
		};
	};
	uint32_t MajorVersion;
	uint32_t MinorVersion;
	uint32_t BuildNumber;
	uint32_t PlatformId;
	RVA      CSDVersionRva;
	union
	{
		uint32_t Reserved1;
		struct
		{
			uint16_t SuiteMask;
			uint16_t Reserved2;
		};
	};
	union
	{
		struct
		{
			uint32_t VendorId[3];
			uint32_t VersionInformation;
			uint32_t FeatureInformation;
			uint32_t AMDExtendedCpuFeatures;
		} X86CpuInfo;
		struct
		{
			uint64_t ProcessorFeatures[2];
		} OtherCpuInfo;
	} Cpu;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//
struct MINIDUMP_THREAD_EX
{
	uint32_t                     ThreadId;      //
	uint32_t                     SuspendCount;  //
	uint32_t                     PriorityClass; //
	uint32_t                     Priority;      //
	uint64_t                     Teb;           //
	MINIDUMP_MEMORY_DESCRIPTOR   Stack;         //
	MINIDUMP_LOCATION_DESCRIPTOR ThreadContext; //
	MINIDUMP_MEMORY_DESCRIPTOR   BackingStore;  //
};

//
struct MINIDUMP_THREAD_EX_LIST
{
	uint32_t           NumberOfThreads; //
	MINIDUMP_THREAD_EX Threads[];       //
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//
struct MINIDUMP_MEMORY64_LIST
{
	uint64_t                     NumberOfMemoryRanges; //
	RVA64                        BaseRva;              //
	MINIDUMP_MEMORY_DESCRIPTOR64 MemoryRanges[];       //
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//
struct MINIDUMP_HANDLE_DESCRIPTOR
{
	uint64_t Handle;        //
	RVA      TypeNameRva;   //
	RVA      ObjectNameRva; //
	uint32_t Attributes;    //
	uint32_t GrantedAccess; //
	uint32_t HandleCount;   //
	uint32_t PointerCount;  //
};

//
enum MINIDUMP_HANDLE_OBJECT_INFORMATION_TYPE : uint32_t
{
	MiniHandleObjectInformationNone, //
	MiniThreadInformation1,          //
	MiniMutantInformation1,          //
	MiniMutantInformation2,          //
	MiniProcessInformation1,         //
	MiniProcessInformation2,         //
};

//
struct MINIDUMP_HANDLE_OBJECT_INFORMATION
{
	RVA                                     NextInfoRva; // 0 if there are no more elements in the list.
	MINIDUMP_HANDLE_OBJECT_INFORMATION_TYPE InfoType;    //
	uint32_t                                SizeOfInfo;  //
};

//
struct MINIDUMP_HANDLE_DESCRIPTOR_2
{
	uint64_t Handle;        //
	RVA      TypeNameRva;   //
	RVA      ObjectNameRva; //
	uint32_t Attributes;    //
	uint32_t GrantedAccess; //
	uint32_t HandleCount;   //
	uint32_t PointerCount;  //
	RVA      ObjectInfoRva; // 0 if there is no extra information.
	uint32_t Reserved0;     //
};

//
struct MINIDUMP_HANDLE_DATA_STREAM
{
	uint32_t SizeOfHeader;        //
	uint32_t SizeOfDescriptor;    //
	uint32_t NumberOfDescriptors; //
	uint32_t Reserved;            //
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//
struct MINIDUMP_FUNCTION_TABLE_DESCRIPTOR
{
	uint64_t MinimumAddress; //
	uint64_t MaximumAddress; //
	uint64_t BaseAddress;    //
	uint32_t EntryCount;     //
	uint32_t SizeOfAlignPad; //
};

//
struct MINIDUMP_FUNCTION_TABLE_STREAM
{
	uint32_t SizeOfHeader;           //
	uint32_t SizeOfDescriptor;       //
	uint32_t SizeOfNativeDescriptor; //
	uint32_t SizeOfFunctionEntry;    //
	uint32_t NumberOfDescriptors;    //
	uint32_t SizeOfAlignPad;         //
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//
struct MINIDUMP_UNLOADED_MODULE
{
	uint64_t BaseOfImage;   //
	uint32_t SizeOfImage;   //
	uint32_t CheckSum;      //
	uint32_t TimeDateStamp; //
	RVA      ModuleNameRva; //
};

//
struct MINIDUMP_UNLOADED_MODULE_LIST
{
	uint32_t SizeOfHeader;    //
	uint32_t SizeOfEntry;     //
	uint32_t NumberOfEntries; //
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//
enum : uint32_t
{
	MINIDUMP_MISC1_PROCESS_ID    = 0x00000001, //
	MINIDUMP_MISC1_PROCESS_TIMES = 0x00000002, //
};

//
struct MINIDUMP_MISC_INFO
{
	uint32_t SizeOfInfo;        //
	uint32_t Flags1;            //
	uint32_t ProcessId;         //
	uint32_t ProcessCreateTime; //
	uint32_t ProcessUserTime;   //
	uint32_t ProcessKernelTime; //
};

//
struct MINIDUMP_MISC_INFO_2
{
	uint32_t SizeOfInfo;                //
	uint32_t Flags1;                    //
	uint32_t ProcessId;                 //
	uint32_t ProcessCreateTime;         //
	uint32_t ProcessUserTime;           //
	uint32_t ProcessKernelTime;         //
	uint32_t ProcessorMaxMhz;           //
	uint32_t ProcessorCurrentMhz;       //
	uint32_t ProcessorMhzLimit;         //
	uint32_t ProcessorMaxIdleState;     //
	uint32_t ProcessorCurrentIdleState; //
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//
enum : uint32_t
{
	MEM_COMMIT  = 0x1000,  //
	MEM_RESERVE = 0x2000,  //
	MEM_FREE    = 0x10000, //
};

//
enum : uint32_t
{
	MEM_PRIVATE = 0x20000,   //
	MEM_MAPPED  = 0x40000,   //
	MEM_IMAGE   = 0x1000000, //
};

//
struct MINIDUMP_MEMORY_INFO
{
	uint64_t BaseAddress;       //
	uint64_t AllocationBase;    //
	uint32_t AllocationProtect; //
	uint32_t __alignment1;      //
	uint64_t RegionSize;        //
	uint32_t State;             //
	uint32_t Protect;           //
	uint32_t Type;              //
	uint32_t __alignment2;      //
};

//
struct MINIDUMP_MEMORY_INFO_LIST
{
	uint32_t SizeOfHeader;    //
	uint32_t SizeOfEntry;     //
	uint64_t NumberOfEntries; //
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//
enum : uint32_t
{
	MINIDUMP_THREAD_INFO_ERROR_THREAD    = 0x00000001, // A placeholder thread due to an error accessing the thread. No thread information exists beyond the thread identifier.
	MINIDUMP_THREAD_INFO_EXITED_THREAD   = 0x00000004, // The thread has exited (not running any code) at the time of the dump.
	MINIDUMP_THREAD_INFO_INVALID_CONTEXT = 0x00000010, // Thread context could not be retrieved.
	MINIDUMP_THREAD_INFO_INVALID_INFO    = 0x00000008, // Thread information could not be retrieved.
	MINIDUMP_THREAD_INFO_INVALID_TEB     = 0x00000020, // TEB information could not be retrieved.
	MINIDUMP_THREAD_INFO_WRITING_THREAD  = 0x00000002, // This is the thread that called MiniDumpWriteDump.
};

//
struct MINIDUMP_THREAD_INFO
{
	uint32_t ThreadId;     //
	uint32_t DumpFlags;    //
	uint32_t DumpError;    // HRESULT indicating the dump status.
	uint32_t ExitStatus;   // Thread exit code.
	uint64_t CreateTime;   // Thread creation time in 100 ns intervals since January 1, 1601 (UTC).
	uint64_t ExitTime;     // Thread exit time in 100 ns intervals since January 1, 1601 (UTC).
	uint64_t KernelTime;   // Time spent in kernel mode in 100 ns intervals.
	uint64_t UserTime;     // Time spent in user mode in 100 ns intervals.
	uint64_t StartAddress; //
	uint64_t Affinity;     //
};

//
struct MINIDUMP_THREAD_INFO_LIST
{
	uint32_t SizeOfHeader;    //
	uint32_t SizeOfEntry;     //
	uint32_t NumberOfEntries; //
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Handle operation type.
enum eHANDLE_TRACE_OPERATIONS : uint32_t
{
	OperationDbUnused, //
	OperationDbOPEN,   // Open (create) handle operation.
	OperationDbCLOSE,  // Close handle operation.
	OperationDbBADREF, // Invalid handle operation.
};

enum
{
	AVRF_MAX_TRACES = 32, //
};

//
struct AVRF_BACKTRACE_INFORMATION
{
	uint32_t Depth;                            // Number of traces collected.
	uint32_t Index;                            //
	uint64_t ReturnAddresses[AVRF_MAX_TRACES]; //
};

//
struct AVRF_HANDLE_OPERATION
{
	uint64_t                   Handle;               //
	uint32_t                   ProcessId;            //
	uint32_t                   ThreadId;             //
	eHANDLE_TRACE_OPERATIONS   OperationType;        //
	uint32_t                   Spare0;               //
	AVRF_BACKTRACE_INFORMATION BackTraceInformation; //
};

//
struct MINIDUMP_HANDLE_OPERATION_LIST
{
	uint32_t SizeOfHeader;    //
	uint32_t SizeOfEntry;     //
	uint32_t NumberOfEntries; //
	uint32_t Reserved;        //
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//
struct MINIDUMP_USER_STREAM
{
	uint32_t Type;       //
	uint32_t BufferSize; //
	uint8_t  Buffer[];   //
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// CONTEXT (x86)
struct ContextX86
{
	enum : uint32_t
	{
		I386              = 0x00010000, // CONTEXT_I386
		Control           = 0x00000001, // CONTEXT_CONTROL
		Integer           = 0x00000002, // CONTEXT_INTEGER
		Segments          = 0x00000004, // CONTEXT_SEGMENTS
		FloatingPoint     = 0x00000008, // CONTEXT_FLOATING_POINT
		DebugRegisters    = 0x00000010, // CONTEXT_DEBUG_REGISTERS
		ExtendedRegisters = 0x00000020, // CONTEXT_EXTENDED_REGISTERS
	} flags;

	// CONTEXT_DEBUG_REGISTERS
	uint32_t dr0;
	uint32_t dr1;
	uint32_t dr2;
	uint32_t dr3;
	uint32_t dr6;
	uint32_t dr7;

	// CONTEXT_FLOATING_POINT
	struct
	{
		uint32_t control_word;
		uint32_t status_word;
		uint32_t tag_word;
		uint32_t error_offset;
		uint32_t error_selector;
		uint32_t data_offset;
		uint32_t data_selector;
		uint8_t  register_area[80];
		uint32_t cr0_npx_state;
	} float_save;

	// CONTEXT_SEGMENTS
	uint32_t gs;
	uint32_t fs;
	uint32_t es;
	uint32_t ds;

	// CONTEXT_INTEGER
	uint32_t edi;
	uint32_t esi;
	uint32_t ebx;
	uint32_t edx;
	uint32_t ecx;
	uint32_t eax;

	// CONTEXT_CONTROL
	uint32_t ebp;
	uint32_t eip;
	uint32_t cs; // "Must be sanitized", whatever that means.
	uint32_t eflags; // "Must be sanitized", whatever that means.
	uint32_t esp;
	uint32_t ss;

	// CONTEXT_EXTENDED_REGISTERS
	uint8_t extended_registers[512];
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma pack(pop)
