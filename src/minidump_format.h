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
			Token               = 19,     // ... (TokenStream).
			JavaScriptData      = 20,     // ... (JavaScriptDataStream).
			SystemMemoryInfo    = 21,     // ... (SystemMemoryInfoStream).
			ProcessVmCounters   = 22,     // ... (ProcessVmCountersStream).
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

	// Thread context for x86 (CONTEXT).
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

	////////////////////////////////////////////////////////////
	//
	// Module information (ModuleListStream).
	//
	////////////////////////////////////////////////////////////

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

	// File version information (VS_FIXEDFILEINFO).
	struct VersionInfo
	{
		uint32_t signature;          //
		uint32_t version;            // Of this structure.
		uint16_t file_version[4];    // Minor version, major verion, minor revision, major revision.
		uint16_t product_version[4]; // Same as above.
		uint32_t file_flags_mask;    //
		uint32_t file_flags;         // VS_FF_*.
		uint32_t file_os;            //
		uint32_t file_type;          //
		uint32_t file_subtype;       //
		uint32_t file_date[2];       //

		static constexpr uint32_t Signature = 0xfeef04bd;
		static constexpr uint32_t Version = 0x00010000;
	};

	// Module list header (MINIDUMP_MODULE_LIST).
	struct ModuleListHeader
	{
		uint32_t entry_count; //
	};

	// Module list entry (MINIDUMP_MODULE).
	struct Module
	{
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

		static constexpr uint32_t MinSize = 24;
		static constexpr uint32_t Signature = 0x53445352; // "RSDS".
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
		uint32_t  __alignment;     //
		Exception ExceptionRecord; //
		Location  thread_context;  //
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
	// Function table information (FunctionTableStream).
	//
	////////////////////////////////////////////////////////////

	// (MINIDUMP_FUNCTION_TABLE_STREAM).
	struct FunctionTableStream
	{
		uint32_t SizeOfHeader;           //
		uint32_t SizeOfDescriptor;       //
		uint32_t SizeOfNativeDescriptor; //
		uint32_t SizeOfFunctionEntry;    //
		uint32_t NumberOfDescriptors;    //
		uint32_t SizeOfAlignPad;         //
	};

	// (MINIDUMP_FUNCTION_TABLE_DESCRIPTOR).
	struct FunctionTableDescriptor
	{
		uint64_t MinimumAddress; //
		uint64_t MaximumAddress; //
		uint64_t BaseAddress;    //
		uint32_t EntryCount;     //
		uint32_t SizeOfAlignPad; //
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

	// (TIME_ZONE_INFORMATION).
	struct TimeZoneInformation
	{
		int32_t    bias;              //
		char16_t   standard_name[32]; //
		SystemTime standard_date;     //
		int32_t    standard_bias;     //
		char16_t   daylight_name[32]; //
		SystemTime daylight_date;     //
		int32_t    daylight_bias;     //
	};

	// (MINIDUMP_MISC_INFO_3).
	struct MiscInfo3 : MiscInfo2
	{
		enum : uint32_t
		{
			ProcessIntegrity    = 0x00000010, // (MINIDUMP_MISC3_PROCESS_INTEGRITY).
			ProcessExecuteFlags = 0x00000020, // (MINIDUMP_MISC3_PROCESS_EXECUTE_FLAGS).
			Timezone            = 0x00000040, // (MINIDUMP_MISC3_TIMEZONE).
			ProtectedProcess    = 0x00000080, // (MINIDUMP_MISC3_PROTECTED_PROCESS).
		};

		uint32_t            process_integrity_level; //
		uint32_t            process_execute_flags;   //
		uint32_t            protected_process;       //
		uint32_t            time_zone_id;            //
		TimeZoneInformation time_zone;               //
	};

	// (MINIDUMP_MISC_INFO_4).
	struct MiscInfo4 : MiscInfo3
	{
		enum : uint32_t
		{
			BuildString = 0x00000100, // (MINIDUMP_MISC4_BUILDSTRING).
		};

		uint16_t build_string[260];      //
		uint16_t debug_build_string[40]; //
	};

	// (XSTATE_FEATURE).
	struct XStateFeature
	{
		uint32_t offset;
		uint32_t size;
	};

	// (XSTATE_CONFIG_FEATURE_MSC_INFO).
	struct XStateInfo
	{
		uint32_t      size;             //
		uint32_t      context_size;     //
		uint64_t      enabled_features; //
		XStateFeature features[64];     //
	};

	// (MINIDUMP_MISC_INFO_5).
	struct MiscInfo5 : MiscInfo4
	{
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

	// (MINIDUMP_MEMORY_INFO).
	struct MemoryInfo
	{
		uint64_t base;              //
		uint64_t AllocationBase;    //
		uint32_t AllocationProtect; //
		uint32_t __alignment1;      //
		uint64_t size;              //
		uint32_t state;             //
		uint32_t Protect;           //
		uint32_t Type;              //
		uint32_t __alignment2;      //
	};

	////////////////////////////////////////////////////////////
	//
	// Thread state information (ThreadInfoListStream).
	//
	////////////////////////////////////////////////////////////

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

	////////////////////////////////////////////////////////////
	//
	// Process VM counters (ProcessVmCountersStream).
	//
	////////////////////////////////////////////////////////////

	// (MINIDUMP_PROCESS_VM_COUNTERS_1).
	// See also PROCESS_MEMORY_COUNTERS.
	struct VmCounters1
	{
		uint16_t revision;                        //
		uint16_t flags;                           // Zero in revision 1.
		uint32_t page_fault_count;                //
		uint64_t peak_working_set_size;           //
		uint64_t working_set_size;                //
		uint64_t quota_peak_paged_pool_usage;     //
		uint64_t quota_paged_pool_usage;          //
		uint64_t quota_peak_non_paged_pool_usage; //
		uint64_t quota_non_paged_pool_usage;      //
		uint64_t page_file_usage;                 //
		uint64_t peak_page_file_usage;            //
		uint64_t private_usage;                   //

		static constexpr uint16_t Revision = 1;
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
