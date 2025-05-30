/**
 * @file dirctl-test.c
 *
 * @copyright 2015-2025 Bill Zissimopoulos
 */
/*
 * This file is part of WinFsp.
 *
 * You can redistribute it and/or modify it under the terms of the GNU
 * General Public License version 3 as published by the Free Software
 * Foundation.
 *
 * Licensees holding a valid commercial license may use this software
 * in accordance with the commercial license agreement provided in
 * conjunction with the software.  The terms and conditions of any such
 * commercial license agreement shall govern, supersede, and render
 * ineffective any application of the GPLv3 license to this software,
 * notwithstanding of any reference thereto in the software or
 * associated repository.
 */

#include <winfsp/winfsp.h>
#include <tlib/testsuite.h>
#include <process.h>
#include <sddl.h>
#include <strsafe.h>
#include "memfs.h"

#include "winfsp-tests.h"

typedef struct _FILE_FULL_DIR_INFORMATION
{
    ULONG NextEntryOffset;
    ULONG FileIndex;
    LARGE_INTEGER CreationTime;
    LARGE_INTEGER LastAccessTime;
    LARGE_INTEGER LastWriteTime;
    LARGE_INTEGER ChangeTime;
    LARGE_INTEGER EndOfFile;
    LARGE_INTEGER AllocationSize;
    ULONG FileAttributes;
    ULONG FileNameLength;
    ULONG EaSize;
    WCHAR FileName[1];
} FILE_FULL_DIR_INFORMATION, *PFILE_FULL_DIR_INFORMATION;

typedef struct _FILE_DIRECTORY_INFORMATION
{
    ULONG NextEntryOffset;
    ULONG FileIndex;
    LARGE_INTEGER CreationTime;
    LARGE_INTEGER LastAccessTime;
    LARGE_INTEGER LastWriteTime;
    LARGE_INTEGER ChangeTime;
    LARGE_INTEGER EndOfFile;
    LARGE_INTEGER AllocationSize;
    ULONG FileAttributes;
    ULONG FileNameLength;
    WCHAR FileName[1];
} FILE_DIRECTORY_INFORMATION, *PFILE_DIRECTORY_INFORMATION;

NTSTATUS
NTAPI
NtQueryDirectoryFile (
    HANDLE FileHandle,
    HANDLE Event,
    PIO_APC_ROUTINE ApcRoutine,
    PVOID ApcContext,
    PIO_STATUS_BLOCK IoStatusBlock,
    PVOID FileInformation,
    ULONG Length,
    FILE_INFORMATION_CLASS FileInformationClass,
    BOOLEAN ReturnSingleEntry,
    PUNICODE_STRING FileName,
    BOOLEAN RestartScan);

