/*
**  GeneralsX Web Port — Minimal windows_base.h for Emscripten
**
**  On Linux/macOS, DXVK provides windows_base.h with Wine-compatible Win32
*types.
**  On Emscripten, DXVK is not available. This stub provides the minimum set of
**  types and macros needed by the SAGE engine code, matching DXVK's definitions
**  for binary compatibility with the rest of the compat layer.
**
**  GeneralsX @feature WebPort 09/03/2026
*/

#ifndef __WINE_WINDOWS_BASE_H
#define __WINE_WINDOWS_BASE_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
/* ===================================================================
 * Basic Windows types — must match bittype.h and types_compat.h
 * Use uint32_t (not unsigned long) to avoid typedef redefinition
 * errors on platforms where unsigned long != unsigned int.
 * =================================================================== */

#ifndef _DWORD_DEFINED
typedef unsigned long DWORD;
#define _DWORD_DEFINED
#endif

#ifndef _WORD_DEFINED
typedef unsigned short WORD;
#define _WORD_DEFINED
#endif

#ifndef _BYTE_DEFINED
typedef unsigned char BYTE;
#define _BYTE_DEFINED
#endif

#ifndef _ULONG_DEFINED
typedef unsigned long ULONG;
#define _ULONG_DEFINED
#endif

#ifndef LONG
typedef int32_t LONG;
#endif

/* WINBOOL: Used by DXVK and some game code as the "real" BOOL to avoid
 * conflicts with Objective-C BOOL on macOS. */
#ifndef WINBOOL
typedef int WINBOOL;
#endif

/* LARGE_INTEGER: Used for high-resolution timing (QueryPerformanceCounter).
 * Uses anonymous struct so .LowPart and .HighPart are accessible directly,
 * matching the Windows SDK layout where DUMMYSTRUCTNAME is an anonymous struct.
 * Also expose .u.LowPart/.u.HighPart for code that uses the named form. */
#ifndef _LARGE_INTEGER_DEFINED
#define _LARGE_INTEGER_DEFINED
typedef union _LARGE_INTEGER {
  struct {
    uint32_t LowPart;
    int32_t HighPart;
  };
  struct {
    uint32_t LowPart;
    int32_t HighPart;
  } u;
  int64_t QuadPart;
} LARGE_INTEGER;

typedef union _ULARGE_INTEGER {
  struct {
    uint32_t LowPart;
    uint32_t HighPart;
  };
  struct {
    uint32_t LowPart;
    uint32_t HighPart;
  } u;
  uint64_t QuadPart;
} ULARGE_INTEGER;
#endif

/* PALETTEENTRY: Used by d3d8types.h for palette support */
typedef struct tagPALETTEENTRY {
  unsigned char peRed;
  unsigned char peGreen;
  unsigned char peBlue;
  unsigned char peFlags;
} PALETTEENTRY;

/* RGNDATA: Used by DirectX for clip regions */
typedef struct _RGNDATAHEADER {
  DWORD dwSize;
  DWORD iType;
  DWORD nCount;
  DWORD nRgnSize;
  int32_t rcBound_left;
  int32_t rcBound_top;
  int32_t rcBound_right;
  int32_t rcBound_bottom;
} RGNDATAHEADER;

typedef struct _RGNDATA {
  RGNDATAHEADER rdh;
  char Buffer[1];
} RGNDATA;

/* RECT, POINT, SIZE: Using struct form (not typedef) to match
 * forward declarations in wnd_compat.h and gdi_compat.h */
struct RECT {
  int32_t left;
  int32_t top;
  int32_t right;
  int32_t bottom;
};

struct POINT {
  int32_t x;
  int32_t y;
};
typedef POINT *LPPOINT;
typedef short SHORT;

struct SIZE {
  int32_t cx;
  int32_t cy;
};

/* GUID, IUnknown: Defined in com_compat.h
 * Do NOT define them here to avoid conflicts with compat layer. */

/* EXCEPTION_POINTERS: minimal opaque declaration for StackDump stubs */
#ifndef _EXCEPTION_POINTERS_DEFINED
#define _EXCEPTION_POINTERS_DEFINED
typedef struct _EXCEPTION_POINTERS EXCEPTION_POINTERS;
#endif

/* MEMORYSTATUS: Used by GetMemoryStatus compat stubs */
#ifndef _MEMORYSTATUS_DEFINED
#define _MEMORYSTATUS_DEFINED
typedef struct _MEMORYSTATUS {
  DWORD dwLength;
  DWORD dwMemoryLoad;
  DWORD dwTotalPhys;
  DWORD dwAvailPhys;
  DWORD dwTotalPageFile;
  DWORD dwAvailPageFile;
  DWORD dwTotalVirtual;
  DWORD dwAvailVirtual;
} MEMORYSTATUS;

/* LPMEMORYSTATUS pointer type */
typedef MEMORYSTATUS *LPMEMORYSTATUS;

/* Stub implementation for non-Windows platforms */
inline void GlobalMemoryStatus(LPMEMORYSTATUS lpBuffer) {
  if (lpBuffer) {
    lpBuffer->dwLength = sizeof(MEMORYSTATUS);
    lpBuffer->dwMemoryLoad = 50;
    lpBuffer->dwTotalPhys = 2147483647; // 2GB
    lpBuffer->dwAvailPhys = 2147483647;
    lpBuffer->dwTotalPageFile = 2147483647;
    lpBuffer->dwAvailPageFile = 2147483647;
    lpBuffer->dwTotalVirtual = 2147483647;
    lpBuffer->dwAvailVirtual = 2147483647;
  }
}

