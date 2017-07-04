/* https://en.wikipedia.org/wiki/Operating_system_abstraction_layer */

/*
 * Copyright 2015-2017 Leonid Yuriev <leo@yuriev.ru>
 * and other libmdbx authors: please see AUTHORS file.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted only as authorized by the OpenLDAP
 * Public License.
 *
 * A copy of this license is available in the file LICENSE in the
 * top-level directory of the distribution or, alternatively, at
 * <http://www.OpenLDAP.org/license.html>.
 */

#include "./bits.h"

#if defined(_WIN32) || defined(_WIN64)
static int waitstatus2errcode(DWORD result) {
  switch (result) {
  case WAIT_OBJECT_0:
    return MDBX_SUCCESS;
  case WAIT_FAILED:
    return GetLastError();
  case WAIT_ABANDONED:
    return ERROR_ABANDONED_WAIT_0;
  case WAIT_IO_COMPLETION:
    return ERROR_USER_APC;
  case WAIT_TIMEOUT:
    return ERROR_TIMEOUT;
  default:
    return ERROR_UNHANDLED_ERROR;
  }
}

/* Map a result from an NTAPI call to WIN32 error code. */
static int ntstatus2errcode(NTSTATUS status) {
  DWORD dummy;
  OVERLAPPED ov;
  memset(&ov, 0, sizeof(ov));
  ov.Internal = status;
  return GetOverlappedResult(NULL, &ov, &dummy, FALSE) ? MDBX_SUCCESS
                                                       : GetLastError();
}

/* We use native NT APIs to setup the memory map, so that we can
 * let the DB file grow incrementally instead of always preallocating
 * the full size. These APIs are defined in <wdm.h> and <ntifs.h>
 * but those headers are meant for driver-level development and
 * conflict with the regular user-level headers, so we explicitly
 * declare them here. Using these APIs also means we must link to
 * ntdll.dll, which is not linked by default in user code. */
#pragma comment(lib, "ntdll.lib")

#ifndef NT_SUCCESS
#define NT_SUCCESS(x) ((x) >= 0)
#define STATUS_SUCCESS ((NTSTATUS)0)
#endif
typedef struct _UNICODE_STRING {
  USHORT Length;
  USHORT MaximumLength;
  PWSTR Buffer;
} UNICODE_STRING, *PUNICODE_STRING;

typedef struct _OBJECT_ATTRIBUTES {
  ULONG Length;
  HANDLE RootDirectory;
  PUNICODE_STRING ObjectName;
  ULONG Attributes;
  PVOID SecurityDescriptor;
  PVOID SecurityQualityOfService;
} OBJECT_ATTRIBUTES, *POBJECT_ATTRIBUTES;

extern NTSTATUS NTAPI NtCreateSection(
    OUT PHANDLE SectionHandle, IN ACCESS_MASK DesiredAccess,
    IN OPTIONAL POBJECT_ATTRIBUTES ObjectAttributes,
    IN OPTIONAL PLARGE_INTEGER MaximumSize, IN ULONG SectionPageProtection,
    IN ULONG AllocationAttributes, IN OPTIONAL HANDLE FileHandle);

extern NTSTATUS NTAPI NtExtendSection(IN HANDLE SectionHandle,
                                      IN PLARGE_INTEGER NewSectionSize);

typedef enum _SECTION_INHERIT { ViewShare = 1, ViewUnmap = 2 } SECTION_INHERIT;

extern NTSTATUS NTAPI NtMapViewOfSection(
    IN HANDLE SectionHandle, IN HANDLE ProcessHandle, IN OUT PVOID *BaseAddress,
    IN ULONG_PTR ZeroBits, IN SIZE_T CommitSize,
    IN OUT OPTIONAL PLARGE_INTEGER SectionOffset, IN OUT PSIZE_T ViewSize,
    IN SECTION_INHERIT InheritDisposition, IN ULONG AllocationType,
    IN ULONG Win32Protect);

extern NTSTATUS NTAPI NtUnmapViewOfSection(IN HANDLE ProcessHandle,
                                           IN OPTIONAL PVOID BaseAddress);

extern NTSTATUS NTAPI NtClose(HANDLE Handle);

