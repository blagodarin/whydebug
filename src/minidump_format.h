#pragma once

#include <cstdint>

#pragma pack(push, 1)

namespace minidump
{
	////////////////////////////////////////////////////////////
	//
	// Common structures.
	//
	////////////////////////////////////////////////////////////

	// (MINIDUMP_LOCATION_DESCRIPTOR).
	struct Location
	{
		uint32_t size;   //
		uint32_t offset; //
	};

	// (MINIDUMP_MEMORY_DESCRIPTOR).
	struct MemoryRange
	{
		uint64_t base;     //
		Location location; //
	};

	// UTF-16 string header (MINIDUMP_STRING).
	struct StringHeader
	{
		uint32_t size; // Size of the string in *bytes*, excluding null-terminator.
	};

	////////////////////////////////////////////////////////////
	//
	//
	//
	////////////////////////////////////////////////////////////

	// File header (MINIDUMP_HEADER).
	struct Header
	{
		uint32_t signature;               //
		uint16_t version;                 //
		uint16_t implementation_specific; // Officially documented as part of 32-bit version value.
		uint32_t stream_count;            //
		uint32_t stream_list_offset;      //
		uint32_t checksum;                // Usually zero.
		uint32_t timestamp;               // 32-bit time_t value.
		uint64_t flags;                   // Mask of MINIDUMP_TYPE values, unrelated to actual contents.

		static constexpr uint32_t Signature = 0x504d444d; // "MDMP" (MINIDUMP_SIGNATURE).
		static constexpr uint16_t Version = 0xa793; // (MINIDUMP_VERSION).
	};

	// Stream list entry (MINIDUMP_DIRECTORY).
	struct Stream
	{
		enum class Type : uint32_t
		{
			Unused              = 0,      // Reserved.
			Reserved0           = 1,      // Reserved.
			Reserved1           = 2,      // Reserved.
			ThreadList          = 3,      // Thread information (ThreadListStream).
			ModuleList          = 4,      // Module information (ModuleListStream).
			MemoryList          = 5,      // Memory allocation information (MemoryListStream).
			Exception           = 6,      // Exception information (ExceptionStream).
			SystemInfo          = 7,      // General system information (SystemInfoStream).
			ThreadExList        = 8,      // Extended thread information (ThreadExListStream).
			Memory64List        = 9,      // Memory allocation information (Memory64ListStream).
			CommentA            = 10,     // ANSI string used for documentation purposes (CommentStreamA).
			CommentW            = 11,     // Unicode string used for documentation purposes (CommentStreamW).
			HandleData          = 12,     // High-level information about the active operating system handles (HandleDataStream).
			FunctionTable       = 13,     // Function table information (FunctionTableStream).
			UnloadedModuleList  = 14,     // Module information for the unloaded modules (UnloadedModuleListStream).
			MiscInfo            = 15,     // Miscellaneous information (MiscInfoStream).
			MemoryInfoList      = 16,     // Memory region description information (MemoryInfoListStream).
			ThreadInfoList      = 17,     // Thread state information (ThreadInfoListStream).
			HandleOperationList = 18,     // Operation list information (HandleOperationListStream).
			Tokens              = 19,     // Token information (TokenStream).
			JavaScriptData      = 20,     // ... (JavaScriptDataStream).
			SystemMemoryInfo    = 21,     // Global system memory/performance information (SystemMemoryInfoStream).
			ProcessVmCounters   = 22,     // Process VM counters (ProcessVmCountersStream).
			LastReserved        = 0xffff, // Any value greater than this value will not be used by the system (LastReservedStream).
		};

		Type     type;     //
		Location location; //
	};

	////////////////////////////////////////////////////////////
	//
	// Thread information (ThreadListStream).
	//
	////////////////////////////////////////////////////////////

	// Thread list header (MINIDUMP_THREAD_LIST).
	struct ThreadListHeader
	{
		uint32_t entry_count; //
	};

	// Thread list entry (MINIDUMP_THREAD).
	struct Thread
	{
		uint32_t    id;             //
		uint32_t    suspend_count;  //
		uint32_t    priority_class; //
		uint32_t    priority;       //
		uint64_t    teb;            //
		MemoryRange stack;          //
		Location    context;        // Thread context location (see below).
	};