#endif

/* Calling convention macros (no-ops on Emscripten/wasm) */
#ifndef STDMETHODCALLTYPE
#define STDMETHODCALLTYPE
#endif

#ifndef STDMETHOD
#define STDMETHOD(method) virtual long STDMETHODCALLTYPE method
#endif

#ifndef STDMETHOD_
#define STDMETHOD_(type, method) virtual type STDMETHODCALLTYPE method
#endif

#ifndef PURE
#define PURE = 0
#endif

/* DECLARE_INTERFACE macros */
#ifndef DECLARE_INTERFACE
#ifdef __cplusplus
#define DECLARE_INTERFACE(iface) struct iface
#define DECLARE_INTERFACE_(iface, baseiface) struct iface : public baseiface
#else
#define DECLARE_INTERFACE(iface)                                               \
  typedef struct iface {                                                       \
    struct iface##Vtbl *lpVtbl;                                                \
  } iface;                                                                     \
  struct iface##Vtbl
#define DECLARE_INTERFACE_(iface, baseiface) DECLARE_INTERFACE(iface)
#endif
#endif

/* Monitor info stubs (dx8wrapper.cpp uses GetMonitorInfo for window sizing) */
#ifndef MONITORINFO
typedef struct tagMONITORINFO {
  DWORD cbSize;
  RECT rcMonitor;
  RECT rcWork;
  DWORD dwFlags;
} MONITORINFO, *LPMONITORINFO;
#define MONITOR_DEFAULTTOPRIMARY 1
#define MONITOR_DEFAULTTONEAREST 2
static inline void *MonitorFromWindow(void *hWnd, DWORD dwFlags) {
  return (void *)1;
}
static inline int GetMonitorInfo(void *hMonitor, LPMONITORINFO lpmi) {
  if (lpmi) {
    lpmi->cbSize = sizeof(MONITORINFO);
    lpmi->rcMonitor.left = 0;
    lpmi->rcMonitor.top = 0;
    lpmi->rcMonitor.right = 1920;
    lpmi->rcMonitor.bottom = 1080;
    lpmi->rcWork = lpmi->rcMonitor;
    lpmi->dwFlags = 1;
  }
  return 1;
}
#endif

typedef struct _OSVERSIONINFO {
  unsigned int dwOSVersionInfoSize;
  unsigned int dwMajorVersion;
  unsigned int dwMinorVersion;
  unsigned int dwBuildNumber;
  unsigned int dwPlatformId;
  char szCSDVersion[128];
} OSVERSIONINFO;

#define VER_PLATFORM_WIN32_WINDOWS 1
#define VER_PLATFORM_WIN32_NT 2
#define GetVersionEx(...) (0)

/* WAVEFORMAT / LPWAVEFORMAT - use same WAVEFORMAT_DEFINED guard as MilesStub.h
 */
#ifndef WAVEFORMAT_DEFINED
#define WAVEFORMAT_DEFINED
#ifndef WAVE_FORMAT_PCM
#define WAVE_FORMAT_PCM 0x0001
#endif
#ifndef _WAVEFORMAT_DEFINED
#define _WAVEFORMAT_DEFINED
typedef struct WAVEFORMAT {
  WORD wFormatTag;
  WORD nChannels;
  DWORD nSamplesPerSec;
  DWORD nAvgBytesPerSec;
  WORD nBlockAlign;
} WAVEFORMAT, *PWAVEFORMAT, *LPWAVEFORMAT;
#endif
#endif /* WAVEFORMAT_DEFINED */

#define _int64 long long
#define __int64 long long
typedef int CRITICAL_SECTION;
inline void InitializeCriticalSection(CRITICAL_SECTION *x) {}
inline void DeleteCriticalSection(CRITICAL_SECTION *x) {}
inline void EnterCriticalSection(CRITICAL_SECTION *x) {}
inline void LeaveCriticalSection(CRITICAL_SECTION *x){}
#define S_OK ((HRESULT)0)
#define E_FAIL ((HRESULT)0x80004005)
#define SEVERITY_ERROR 1
#define FACILITY_ITF 4
#define MAKE_HRESULT(sev, fac, code)                                           \
  ((HRESULT)(((unsigned long)(sev) << 31) | ((unsigned long)(fac) << 16) |     \
             ((unsigned long)(code))))

#define HKEY_LOCAL_MACHINE ((HKEY)0x80000002)
#define KEY_READ 0x20019
#define ERROR_SUCCESS 0
#define REG_DWORD 4

/* timeGetTime is defined in windows_compat.h — do not redefine here */

#include <strings.h>
inline int lstrcmpi(const char *String1, const char *String2) {
  if (!String1 || !String2)
    return String1 ? 1 : (String2 ? -1 : 0);
  return strcasecmp(String1, String2);
}

/* Windows type aliases needed by dx8wrapper.h and other engine code */
#ifndef CONST
#define CONST const
#endif

#ifndef VOID
#define VOID void
#endif

