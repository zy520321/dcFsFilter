#include "fsCommunication.h"
#include "fsData.h"
#include <ntifs.h>
#include <wdm.h>
#include "userkernel.h"
#include "Head.h"
#include "EncFile.h"
#include "regMgr.h"

PFLT_PORT g_FltPort = NULL;
PFLT_PORT g_FltClientPort = NULL;

//NTKERNELAPI UCHAR * PsGetProcessImageFileName(__in PEPROCESS Process);

BOOLEAN InitCommunication(__in PFLT_FILTER FltFilter)
{
	NTSTATUS Status = STATUS_UNSUCCESSFUL;
	PSECURITY_DESCRIPTOR Sd = NULL;
	OBJECT_ATTRIBUTES ob = { 0 };
	UNICODE_STRING ustrPort;
	RtlInitUnicodeString(&ustrPort, PortName);
	__try
	{
		Status = FltBuildDefaultSecurityDescriptor(&Sd, FLT_PORT_ALL_ACCESS);
		if (!NT_SUCCESS(Status))
		{
			__leave;
		}
		InitializeObjectAttributes(&ob, &ustrPort, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, Sd);
		Status = FltCreateCommunicationPort(FltFilter, &g_FltPort, &ob, NULL, ConnectNotify, DisConnectNotify, MessageNotify, 1);
		if (!NT_SUCCESS(Status))
		{
			__leave;
		}
		Status = STATUS_SUCCESS;
	}
	__finally
	{
		if (Sd != NULL)
		{
			FltFreeSecurityDescriptor(Sd);
		}
	}
	
	return (STATUS_SUCCESS == Status ? TRUE : FALSE);
}


NTSTATUS FLTAPI ConnectNotify(
	__in PFLT_PORT ClientPort,
	__in_opt PVOID ServerPortCookie,
	__in_bcount_opt(SizeOfContext) PVOID ConnectionContext,
	__in ULONG SizeOfContext,
	__deref_out_opt PVOID *ConnectionPortCookie
	)
{
	NTSTATUS Status = STATUS_SUCCESS;
	HANDLE ProcessID = NULL;
	PEPROCESS Process = NULL;
	PUCHAR ProcessImageName = NULL;
	
	UNREFERENCED_PARAMETER(ServerPortCookie);
	UNREFERENCED_PARAMETER(ConnectionContext);
	UNREFERENCED_PARAMETER(SizeOfContext);
	UNREFERENCED_PARAMETER(ConnectionPortCookie);
	
	__try
	{
		g_FltClientPort = ClientPort;
		ProcessID = PsGetCurrentProcessId();
		if (NULL == ProcessID)
		{
			__leave;
		}
		Status = PsLookupProcessByProcessId(ProcessID, &Process);
		if (!NT_SUCCESS(Status))
		{
			__leave;
		}
// 		ProcessImageName = PsGetProcessImageFileName(Process);
// 		if (NULL == ProcessImageName)
// 		{
// 			__leave;
// 		}
		//DbgPrint("process image name = %s....\n", ProcessImageName);
	}
	__finally
	{
		if (Process != NULL)
		{
			ObDereferenceObject(Process);
		}
	}

	KdPrint(("Recv Connect Msg, process id=0x%x......\n", ProcessID));

	return Status;
}

VOID FLTAPI DisConnectNotify(__in_opt PVOID ConnectionCookie)
{
	g_FltClientPort = NULL;
}

NTSTATUS FLTAPI MessageNotify(__in_opt PVOID PortCookie, __in_bcount_opt(InputBufferLength) PVOID InputBuffer, __in ULONG InputBufferLength, __out_bcount_part_opt(OutputBufferLength, *ReturnOutputBufferLength) PVOID OutputBuffer, __in ULONG OutputBufferLength, __out PULONG ReturnOutputBufferLength)
{
	NTSTATUS			ntStatus = STATUS_UNSUCCESSFUL;

	ESafeCommandType	cmdType;
	PSystemApi32Use		pSyatem = { 0 };
	PDRV_DATA DrvData = GetDrvData();

	UNREFERENCED_PARAMETER(PortCookie);
	UNREFERENCED_PARAMETER(InputBufferLength);
	UNREFERENCED_PARAMETER(OutputBuffer);
	UNREFERENCED_PARAMETER(OutputBufferLength);
	UNREFERENCED_PARAMETER(ReturnOutputBufferLength);


	__try
	{
		if (!g_FltPort || !g_FltClientPort)
		{
			KdPrint(("comm parameter error"));
			__leave;
		}

		cmdType = ((PCommandMsg)InputBuffer)->CommandType;
		switch (cmdType)
		{
		case eSetSystemUse:
		{
			pSyatem = (PSystemApi32Use)(((PCommandMsg)InputBuffer)->MsgInfo);

			KeEnterCriticalRegion();

			if (!strlen(DrvData->szbtKey))
			{
				RtlCopyMemory(&DrvData->SystemUser, pSyatem, sizeof(SystemApi32Use));
				RtlCopyMemory(DrvData->szbtKey, pSyatem->Key, KEY_LEN);

				if (strlen(DrvData->szbtKey))
				{
					//if (CheckDebugFlag(DEBUG_FLAG_PRINT_DOGID_AND_KEY))
					{
						KdPrint(("[DogID] %d - 0x%x", DrvData->SystemUser.DogID));
						KdPrint(("[Key] %hs", DrvData->szbtKey));
					}

					g_bSafeDataReady = TRUE;
				}
				else
					KdPrint(("DogID and Key error"));
			}
			else
			{
				KdPrint(("DogID and Key already set"));
				g_bSafeDataReady = TRUE;
			}		

			KeLeaveCriticalRegion();

			ntStatus = STATUS_SUCCESS;
			break;
		}
		case eSetProcAuthentic:
			break;
		case eAppendPolicy:
		{
			int i = 0;
			int nCount = ((PCommandMsg)InputBuffer)->nBytes;
			PUSER_PROCESSINFO pItem = (PUSER_PROCESSINFO)(((PCommandMsg)InputBuffer)->MsgInfo);
			for (i; i < nCount; i++)
			{
				if ((pItem + i)->bControled)
				{
					InsertControlProcess((pItem + i)->wszProcessName, (pItem + i)->length);
				}
				else
				{
					DeleteControlProcess((pItem + i)->wszProcessName, (pItem + i)->length);
				}
			}
			
			ntStatus = STATUS_SUCCESS;
		}
			break;
		case eSetKenlPID:
		{
			pSyatem = (PSystemApi32Use)(((PCommandMsg)InputBuffer)->MsgInfo);
			DrvData->SystemUser.dwClientProcessID = pSyatem->dwClientProcessID;
		}
			break;
		default:
		{
			KdPrint(("cmdType error. cmdType(%d)", cmdType));
			__leave;
		}
		}
	}
	__finally
	{
		;
	}

	return ntStatus;
}

VOID UnInitCommunication()
{
	if (g_FltPort != NULL)
	{
		FltCloseCommunicationPort(g_FltPort);
	}
}