	union ThreadContext
	{
		enum : uint32_t
		{
			Control           = 0x00000001, // (CONTEXT_CONTROL).
			Integer           = 0x00000002, // (CONTEXT_INTEGER).
			Segments          = 0x00000004, // (CONTEXT_SEGMENTS).
			FloatingPoint     = 0x00000008, // (CONTEXT_FLOATING_POINT).
			DebugRegisters    = 0x00000010, // (CONTEXT_DEBUG_REGISTERS).
			ExtendedRegisters = 0x00000020, // (CONTEXT_EXTENDED_REGISTERS).
			XState            = 0x00000040, // (CONTEXT_XSTATE).

			X86               = 0x00010000, // (CONTEXT_I386).
			X64               = 0x00100000, // (CONTEXT_AMD64).

			StateMask         = 0x000000ff,
			CpuMask           = 0xffffff00,
		};

		// Thread context for x86 (CONTEXT).
		struct
		{
			uint32_t context_flags;

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
		} x86;

		// Thread context for x64 (CONTEXT).
		struct
		{
			// x86/x64 128-bit SSE register.
			union Xmm
			{
				int8_t   i8[16];
				int16_t  i16[8];
				int32_t  i32[4];
				int64_t  i64[2];
				uint8_t  u8[16];
				uint16_t u16[8];
				uint32_t u32[4];
				uint64_t u64[2];
				float    f32[4];
				double   f64[2];
			};

			uint64_t p1_home;
			uint64_t p2_home;
			uint64_t p3_home;
			uint64_t p4_home;
			uint64_t p5_home;
			uint64_t p6_home;

			uint32_t context_flags;
			uint32_t mx_csr;

			// CONTEXT_CONTROL
			uint16_t cs;

			// CONTEXT_SEGMENTS
			uint16_t ds;
			uint16_t es;
			uint16_t fs;
			uint16_t gs;

			// CONTEXT_CONTROL
			uint16_t ss;
			uint32_t eflags;

			// CONTEXT_DEBUG_REGISTERS
			uint64_t dr0;
			uint64_t dr1;
			uint64_t dr2;
			uint64_t dr3;
			uint64_t dr6;
			uint64_t dr7;

			// CONTEXT_INTEGER
			uint64_t rax;
			uint64_t rcx;
			uint64_t rdx;
			uint64_t rbx;

			// CONTEXT_CONTROL
			uint64_t rsp;

			// CONTEXT_INTEGER
			uint64_t rbp;
			uint64_t rsi;
			uint64_t rdi;
			uint64_t r8;
			uint64_t r9;
			uint64_t r10;
			uint64_t r11;
			uint64_t r12;
			uint64_t r13;
			uint64_t r14;
			uint64_t r15;

			// CONTEXT_CONTROL
			uint64_t rip;

			// CONTEXT_FLOATING_POINT
			union
			{
				struct
				{
					uint16_t control_word;
					uint16_t status_word;
					uint8_t  tag_word;
					uint8_t  reserved1;
					uint16_t error_opcode;
					uint32_t error_offset;
					uint16_t error_selector;
					uint16_t reserved2;
					uint32_t data_offset;
					uint16_t data_selector;
					uint16_t reserved3;
					uint32_t mx_csr;
					uint32_t mx_csr_mask;
					Xmm      float_registers[8];
					Xmm      xmm_registers[16];
					uint8_t  reserved4[96];
				} float_save;
				struct
				{
					Xmm header[2];
					Xmm legacy[8];
					Xmm xmm0;
					Xmm xmm1;
					Xmm xmm2;
					Xmm xmm3;
					Xmm xmm4;
					Xmm xmm5;
					Xmm xmm6;
					Xmm xmm7;
					Xmm xmm8;
					Xmm xmm9;
					Xmm xmm10;
					Xmm xmm11;
					Xmm xmm12;
					Xmm xmm13;
					Xmm xmm14;
					Xmm xmm15;
				} sse;
			};

			Xmm      vector_register[26];
			uint64_t vector_control;

			// CONTEXT_DEBUG_REGISTERS
			uint64_t debug_control;
			uint64_t last_branch_to_rip;
			uint64_t last_branch_from_rip;
			uint64_t last_exception_to_rip;
			uint64_t last_exception_from_rip;
		} x64;
	};