static void querydir_dotest(ULONG Flags, PWSTR Prefix, ULONG FileInfoTimeout, ULONG SleepTimeout)
{
    void *memfs = memfs_start_ex(Flags, FileInfoTimeout);

    HANDLE Handle;
    BOOL Success;
    WCHAR FilePath[MAX_PATH];
    WIN32_FIND_DATAW FindData;
    ULONG FileCount, FileTotal;

    for (int i = 1; 10 >= i; i++)
    {
        StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\dir%d",
            Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs), i);
        Success = CreateDirectoryW(FilePath, 0);
        ASSERT(Success);
    }

    for (int j = 1; 400 >= j; j++)
    {
        StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\fileABCDEFGHIJKLMNOPQRSTUVXWYZfile%d",
            Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs), j);
        Handle = CreateFileW(FilePath, GENERIC_ALL, 0, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
        ASSERT(INVALID_HANDLE_VALUE != Handle);
        Success = CloseHandle(Handle);
        ASSERT(Success);
    }

    for (int j = 1; 100 >= j; j++)
    {
        StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\dir5\\file%d",
            Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs), j);
        Handle = CreateFileW(FilePath, GENERIC_ALL, 0, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
        ASSERT(INVALID_HANDLE_VALUE != Handle);
        Success = CloseHandle(Handle);
        ASSERT(Success);
    }

    DWORD times[2];
    times[0] = GetTickCount();

    if (-1 != Flags)
    {
        StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\*",
            Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));
        Handle = FindFirstFileW(FilePath, &FindData);
        ASSERT(INVALID_HANDLE_VALUE != Handle);

        FileCount = 0;
        do
        {
            ASSERT(
                0 == mywcscmp(FindData.cFileName, 4, L"file", 4) ||
                0 == mywcscmp(FindData.cFileName, 3, L"dir", 3));

            FileCount++;

            if (0 < SleepTimeout && 5 == FileCount)
                Sleep(SleepTimeout);
        } while (FindNextFileW(Handle, &FindData));
        ASSERT(ERROR_NO_MORE_FILES == GetLastError());

        ASSERT(410 == FileCount);

        Success = FindClose(Handle);
        ASSERT(Success);
    }

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\dir5\\*",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));
    Handle = FindFirstFileW(FilePath, &FindData);
    ASSERT(INVALID_HANDLE_VALUE != Handle);

    FileCount = FileTotal = 0;
    do
    {
        unsigned long ul;
        wchar_t *endp;

        if (2 > FileCount)
        {
            FileCount++;
            ASSERT(0 != (FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY));
            ASSERT(FileCount == wcslen(FindData.cFileName));
            ASSERT(0 == mywcscmp(FindData.cFileName, FileCount, L"..", FileCount));
            continue;
        }

        ASSERT(0 == (FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY));
        ASSERT(0 == mywcscmp(FindData.cFileName, 4, L"file", 4));
        ul = wcstoul(FindData.cFileName + 4, &endp, 10);
        ASSERT(0 != ul);
        ASSERT(L'\0' == *endp);

        FileCount++;
        FileTotal += ul;

        if (0 < SleepTimeout && 5 == FileCount)
            Sleep(SleepTimeout);
    } while (FindNextFileW(Handle, &FindData));
    ASSERT(ERROR_NO_MORE_FILES == GetLastError());

    ASSERT(102 == FileCount);
    ASSERT(101 * 100 / 2 == FileTotal);

    Success = FindClose(Handle);
    ASSERT(Success);

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\dir*",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));
    Handle = FindFirstFileW(FilePath, &FindData);
    ASSERT(INVALID_HANDLE_VALUE != Handle);

    FileCount = FileTotal = 0;
    do
    {
        unsigned long ul;
        wchar_t *endp;

        ASSERT(0 != (FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY));
        ASSERT(0 == mywcscmp(FindData.cFileName, 3, L"dir", 3));
        ul = wcstoul(FindData.cFileName + 3, &endp, 10);
        ASSERT(0 != ul);
        ASSERT(L'\0' == *endp);

        FileCount++;
        FileTotal += ul;

        if (0 < SleepTimeout && 5 == FileCount)
            Sleep(SleepTimeout);
    } while (FindNextFileW(Handle, &FindData));
    ASSERT(ERROR_NO_MORE_FILES == GetLastError());

    ASSERT(10 == FileCount);
    ASSERT(11 * 10 / 2 == FileTotal);

    Success = FindClose(Handle);
    ASSERT(Success);

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\file*",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));
    Handle = FindFirstFileW(FilePath, &FindData);
    ASSERT(INVALID_HANDLE_VALUE != Handle);

    FileCount = FileTotal = 0;
    do
    {
        unsigned long ul;
        wchar_t *endp;
        size_t wcscnt = sizeof "fileABCDEFGHIJKLMNOPQRSTUVXWYZfile" - 1/* count of wchar_t*/;

        ASSERT(0 == (FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY));
        ASSERT(0 == mywcscmp(FindData.cFileName, (int)wcscnt,
            L"fileABCDEFGHIJKLMNOPQRSTUVXWYZfile", (int)wcscnt));
        ul = wcstoul(FindData.cFileName + wcscnt, &endp, 10);
        ASSERT(0 != ul);
        ASSERT(L'\0' == *endp);

        FileCount++;
        FileTotal += ul;

        if (0 < SleepTimeout && 5 == FileCount)
            Sleep(SleepTimeout);
    } while (FindNextFileW(Handle, &FindData));
    ASSERT(ERROR_NO_MORE_FILES == GetLastError());

    ASSERT(400 == FileCount);
    ASSERT(401 * 400 / 2 == FileTotal);

    Success = FindClose(Handle);
    ASSERT(Success);

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\DOES-NOT-EXIST",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));
    Handle = FindFirstFileW(FilePath, &FindData);
    ASSERT(INVALID_HANDLE_VALUE == Handle);
    ASSERT(ERROR_FILE_NOT_FOUND == GetLastError());

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\DOES-NOT-EXIST*",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));
    Handle = FindFirstFileW(FilePath, &FindData);
    ASSERT(INVALID_HANDLE_VALUE == Handle);
    ASSERT(ERROR_FILE_NOT_FOUND == GetLastError());

    times[1] = GetTickCount();
    FspDebugLog(__FUNCTION__ "(Flags=%lx, Prefix=\"%S\", FileInfoTimeout=%ld, SleepTimeout=%ld): %ldms\n",
        Flags, Prefix, FileInfoTimeout, SleepTimeout, times[1] - times[0]);

    for (int j = 1; 100 >= j; j++)
    {
        StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\dir5\\file%d",
            Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs), j);
        Success = DeleteFileW(FilePath);
        ASSERT(Success);
    }

    for (int j = 1; 400 >= j; j++)
    {
        StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\fileABCDEFGHIJKLMNOPQRSTUVXWYZfile%d",
            Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs), j);
        Success = DeleteFileW(FilePath);
        ASSERT(Success);
    }

    for (int i = 1; 10 >= i; i++)
    {
        StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\dir%d",
            Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs), i);
        Success = RemoveDirectoryW(FilePath);
        ASSERT(Success);
    }

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\file*",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));
    Handle = FindFirstFileW(FilePath, &FindData);
    ASSERT(INVALID_HANDLE_VALUE == Handle);
    ASSERT(ERROR_FILE_NOT_FOUND == GetLastError());

    memfs_stop(memfs);
}