#ifndef FALSE
#define FALSE 0
#define TRUE 1
#endif

#ifndef NULL
#define NULL 0
#endif

/* Handle types (opaque pointers) */
#ifndef DECLARE_HANDLE
#define DECLARE_HANDLE(name)                                                   \
  typedef struct name##__ {                                                    \
    int unused;                                                                \
  } *name
#endif

#ifndef HWND
DECLARE_HANDLE(HWND);
#endif
#ifndef HINSTANCE
DECLARE_HANDLE(HINSTANCE);
#endif
#ifndef HMODULE
typedef HINSTANCE HMODULE;
#endif
#ifndef HANDLE
typedef void *HANDLE;
#endif
#ifndef HRESULT
typedef long HRESULT;
#endif

#ifndef MAX_PATH
#define MAX_PATH 260
#endif

#ifndef _FILETIME_DEFINED
#define _FILETIME_DEFINED
typedef struct _FILETIME {
  uint32_t dwLowDateTime;
  uint32_t dwHighDateTime;
} FILETIME, *PFILETIME, *LPFILETIME;
#endif

typedef struct _WIN32_FIND_DATAA {
  uint32_t dwFileAttributes;
  FILETIME ftCreationTime;
  FILETIME ftLastAccessTime;
  FILETIME ftLastWriteTime;
  uint32_t nFileSizeHigh;
  uint32_t nFileSizeLow;
  uint32_t dwReserved0;
  uint32_t dwReserved1;
  char cFileName[MAX_PATH];
  char cAlternateFileName[14];
} WIN32_FIND_DATAA, *PWIN32_FIND_DATAA, *LPWIN32_FIND_DATAA;

typedef WIN32_FIND_DATAA WIN32_FIND_DATA;

#ifndef INVALID_HANDLE_VALUE
#define INVALID_HANDLE_VALUE ((void *)(long)-1)
#endif

inline void *FindFirstFileA(const char *lpFileName,
                            WIN32_FIND_DATAA *lpFindFileData) {
  return INVALID_HANDLE_VALUE;
}
inline int FindNextFileA(void *hFindFile, WIN32_FIND_DATAA *lpFindFileData) {
  return 0;
}
inline int FindClose(void *hFindFile) { return 1; }

#ifndef FindFirstFile
#define FindFirstFile FindFirstFileA
#endif
#ifndef FindNextFile
#define FindNextFile FindNextFileA
#endif

#ifndef LPDISPATCH
typedef void *LPDISPATCH;
#endif

#ifndef HKEY
typedef void *HKEY;
#endif

#ifndef ZeroMemory
#define ZeroMemory(Destination, Length) memset((Destination), 0, (Length))
#endif

/* Function pointer types */
#ifndef FARPROC
typedef int (*FARPROC)(void);
#endif
#ifndef PROC
typedef int (*PROC)(void);
#endif

inline HINSTANCE LoadLibrary(const char *) { return (HINSTANCE)0; }
inline FARPROC GetProcAddress(HINSTANCE, const char *) { return 0; }
inline void FreeLibrary(HINSTANCE) {}

#ifndef _MCW_RC
#define _MCW_RC 0
#endif
#ifndef _RC_NEAR
#define _RC_NEAR 0
#endif
#ifndef _MCW_PC
#define _MCW_PC 0
#endif
#ifndef _PC_24
#define _PC_24 0
#endif
inline void _fpreset() {}
inline unsigned int _statusfp() { return 0; }
inline unsigned int _controlfp(unsigned int newval, unsigned int mask) {
  return 0;
}

/* Pointer types */
#ifndef LPVOID
typedef void *LPVOID;
#endif
#ifndef LPCVOID
typedef const void *LPCVOID;
#endif
#ifndef LPSTR
typedef char *LPSTR;
#endif
#ifndef LPCSTR
typedef const char *LPCSTR;
#endif
#ifndef LPCTSTR
typedef const char *LPCTSTR;
#endif
#ifndef LPTSTR
typedef char *LPTSTR;
#endif
#ifndef LPDWORD
typedef DWORD *LPDWORD;
#endif
#ifndef LPBOOL
typedef int *LPBOOL;
#endif
#ifndef UINT_PTR
typedef unsigned long UINT_PTR;
#endif
#ifndef LONG_PTR
typedef long LONG_PTR;
#endif

/* Windows string types */
#ifndef WINAPI
#define WINAPI
#endif
#ifndef CALLBACK
#define CALLBACK
#endif
#ifndef APIENTRY
#define APIENTRY
#endif
#ifndef WINAPIV
#define WINAPIV
#endif

/* Thread / process handle types */
#ifndef LPTHREAD_START_ROUTINE
typedef unsigned int (*LPTHREAD_START_ROUTINE)(void *);
#endif

/* SYSTEMTIME (used by GetLocalTime) */
#ifndef _SYSTEMTIME_DEFINED
#define _SYSTEMTIME_DEFINED
typedef struct _SYSTEMTIME {
  uint16_t wYear;
  uint16_t wMonth;
  uint16_t wDayOfWeek;
  uint16_t wDay;
  uint16_t wHour;
  uint16_t wMinute;
  uint16_t wSecond;
  uint16_t wMilliseconds;
} SYSTEMTIME;
#endif