extern NTSTATUS NTAPI NtAllocateVirtualMemory(
    IN HANDLE ProcessHandle, IN OUT PVOID *BaseAddress, IN ULONG ZeroBits,
    IN OUT PULONG RegionSize, IN ULONG AllocationType, IN ULONG Protect);

extern NTSTATUS NTAPI NtFreeVirtualMemory(IN HANDLE ProcessHandle,
                                          IN PVOID *BaseAddress,
                                          IN OUT PULONG RegionSize,
                                          IN ULONG FreeType);

#endif /* _WIN32 || _WIN64 */

/*----------------------------------------------------------------------------*/

#ifndef _MSC_VER
/* Prototype should match libc runtime. ISO POSIX (2003) & LSB 3.1 */
__nothrow __noreturn void __assert_fail(const char *assertion, const char *file,
                                        unsigned line, const char *function);
#else
__extern_C __declspec(dllimport) void __cdecl _assert(char const *message,
                                                      char const *filename,
                                                      unsigned line);
#endif /* _MSC_VER */

#ifndef mdbx_assert_fail
void __cold mdbx_assert_fail(const MDBX_env *env, const char *msg,
                             const char *func, int line) {
#if MDBX_DEBUG
  if (env && env->me_assert_func) {
    env->me_assert_func(env, msg, func, line);
    return;
  }
#else
  (void)env;
#endif /* MDBX_DEBUG */

  if (mdbx_debug_logger)
    mdbx_debug_log(MDBX_DBG_ASSERT, func, line, "assert: %s\n", msg);
#ifndef _MSC_VER
  __assert_fail(msg, "mdbx", line, func);
#else
  _assert(msg, func, line);
#endif /* _MSC_VER */
}
#endif /* mdbx_assert_fail */

__cold void mdbx_panic(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
#ifdef _MSC_VER
  if (IsDebuggerPresent()) {
    OutputDebugString("\r\n" FIXME "\r\n");
    FatalExit(ERROR_UNHANDLED_ERROR);
  }
#elif _XOPEN_SOURCE >= 700 || _POSIX_C_SOURCE >= 200809L ||                    \
    (__GLIBC_PREREQ(1, 0) && !__GLIBC_PREREQ(2, 10) && defined(_GNU_SOURCE))
  vdprintf(STDERR_FILENO, fmt, ap);
#else
#error FIXME
#endif
  va_end(ap);
  abort();
}

/*----------------------------------------------------------------------------*/

#ifndef mdbx_asprintf
int mdbx_asprintf(char **strp, const char *fmt, ...) {
  va_list ap, ones;

  va_start(ap, fmt);
  va_copy(ones, ap);
#ifdef _MSC_VER
  int needed = _vscprintf(fmt, ap);
#elif defined(vsnprintf) || defined(_BSD_SOURCE) || _XOPEN_SOURCE >= 500 ||    \
    defined(_ISOC99_SOURCE) || _POSIX_C_SOURCE >= 200112L
  int needed = vsnprintf(nullptr, 0, fmt, ap);
#else
#error FIXME
#endif
  va_end(ap);

  if (unlikely(needed < 0 || needed >= INT_MAX)) {
    *strp = NULL;
    va_end(ones);
    return needed;
  }

  *strp = malloc(needed + 1);
  if (unlikely(*strp == NULL)) {
    va_end(ones);
    SetLastError(MDBX_ENOMEM);
    return -1;
  }

#if defined(vsnprintf) || defined(_BSD_SOURCE) || _XOPEN_SOURCE >= 500 ||      \
    defined(_ISOC99_SOURCE) || _POSIX_C_SOURCE >= 200112L
  int actual = vsnprintf(*strp, needed + 1, fmt, ones);
#else
#error FIXME
#endif
  va_end(ones);

  assert(actual == needed);
  if (unlikely(actual < 0)) {
    free(*strp);
    *strp = NULL;
  }
  return actual;
}
#endif /* mdbx_asprintf */

#ifndef mdbx_memalign_alloc
int mdbx_memalign_alloc(size_t alignment, size_t bytes, void **result) {
#if _MSC_VER
  *result = _aligned_malloc(bytes, alignment);
  return *result ? MDBX_SUCCESS : MDBX_ENOMEM /* ERROR_OUTOFMEMORY */;
#elif __GLIBC_PREREQ(2, 16) || __STDC_VERSION__ >= 201112L
  *result = memalign(alignment, bytes);
  return *result ? MDBX_SUCCESS : errno;
#elif _POSIX_VERSION >= 200112L
  *result = NULL;
  return posix_memalign(result, alignment, bytes);
#else
#error FIXME
#endif
}
#endif /* mdbx_memalign_alloc */

