#include "fsInformation.h"
#include "fsData.h"

//fastFat�У��ļ���Ϣ�ڴ���FCBʱ�ͱ�����FCB�ṹ��
FLT_PREOP_CALLBACK_STATUS PtPreQueryInformation(__inout PFLT_CALLBACK_DATA Data, __in PCFLT_RELATED_OBJECTS FltObjects, __deref_out_opt PVOID *CompletionContext)
{
	FLT_PREOP_CALLBACK_STATUS FltStatus = FLT_PREOP_SUCCESS_NO_CALLBACK;
	NTSTATUS ntStatus = STATUS_SUCCESS;
	ULONG uProcessType = 0;
	BOOLEAN bTopLevelIrp = FALSE;
	PDEF_IRP_CONTEXT IrpContext = NULL;

	UNREFERENCED_PARAMETER(CompletionContext);
	
	PAGED_CODE();

	FsRtlEnterFileSystem();
	if (!IsMyFakeFcb(FltObjects->FileObject))
	{
		FsRtlExitFileSystem();
		return FltStatus;
	}
	if (FLT_IS_IRP_OPERATION(Data))
	{
		__try
		{
			bTopLevelIrp = FsIsIrpTopLevel(Data);
			IrpContext = FsCreateIrpContext(Data, FltObjects, CanFsWait(Data));
			if (NULL == IrpContext)
			{
				FsRaiseStatus(IrpContext, STATUS_INSUFFICIENT_RESOURCES);
			}
			ntStatus = FsCommonQueryInformation(Data, FltObjects, IrpContext);
			if (!NT_SUCCESS(ntStatus))
			{
				Data->IoStatus.Status = ntStatus;
				Data->IoStatus.Information = 0;
			}
			FltStatus = FLT_PREOP_COMPLETE;
		}
		__finally
		{
			if (bTopLevelIrp)
			{
				IoSetTopLevelIrp(NULL);
			}
		}
		FsCompleteRequest(&IrpContext, &Data, STATUS_SUCCESS, FALSE);
	}
	else if (FLT_IS_FASTIO_OPERATION(Data))
	{
		FltStatus = FLT_PREOP_DISALLOW_FASTIO;
	}
	else
	{
		Data->IoStatus.Status = STATUS_INVALID_DEVICE_REQUEST;
		Data->IoStatus.Information = 0;
		FltStatus = FLT_PREOP_COMPLETE;
	}
	
	FsRtlExitFileSystem();
	return FltStatus;
}

FLT_POSTOP_CALLBACK_STATUS PtPostQueryInformation(__inout PFLT_CALLBACK_DATA Data, __in PCFLT_RELATED_OBJECTS FltObjects, __in_opt PVOID CompletionContext, __in FLT_POST_OPERATION_FLAGS Flags)
{
	UNREFERENCED_PARAMETER(Data);
	UNREFERENCED_PARAMETER(FltObjects);
	UNREFERENCED_PARAMETER(CompletionContext);
	UNREFERENCED_PARAMETER(Flags);

	return FLT_POSTOP_FINISHED_PROCESSING;
}

NTSTATUS FsCommonQueryInformation(__inout PFLT_CALLBACK_DATA Data, __in PCFLT_RELATED_OBJECTS FltObjects, __in PDEF_IRP_CONTEXT IrpContext)
{
	NTSTATUS ntStatus = STATUS_UNSUCCESSFUL;
	PFILE_BASIC_INFORMATION FileBasicInfo = NULL;
	PFILE_STANDARD_INFORMATION FileStandardInfo = NULL;
	PDEFFCB Fcb = NULL;
	FILE_INFORMATION_CLASS FileInfoClass = Data->Iopb->Parameters.QueryFileInformation.FileInformationClass;
	PVOID pFileInfoBuffer = Data->Iopb->Parameters.QueryFileInformation.InfoBuffer;

	//��ѯ��Ϣ���Ƿ���Ҫ��ռFCB��Դ������
	__try
	{
		Fcb = FltObjects->FileObject->FsContext;
		if (NULL == Fcb)
		{
			DbgPrint("QueryInformation:Fcb is not exit!\n");
			__leave;
		}

		switch (FileInfoClass)
		{
		case FileBasicInformation:
			if (Data->Iopb->Parameters.QueryFileInformation.Length < sizeof(FILE_BASIC_INFORMATION))
			{
				DbgPrint("QueryInformation:length(%d) < sizeof(FILE_BASIC_INFORMATION)...\n", Data->Iopb->Parameters.QueryFileInformation.Length);
				try_return(ntStatus);
			}
			FileBasicInfo = (PFILE_BASIC_INFORMATION)pFileInfoBuffer;
			FileBasicInfo->CreationTime.QuadPart = Fcb->CreationTime;
			FileBasicInfo->ChangeTime.QuadPart = Fcb->LastChangeTime;
			FileBasicInfo->FileAttributes = Fcb->Attribute;
			FileBasicInfo->LastAccessTime.QuadPart = Fcb->LastAccessTime;
			FileBasicInfo->LastWriteTime.QuadPart = Fcb->LastChangeTime;
			break;
		case FileAllInformation:
			break;
		case FileStandardInformation:
			if (Data->Iopb->Parameters.QueryFileInformation.Length < sizeof(FILE_STANDARD_INFORMATION))
			{
				DbgPrint("QueryInformation:length(%d) < sizeof(FILE_STANDARD_INFORMATION)...\n", Data->Iopb->Parameters.QueryFileInformation.Length);
				try_return(ntStatus);
			}
			FileStandardInfo = (PFILE_STANDARD_INFORMATION)pFileInfoBuffer;
			FileStandardInfo->AllocationSize = Fcb->Header.AllocationSize;
			FileStandardInfo->DeletePending = BooleanFlagOn(Fcb->FcbState, FCB_STATE_DELETE_ON_CLOSE);;
			FileStandardInfo->Directory = Fcb->Directory;
			FileStandardInfo->NumberOfLinks = Fcb->LinkCount;
			FileStandardInfo->EndOfFile = Fcb->Header.FileSize;
			break;

		default:
			try_return(ntStatus);
			break;
		}
		ntStatus = STATUS_SUCCESS;
	try_exit:NOTHING;
	}
	__finally
	{

	}
	return ntStatus;
}