#define LOCALE_SYSTEM_DEFAULT 0x0800
inline int GetDateFormat(unsigned int Locale, unsigned int dwFlags,
                         const SYSTEMTIME *lpDate, const char *lpFormat,
                         char *lpDateStr, int cchDate) {
  if (!lpDateStr || cchDate <= 0)
    return 0;
  time_t t = time(nullptr);
  struct tm *tm_info = localtime(&t);
  if (!strcmp(lpFormat, "yyyy")) {
    sprintf(lpDateStr, "%04d", tm_info->tm_year + 1900);
  } else if (!strcmp(lpFormat, "MM")) {
    sprintf(lpDateStr, "%02d", tm_info->tm_mon + 1);
  } else if (!strcmp(lpFormat, "dd")) {
    sprintf(lpDateStr, "%02d", tm_info->tm_mday);
  } else {
    lpDateStr[0] = '\0';
  }
  return strlen(lpDateStr);
}

#ifndef wsprintf
#define wsprintf sprintf
#endif

// Clean Auto-fix 5
// Clean Auto-fix 5
// timeGetTime is implemented by CompatLib or Emscripten backend

// We define them as inline functions so :: calls work
inline HANDLE CreateEvent(void *a, int b, int c, const char *d) {
  return (HANDLE)0;
}
inline HANDLE CreateEventA(void *a, int b, int c, const char *d) {
  return (HANDLE)0;
}
inline HANDLE CreateEventW(void *a, int b, int c, const unsigned short *d) {
  return (HANDLE)0;
}
inline int SetEvent(HANDLE a) { return 1; }
#define WAIT_TIMEOUT 258L
inline unsigned long WaitForSingleObject(HANDLE a, unsigned long b) {
  return WAIT_TIMEOUT;
}
inline unsigned long _beginthread(void (*a)(void *), unsigned b, void *c) {
  return 0;
}

typedef void *HGLOBAL;
typedef long LPARAM;
typedef unsigned int WPARAM;
// DIDEVICEOBJECTDATA is also defined in dinput.h (with proper field types).
// Use a shared guard so whichever header is included first wins.
#ifndef _DIDEVICEOBJECTDATA_DEFINED
#define _DIDEVICEOBJECTDATA_DEFINED
struct DIDEVICEOBJECTDATA {
  unsigned int dwOfs;
  unsigned int dwData;
  unsigned int dwTimeStamp;
  unsigned int dwSequence;
  unsigned long *dwAppData;
};
#endif
#define GMEM_FIXED 0
#define MB_OK 0x0
#define MB_ICONSTOP 0x10
#define MB_TASKMODAL 0x2000
#define MB_SETFOREGROUND 0x10000
#define GetCurrentProcess() ((HANDLE) - 1)
// GetCurrentThreadId implemented by CompatLib or Emscripten backend

#ifndef _LARGE_INTEGER_DEFINED
#define _LARGE_INTEGER_DEFINED
typedef union _LARGE_INTEGER {
  struct {
    int LowPart;
    int HighPart;
  } DUMMYSTRUCTNAME;
  struct {
    int LowPart;
    int HighPart;
  } u;
  long long QuadPart;
} LARGE_INTEGER;
#endif

// GeneralsX @fix WebPort 15/03/2026 — return meaningful high-resolution timing.
// emscripten_get_now() returns milliseconds as double with microsecond precision.
// We simulate a 1MHz performance counter (microsecond resolution).
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#define QueryPerformanceCounter(x) \
  ((x)->QuadPart = (long long)(emscripten_get_now() * 1000.0), 1)
#define QueryPerformanceFrequency(x) ((x)->QuadPart = 1000000LL, 1)
#else
#define QueryPerformanceCounter(x) ((x)->QuadPart = 0, 1)
#define QueryPerformanceFrequency(x) ((x)->QuadPart = 1, 1)
#endif

#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#define GlobalAlloc(a, b) malloc(b)
#define GlobalFree(a) free(a)
#define GlobalReAlloc(a, b, c) realloc(a, b)
#define GlobalSize(a) 0

#define RegOpenKeyEx(a, b, c, d, e) 1
#define RegQueryValueEx(a, b, c, d, e, f) 1
#define RegCloseKey(a) 0

#define SOCKET_ERROR (-1)
#define WSAGetLastError() errno
#define WSAEWOULDBLOCK EWOULDBLOCK

