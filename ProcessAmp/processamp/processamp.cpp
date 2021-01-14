#include <ntifs.h>
#include <ntddk.h>
#include "procampCommon.h"

#define DRIVER_PREFIX "ProcessAmp: "

void ProcessAmpUnload(PDRIVER_OBJECT DriverObject);
NTSTATUS ProcessAmpCreateClose(PDEVICE_OBJECT, PIRP Irp);
NTSTATUS ProcessAmpDeviceControl(PDEVICE_OBJECT, PIRP Irp);

extern "C" NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING /*RegistryPath*/) {
	KdPrint((DRIVER_PREFIX "DriverEntry called\n"));
	DriverObject->DriverUnload = ProcessAmpUnload;
	DriverObject->MajorFunction[IRP_MJ_CREATE] = ProcessAmpCreateClose;
	DriverObject->MajorFunction[IRP_MJ_CLOSE] =  ProcessAmpCreateClose;
	DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = ProcessAmpDeviceControl;

	UNICODE_STRING devName;
	RtlInitUnicodeString(&devName, L"\\Device\\processamp");
	PDEVICE_OBJECT DeviceObject;
	NTSTATUS status = IoCreateDevice(DriverObject, 0, &devName, FILE_DEVICE_UNKNOWN, 0, FALSE, &DeviceObject);
	if (!NT_SUCCESS(status)) {
		KdPrint((DRIVER_PREFIX "Error in IoCreateDevice (0x%08X)\n", status));
		return status;
	}
	DeviceObject->Flags |= DO_BUFFERED_IO;

	UNICODE_STRING symLink = RTL_CONSTANT_STRING(L"\\??\\processamp");
	status = IoCreateSymbolicLink(&symLink, &devName);
	if (!NT_SUCCESS(status)) {
		IoDeleteDevice(DeviceObject);
		KdPrint((DRIVER_PREFIX "Error in IoCreateSymbolicLink (0x%08X)\n", status));
		return status;
	}

	return STATUS_SUCCESS;
}

void ProcessAmpUnload(PDRIVER_OBJECT DriverObject) {
	KdPrint((DRIVER_PREFIX "Unload called\n"));
	UNICODE_STRING symLink = RTL_CONSTANT_STRING(L"\\??\\processamp");
	IoDeleteSymbolicLink(&symLink);
	IoDeleteDevice(DriverObject->DeviceObject);
}

NTSTATUS ProcessAmpCreateClose(PDEVICE_OBJECT, PIRP Irp) {
	NTSTATUS status = STATUS_SUCCESS;
	Irp->IoStatus.Status = status;
	Irp->IoStatus.Information = 0;
	IoCompleteRequest(Irp, 0);
	return status;
}

NTSTATUS ProcessAmpDeviceControl(PDEVICE_OBJECT, PIRP Irp) {
	PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation(Irp);
	NTSTATUS status = STATUS_INVALID_DEVICE_REQUEST;
	auto& dic = IrpSp->Parameters.DeviceIoControl;

	switch (dic.IoControlCode) {
		case IOCTL_PROCAMP_SET_PRIORITY:
			if (dic.InputBufferLength < sizeof(ThreadData)) {
				status = STATUS_BUFFER_TOO_SMALL;
				break;
			}
			auto data = (ThreadData*)Irp->AssociatedIrp.SystemBuffer; //dic.Type3InputBuffer;
			if (data->Priority < 1 || data->Priority > 31) {
				status = STATUS_INVALID_PARAMETER;
				break;
			}

			PETHREAD Thread;
			status = PsLookupThreadByThreadId(UlongToHandle(data->ThreadId), &Thread);
			if (!NT_SUCCESS(status))
				break;
			KeSetPriorityThread((PKTHREAD)Thread, data->Priority);
			//KeSetBasePriorityThread((PKTHREAD)Thread, 2);
			ObDereferenceObject(Thread);
			break;
	}

	Irp->IoStatus.Status = status;
	Irp->IoStatus.Information = 0;
	IoCompleteRequest(Irp, 0);
	return status;
}