#ifndef mdbx_memalign_free
void mdbx_memalign_free(void *ptr) {
#if _MSC_VER
  _aligned_free(ptr);
#else
  free(ptr);
#endif
}
#endif /* mdbx_memalign_free */

/*----------------------------------------------------------------------------*/

int mdbx_condmutex_init(mdbx_condmutex_t *condmutex) {
#if defined(_WIN32) || defined(_WIN64)
  int rc = MDBX_SUCCESS;
  condmutex->event = NULL;
  condmutex->mutex = CreateMutex(NULL, FALSE, NULL);
  if (!condmutex->mutex)
    return GetLastError();

  condmutex->event = CreateEvent(NULL, FALSE, FALSE, NULL);
  if (!condmutex->event) {
    rc = GetLastError();
    (void)CloseHandle(condmutex->mutex);
    condmutex->mutex = NULL;
  }
  return rc;
#else
  memset(condmutex, 0, sizeof(mdbx_condmutex_t));
  int rc = pthread_mutex_init(&condmutex->mutex, NULL);
  if (rc == 0) {
    rc = pthread_cond_init(&condmutex->cond, NULL);
    if (rc != 0)
      (void)pthread_mutex_destroy(&condmutex->mutex);
  }
  return rc;
#endif
}

static bool is_allzeros(const void *ptr, size_t bytes) {
  const uint8_t *u8 = ptr;
  for (size_t i = 0; i < bytes; ++i)
    if (u8[i] != 0)
      return false;
  return true;
}

int mdbx_condmutex_destroy(mdbx_condmutex_t *condmutex) {
  int rc = MDBX_EINVAL;
#if defined(_WIN32) || defined(_WIN64)
  if (condmutex->event) {
    rc = CloseHandle(condmutex->event) ? MDBX_SUCCESS : GetLastError();
    if (rc == MDBX_SUCCESS)
      condmutex->event = NULL;
  }
  if (condmutex->mutex) {
    rc = CloseHandle(condmutex->mutex) ? MDBX_SUCCESS : GetLastError();
    if (rc == MDBX_SUCCESS)
      condmutex->mutex = NULL;
  }
#else
  if (!is_allzeros(&condmutex->cond, sizeof(condmutex->cond))) {
    rc = pthread_cond_destroy(&condmutex->cond);
    if (rc == 0)
      memset(&condmutex->cond, 0, sizeof(condmutex->cond));
  }
  if (!is_allzeros(&condmutex->mutex, sizeof(condmutex->mutex))) {
    rc = pthread_mutex_destroy(&condmutex->mutex);
    if (rc == 0)
      memset(&condmutex->mutex, 0, sizeof(condmutex->mutex));
  }
#endif
  return rc;
}

int mdbx_condmutex_lock(mdbx_condmutex_t *condmutex) {
#if defined(_WIN32) || defined(_WIN64)
  DWORD code = WaitForSingleObject(condmutex->mutex, INFINITE);
  return waitstatus2errcode(code);
#else
  return pthread_mutex_lock(&condmutex->mutex);
#endif
}

int mdbx_condmutex_unlock(mdbx_condmutex_t *condmutex) {
#if defined(_WIN32) || defined(_WIN64)
  return ReleaseMutex(condmutex->mutex) ? MDBX_SUCCESS : GetLastError();
#else
  return pthread_mutex_unlock(&condmutex->mutex);
#endif
}

int mdbx_condmutex_signal(mdbx_condmutex_t *condmutex) {
#if defined(_WIN32) || defined(_WIN64)
  return SetEvent(condmutex->event) ? MDBX_SUCCESS : GetLastError();
#else
  return pthread_cond_signal(&condmutex->cond);
#endif
}