void querydir_test(void)
{
    if (NtfsTests)
    {
        WCHAR DirBuf[MAX_PATH];
        GetTestDirectory(DirBuf);
        querydir_dotest(-1, DirBuf, 0, 0);
    }
    if (WinFspDiskTests)
    {
        querydir_dotest(MemfsDisk, 0, 0, 0);
        querydir_dotest(MemfsDisk, 0, 1000, 0);
    }
    if (WinFspNetTests)
    {
        querydir_dotest(MemfsNet, L"\\\\memfs\\share", 0, 0);
        querydir_dotest(MemfsNet, L"\\\\memfs\\share", 1000, 0);
    }
}

static inline size_t hash_chars(const wchar_t *s, size_t length)
{
    /* djb2: see http://www.cse.yorku.ca/~oz/hash.html */
    size_t h = 5381;
    for (const wchar_t *t = s + length; t > s; ++s)
        h = 33 * h + *s;
    return h;
}
static NTSTATUS querydir_nodup_dotest_thread_loop(void *FilePath)
{
    NTSTATUS Result;
    void *Buffer = 0;
    HANDLE Handle = INVALID_HANDLE_VALUE;
    IO_STATUS_BLOCK IoStatus;
    size_t hash[64], hash_count = 0;

    Buffer = malloc(4096);
    if (0 == Buffer)
    {
        Result = STATUS_INSUFFICIENT_RESOURCES;
        goto exit;
    }

    Handle = CreateFileW(
        FilePath,
        FILE_LIST_DIRECTORY,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        0,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS,
        0);
    if (INVALID_HANDLE_VALUE == Handle)
    {
        Result = FspNtStatusFromWin32(GetLastError());
        goto exit;
    }

    for (;;)
    {
        Result = NtQueryDirectoryFile(
            Handle,
            0, 0, 0,
            &IoStatus,
            Buffer,
            4096,
            2/*FileFullDirectoryInformation*/,
            FALSE,
            0,
            FALSE);
        if (STATUS_NO_MORE_FILES == Result)
            break;
        if (!NT_SUCCESS(Result))
            goto exit;

        for (FILE_FULL_DIR_INFORMATION *DirInfo = Buffer;;
            DirInfo = (PVOID)((PUINT8)DirInfo + DirInfo->NextEntryOffset))
        {
            size_t h = hash_chars(DirInfo->FileName, DirInfo->FileNameLength / sizeof(WCHAR));
            for (size_t i = 0; hash_count > i; i++)
                if (hash[i] == h)
                {
                    WCHAR FileName[256];
                    memcpy(FileName, DirInfo->FileName, DirInfo->FileNameLength);
                    FileName[DirInfo->FileNameLength / sizeof(WCHAR)] = L'\0';
                    FspDebugLog(__FUNCTION__ ": duplicate \"%S\"\n", FileName);
                    Result = STATUS_UNSUCCESSFUL;
                    goto exit;
                }
            if (sizeof hash / sizeof hash[0] > hash_count)
                hash[hash_count++] = h;
            if (0 == DirInfo->NextEntryOffset)
                break;
        }
    }

    Result = STATUS_SUCCESS;

exit:
    if (INVALID_HANDLE_VALUE != Handle)
        CloseHandle(Handle);

    free(Buffer);

    return Result;
}
static unsigned __stdcall querydir_nodup_dotest_thread(void *FilePath)
{
    FspDebugLog(__FUNCTION__ ": \"%S\"\n", FilePath);

    NTSTATUS Result;

    for (size_t i = 0; 5000 > i; i++)
    {
        Result = querydir_nodup_dotest_thread_loop(FilePath);
        if (!NT_SUCCESS(Result))
            goto exit;
    }

    Result = STATUS_SUCCESS;

exit:
    return Result;
}