	////////////////////////////////////////////////////////////
	//
	// Module information (ModuleListStream).
	//
	////////////////////////////////////////////////////////////

	// Module list header (MINIDUMP_MODULE_LIST).
	struct ModuleListHeader
	{
		uint32_t entry_count; //
	};

	// Module list entry (MINIDUMP_MODULE).
	struct Module
	{
		// Module version information (VS_FIXEDFILEINFO).
		struct VersionInfo
		{
			uint32_t signature;          //
			uint32_t version;            // Of this structure.
			uint16_t file_version[4];    // Minor version, major verion, minor revision, major revision.
			uint16_t product_version[4]; // Same as above.
			uint32_t file_flags_mask;    //
			uint32_t file_flags;         //
			uint32_t file_os;            //
			uint32_t file_type;          //
			uint32_t file_subtype;       //
			uint32_t file_date[2];       //

			static constexpr uint32_t Signature = 0xfeef04bd;
			static constexpr uint32_t Version = 0x00010000;
		};

		uint64_t    image_base;   //
		uint32_t    image_size;   //
		uint32_t    check_sum;    //
		uint32_t    timestamp;    // 32-bit time_t value.
		uint32_t    name_offset;  //
		VersionInfo version_info; //
		Location    cv_record;    // CodeView record location (see below).
		Location    misc_record;  //
		uint32_t    reserved[4];  //
	};

	// CodeView record for PDB 7.0.
	struct CodeViewRecordPDB70
	{
		uint32_t signature;    //
		uint8_t  pdb_guid[16]; //
		uint32_t pdb_age;      //
		char     pdb_name[];   //

		static constexpr uint32_t Signature = 0x53445352; // "RSDS".
		static constexpr uint32_t MinSize = 24;
	};

	////////////////////////////////////////////////////////////
	//
	// Memory allocation information (MemoryListStream).
	//
	////////////////////////////////////////////////////////////

	// Memory list header (MINIDUMP_MEMORY_LIST).
	struct MemoryListHeader
	{
		uint32_t entry_count; //
	};

	////////////////////////////////////////////////////////////
	//
	// Exception information (ExceptionStream).
	//
	////////////////////////////////////////////////////////////

	// (MINIDUMP_EXCEPTION).
	struct Exception
	{
		uint32_t ExceptionCode;
		uint32_t ExceptionFlags;
		uint64_t ExceptionRecord;
		uint64_t ExceptionAddress;
		uint32_t NumberParameters;
		uint32_t __unusedAlignment;
		uint64_t ExceptionInformation[15];
	};

	// (MINIDUMP_EXCEPTION_STREAM).
	struct ExceptionStream
	{
		uint32_t  thread_id;       //
		uint32_t  _alignment;      //
		Exception ExceptionRecord; //
		Location  context;         //
	};

	////////////////////////////////////////////////////////////
	//
	// General system information (SystemInfoStream).
	//
	////////////////////////////////////////////////////////////

	// (MINIDUMP_SYSTEM_INFO).
	struct SystemInfo
	{
		enum : uint16_t
		{
			X86     = 0,      // PROCESSOR_ARCHITECTURE_INTEL
			ARM     = 5,      // PROCESSOR_ARCHITECTURE_ARM
			IA64    = 6,      // PROCESSOR_ARCHITECTURE_IA64
			X64     = 9,      // PROCESSOR_ARCHITECTURE_AMD64
			Unknown = 0xffff, // PROCESSOR_ARCHITECTURE_UNKNOWN
		} cpu_architecture;
		uint16_t cpu_family;
		uint8_t cpu_stepping;
		uint8_t cpu_model;
		uint8_t cpu_cores;
		enum : uint8_t
		{
			Workstation      = 1, // VER_NT_WORKSTATION
			DomainController = 2, // VER_NT_DOMAIN_CONTROLLER
			Server           = 3, // VER_NT_SERVER
		} product_type;
		uint32_t major_version;
		uint32_t minor_version;
		uint32_t build_number;
		enum : uint32_t
		{
			WindowsNt = 2, // VER_PLATFORM_WIN32_NT
		} platform_id;
		uint32_t service_pack_name_offset;
		uint16_t suite_mask;
		uint16_t _reserved;
		union
		{
			struct
			{
				uint8_t  vendor_id[12];
				uint32_t VersionInformation;
				uint32_t FeatureInformation;
				uint32_t AMDExtendedCpuFeatures;
			} x86;
			struct
			{
				uint64_t features[2];
			} other;
		} cpu;
	};