int mdbx_condmutex_wait(mdbx_condmutex_t *condmutex) {
#if defined(_WIN32) || defined(_WIN64)
  DWORD code =
      SignalObjectAndWait(condmutex->mutex, condmutex->event, INFINITE, FALSE);
  if (code == WAIT_OBJECT_0)
    code = WaitForSingleObject(condmutex->mutex, INFINITE);
  return waitstatus2errcode(code);
#else
  return pthread_cond_wait(&condmutex->cond, &condmutex->mutex);
#endif
}

/*----------------------------------------------------------------------------*/

int mdbx_fastmutex_init(mdbx_fastmutex_t *fastmutex) {
#if defined(_WIN32) || defined(_WIN64)
  InitializeCriticalSection(fastmutex);
  return MDBX_SUCCESS;
#else
  return pthread_mutex_init(fastmutex, NULL);
#endif
}

int mdbx_fastmutex_destroy(mdbx_fastmutex_t *fastmutex) {
#if defined(_WIN32) || defined(_WIN64)
  DeleteCriticalSection(fastmutex);
  return MDBX_SUCCESS;
#else
  return pthread_mutex_destroy(fastmutex);
#endif
}

int mdbx_fastmutex_acquire(mdbx_fastmutex_t *fastmutex) {
#if defined(_WIN32) || defined(_WIN64)
  EnterCriticalSection(fastmutex);
  return MDBX_SUCCESS;
#else
  return pthread_mutex_lock(fastmutex);
#endif
}

int mdbx_fastmutex_release(mdbx_fastmutex_t *fastmutex) {
#if defined(_WIN32) || defined(_WIN64)
  LeaveCriticalSection(fastmutex);
  return MDBX_SUCCESS;
#else
  return pthread_mutex_unlock(fastmutex);
#endif
}

/*----------------------------------------------------------------------------*/

int mdbx_openfile(const char *pathname, int flags, mode_t mode,
                  mdbx_filehandle_t *fd) {
  *fd = INVALID_HANDLE_VALUE;
#if defined(_WIN32) || defined(_WIN64)
  (void)mode;

  DWORD DesiredAccess;
  DWORD ShareMode = FILE_SHARE_READ | FILE_SHARE_WRITE;
  DWORD FlagsAndAttributes = FILE_ATTRIBUTE_NORMAL;
  switch (flags & (O_RDONLY | O_WRONLY | O_RDWR)) {
  default:
    return ERROR_INVALID_PARAMETER;
  case O_RDONLY:
    DesiredAccess = GENERIC_READ;
    break;
  case O_WRONLY: /* assume for MDBX_env_copy() and friends output */
    DesiredAccess = GENERIC_WRITE;
    ShareMode = 0;
    FlagsAndAttributes |= FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH;
    break;
  case O_RDWR:
    DesiredAccess = GENERIC_READ | GENERIC_WRITE;
    break;
  }

  DWORD CreationDisposition;
  switch (flags & (O_EXCL | O_CREAT)) {
  default:
    return ERROR_INVALID_PARAMETER;
  case 0:
    CreationDisposition = OPEN_EXISTING;
    break;
  case O_EXCL | O_CREAT:
    CreationDisposition = CREATE_NEW;
    FlagsAndAttributes |= FILE_ATTRIBUTE_NOT_CONTENT_INDEXED;
    break;
  case O_CREAT:
    CreationDisposition = OPEN_ALWAYS;
    FlagsAndAttributes |= FILE_ATTRIBUTE_NOT_CONTENT_INDEXED;
    break;
  }

  *fd = CreateFileA(pathname, DesiredAccess, ShareMode, NULL,
                    CreationDisposition, FlagsAndAttributes, NULL);

  if (*fd == INVALID_HANDLE_VALUE)
    return GetLastError();
  if ((flags & O_CREAT) && GetLastError() != ERROR_ALREADY_EXISTS) {
    /* set FILE_ATTRIBUTE_NOT_CONTENT_INDEXED for new file */
    DWORD FileAttributes = GetFileAttributesA(pathname);
    if (FileAttributes == INVALID_FILE_ATTRIBUTES ||
        !SetFileAttributesA(pathname, FileAttributes |
                                          FILE_ATTRIBUTE_NOT_CONTENT_INDEXED)) {
      int rc = GetLastError();
      CloseHandle(*fd);
      *fd = INVALID_HANDLE_VALUE;
      return rc;
    }
  }
#else

#ifdef O_CLOEXEC
  flags |= O_CLOEXEC;
#endif
  *fd = open(pathname, flags, mode);
  if (*fd < 0)
    return errno;
#if defined(FD_CLOEXEC) && defined(F_GETFD)
  flags = fcntl(*fd, F_GETFD);
  if (flags >= 0)
    (void)fcntl(*fd, F_SETFD, flags | FD_CLOEXEC);
#endif
#endif
  return MDBX_SUCCESS;
}