static void querydir_nodup_dotest(ULONG Flags, PWSTR Prefix, ULONG FileInfoTimeout)
{
    void *memfs = memfs_start_ex(Flags, FileInfoTimeout);

    static WCHAR *FileNames[] =
    {
        L"Extensions.cs",
        L"JsonSchema.cs",
        L"JsonSchemaBuilder.cs",
        L"JsonSchemaConstants.cs",
        L"JsonSchemaException.cs",
        L"JsonSchemaGenerator.cs",
        L"JsonSchemaModel.cs",
        L"JsonSchemaModelBuilder.cs",
        L"JsonSchemaNode.cs",
        L"JsonSchemaNodeCollection.cs",
        L"JsonSchemaResolver.cs",
        L"JsonSchemaType.cs",
        L"JsonSchemaWriter.cs",
        L"UndefinedSchemaIdHandling.cs",
        L"ValidationEventArgs.cs",
        L"ValidationEventHandler.cs",
    };
    HANDLE Handle;
    BOOL Success;
    WCHAR FilePath[MAX_PATH];
    HANDLE Threads[2];
    DWORD ExitCode;

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\dir0",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));
    Success = CreateDirectoryW(FilePath, 0);
    ASSERT(Success);

    for (size_t i = 0; sizeof FileNames / sizeof FileNames[0] > i; i++)
    {
        StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\dir0\\%s",
            Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs), FileNames[i]);
        Handle = CreateFileW(FilePath, GENERIC_ALL, 0, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
        ASSERT(INVALID_HANDLE_VALUE != Handle);
        Success = CloseHandle(Handle);
        ASSERT(Success);
    }

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\dir0",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));
    for (size_t i = 0; sizeof Threads / sizeof Threads[0] > i; i++)
    {
        Threads[i] = (HANDLE)_beginthreadex(0, 0, querydir_nodup_dotest_thread, FilePath, 0, 0);
        ASSERT(0 != Threads[i]);
    }
    for (size_t i = 0; sizeof Threads / sizeof Threads[0] > i; i++)
    {
        WaitForSingleObject(Threads[i], INFINITE);
        GetExitCodeThread(Threads[i], &ExitCode);
        CloseHandle(Threads[i]);
        ASSERT(0 == ExitCode);
    }

    for (size_t i = 0; sizeof FileNames / sizeof FileNames[0] > i; i++)
    {
        StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\dir0\\%s",
            Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs), FileNames[i]);
        Success = DeleteFileW(FilePath);
        ASSERT(Success);
    }

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\dir0",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));
    Success = RemoveDirectoryW(FilePath);
    ASSERT(Success);

    memfs_stop(memfs);
}