	////////////////////////////////////////////////////////////
	//
	// Extended thread information (ThreadExListStream).
	//
	////////////////////////////////////////////////////////////

	// (MINIDUMP_THREAD_EX_LIST).
	struct ThreadExListHeader : ThreadListHeader
	{
	};

	// (MINIDUMP_THREAD_EX).
	struct ThreadEx : Thread
	{
		MemoryRange backing_store; //
	};

	////////////////////////////////////////////////////////////
	//
	// Memory allocation information (Memory64ListStream).
	//
	////////////////////////////////////////////////////////////

	// (MINIDUMP_MEMORY64_LIST).
	struct Memory64ListHeader
	{
		uint64_t entry_count; //
		uint64_t offset;      //
	};

	// (MINIDUMP_MEMORY_DESCRIPTOR64).
	struct Memory64Range
	{
		uint64_t base; //
		uint64_t size; //
	};

	////////////////////////////////////////////////////////////
	//
	// High-level information about the active operating system handles (HandleDataStream).
	//
	////////////////////////////////////////////////////////////

	// (MINIDUMP_HANDLE_DATA_STREAM).
	struct HandleDataHeader
	{
		uint32_t header_size; //
		uint32_t entry_size;  //
		uint32_t entry_count; //
		uint32_t reserved;    //
	};

	// (MINIDUMP_HANDLE_DESCRIPTOR).
	struct HandleData
	{
		uint64_t handle;             //
		uint32_t type_name_offset;   // 0 if there is no type name stored.
		uint32_t object_name_offset; // 0 if there is no object name stored.
		uint32_t attributes;         //
		uint32_t granted_access;     //
		uint32_t handle_count;       //
		uint32_t pointer_count;      //
	};

	// (MINIDUMP_HANDLE_DESCRIPTOR_2).
	struct HandleData2 : HandleData
	{
		uint32_t object_info_offset; // Offset to HandleObjectInfo; 0 if there is no extra information.
		uint32_t reserved;           //
	};

	// (MINIDUMP_HANDLE_OBJECT_INFORMATION).
	struct HandleObjectInfo
	{
		// (MINIDUMP_HANDLE_OBJECT_INFORMATION_TYPE).
		enum class Type : uint32_t
		{
			None,     //
			Thread1,  //
			Mutant1,  //
			Mutant2,  //
			Process1, //
			Process2, //
		};

		uint32_t next_offset; // 0 if there are no more elements in the list.
		Type     type;        //
		uint32_t size;        // Size of the information that follows this structure.
	};

	////////////////////////////////////////////////////////////
	//
	// Module information for the unloaded modules (UnloadedModuleListStream).
	//
	////////////////////////////////////////////////////////////

	// (MINIDUMP_UNLOADED_MODULE_LIST).
	struct UnloadedModuleListHeader
	{
		uint32_t header_size; //
		uint32_t entry_size;  //
		uint32_t entry_count; //
	};

	// (MINIDUMP_UNLOADED_MODULE).
	struct UnloadedModule
	{
		uint64_t image_base;      //
		uint32_t image_size;      //
		uint32_t check_sum;       //
		uint32_t time_date_stamp; //
		uint32_t name_offset;     //
	};

	////////////////////////////////////////////////////////////
	//
	// Miscellaneous information (MiscInfoStream).
	//
	////////////////////////////////////////////////////////////

	// (MINIDUMP_MISC_INFO).
	struct MiscInfo
	{
		enum : uint32_t
		{
			ProcessId    = 0x00000001, // (MINIDUMP_MISC1_PROCESS_ID).
			ProcessTimes = 0x00000002, // (MINIDUMP_MISC1_PROCESS_TIMES).
		};

		uint32_t size;                //
		uint32_t flags;               //
		uint32_t process_id;          //
		uint32_t process_create_time; //
		uint32_t process_user_time;   //
		uint32_t process_kernel_time; //
	};