int mdbx_closefile(mdbx_filehandle_t fd) {
#if defined(_WIN32) || defined(_WIN64)
  return CloseHandle(fd) ? MDBX_SUCCESS : GetLastError();
#else
  return (close(fd) == 0) ? MDBX_SUCCESS : errno;
#endif
}

int mdbx_pread(mdbx_filehandle_t fd, void *buf, size_t bytes, uint64_t offset) {
  if (bytes > MAX_WRITE)
    return MDBX_EINVAL;
#if defined(_WIN32) || defined(_WIN64)

  OVERLAPPED ov;
  ov.hEvent = 0;
  ov.Offset = (DWORD)offset;
  ov.OffsetHigh = HIGH_DWORD(offset);

  DWORD read = 0;
  if (unlikely(!ReadFile(fd, buf, (DWORD)bytes, &read, &ov))) {
    int rc = GetLastError();
    return (rc == MDBX_SUCCESS) ? /* paranoia */ ERROR_READ_FAULT : rc;
  }
#else
  STATIC_ASSERT_MSG(sizeof(off_t) >= sizeof(size_t),
                    "libmdbx requires 64-bit file I/O on 64-bit systems");
  ssize_t read = pread(fd, buf, bytes, offset);
  if (read < 0) {
    int rc = errno;
    return (rc == MDBX_SUCCESS) ? /* paranoia */ MDBX_EIO : rc;
  }
#endif
  return (bytes == (size_t)read) ? MDBX_SUCCESS : MDBX_ENODATA;
}

int mdbx_pwrite(mdbx_filehandle_t fd, const void *buf, size_t bytes,
                uint64_t offset) {
#if defined(_WIN32) || defined(_WIN64)
  if (bytes > MAX_WRITE)
    return ERROR_INVALID_PARAMETER;

  OVERLAPPED ov;
  ov.hEvent = 0;
  ov.Offset = (DWORD)offset;
  ov.OffsetHigh = HIGH_DWORD(offset);

  DWORD written;
  if (likely(WriteFile(fd, buf, (DWORD)bytes, &written, &ov)))
    return (bytes == written) ? MDBX_SUCCESS : MDBX_EIO /* ERROR_WRITE_FAULT */;
  return GetLastError();
#else
  int rc;
  ssize_t written;
  do {
    STATIC_ASSERT_MSG(sizeof(off_t) >= sizeof(size_t),
                      "libmdbx requires 64-bit file I/O on 64-bit systems");
    written = pwrite(fd, buf, bytes, offset);
    if (likely(bytes == (size_t)written))
      return MDBX_SUCCESS;
    rc = errno;
  } while (rc == EINTR);
  return (written < 0) ? rc : MDBX_EIO /* Use which error code (ENOSPC)? */;
#endif
}

int mdbx_pwritev(mdbx_filehandle_t fd, struct iovec *iov, int iovcnt,
                 uint64_t offset, size_t expected_written) {
#if defined(_WIN32) || defined(_WIN64)
  size_t written = 0;
  for (int i = 0; i < iovcnt; ++i) {
    int rc = mdbx_pwrite(fd, iov[i].iov_base, iov[i].iov_len, offset);
    if (unlikely(rc != MDBX_SUCCESS))
      return rc;
    written += iov[i].iov_len;
    offset += iov[i].iov_len;
  }
  return (expected_written == written) ? MDBX_SUCCESS
                                       : MDBX_EIO /* ERROR_WRITE_FAULT */;
#else
  int rc;
  ssize_t written;
  do {
    STATIC_ASSERT_MSG(sizeof(off_t) >= sizeof(size_t),
                      "libmdbx requires 64-bit file I/O on 64-bit systems");
    written = pwritev(fd, iov, iovcnt, offset);
    if (likely(expected_written == (size_t)written))
      return MDBX_SUCCESS;
    rc = errno;
  } while (rc == EINTR);
  return (written < 0) ? rc : MDBX_EIO /* Use which error code? */;
#endif
}