static void querydir_nodup_test(void)
{
    /*
     * Test GitHub issue #475. Credit for the investigation and reproduction
     * of this issue goes to GitHub user @hach-que.
     */

    if (NtfsTests)
    {
        WCHAR DirBuf[MAX_PATH];
        GetTestDirectory(DirBuf);
        querydir_nodup_dotest(-1, DirBuf, 0);
    }
    if (WinFspDiskTests)
    {
        querydir_nodup_dotest(MemfsDisk | MemfsNoSlowio, 0, 0);
        querydir_nodup_dotest(MemfsDisk | MemfsNoSlowio, 0, 1000);
    }
#if 0
    if (WinFspNetTests)
    {
        querydir_nodup_dotest(MemfsNet | MemfsNoSlowio, L"\\\\memfs\\share", 0);
        querydir_nodup_dotest(MemfsNet | MemfsNoSlowio, L"\\\\memfs\\share", 1000);
    }
#endif
}

static void querydir_single_dotest(ULONG Flags, PWSTR Prefix, ULONG FileInfoTimeout, ULONG SleepTimeout)
{
    void *memfs = memfs_start_ex(Flags, FileInfoTimeout);

    HANDLE Handle;
    BOOL Success;
    WCHAR CurrentDirectory[MAX_PATH], FileName[MAX_PATH];
    WIN32_FIND_DATAW FindData;

    StringCbPrintfW(FileName, sizeof FileName, L"%s%s\\",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Success = GetCurrentDirectoryW(MAX_PATH, CurrentDirectory);
    ASSERT(Success);

    Success = SetCurrentDirectoryW(FileName);
    ASSERT(Success);

    for (ULONG Index = 0; 1000 > Index; Index++)
    {
        StringCbPrintfW(FileName, sizeof FileName, L"xxxxxxx-file%lu", Index);
        Handle = CreateFileW(FileName,
            GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
            0,
            CREATE_NEW, FILE_ATTRIBUTE_NORMAL,
            0);
        ASSERT(INVALID_HANDLE_VALUE != Handle);
        Success = CloseHandle(Handle);
        ASSERT(Success);
    }

    Handle = FindFirstFileW(L"*", &FindData);
    ASSERT(INVALID_HANDLE_VALUE != Handle);
    do
    {
    } while (FindNextFileW(Handle, &FindData));
    Success = FindClose(Handle);
    ASSERT(Success);

    for (ULONG Index = 0; 1000 > Index; Index++)
    {
        StringCbPrintfW(FileName, sizeof FileName, L"xxxxxxx-file%lu", Index);
        Handle = FindFirstFileW(FileName, &FindData);
        ASSERT(INVALID_HANDLE_VALUE != Handle);
        do
        {
        } while (FindNextFileW(Handle, &FindData));
        Success = FindClose(Handle);
        ASSERT(Success);
    }

    for (ULONG Index = 0; 1000 > Index; Index++)
    {
        StringCbPrintfW(FileName, sizeof FileName, L"xxxxxxx-file%lu", Index);
        Success = DeleteFileW(FileName);
        ASSERT(Success);
    }

    Success = RealSetCurrentDirectoryW(CurrentDirectory);
    ASSERT(Success);

    memfs_stop(memfs);
}

void querydir_single_test(void)
{
    if (OptShareName)
        return;

    if (NtfsTests)
    {
        WCHAR DirBuf[MAX_PATH];
        GetTestDirectory(DirBuf);
        querydir_single_dotest(-1, DirBuf, 0, 0);
    }
    if (WinFspDiskTests)
    {
        querydir_single_dotest(MemfsDisk, 0, 0, 0);
        querydir_single_dotest(MemfsDisk, 0, 1000, 0);
    }
    if (WinFspNetTests)
    {
        querydir_single_dotest(MemfsNet, L"\\\\memfs\\share", 0, 0);
        querydir_single_dotest(MemfsNet, L"\\\\memfs\\share", 1000, 0);
    }
}

void querydir_expire_cache_test(void)
{
    if (WinFspDiskTests)
    {
        querydir_dotest(MemfsDisk, 0, 500, 750);
    }
    if (WinFspNetTests)
    {
        querydir_dotest(MemfsNet, L"\\\\memfs\\share", 500, 750);
    }
}

static void querydir_buffer_overflow_dotest(ULONG Flags, PWSTR Prefix, ULONG FileInfoTimeout, ULONG SleepTimeout)
{
    void *memfs = memfs_start_ex(Flags, FileInfoTimeout);

    HANDLE Handle;
    BOOL Success;
    NTSTATUS Status;
    IO_STATUS_BLOCK IoStatus;
    WCHAR FilePath[MAX_PATH];
    FILE_DIRECTORY_INFORMATION DirInfo;
    UNICODE_STRING Pattern;

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\file0",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, CREATE_NEW,
        FILE_ATTRIBUTE_NORMAL, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);
    CloseHandle(Handle);

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Handle = CreateFileW(FilePath,
        GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);

    Pattern.Length = Pattern.MaximumLength = sizeof L"file0" - sizeof(WCHAR);
    Pattern.Buffer = L"file0";

    memset(&DirInfo, 0, sizeof DirInfo);
    Status = NtQueryDirectoryFile(
        Handle,
        0, 0, 0,
        &IoStatus,
        &DirInfo,
        0,
        FileDirectoryInformation,
        FALSE,
        &Pattern,
        FALSE);
    ASSERT(STATUS_INFO_LENGTH_MISMATCH == Status);

    memset(&DirInfo, 0, sizeof DirInfo);
    Status = NtQueryDirectoryFile(
        Handle,
        0, 0, 0,
        &IoStatus,
        &DirInfo,
        sizeof(FILE_DIRECTORY_INFORMATION),
        FileDirectoryInformation,
        FALSE,
        &Pattern,
        FALSE);
    ASSERT(STATUS_BUFFER_OVERFLOW == Status);
    ASSERT(sizeof(FILE_DIRECTORY_INFORMATION) == IoStatus.Information);
    ASSERT(Pattern.Length == DirInfo.FileNameLength);

    memset(&DirInfo, 0, sizeof DirInfo);
    Status = NtQueryDirectoryFile(
        Handle,
        0, 0, 0,
        &IoStatus,
        &DirInfo,
        sizeof(FILE_DIRECTORY_INFORMATION),
        FileDirectoryInformation,
        TRUE,
        &Pattern,
        FALSE);
    ASSERT(STATUS_BUFFER_OVERFLOW == Status);
    ASSERT(sizeof(FILE_DIRECTORY_INFORMATION) == IoStatus.Information);
    ASSERT(Pattern.Length == DirInfo.FileNameLength);

    CloseHandle(Handle);

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\file0",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Success = DeleteFileW(FilePath);
    ASSERT(Success);

    memfs_stop(memfs);
}