	// (MINIDUMP_MISC_INFO_2).
	struct MiscInfo2 : MiscInfo
	{
		enum : uint32_t
		{
			ProcessorPowerInfo = 0x00000004, // (MINIDUMP_MISC1_PROCESSOR_POWER_INFO).
		};

		uint32_t processor_max_mhz;            //
		uint32_t processor_current_mhz;        //
		uint32_t processor_mhz_limit;          //
		uint32_t processor_max_idle_state;     //
		uint32_t processor_current_idle_state; //
	};

	// (MINIDUMP_MISC_INFO_3).
	struct MiscInfo3 : MiscInfo2
	{
		// (TIME_ZONE_INFORMATION).
		struct TimeZoneInfo
		{
			// (SYSTEMTIME).
			struct SystemTime
			{
				uint16_t year;         //
				uint16_t month;        // 1 to 12.
				uint16_t day_of_week;  // 0 to 6, 0 is Sunday.
				uint16_t day;          // 1 to 31.
				uint16_t hour;         // 0 to 23.
				uint16_t minute;       // 0 to 59.
				uint16_t second;       // 0 to 59.
				uint16_t milliseconds; // 0 to 999.
			};

			int32_t    bias;              //
			char16_t   standard_name[32]; //
			SystemTime standard_date;     //
			int32_t    standard_bias;     //
			char16_t   daylight_name[32]; //
			SystemTime daylight_date;     //
			int32_t    daylight_bias;     //
		};

		enum : uint32_t
		{
			ProcessIntegrity    = 0x00000010, // (MINIDUMP_MISC3_PROCESS_INTEGRITY).
			ProcessExecuteFlags = 0x00000020, // (MINIDUMP_MISC3_PROCESS_EXECUTE_FLAGS).
			TimeZone            = 0x00000040, // (MINIDUMP_MISC3_TIMEZONE).
			ProtectedProcess    = 0x00000080, // (MINIDUMP_MISC3_PROTECTED_PROCESS).
		};

		uint32_t     process_integrity_level; //
		uint32_t     process_execute_flags;   //
		uint32_t     protected_process;       //
		uint32_t     time_zone_id;            //
		TimeZoneInfo time_zone;               //
	};

	// (MINIDUMP_MISC_INFO_4).
	struct MiscInfo4 : MiscInfo3
	{
		enum : uint32_t
		{
			BuildString = 0x00000100, // (MINIDUMP_MISC4_BUILDSTRING).
		};

		char16_t build_string[260];      //
		char16_t debug_build_string[40]; //
	};

	// (MINIDUMP_MISC_INFO_5).
	struct MiscInfo5 : MiscInfo4
	{
		// (XSTATE_CONFIG_FEATURE_MSC_INFO).
		struct XStateInfo
		{
			// (XSTATE_FEATURE).
			struct Feature
			{
				uint32_t offset;
				uint32_t size;
			};

			uint32_t size;             //
			uint32_t context_size;     //
			uint64_t enabled_features; //
			Feature  features[64];     //
		};

		enum : uint32_t
		{
			ProcessCookie = 0x00000200, // (MINIDUMP_MISC5_PROCESS_COOKIE).
		};

		XStateInfo xstate;         //
		uint32_t   process_cookie; //
	};

	////////////////////////////////////////////////////////////
	//
	// Memory region description information (MemoryInfoListStream).
	//
	////////////////////////////////////////////////////////////

	// (MINIDUMP_MEMORY_INFO_LIST).
	struct MemoryInfoListHeader
	{
		uint32_t header_size; //
		uint32_t entry_size;  //
		uint64_t entry_count; //
	};

	// (MINIDUMP_MEMORY_INFO).
	struct MemoryInfo
	{
		enum class State : uint32_t
		{
			Committed =  0x1000, // (MEM_COMMIT).
			Reserved  =  0x2000, // (MEM_RESERVE).
			Free      = 0x10000, // (MEM_FREE).
		};

		enum class Type : uint32_t
		{
			Undefined =         0, //
			Private   =   0x20000, // (MEM_PRIVATE).
			Mapped    =   0x40000, // (MEM_MAPPED).
			Image     = 0x1000000, // (MEM_IMAGE).
		};