int mdbx_write(mdbx_filehandle_t fd, const void *buf, size_t bytes) {
#ifdef SIGPIPE
  sigset_t set, old;
  sigemptyset(&set);
  sigaddset(&set, SIGPIPE);
  int rc = pthread_sigmask(SIG_BLOCK, &set, &old);
  if (rc != 0)
    return rc;
#endif

  const char *ptr = buf;
  for (;;) {
    size_t chunk = (MAX_WRITE < bytes) ? MAX_WRITE : bytes;
#if defined(_WIN32) || defined(_WIN64)
    DWORD written;
    if (unlikely(!WriteFile(fd, ptr, (DWORD)chunk, &written, NULL)))
      return GetLastError();
#else
    ssize_t written = write(fd, ptr, chunk);
    if (written < 0) {
      int rc = errno;
#ifdef SIGPIPE
      if (rc == EPIPE) {
        /* Collect the pending SIGPIPE, otherwise at least OS X
         * gives it to the process on thread-exit (ITS#8504). */
        int tmp;
        sigwait(&set, &tmp);
        written = 0;
        continue;
      }
      pthread_sigmask(SIG_SETMASK, &old, NULL);
#endif
      return rc;
    }
#endif
    if (likely(bytes == (size_t)written)) {
#ifdef SIGPIPE
      pthread_sigmask(SIG_SETMASK, &old, NULL);
#endif
      return MDBX_SUCCESS;
    }
    ptr += written;
    bytes -= written;
  }
}

int mdbx_filesync(mdbx_filehandle_t fd, bool fullsync) {
#if defined(_WIN32) || defined(_WIN64)
  (void)fullsync;
  return FlushFileBuffers(fd) ? MDBX_SUCCESS : GetLastError();
#elif __GLIBC_PREREQ(2, 16) || _BSD_SOURCE || _XOPEN_SOURCE ||                 \
    (__GLIBC_PREREQ(2, 8) && _POSIX_C_SOURCE >= 200112L)
  for (;;) {
/* LY: It is no reason to use fdatasync() here, even in case
 * no such bug in a kernel. Because "no-bug" mean that a kernel
 * internally do nearly the same, e.g. fdatasync() == fsync()
 * when no-kernel-bug and file size was changed.
 *
 * So, this code is always safe and without appreciable
 * performance degradation.
 *
 * For more info about of a corresponding fdatasync() bug
 * see http://www.spinics.net/lists/linux-ext4/msg33714.html */
#if _POSIX_C_SOURCE >= 199309L || _XOPEN_SOURCE >= 500 ||                      \
    defined(_POSIX_SYNCHRONIZED_IO)
    if (!fullsync && fdatasync(fd) == 0)
      return MDBX_SUCCESS;
#else
    (void)fullsync;
#endif
    if (fsync(fd) == 0)
      return MDBX_SUCCESS;
    int rc = errno;
    if (rc != EINTR)
      return rc;
  }
#else
#error FIXME
#endif
}

int mdbx_filesize_sync(mdbx_filehandle_t fd) {
#if defined(_WIN32) || defined(_WIN64)
  (void)fd;
  /* Nothing on Windows (i.e. newer 100% steady) */
  return MDBX_SUCCESS;
#else
  for (;;) {
    if (fsync(fd) == 0)
      return MDBX_SUCCESS;
    int rc = errno;
    if (rc != EINTR)
      return rc;
  }
#endif
}

int mdbx_filesize(mdbx_filehandle_t fd, uint64_t *length) {
#if defined(_WIN32) || defined(_WIN64)
  BY_HANDLE_FILE_INFORMATION info;
  if (!GetFileInformationByHandle(fd, &info))
    return GetLastError();
  *length = info.nFileSizeLow | (uint64_t)info.nFileSizeHigh << 32;
#else
  struct stat st;

  STATIC_ASSERT_MSG(sizeof(off_t) <= sizeof(uint64_t),
                    "libmdbx requires 64-bit file I/O on 64-bit systems");
  if (fstat(fd, &st))
    return errno;

  *length = st.st_size;
#endif
  return MDBX_SUCCESS;
}