#define WSABASEERR 10000
#define WSAEINTR 10004
#define WSAEBADF 10009
#define WSAEACCES 10013
#define WSAEFAULT 10014
#define WSAEINVAL 10022
#define WSAEMFILE 10024
#define WSAEINPROGRESS 10036
#define WSAEALREADY 10037
#define WSAENOTSOCK 10038
#define WSAEDESTADDRREQ 10039
#define WSAEMSGSIZE 10040
#define WSAEPROTOTYPE 10041
#define WSAENOPROTOOPT 10042
#define WSAEPROTONOSUPPORT 10043
#define WSAESOCKTNOSUPPORT 10044
#define WSAEOPNOTSUPP 10045
#define WSAEPFNOSUPPORT 10046
#define WSAEAFNOSUPPORT 10047
#define WSAEADDRINUSE 10048
#define WSAEADDRNOTAVAIL 10049
#define WSAENETDOWN 10050
#define WSAENETUNREACH 10051
#define WSAENETRESET 10052
#define WSAECONNABORTED 10053
#define WSAECONNRESET 10054
#define WSAENOBUFS 10055
#define WSAEISCONN 10056
#define WSAENOTCONN 10057
#define WSAESHUTDOWN 10058
#define WSAETOOMANYREFS 10059
#define WSAETIMEDOUT 10060
#define WSAECONNREFUSED 10061
#define WSAELOOP 10062
#define WSAENAMETOOLONG 10063
#define WSAEHOSTDOWN 10064
#define WSAEHOSTUNREACH 10065
#define WSAENOTEMPTY 10066
#define WSAEPROCLIM 10067
#define WSAEUSERS 10068
#define WSAEDQUOT 10069
#define WSAESTALE 10070
#define WSAEREMOTE 10071
#define WSAEDISCON 10101
#define WSASYSNOTREADY 10091
#define WSAVERNOTSUPPORTED 10092
#define WSANOTINITIALISED 10093
#define WSAHOST_NOT_FOUND 11001
#define WSATRY_AGAIN 11002
#define WSANO_RECOVERY 11003
#define WSANO_DATA 11004

#define MAKEWORD(a, b)                                                         \
  ((WORD)(((BYTE)(((unsigned long)(a)) & 0xff)) |                              \
          ((WORD)((BYTE)(((unsigned long)(b)) & 0xff))) << 8))
#define LOBYTE(w) ((BYTE)(((unsigned long)(w)) & 0xff))
#define HIBYTE(w) ((BYTE)((((unsigned long)(w)) >> 8) & 0xff))
typedef struct WSAData {
  WORD wVersion;
  WORD wHighVersion;
  char szDescription[257];
  char szSystemStatus[129];
  unsigned short iMaxSockets;
  unsigned short iMaxUdpDg;
  char *lpVendorInfo;
} WSADATA, *LPWSADATA;
// Report the requested version: callers (Transport::init, IPEnumeration) reject a
// WSADATA whose wVersion isn't 2.2, and an unset one made the LAN lobby fail to open.
inline int WSAStartup(WORD wVersionRequired, LPWSADATA lpWSAData) {
  if (lpWSAData) { memset(lpWSAData, 0, sizeof(*lpWSAData)); lpWSAData->wVersion = lpWSAData->wHighVersion = wVersionRequired; }
  return 0;
}
inline int WSACleanup() { return 0; }
// Remap WSA error codes to POSIX equivalents for Emscripten socket code.
// Use #undef to avoid -Wmacro-redefined (these are also defined as numeric
// constants earlier in this file for GameSpy compatibility).
#undef WSAEINVAL
#define WSAEINVAL EINVAL
#undef WSAEALREADY
#define WSAEALREADY EALREADY
#undef WSAEISCONN
#define WSAEISCONN EISCONN
#undef WSAECONNRESET
#define WSAECONNRESET ECONNRESET
#undef WSAENOTCONN
#define WSAENOTCONN ENOTCONN

#ifndef _splitpath
#define _splitpath(path, drv, dir, name, ext)                                  \
  do {                                                                         \
    if (drv)                                                                   \
      *(drv) = 0;                                                              \
    if (dir)                                                                   \
      *(dir) = 0;                                                              \
    if (name)                                                                  \
      *(name) = 0;                                                             \
    if (ext)                                                                   \
      *(ext) = 0;                                                              \
  } while (0)
#endif

#ifndef INVALID_HANDLE_VALUE
#define INVALID_HANDLE_VALUE ((HANDLE) - 1)
#endif
#define GENERIC_READ 0x80000000
#define GENERIC_WRITE 0x40000000
#define OPEN_EXISTING 3
#define FILE_ATTRIBUTE_NORMAL 0x80
inline int ReadFile(HANDLE a, void *b, unsigned long c, unsigned long *d,
                    void *e) {
  return 0;
}
inline int WriteFile(HANDLE a, const void *b, unsigned long c, unsigned long *d,
                     void *e) {
  return 0;
}
inline int CloseHandle(HANDLE a) { return 1; }

inline unsigned int GetModuleFileNameA(HMODULE, char *, unsigned int) {
  return 0;
}
inline unsigned int GetModuleFileNameW(HMODULE, wchar_t *, unsigned int) {
  return 0;
}
#define GetModuleFileName GetModuleFileNameA
#define SetUnhandledExceptionFilter(a) 0
#define iswascii isascii
#define MB_ICONWARNING 0x30
#define MB_DEFBUTTON3 0x200
inline void DebugBreak() {}
#define CreateMutex(a, b, c) ((HANDLE)1)
#define ERROR_ALREADY_EXISTS 183L
#define AddFontResource(a) 1
#define RemoveFontResource(a) 1
#define MB_ABORTRETRYIGNORE 0
#define IDABORT 3
typedef void *HKL;
#define GetKeyboardLayout(a) nullptr

#define CreateFile(...) ((HANDLE) - 1)
inline int MessageBoxA(HWND, const char *, const char *, unsigned int) {
  return 0;
}
#define MessageBox MessageBoxA
#define IDIGNORE 5
#define IDRETRY 4
#define IDYES 6
#define MB_YESNO 4
#define MB_ICONQUESTION 0x20