void querydir_buffer_overflow_test(void)
{
    if (OptShareName)
        return;

    if (NtfsTests)
    {
        WCHAR DirBuf[MAX_PATH];
        GetTestDirectory(DirBuf);
        querydir_buffer_overflow_dotest(-1, DirBuf, 0, 0);
    }
    if (WinFspDiskTests)
    {
        querydir_buffer_overflow_dotest(MemfsDisk, 0, 0, 0);
        querydir_buffer_overflow_dotest(MemfsDisk, 0, 1000, 0);
    }
    if (WinFspNetTests)
    {
        querydir_buffer_overflow_dotest(MemfsNet, L"\\\\memfs\\share", 0, 0);
        querydir_buffer_overflow_dotest(MemfsNet, L"\\\\memfs\\share", 1000, 0);
    }
}

static VOID querydir_namelen_exists(PWSTR FilePath)
{
    HANDLE Handle;
    WIN32_FIND_DATAW FindData;

    Handle = FindFirstFileW(FilePath, &FindData);
    ASSERT(INVALID_HANDLE_VALUE != Handle);
    FindClose(Handle);
}

static void querydir_namelen_dotest(ULONG Flags, PWSTR Prefix, PWSTR Drive)
{
    /* based on create_namelen_dotest */

    void *memfs = memfs_start(Flags);

    WCHAR FilePath[1024];
    PWSTR FilePathBgn, P, EndP;
    DWORD MaxComponentLength;
    HANDLE Handle;
    BOOL Success;

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Drive ? Drive : memfs_volumename(memfs));

    Success = GetVolumeInformationW(FilePath,
        0, 0,
        0, &MaxComponentLength, 0,
        0, 0);
    ASSERT(Success);

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));
    FilePathBgn = FilePath + wcslen(FilePath);

    for (P = FilePathBgn, EndP = P + MaxComponentLength - 1; EndP > P; P++)
        *P = (P - FilePathBgn) % 10 + '0';
    *P = L'\0';

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0,
        CREATE_NEW, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_DELETE_ON_CLOSE, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);
    querydir_namelen_exists(FilePath);
    Success = CloseHandle(Handle);
    ASSERT(Success);

    for (P = FilePathBgn, EndP = P + MaxComponentLength; EndP > P; P++)
        *P = (P - FilePathBgn) % 10 + '0';
    *P = L'\0';

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0,
        CREATE_NEW, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_DELETE_ON_CLOSE, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);
    querydir_namelen_exists(FilePath);
    Success = CloseHandle(Handle);
    ASSERT(Success);

    for (P = FilePathBgn, EndP = P + MaxComponentLength + 1; EndP > P; P++)
        *P = (P - FilePathBgn) % 10 + '0';
    *P = L'\0';

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0,
        CREATE_NEW, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_DELETE_ON_CLOSE, 0);
    ASSERT(INVALID_HANDLE_VALUE == Handle);
    ASSERT(ERROR_INVALID_NAME == GetLastError());

    for (P = FilePathBgn, EndP = P + MaxComponentLength - 1; EndP > P; P++)
        *P = (P - FilePathBgn) % 10 + 0x3041;
    *P = L'\0';

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0,
        CREATE_NEW, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_DELETE_ON_CLOSE, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);
    querydir_namelen_exists(FilePath);
    Success = CloseHandle(Handle);
    ASSERT(Success);

    for (P = FilePathBgn, EndP = P + MaxComponentLength; EndP > P; P++)
        *P = (P - FilePathBgn) % 10 + 0x3041;
    *P = L'\0';

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0,
        CREATE_NEW, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_DELETE_ON_CLOSE, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);
    querydir_namelen_exists(FilePath);
    Success = CloseHandle(Handle);
    ASSERT(Success);

    for (P = FilePathBgn, EndP = P + MaxComponentLength + 1; EndP > P; P++)
        *P = (P - FilePathBgn) % 10 + 0x3041;
    *P = L'\0';

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0,
        CREATE_NEW, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_DELETE_ON_CLOSE, 0);
    ASSERT(INVALID_HANDLE_VALUE == Handle);
    ASSERT(ERROR_INVALID_NAME == GetLastError());

    memfs_stop(memfs);
}