int mdbx_ftruncate(mdbx_filehandle_t fd, uint64_t length) {
#if defined(_WIN32) || defined(_WIN64)
  LARGE_INTEGER li;
  li.QuadPart = length;
  return (SetFilePointerEx(fd, li, NULL, FILE_BEGIN) && SetEndOfFile(fd))
             ? MDBX_SUCCESS
             : GetLastError();
#else
  STATIC_ASSERT_MSG(sizeof(off_t) >= sizeof(size_t),
                    "libmdbx requires 64-bit file I/O on 64-bit systems");
  return ftruncate(fd, length) == 0 ? MDBX_SUCCESS : errno;
#endif
}

/*----------------------------------------------------------------------------*/

int mdbx_thread_key_create(mdbx_thread_key_t *key) {
#if defined(_WIN32) || defined(_WIN64)
  *key = TlsAlloc();
  return (*key != TLS_OUT_OF_INDEXES) ? MDBX_SUCCESS : GetLastError();
#else
  return pthread_key_create(key, mdbx_rthc_dtor);
#endif
}

void mdbx_thread_key_delete(mdbx_thread_key_t key) {
#if defined(_WIN32) || defined(_WIN64)
  mdbx_ensure(NULL, TlsFree(key));
#else
  mdbx_ensure(NULL, pthread_key_delete(key) == 0);
#endif
}

void *mdbx_thread_rthc_get(mdbx_thread_key_t key) {
#if defined(_WIN32) || defined(_WIN64)
  return TlsGetValue(key);
#else
  return pthread_getspecific(key);
#endif
}

void mdbx_thread_rthc_set(mdbx_thread_key_t key, const void *value) {
#if defined(_WIN32) || defined(_WIN64)
  mdbx_ensure(NULL, TlsSetValue(key, (void *)value));
#else
  mdbx_ensure(NULL, pthread_setspecific(key, value) == 0);
#endif
}

int mdbx_thread_create(mdbx_thread_t *thread,
                       THREAD_RESULT(THREAD_CALL *start_routine)(void *),
                       void *arg) {
#if defined(_WIN32) || defined(_WIN64)
  *thread = CreateThread(NULL, 0, start_routine, arg, 0, NULL);
  return *thread ? MDBX_SUCCESS : GetLastError();
#else
  return pthread_create(thread, NULL, start_routine, arg);
#endif
}

int mdbx_thread_join(mdbx_thread_t thread) {
#if defined(_WIN32) || defined(_WIN64)
  DWORD code = WaitForSingleObject(thread, INFINITE);
  return waitstatus2errcode(code);
#else
  void *unused_retval = &unused_retval;
  return pthread_join(thread, &unused_retval);
#endif
}

/*----------------------------------------------------------------------------*/

int mdbx_msync(void *addr, size_t length, int async) {
#if defined(_WIN32) || defined(_WIN64)
  if (async)
    return MDBX_SUCCESS;
  return FlushViewOfFile(addr, length) ? MDBX_SUCCESS : GetLastError();
#else
  const int mode = async ? MS_ASYNC : MS_SYNC;
  return (msync(addr, length, mode) == 0) ? MDBX_SUCCESS : errno;
#endif
}

int mdbx_mmap(int flags, mdbx_mmap_param_t *map, size_t length, size_t limit) {
#if defined(_WIN32) || defined(_WIN64)
  NTSTATUS rc = NtCreateSection(
      &map->section,
      /* DesiredAccess */ SECTION_MAP_READ | SECTION_EXTEND_SIZE |
          ((flags & MDBX_WRITEMAP) ? SECTION_MAP_WRITE : 0),
      /* ObjectAttributes */ NULL, /* MaximumSize */ NULL,
      /* SectionPageProtection */ (flags & MDBX_RDONLY) ? PAGE_READONLY
                                                        : PAGE_READWRITE,
      /* AllocationAttributes */ SEC_RESERVE, map->fd);

  if (!NT_SUCCESS(rc)) {
    map->section = 0;
    map->address = MAP_FAILED;
    return ntstatus2errcode(rc);
  }

  map->address = NULL;
  SIZE_T ViewSize = limit;
  rc = NtMapViewOfSection(
      map->section, GetCurrentProcess(), &map->address,
      /* ZeroBits */ 0,
      /* CommitSize */ length,
      /* SectionOffset */ NULL, &ViewSize,
      /* InheritDisposition */ ViewUnmap,
      /* AllocationType */ (flags & MDBX_RDONLY) ? 0 : MEM_RESERVE,
      /* Win32Protect */ (flags & MDBX_WRITEMAP) ? PAGE_READWRITE
                                                 : PAGE_READONLY);

  if (!NT_SUCCESS(rc)) {
    NtClose(map->section);
    map->section = 0;
    map->address = MAP_FAILED;
    return ntstatus2errcode(rc);
  }

  assert(map->address != MAP_FAILED);
  return MDBX_SUCCESS;
#else
  (void)length;
  map->address = mmap(
      NULL, limit, (flags & MDBX_WRITEMAP) ? PROT_READ | PROT_WRITE : PROT_READ,
      MAP_SHARED, map->fd, 0);
  return (map->address != MAP_FAILED) ? MDBX_SUCCESS : errno;
#endif
}