#ifndef IN
#define IN
#endif

#ifndef OUT
#define OUT
#endif

typedef long AsnInteger;
typedef long AsnInteger32;

#define SnmpVarBindList RFC1157VarBindList
#define GetCurrentTime() timeGetTime()
#define SNMP_PDU_GETNEXT 161

typedef struct {
  unsigned int idLength;
  unsigned int *ids;
} AsnObjectIdentifier;

typedef struct {
  unsigned char *stream;
  unsigned int length;
  unsigned int dynamic;
} AsnOctetString;

typedef struct {
  int number;
  AsnOctetString string;
  AsnObjectIdentifier object;
  AsnOctetString sequence;
  AsnOctetString address;
  unsigned int counter;
  unsigned int gauge;
  unsigned int ticks;
  AsnOctetString arbitrary;
} AsnValue;

typedef struct {
  unsigned char asnType;
  AsnValue asnValue;
} AsnAny;

typedef struct {
  AsnObjectIdentifier name;
  AsnAny value;
} RFC1157VarBind;

typedef struct {
  RFC1157VarBind *list;
  unsigned int len;
} RFC1157VarBindList;
#define MB_ICONERROR 0x10
#define MB_SYSTEMMODAL 0x1000
#define MB_TASKMODAL 0x2000
#define SW_HIDE 0

inline int MessageBoxW(HWND, const wchar_t *, const wchar_t *, unsigned int) {
  return 0;
}
#define HWND_NOTOPMOST ((HWND) - 2)

inline bool ShowWindow(HWND, int) { return false; }
#define _P_NOWAIT 1
#define _spawnl(...) (-1)

#define GetLastError(...) (0)
#define FormatMessage(...) (0)
#define FormatMessageW(...) (0)
#define FORMAT_MESSAGE_FROM_SYSTEM (0)

#define LPITEMIDLIST void *
#define CSIDL_DESKTOPDIRECTORY 0
#define SHGetSpecialFolderLocation(...) (-1)
#define SHGetPathFromIDList(...) (0)
#define DeleteFile(f) (remove(f) == 0)

/* Virtual Keys */
#define VK_BACK 0x08
#define VK_TAB 0x09
#define VK_RETURN 0x0D
#define VK_ESCAPE 0x1B
#define VK_SPACE 0x20
#define VK_PRIOR 0x21
#define VK_NEXT 0x22
#define VK_END 0x23
#define VK_HOME 0x24
#define VK_LEFT 0x25
#define VK_UP 0x26
#define VK_RIGHT 0x27
#define VK_DOWN 0x28
#define VK_INSERT 0x2D
#define VK_DELETE 0x2E
#include <stdio.h>
inline char *_itoa(int value, char *str, int base) {
  if (base == 10)
    sprintf(str, "%d", value);
  else if (base == 16)
    sprintf(str, "%x", value);
  return str;
}
inline char *_ultoa(unsigned long value, char *str, int base) {
  if (base == 10)
    sprintf(str, "%lu", value);
  else if (base == 16)
    sprintf(str, "%lx", value);
  return str;
}
inline char *_i64toa(long long value, char *str, int base) {
  if (base == 10)
    sprintf(str, "%lld", value);
  else if (base == 16)
    sprintf(str, "%llx", value);
  return str;
}
inline char *_ui64toa(unsigned long long value, char *str, int base) {
  if (base == 10)
    sprintf(str, "%llu", value);
  else if (base == 16)
    sprintf(str, "%llx", value);
  return str;
}
#define itoa _itoa
#define IsBadReadPtr(a, b) 0
#ifndef wvsprintf
#define wvsprintf vsprintf
#endif
#define ioctlsocket ioctl
#define _strnicmp strncasecmp
#define _wtoi(x) wcstol(x, nullptr, 10)
#ifndef _getcwd
#define _getcwd getcwd
#endif
#define _S_IWRITE 0200
#define _S_IREAD 0400
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
#ifndef FAR
#define FAR
#endif
#define closesocket close
#define recvfrom(a, b, c, d, e, f)                                             \
  ::recvfrom(a, (void *)(b), c, d, e, (socklen_t *)(f))
#define accept(a, b, c) ::accept(a, b, (socklen_t *)(c))
#define getsockopt(a, b, c, d, e) ::getsockopt(a, b, c, d, (socklen_t *)(e))
#define getsockname(a, b, c) ::getsockname(a, b, (socklen_t *)(c))
#ifndef _O_CREAT
#define _O_CREAT O_CREAT
#endif
#ifndef _O_RDWR
#define _O_RDWR O_RDWR
#endif
#define _close close
#define _open open

