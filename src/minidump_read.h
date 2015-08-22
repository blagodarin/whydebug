#pragma once

class File;
class Minidump;
struct MINIDUMP_DIRECTORY;

void read_misc_info(Minidump&, File&, const MINIDUMP_DIRECTORY&);
void read_module_list(Minidump&, File&, const MINIDUMP_DIRECTORY&);
void read_thread_list(Minidump&, File&, const MINIDUMP_DIRECTORY&);
void read_thread_info_list(Minidump&, File&, const MINIDUMP_DIRECTORY&);