int mdbx_munmap(mdbx_mmap_param_t *map, size_t length) {
#if defined(_WIN32) || defined(_WIN64)
  (void)length;
  if (map->section)
    NtClose(map->section);
  NTSTATUS rc = NtUnmapViewOfSection(GetCurrentProcess(), map->address);
  return NT_SUCCESS(rc) ? MDBX_SUCCESS : ntstatus2errcode(rc);
#else
  return (munmap(map->address, length) == 0) ? MDBX_SUCCESS : errno;
#endif
}

int mdbx_mlock(mdbx_mmap_param_t *map, size_t length) {
#if defined(_WIN32) || defined(_WIN64)
  return VirtualLock(map->address, length) ? MDBX_SUCCESS : GetLastError();
#else
  return (mlock(map->address, length) == 0) ? MDBX_SUCCESS : errno;
#endif
}

int mdbx_mresize(int flags, mdbx_mmap_param_t *map, size_t current,
                 size_t wanna) {
#if defined(_WIN32) || defined(_WIN64)
  if (wanna > current) {
    /* growth */
    uint8_t *ptr = (uint8_t *)map->address + current;
    return (ptr == VirtualAlloc(ptr, wanna - current, MEM_COMMIT,
                                (flags & MDBX_WRITEMAP) ? PAGE_READWRITE
                                                        : PAGE_READONLY))
               ? MDBX_SUCCESS
               : GetLastError();
  }
  /* Windows is unable shrinking a mapped file */
  return MDBX_RESULT_TRUE;
#else
  (void)flags;
  (void)current;
  return mdbx_ftruncate(map->fd, wanna);
#endif
}

int mdbx_mremap(int flags, mdbx_mmap_param_t *map, size_t old_limit,
                size_t new_limit) {
#if defined(_WIN32) || defined(_WIN64)
  (void)flags;
  if (old_limit > new_limit) {
    /* Windows is unable shrinking a mapped section */
    return ERROR_USER_MAPPED_FILE;
  }
  LARGE_INTEGER new_size;
  new_size.QuadPart = new_limit;
  NTSTATUS rc = NtExtendSection(map->section, &new_size);
  return NT_SUCCESS(rc) ? MDBX_SUCCESS : ntstatus2errcode(rc);
#else
  (void)flags;
  void *ptr = mremap(map->address, old_limit, new_limit, 0);
  if (ptr == MAP_FAILED)
    return errno;
  map->address = ptr;
  return MDBX_SUCCESS;
#endif
}

/*----------------------------------------------------------------------------*/

__cold void mdbx_osal_jitter(bool tiny) {
  for (;;) {
#if defined(_M_IX86) || defined(_M_X64) || defined(__i386__) ||                \
    defined(__x86_64__)
    const unsigned salt = 277u * (unsigned)__rdtsc();
#else
    const unsigned salt = rand();
#endif

    const unsigned coin = salt % (tiny ? 29u : 43u);
    if (coin < 43 / 3)
      break;
#if defined(_WIN32) || defined(_WIN64)
    SwitchToThread();
    if (coin > 43 * 2 / 3)
      Sleep(1);
#else
    sched_yield();
    if (coin > 43 * 2 / 3)
      usleep(coin);
#endif
  }
}