		uint64_t base;                  //
		uint64_t allocation_base;       //
		uint32_t allocation_protection; //
		uint32_t _alignment1;           //
		uint64_t size;                  //
		State    state;                 //
		uint32_t protection;            //
		Type     type;                  //
		uint32_t _alignment2;           //
	};

	////////////////////////////////////////////////////////////
	//
	// Thread state information (ThreadInfoListStream).
	//
	////////////////////////////////////////////////////////////

	// Thread information list header (MINIDUMP_THREAD_INFO_LIST).
	struct ThreadInfoListHeader
	{
		uint32_t header_size; //
		uint32_t entry_size;  //
		uint32_t entry_count; //
	};

	// Thread information list entry (MINIDUMP_THREAD_INFO).
	struct ThreadInfo
	{
		enum : uint32_t
		{
			Inaccessible  = 0x00000001, // The thread is inaccessible, only its ID is known (MINIDUMP_THREAD_INFO_ERROR_THREAD).
			Finished      = 0x00000004, // The thread has finished (MINIDUMP_THREAD_INFO_EXITED_THREAD).
			BadContext    = 0x00000010, // Thread context could not be retrieved (MINIDUMP_THREAD_INFO_INVALID_CONTEXT).
			BadInfo       = 0x00000008, // Thread information could not be retrieved (MINIDUMP_THREAD_INFO_INVALID_INFO).
			BadTeb        = 0x00000020, // TEB information could not be retrieved (MINIDUMP_THREAD_INFO_INVALID_TEB).
			WritingThread = 0x00000002, // This is the thread that called MiniDumpWriteDump (MINIDUMP_THREAD_INFO_WRITING_THREAD).
		};

		uint32_t thread_id;     //
		uint32_t dump_flags;    //
		uint32_t dump_error;    // HRESULT indicating the dump status.
		uint32_t exit_status;   // Thread exit code.
		uint64_t create_time;   // Thread creation time in 100 ns intervals since January 1, 1601 (UTC).
		uint64_t exit_time;     // Thread exit time in 100 ns intervals since January 1, 1601 (UTC).
		uint64_t kernel_time;   // Time spent in kernel mode in 100 ns intervals.
		uint64_t user_time;     // Time spent in user mode in 100 ns intervals.
		uint64_t start_address; //
		uint64_t affinity;      //
	};

	////////////////////////////////////////////////////////////
	//
	// Token information (TokenStream).
	//
	////////////////////////////////////////////////////////////

	// (MINIDUMP_TOKEN_INFO_LIST).
	struct TokenInfoListHeader
	{
		uint32_t total_size;        // Total bytes in the stream.
		uint32_t entry_count;       //
		uint32_t header_size;       //
		uint32_t entry_header_size; //
	};

	// (MINIDUMP_TOKEN_INFO_HEADER).
	struct TokenInfoHeader
	{
		uint32_t size;   //
		uint32_t id;     // Process/thread identifier.
		uint64_t handle; //
	};

	////////////////////////////////////////////////////////////
	//
	// Global system memory/performance information (SystemMemoryInfoStream).
	//
	////////////////////////////////////////////////////////////

	// (MINIDUMP_SYSTEM_MEMORY_INFO_1).
	struct SystemMemoryInfo1
	{
		// (MINIDUMP_SYSTEM_BASIC_INFORMATION).
		struct BasicInfo
		{
			uint32_t TimerResolution;
			uint32_t PageSize;
			uint32_t NumberOfPhysicalPages;
			uint32_t LowestPhysicalPageNumber;
			uint32_t HighestPhysicalPageNumber;
			uint32_t AllocationGranularity;
			uint64_t MinimumUserModeAddress;
			uint64_t MaximumUserModeAddress;
			uint64_t ActiveProcessorsAffinityMask;
			uint32_t NumberOfProcessors;
		};

		// (MINIDUMP_SYSTEM_FILECACHE_INFORMATION).
		struct FileCacheInfo
		{
			uint64_t CurrentSize;
			uint64_t PeakSize;
			uint32_t PageFaultCount;
			uint64_t MinimumWorkingSet;
			uint64_t MaximumWorkingSet;
			uint64_t CurrentSizeIncludingTransitionInPages;
			uint64_t PeakSizeIncludingTransitionInPages;
			uint32_t TransitionRePurposeCount;
			uint32_t Flags;
		};

