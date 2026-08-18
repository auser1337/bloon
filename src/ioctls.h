#pragma once

#include <wdm.h>

#define IOCTL_READ_MEMORY CTL_CODE(FILE_DEVICE_UNKNOWN, 0x67, METHOD_BUFFERED, FILE_ANY_ACCESS)

typedef struct ReadMemoryRequest
{
    UINT32 pid;
    // 4 bytes of padding
    UINT64 address;
    UINT64 size;
} ReadRequest;
static_assert(sizeof(ReadRequest) == 0x18, "`sizeof(ReadRequest) != 0x18`");