template <typename F>
inline HANDLE CreateThread(void *a, unsigned long b, F c, void *d,
                           unsigned long e, unsigned long *f) {
  return nullptr;
}
inline int TerminateThread(HANDLE a, unsigned long b) { return 1; }
#ifndef OutputDebugString
#define OutputDebugString(a) printf("%s", a)
#endif
// CreateDirectory on Emscripten: actually mkdir the path so that
// IDBFS-mounted directories (e.g., /userdata/Save) exist when the engine
// later tries to fopen save files inside them. We normalize backslashes to
// forward slashes inline because the engine builds paths with backslashes.
// Returns non-zero on success (matches Win32 BOOL convention loosely — the
// engine doesn't check the return anyway).
// GeneralsX @feature WebPort 2026-05-04 — IDBFS persistence (CreateDirectory)
#ifdef __EMSCRIPTEN__
#include <sys/stat.h>
#include <errno.h>
inline int GX_CreateDirectory_Web(const char *path) {
  if (!path || !*path) return 0;
  char buf[1024];
  size_t len = strlen(path);
  if (len >= sizeof(buf)) return 0;
  for (size_t i = 0; i < len; ++i) {
    buf[i] = (path[i] == '\\') ? '/' : path[i];
  }
  buf[len] = '\0';
  // Strip a trailing slash so mkdir doesn't get confused by `/userdata/`.
  while (len > 1 && buf[len - 1] == '/') {
    buf[--len] = '\0';
  }
  if (mkdir(buf, 0755) == 0) return 1;
  if (errno == EEXIST) return 1;  // matches Win32 ERROR_ALREADY_EXISTS path
  return 0;
}
#define CreateDirectory(a, b) GX_CreateDirectory_Web(a)
#else
#define CreateDirectory(a, b) 0
#endif
struct hostent;
typedef struct hostent HOSTENT;
#define CopyFile(a, b, c) 0
#define GetComputerName(a, b) 0
#define GetUserName(a, b) 0
#define GetDoubleClickTime() 500
#define ERROR_FILE_EXISTS 80
#define GWL_STYLE -16
#define WS_CAPTION 0x00C00000

#ifndef EnumThreadWindows
#define EnumThreadWindows(a, b, c) 0
#endif
#ifndef GetWindowLong
inline long GetWindowLong(HWND hwnd, int nIndex) { return 0; }
#endif

#ifndef GetLocalTime
#define GetLocalTime(x)                                                        \
  do {                                                                         \
  } while (0)
#endif

// To fix WAVEFORMAT, we rename LPWAVEFORMAT and WAVEFORMAT in windows_base.h to
// something else if needed, but it's simpler to just not define it if mss.h
// will define it. We'll leave the text as is before the auto-fix.

#ifndef BOOL
typedef int BOOL;
#endif

#ifndef UINT
typedef unsigned int UINT;
#endif

inline void GetClientRect(HWND hwnd, RECT *rect) {
  if (rect) {
    rect->left = 0;
    rect->top = 0;
    rect->right = 800;
    rect->bottom = 600;
  }
}
inline void AdjustWindowRect(RECT *rect, DWORD style, BOOL menu) {}
inline void SetWindowPos(HWND hwnd, HWND hwndInsertAfter, int x, int y, int cx,
                         int cy, UINT flags) {}
#define HWND_TOPMOST ((HWND) - 1)
#define SWP_NOSIZE 0x0001
#define SWP_NOMOVE 0x0002
#define SWP_NOZORDER 0x0004

typedef void *HDC;
inline HWND GetDesktopWindow() { return nullptr; }
inline HDC GetDC(HWND) { return nullptr; }
inline void ReleaseDC(HWND, HDC) {}
inline void SetDeviceGammaRamp(HDC, void *) {}

#ifndef _WINGDI_
#pragma pack(push, 2)
typedef struct tagBITMAPFILEHEADER {
  unsigned short bfType;
  unsigned int bfSize;
  unsigned short bfReserved1;
  unsigned short bfReserved2;
  unsigned int bfOffBits;
} BITMAPFILEHEADER, *PBITMAPFILEHEADER;
#pragma pack(pop)

typedef struct tagRGBQUAD {
  unsigned char rgbBlue;
  unsigned char rgbGreen;
  unsigned char rgbRed;
  unsigned char rgbReserved;
} RGBQUAD;

typedef struct tagBITMAPINFOHEADER {
  unsigned int biSize;
  int biWidth;
  int biHeight;
  unsigned short biPlanes;
  unsigned short biBitCount;
  unsigned int biCompression;
  unsigned int biSizeImage;
  int biXPelsPerMeter;
  int biYPelsPerMeter;
  unsigned int biClrUsed;
  unsigned int biClrImportant;
} BITMAPINFOHEADER, *PBITMAPINFOHEADER;

typedef struct tagBITMAPINFO {
  BITMAPINFOHEADER bmiHeader;
  RGBQUAD bmiColors[1];
} BITMAPINFO, *PBITMAPINFO;
#endif

typedef struct tagMSG {
  HWND hwnd;
  UINT message;
  WPARAM wParam;
  LPARAM lParam;
  DWORD time;
  POINT pt;
} MSG, *PMSG, *NPMSG, *LPMSG;

#define SEM_FAILCRITICALERRORS 0x0001
inline UINT SetErrorMode(UINT) { return 0; }
inline BOOL IsIconic(HWND) { return 0; }

template <class Base> class CComObject : public Base {
public:
  CComObject() {}
};

#define LPTR 0x0040
#define BI_RGB 0L
typedef HANDLE HLOCAL;
inline HLOCAL LocalAlloc(UINT, size_t) { return 0; }
inline HLOCAL LocalFree(HLOCAL) { return 0; }

#define IDOK 1
#define PM_NOREMOVE 0x0000

