#include <winfsp/winfsp.h>
#include <tlib/testsuite.h>
#include <process.h>
#include <strsafe.h>

extern int WinFspDiskTests;
extern int WinFspNetTests;

static unsigned __stdcall timeout_pending_dotest_thread(void *FilePath)
{
    FspDebugLog(__FUNCTION__ ": \"%S\"\n", FilePath);

    HANDLE Handle;
    Handle = CreateFileW(FilePath,
        FILE_GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, 0);
    if (INVALID_HANDLE_VALUE == Handle)
        return GetLastError();
    CloseHandle(Handle);
    return 0;
}

void timeout_pending_dotest(PWSTR DeviceName)
{
    NTSTATUS Result;
    BOOL Success;
    FSP_FSCTL_VOLUME_PARAMS Params = { 0 };
    WCHAR VolumePath[MAX_PATH];
    WCHAR FilePath[MAX_PATH];
    HANDLE VolumeHandle;
    HANDLE Thread;
    DWORD ExitCode;

    Params.SectorSize = 16384;
    Params.SerialNumber = 0x12345678;
    Params.IrpTimeout = FspFsctlIrpTimeoutDebug;
    Result = FspFsctlCreateVolume(DeviceName, &Params, 0, VolumePath, sizeof VolumePath);
    ASSERT(STATUS_SUCCESS == Result);

    StringCbPrintfW(FilePath, sizeof FilePath, L"\\\\?\\GLOBALROOT%s\\file0", VolumePath);
    Thread = (HANDLE)_beginthreadex(0, 0, timeout_pending_dotest_thread, FilePath, 0, 0);
    ASSERT(0 != Thread);

    WaitForSingleObject(Thread, INFINITE);
    GetExitCodeThread(Thread, &ExitCode);
    CloseHandle(Thread);

    ASSERT(ERROR_OPERATION_ABORTED == ExitCode);

    Result = FspFsctlOpenVolume(VolumePath, &VolumeHandle);
    ASSERT(STATUS_SUCCESS == Result);

    Result = FspFsctlDeleteVolume(VolumeHandle);
    ASSERT(STATUS_SUCCESS == Result);

    Success = CloseHandle(VolumeHandle);
    ASSERT(Success);
}

void timeout_pending_test(void)
{
    if (WinFspDiskTests)
        timeout_pending_dotest(L"WinFsp.Disk");
    if (WinFspNetTests)
        timeout_pending_dotest(L"WinFsp.Net");
}

static unsigned __stdcall timeout_transact_dotest_thread(void *FilePath)
{
    FspDebugLog(__FUNCTION__ ": \"%S\"\n", FilePath);

    Sleep(1000);

    HANDLE Handle;
    Handle = CreateFileW(FilePath,
        FILE_GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, 0);
    if (INVALID_HANDLE_VALUE == Handle)
        return GetLastError();
    CloseHandle(Handle);
    return 0;
}

void timeout_transact_dotest(PWSTR DeviceName)
{
    NTSTATUS Result;
    BOOL Success;
    FSP_FSCTL_VOLUME_PARAMS Params = { 0 };
    WCHAR VolumePath[MAX_PATH];
    WCHAR FilePath[MAX_PATH];
    HANDLE VolumeHandle;
    HANDLE Thread;
    DWORD ExitCode;

    Params.SectorSize = 16384;
    Params.SerialNumber = 0x12345678;
    Params.TransactTimeout = 2000;
    Params.IrpTimeout = FspFsctlIrpTimeoutDebug;
    Result = FspFsctlCreateVolume(DeviceName, &Params, 0, VolumePath, sizeof VolumePath);
    ASSERT(STATUS_SUCCESS == Result);

    StringCbPrintfW(FilePath, sizeof FilePath, L"\\\\?\\GLOBALROOT%s\\file0", VolumePath);
    Thread = (HANDLE)_beginthreadex(0, 0, timeout_transact_dotest_thread, FilePath, 0, 0);
    ASSERT(0 != Thread);

    Result = FspFsctlOpenVolume(VolumePath, &VolumeHandle);
    ASSERT(STATUS_SUCCESS == Result);

    FSP_FSCTL_DECLSPEC_ALIGN UINT8 RequestBuf[FSP_FSCTL_TRANSACT_REQ_BUFFER_SIZEMIN];
    FSP_FSCTL_DECLSPEC_ALIGN UINT8 ResponseBuf[FSP_FSCTL_TRANSACT_RSP_BUFFER_SIZEMIN];
    UINT8 *RequestBufEnd;
    UINT8 *ResponseBufEnd = ResponseBuf + sizeof ResponseBuf;
    SIZE_T RequestBufSize;
    SIZE_T ResponseBufSize;
    FSP_FSCTL_TRANSACT_REQ *Request = (PVOID)RequestBuf, *NextRequest;
    FSP_FSCTL_TRANSACT_RSP *Response = (PVOID)ResponseBuf;

    ResponseBufSize = 0;
    RequestBufSize = sizeof RequestBuf;
    Result = FspFsctlTransact(VolumeHandle, ResponseBuf, ResponseBufSize, RequestBuf, &RequestBufSize);
    ASSERT(STATUS_SUCCESS == Result);

    RequestBufEnd = RequestBuf + RequestBufSize;

    NextRequest = FspFsctlTransactConsumeRequest(Request, RequestBufEnd);
    ASSERT(0 != NextRequest);

    ASSERT(0 == Request->Version);
    ASSERT(FSP_FSCTL_TRANSACT_REQ_SIZEMAX >= Request->Size);
    ASSERT(0 != Request->Hint);
    ASSERT(FspFsctlTransactCreateKind == Request->Kind);
    ASSERT(FILE_CREATE == ((Request->Req.Create.CreateOptions >> 24) & 0xff));
    ASSERT(FILE_ATTRIBUTE_NORMAL == Request->Req.Create.FileAttributes);
    ASSERT(0 == Request->Req.Create.SecurityDescriptor.Offset);
    ASSERT(0 == Request->Req.Create.SecurityDescriptor.Size);
    ASSERT(0 == Request->Req.Create.AllocationSize);
    ASSERT(FILE_GENERIC_READ == Request->Req.Create.DesiredAccess);
    ASSERT((FILE_SHARE_READ | FILE_SHARE_WRITE) == Request->Req.Create.ShareAccess);
    ASSERT(0 == Request->Req.Create.Ea.Offset);
    ASSERT(0 == Request->Req.Create.Ea.Size);
    ASSERT(Request->Req.Create.UserMode);
    ASSERT(Request->Req.Create.HasTraversePrivilege);
    ASSERT(!Request->Req.Create.OpenTargetDirectory);
    ASSERT(!Request->Req.Create.CaseSensitive);

    WaitForSingleObject(Thread, INFINITE);
    GetExitCodeThread(Thread, &ExitCode);
    CloseHandle(Thread);

    ASSERT(ERROR_OPERATION_ABORTED == ExitCode);

    Result = FspFsctlDeleteVolume(VolumeHandle);
    ASSERT(STATUS_SUCCESS == Result);

    Success = CloseHandle(VolumeHandle);
    ASSERT(Success);
}

void timeout_transact_test(void)
{
    if (WinFspDiskTests)
        timeout_transact_dotest(L"WinFsp.Disk");
    if (WinFspNetTests)
        timeout_transact_dotest(L"WinFsp.Net");
}

void timeout_tests(void)
{
    TEST(timeout_pending_test);
    TEST(timeout_transact_test);
}