		// (MINIDUMP_SYSTEM_BASIC_PERFORMANCE_INFORMATION).
		struct BasicPerfInfo
		{
			uint64_t AvailablePages;
			uint64_t CommittedPages;
			uint64_t CommitLimit;
			uint64_t PeakCommitment;
		};

		// (MINIDUMP_SYSTEM_PERFORMANCE_INFORMATION).
		struct PerfInfo
		{
			uint64_t IdleProcessTime;
			uint64_t IoReadTransferCount;
			uint64_t IoWriteTransferCount;
			uint64_t IoOtherTransferCount;
			uint32_t IoReadOperationCount;
			uint32_t IoWriteOperationCount;
			uint32_t IoOtherOperationCount;
			uint32_t AvailablePages;
			uint32_t CommittedPages;
			uint32_t CommitLimit;
			uint32_t PeakCommitment;
			uint32_t PageFaultCount;
			uint32_t CopyOnWriteCount;
			uint32_t TransitionCount;
			uint32_t CacheTransitionCount;
			uint32_t DemandZeroCount;
			uint32_t PageReadCount;
			uint32_t PageReadIoCount;
			uint32_t CacheReadCount;
			uint32_t CacheIoCount;
			uint32_t DirtyPagesWriteCount;
			uint32_t DirtyWriteIoCount;
			uint32_t MappedPagesWriteCount;
			uint32_t MappedWriteIoCount;
			uint32_t PagedPoolPages;
			uint32_t NonPagedPoolPages;
			uint32_t PagedPoolAllocs;
			uint32_t PagedPoolFrees;
			uint32_t NonPagedPoolAllocs;
			uint32_t NonPagedPoolFrees;
			uint32_t FreeSystemPtes;
			uint32_t ResidentSystemCodePage;
			uint32_t TotalSystemDriverPages;
			uint32_t TotalSystemCodePages;
			uint32_t NonPagedPoolLookasideHits;
			uint32_t PagedPoolLookasideHits;
			uint32_t AvailablePagedPoolPages;
			uint32_t ResidentSystemCachePage;
			uint32_t ResidentPagedPoolPage;
			uint32_t ResidentSystemDriverPage;
			uint32_t CcFastReadNoWait;
			uint32_t CcFastReadWait;
			uint32_t CcFastReadResourceMiss;
			uint32_t CcFastReadNotPossible;
			uint32_t CcFastMdlReadNoWait;
			uint32_t CcFastMdlReadWait;
			uint32_t CcFastMdlReadResourceMiss;
			uint32_t CcFastMdlReadNotPossible;
			uint32_t CcMapDataNoWait;
			uint32_t CcMapDataWait;
			uint32_t CcMapDataNoWaitMiss;
			uint32_t CcMapDataWaitMiss;
			uint32_t CcPinMappedDataCount;
			uint32_t CcPinReadNoWait;
			uint32_t CcPinReadWait;
			uint32_t CcPinReadNoWaitMiss;
			uint32_t CcPinReadWaitMiss;
			uint32_t CcCopyReadNoWait;
			uint32_t CcCopyReadWait;
			uint32_t CcCopyReadNoWaitMiss;
			uint32_t CcCopyReadWaitMiss;
			uint32_t CcMdlReadNoWait;
			uint32_t CcMdlReadWait;
			uint32_t CcMdlReadNoWaitMiss;
			uint32_t CcMdlReadWaitMiss;
			uint32_t CcReadAheadIos;
			uint32_t CcLazyWriteIos;
			uint32_t CcLazyWritePages;
			uint32_t CcDataFlushes;
			uint32_t CcDataPages;
			uint32_t ContextSwitches;
			uint32_t FirstLevelTbFills;
			uint32_t SecondLevelTbFills;
			uint32_t SystemCalls;
			uint64_t CcTotalDirtyPages;
			uint64_t CcDirtyPageThreshold;
			int64_t  ResidentAvailablePages;
			uint64_t SharedCommittedPages;
		};

