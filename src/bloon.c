#include <ntifs.h>
#include <wdm.h>

#include "ioctls.h"

NTKERNELAPI
NTSTATUS
NTAPI
MmCopyVirtualMemory(_In_ PEPROCESS SourceProcess, _In_reads_bytes_(BufferSize) PVOID SourceAddress,
                    _In_ PEPROCESS TargetProcess, _Out_writes_bytes_(BufferSize) PVOID TargetAddress,
                    _In_ SIZE_T BufferSize, _In_ KPROCESSOR_MODE PreviousMode, _Out_ PSIZE_T ReturnSize);

static UNICODE_STRING DEVICE_NAME = RTL_CONSTANT_STRING(L"\\Device\\Bloon");
static UNICODE_STRING SYMBOLIC_LINK_NAME = RTL_CONSTANT_STRING(L"\\DosDevices\\Bloon");

void DriverUnload(PDRIVER_OBJECT driver_object)
{
    if (!NT_SUCCESS(IoDeleteSymbolicLink(&SYMBOLIC_LINK_NAME)))
    {
        KdPrintEx(DPFLTR_SYSTEM_ID, DPFLTR_INFO_LEVEL, "[bloon] `IoDeleteSymbolicLink` failed! Whatever...\n");
    }

    IoDeleteDevice(driver_object->DeviceObject);

    KdPrintEx(DPFLTR_SYSTEM_ID, DPFLTR_INFO_LEVEL, "[bloon] Goodbye, World!\n");
}

NTSTATUS DispatchDefault(PDEVICE_OBJECT _, PIRP irp)
{
    irp->IoStatus.Status = STATUS_SUCCESS;
    irp->IoStatus.Information = 0;
    IoCompleteRequest(irp, IO_NO_INCREMENT);
    return STATUS_SUCCESS;
}

NTSTATUS DispatchIoctl(PDEVICE_OBJECT _, PIRP irp)
{
    NTSTATUS status = STATUS_SUCCESS;
    PEPROCESS process = NULL;
    SIZE_T return_size = 0;

    const PIO_STACK_LOCATION current_irp_stack_location = IoGetCurrentIrpStackLocation(irp);

    switch (current_irp_stack_location->Parameters.DeviceIoControl.IoControlCode)
    {
    case IOCTL_READ_MEMORY:
        KdPrintEx(DPFLTR_SYSTEM_ID, DPFLTR_INFO_LEVEL, "[bloon] `IOCTL_READ_MEMORY`...\n");

        const ULONG input_buffer_length = current_irp_stack_location->Parameters.DeviceIoControl.InputBufferLength;

        if (input_buffer_length < sizeof(ReadRequest))
        {
            KdPrintEx(DPFLTR_SYSTEM_ID, DPFLTR_INFO_LEVEL, "[bloon] `input_buffer_length < sizeof(ReadRequest)`!\n");
            status = STATUS_INVALID_BUFFER_SIZE;
            goto out;
        }

        const ULONG output_buffer_length = current_irp_stack_location->Parameters.DeviceIoControl.OutputBufferLength;
        const PVOID system_buffer = irp->AssociatedIrp.SystemBuffer;
        const ReadRequest *read_request = system_buffer;

        if (output_buffer_length < read_request->size)
        {
            KdPrintEx(DPFLTR_SYSTEM_ID, DPFLTR_INFO_LEVEL, "[bloon] `output_buffer_length < read_request->size`!\n");
            status = STATUS_INVALID_BUFFER_SIZE;
            goto out;
        }

        if (read_request->address == 0 || read_request->address >= (UINT64)MM_HIGHEST_USER_ADDRESS ||
            read_request->size == 0 || read_request->size > (UINT64)MM_HIGHEST_USER_ADDRESS - read_request->address)
        {
            KdPrintEx(DPFLTR_SYSTEM_ID, DPFLTR_INFO_LEVEL, "[bloon] Invalid address!\n");
            status = STATUS_INVALID_PARAMETER;
            goto out;
        }

        status = PsLookupProcessByProcessId((HANDLE)read_request->pid, &process);
        if (!NT_SUCCESS(status))
        {
            KdPrintEx(DPFLTR_SYSTEM_ID, DPFLTR_INFO_LEVEL, "[bloon] `PsLookupProcessByProcessId` failed!\n");
            goto out;
        }

        const SIZE_T read_request_size = read_request->size;

        status = MmCopyVirtualMemory(process, (PVOID)read_request->address, PsGetCurrentProcess(), system_buffer,
                                     read_request->size, KernelMode, &return_size);
        if (!NT_SUCCESS(status))
        {
            KdPrintEx(DPFLTR_SYSTEM_ID, DPFLTR_INFO_LEVEL, "[bloon] `MmCopyVirtualMemory` failed!\n");
            goto err_dereference_process;
        }

        // `read_request` is garbage now, after `MmCopyVirtualMemory`.

        if (return_size != read_request_size)
        {
            KdPrintEx(DPFLTR_SYSTEM_ID, DPFLTR_INFO_LEVEL, "[bloon] `return_size != read_request->size`!\n");
            goto err_dereference_process;
        }

        break;
    default:
        status = STATUS_INVALID_DEVICE_REQUEST;
        goto out;
    }

err_dereference_process:
    ObfDereferenceObject(process);
out:
    irp->IoStatus.Status = status;
    irp->IoStatus.Information = return_size;
    IoCompleteRequest(irp, IO_NO_INCREMENT);
    return status;
}

NTSTATUS DriverEntry(PDRIVER_OBJECT driver_object, PUNICODE_STRING _)
{
    NTSTATUS status = STATUS_SUCCESS;

    KdPrintEx(DPFLTR_SYSTEM_ID, DPFLTR_INFO_LEVEL, "[bloon] Driver started...\n");

    PDEVICE_OBJECT device_object = NULL;

    status = IoCreateDevice(driver_object, 0, &DEVICE_NAME, FILE_DEVICE_UNKNOWN, 0, 0, &device_object);
    if (!NT_SUCCESS(status))
    {
        KdPrintEx(DPFLTR_SYSTEM_ID, DPFLTR_INFO_LEVEL, "[bloon] `IoCreateDevice` failed!\n");
        goto out;
    }

    KdPrintEx(DPFLTR_SYSTEM_ID, DPFLTR_INFO_LEVEL, "[bloon] Device created...\n");

    status = IoCreateSymbolicLink(&SYMBOLIC_LINK_NAME, &DEVICE_NAME);
    if (!NT_SUCCESS(status))
    {
        KdPrintEx(DPFLTR_SYSTEM_ID, DPFLTR_INFO_LEVEL, "[bloon] `IoCreateSymbolicLink` failed!\n");
        goto err_delete_device;
    }

    KdPrintEx(DPFLTR_SYSTEM_ID, DPFLTR_INFO_LEVEL, "[bloon] Symbolic link created...\n");

    driver_object->DriverUnload = DriverUnload;
    driver_object->MajorFunction[IRP_MJ_CREATE] = DispatchDefault;
    driver_object->MajorFunction[IRP_MJ_CLOSE] = DispatchDefault;
    driver_object->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DispatchIoctl;
    goto out;

err_delete_device:
    IoDeleteDevice(device_object);
out:
    return status;
}