static void querydir_namelen_test(void)
{
    if (OptShareName || OptMountPoint)
        return;

    if (NtfsTests)
    {
        WCHAR DirBuf[MAX_PATH], DriveBuf[3];
        GetTestDirectoryAndDrive(DirBuf, DriveBuf);
        querydir_namelen_dotest(-1, DirBuf, DriveBuf);
    }
    if (WinFspDiskTests)
        querydir_namelen_dotest(MemfsDisk, 0, 0);
#if 0
    /* This test does not work when going through the MUP! */
    if (WinFspNetTests)
        querydir_namelen_dotest(MemfsNet, L"\\\\memfs\\share", L"\\\\memfs\\share");
#endif
}

static unsigned __stdcall dirnotify_dotest_thread(void *FilePath)
{
    FspDebugLog(__FUNCTION__ ": \"%S\"\n", FilePath);

    Sleep(1000); /* wait for ReadDirectoryChangesW */

    HANDLE Handle;
    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, 0, 0, CREATE_NEW, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_DELETE_ON_CLOSE, 0);
    if (INVALID_HANDLE_VALUE == Handle)
        return GetLastError();
    CloseHandle(Handle);
    return 0;
}

static void dirnotify_dotest(ULONG Flags, PWSTR Prefix, ULONG FileInfoTimeout, ULONG SleepTimeout)
{
    void *memfs = memfs_start_ex(Flags, FileInfoTimeout);

    WCHAR FilePath[MAX_PATH];
    HANDLE Handle;
    BOOL Success;
    HANDLE Thread;
    DWORD ExitCode;
    DWORD BytesTransferred;
    PFILE_NOTIFY_INFORMATION NotifyInfo;

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\Directory",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Success = CreateDirectoryW(FilePath, 0);
    ASSERT(Success);

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\Directory\\Subdirectory",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Success = CreateDirectoryW(FilePath, 0);
    ASSERT(Success);

    NotifyInfo = malloc(4096);
    ASSERT(0 != NotifyInfo);

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\Directory",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Handle = CreateFileW(FilePath,
        FILE_LIST_DIRECTORY, FILE_SHARE_READ, 0, OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\Directory\\Subdirectory\\file0",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Thread = (HANDLE)_beginthreadex(0, 0, dirnotify_dotest_thread, FilePath, 0, 0);
    ASSERT(0 != Thread);

    Success = ReadDirectoryChangesW(Handle,
        NotifyInfo, 4096, TRUE, FILE_NOTIFY_CHANGE_FILE_NAME, &BytesTransferred, 0, 0);
    ASSERT(Success);
    ASSERT(0 < BytesTransferred);

    ASSERT(FILE_ACTION_ADDED == NotifyInfo->Action);
    ASSERT(wcslen(L"Subdirectory\\file0") * sizeof(WCHAR) == NotifyInfo->FileNameLength);
    ASSERT(0 == mywcscmp(L"Subdirectory\\file0", -1,
        NotifyInfo->FileName, NotifyInfo->FileNameLength / sizeof(WCHAR)));

    if (0 == NotifyInfo->NextEntryOffset)
    {
        Success = ReadDirectoryChangesW(Handle,
            NotifyInfo, 4096, TRUE, FILE_NOTIFY_CHANGE_FILE_NAME, &BytesTransferred, 0, 0);
        ASSERT(Success);
        ASSERT(0 < BytesTransferred);
    }
    else
        NotifyInfo = (PVOID)((PUINT8)NotifyInfo + NotifyInfo->NextEntryOffset);

    ASSERT(FILE_ACTION_REMOVED == NotifyInfo->Action);
    ASSERT(wcslen(L"Subdirectory\\file0") * sizeof(WCHAR) == NotifyInfo->FileNameLength);
    ASSERT(0 == mywcscmp(L"Subdirectory\\file0", -1,
        NotifyInfo->FileName, NotifyInfo->FileNameLength / sizeof(WCHAR)));

    ASSERT(0 == NotifyInfo->NextEntryOffset);

    WaitForSingleObject(Thread, INFINITE);
    GetExitCodeThread(Thread, &ExitCode);
    CloseHandle(Thread);
    ASSERT(0 == ExitCode);

    Success = CloseHandle(Handle);
    ASSERT(Success);

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\Directory\\Subdirectory",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Success = RemoveDirectoryW(FilePath);
    ASSERT(Success);

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\Directory",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Success = RemoveDirectoryW(FilePath);
    ASSERT(Success);

    free(NotifyInfo);

    memfs_stop(memfs);
}