#define MB_ICONEXCLAMATION 0x30
#define MB_ICONINFORMATION 0x40
#define MB_OKCANCEL 0x01
#define MB_APPLMODAL 0x00

#define DSSPEAKER_DIRECTOUT 0x00000000
#define DSSPEAKER_HEADPHONE 0x00000001
#define DSSPEAKER_MONO 0x00000002
#define DSSPEAKER_QUAD 0x00000003
#define DSSPEAKER_STEREO 0x00000004
#define DSSPEAKER_SURROUND 0x00000005
#define DSSPEAKER_5POINT1 0x00000006
#define DSSPEAKER_7POINT1 0x00000007

typedef long LRESULT;
inline BOOL PeekMessage(LPMSG, HWND, UINT, UINT, UINT) { return 0; }
inline BOOL GetMessage(LPMSG, HWND, UINT, UINT) { return 0; }
inline BOOL TranslateMessage(const MSG *) { return 0; }
inline LRESULT DispatchMessage(const MSG *) { return 0; }

typedef HANDLE HCURSOR;

// SetCursor stub — engine convention is:
//   SetCursor(nullptr) → "I will draw the cursor myself; hide the system one"
//   SetCursor(non-null) → "show the system cursor as a fallback"
// On Emscripten we route that signal to the canvas via EmscriptenInput so the
// CSS cursor reflects the engine's intent. The native HCURSOR value is
// meaningless on the web (LoadCursorFromFile returns a fixed sentinel); we
// only care about null vs. non-null.
// GeneralsX @feature WebPort 2026-05-04 — cursor visibility
#ifdef __EMSCRIPTEN__
extern "C" void EmscriptenInput_SetCanvasCursor(int visible);
inline HCURSOR SetCursor(HCURSOR h) {
  EmscriptenInput_SetCanvasCursor(h ? 1 : 0);
  return 0;
}
#else
inline HCURSOR SetCursor(HCURSOR) { return 0; }
#endif
inline BOOL GetCursorPos(POINT *lpPoint) {
  if (lpPoint) {
    lpPoint->x = 0;
    lpPoint->y = 0;
  }
  return TRUE;
}
inline BOOL ScreenToClient(HWND, POINT *) { return TRUE; }

typedef unsigned int EXECUTION_STATE;
#define ES_CONTINUOUS 0x80000000
#define ES_DISPLAY_REQUIRED 0x00000002
#define ES_SYSTEM_REQUIRED 0x00000001
inline EXECUTION_STATE SetThreadExecutionState(EXECUTION_STATE) { return 0; }

#define DSSPEAKER_CONFIG(a) ((a) & 0xFFFF)
struct IDirectSound {
  unsigned int GetSpeakerConfig(DWORD *) { return 0; }
};
typedef IDirectSound *LPDIRECTSOUND;

#define INFINITE 0xFFFFFFFF
inline BOOL ReleaseMutex(HANDLE) { return 0; }

#define HEAP_ZERO_MEMORY 0x00000008
inline HANDLE GetProcessHeap() { return (HANDLE)1; }
inline LPVOID HeapAlloc(HANDLE hHeap, DWORD dwFlags, size_t dwBytes) {
  void *p = malloc(dwBytes);
  if (p && (dwFlags & HEAP_ZERO_MEMORY)) {
    memset(p, 0, dwBytes);
  }
  return p;
}
inline BOOL HeapFree(HANDLE hHeap, DWORD dwFlags, LPVOID lpMem) {
  free(lpMem);
  return TRUE;
}

#define FILE_ATTRIBUTE_DIRECTORY 0x00000010

#define VK_F5 0x74
#define VK_F6 0x75
#define VK_F7 0x76
#define VK_F8 0x77
#define VK_CAPITAL 0x14

#define WM_MOUSEMOVE 0x0200
#define WM_LBUTTONDOWN 0x0201
#define WM_LBUTTONUP 0x0202
#define WM_LBUTTONDBLCLK 0x0203
#define WM_RBUTTONDOWN 0x0204
#define WM_RBUTTONUP 0x0205
#define WM_RBUTTONDBLCLK 0x0206
#define WM_MBUTTONDOWN 0x0207
#define WM_MBUTTONUP 0x0208
#define WM_MBUTTONDBLCLK 0x0209
#define WM_MOUSEWHEEL 0x020A

#ifndef MAKELPARAM
#define MAKELPARAM(l, h) ((LPARAM)(((unsigned short)(((unsigned long)(l)) & 0xffff)) | ((unsigned long)((unsigned short)(((unsigned long)(h)) & 0xffff))) << 16))
#endif

inline DWORD GetFullPathNameA(LPCSTR lpFileName, DWORD nBufferLength,
                              LPSTR lpBuffer, LPSTR *lpFilePart) {
  return 0;
}
inline BOOL ClientToScreen(HWND hWnd, LPPOINT lpPoint) { return TRUE; }
inline HCURSOR LoadCursorFromFileA(LPCSTR lpFileName) { return (HCURSOR)1; }
#define LoadCursorFromFile LoadCursorFromFileA
inline SHORT GetAsyncKeyState(int vKey) { return 0; }
inline SHORT GetKeyState(int nVirtKey) { return 0; }
inline BOOL ClipCursor(const RECT *lpRect) { return TRUE; }

#endif /* __WINE_WINDOWS_BASE_H */