		enum : uint16_t
		{
			FileCache_TransitionRePurposeCount_Flags         = 0x0001, // (MINIDUMP_SYSMEMINFO1_FILECACHE_TRANSITIONREPURPOSECOUNT_FLAGS).
			BasicPerf                                        = 0x0002, // (MINIDUMP_SYSMEMINFO1_BASICPERF).
			Perf_CcTotalDirtyPagesCc_DirtyPagesThreshold     = 0x0004, // (MINIDUMP_SYSMEMINFO1_PERF_CCTOTALDIRTYPAGES_CCDIRTYPAGETHRESHOLD).
			Perf_ResidentAvailablePages_SharedCommittedPages = 0x0008, // (MINIDUMP_SYSMEMINFO1_PERF_RESIDENTAVAILABLEPAGES_SHAREDCOMMITPAGES).
		};

		uint16_t      revision;        //
		uint16_t      flags;           //
		BasicInfo     basic_info;      //
		FileCacheInfo file_cache_info; //
		BasicPerfInfo basic_perf_info; //
		PerfInfo      perf_info;       //

		static constexpr uint16_t Revision = 1;
	};

	////////////////////////////////////////////////////////////
	//
	// Process VM counters (ProcessVmCountersStream).
	//
	////////////////////////////////////////////////////////////

	// (MINIDUMP_PROCESS_VM_COUNTERS_1, see also PROCESS_MEMORY_COUNTERS).
	struct VmCounters1
	{
		uint16_t revision;                  //
		uint16_t flags;                     // Zero in revision 1.
		uint32_t page_fault_count;          //
		uint64_t peak_working_set_size;     //
		uint64_t working_set_size;          //
		uint64_t peak_paged_pool_usage;     //
		uint64_t paged_pool_usage;          //
		uint64_t peak_non_paged_pool_usage; //
		uint64_t non_paged_pool_usage;      //
		uint64_t page_file_usage;           //
		uint64_t peak_page_file_usage;      //
		uint64_t private_usage;             //

		static constexpr uint16_t Revision = 1;
	};

	// (MINIDUMP_PROCESS_VM_COUNTERS_2, see also PROCESS_MEMORY_COUNTERS).
	struct VmCounters2
	{
		enum : uint32_t
		{
			Basic       = 0x00000001, // (MINIDUMP_PROCESS_VM_COUNTERS).
			VirtualSize = 0x00000002, // (MINIDUMP_PROCESS_VM_COUNTERS_VIRTUALSIZE).
			Ex          = 0x00000004, // (MINIDUMP_PROCESS_VM_COUNTERS_EX).
			Ex2         = 0x00000008, // (MINIDUMP_PROCESS_VM_COUNTERS_EX2).
			Job         = 0x00000010, // (MINIDUMP_PROCESS_VM_COUNTERS_JOB).
		};

		uint16_t revision;                      //
		uint16_t flags;                         //
		uint32_t page_fault_count;              //
		uint64_t peak_working_set_size;         //
		uint64_t working_set_size;              //
		uint64_t peak_paged_pool_usage;         //
		uint64_t paged_pool_usage;              //
		uint64_t peak_non_paged_pool_usage;     //
		uint64_t non_paged_pool_usage;          //
		uint64_t page_file_usage;               //
		uint64_t peak_page_file_usage;          //
		uint64_t peak_virtual_size;             // Requires 'VirtualSize'.
		uint64_t virtual_size;                  // Requires 'VirtualSize'.
		uint64_t private_usage;                 // Requires 'Ex'.
		uint64_t private_working_set_size;      // Requires 'Ex2'.
		uint64_t shared_commit_usage;           // Requires 'Ex2'.
		uint64_t job_shared_commit_usage;       // Requires 'Job'.
		uint64_t job_private_commit_usage;      // Requires 'Job'.
		uint64_t job_peak_private_commit_usage; // Requires 'Job'.
		uint64_t job_private_commit_limit;      // Requires 'Job'.
		uint64_t job_total_commit_limit;        // Requires 'Job'.

		static constexpr uint16_t Revision = 2;
	};

	////////////////////////////////////////////////////////////
	//
	// User streams.
	//
	////////////////////////////////////////////////////////////

	// User stream header (MINIDUMP_USER_STREAM).
	struct UserStreamHeader
	{
		uint32_t type;      //
		uint32_t data_size; //
	};
}

#pragma pack(pop)
