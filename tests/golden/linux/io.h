// The golden runner's stand-in for the Microsoft CRT's <io.h> on Linux (issue #91): the descriptor functions the
// ported tests call, with the semantics of src/compat/msvc_io.h where it has them, else POSIX's.

#pragma once

#include "compat/msvc_io.h"

#include <cstdio>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#define _O_RDONLY O_RDONLY
#define _O_WRONLY O_WRONLY
#define _O_RDWR O_RDWR
#define _O_CREAT O_CREAT
#define _O_TRUNC O_TRUNC
#define _O_APPEND O_APPEND
#define _O_BINARY 0
#define _O_TEXT 0
#define _S_IREAD S_IRUSR
#define _S_IWRITE S_IWUSR

inline int _access(const char *Path, int Mode) { return MsvcAccess(Path, Mode); }
inline int _eof(int Descriptor) { return MsvcEof(Descriptor); }
inline int _fileno(FILE *File) { return fileno(File); }
inline int _open(const char *Path, int Flags, int Mode = 0) { return open(Path, Flags, Mode); }
inline int _close(int Descriptor) { return close(Descriptor); }
inline int _dup(int Descriptor) { return dup(Descriptor); }
inline int _dup2(int From, int To) { return dup2(From, To); }
inline int _unlink(const char *Path) { return unlink(Path); }