void dirnotify_test(void)
{
    if (NtfsTests)
    {
        WCHAR DirBuf[MAX_PATH];
        GetTestDirectory(DirBuf);
        dirnotify_dotest(-1, DirBuf, 0, 0);
    }
    if (WinFspDiskTests && !OptNoTraverseToken
        /* WinFsp does not support change notifications without traverse privilege*/)
    {
        dirnotify_dotest(MemfsDisk, 0, 0, 0);
        dirnotify_dotest(MemfsDisk, 0, 1000, 0);
    }
    if (WinFspNetTests && !OptNoTraverseToken
        /* WinFsp does not support change notifications without traverse privilege*/)
    {
        dirnotify_dotest(MemfsNet, L"\\\\memfs\\share", 0, 0);
        dirnotify_dotest(MemfsNet, L"\\\\memfs\\share", 1000, 0);
    }
}

void dirctl_tests(void)
{
    TEST(querydir_test);
    TEST_OPT(querydir_nodup_test);
    if (!OptShareName)
        TEST_OPT(querydir_single_test);
    TEST(querydir_expire_cache_test);
    if (!OptShareName)
        TEST(querydir_buffer_overflow_test);
    if (!OptShareName && !OptMountPoint)
        TEST(querydir_namelen_test);
    TEST(dirnotify_test);
}
