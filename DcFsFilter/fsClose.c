#include "fsClose.h"
#include "fsData.h"

FLT_PREOP_CALLBACK_STATUS PtPreClose(__inout PFLT_CALLBACK_DATA Data, __in PCFLT_RELATED_OBJECTS FltObjects, __deref_out_opt PVOID *CompletionContext)
{
	FLT_PREOP_CALLBACK_STATUS FltStatus = FLT_PREOP_COMPLETE;
	BOOLEAN bTopLevelIrp = FALSE;
	PDEFFCB Fcb = NULL;
	PDEF_CCB Ccb = NULL;
	PLARGE_INTEGER TruncateSize = NULL;
	LARGE_INTEGER LocalTruncateSize;
	BOOLEAN bAcquire = FALSE;

	PAGED_CODE();
#ifdef TEST
	if (IsTest(Data, FltObjects, "PtPreClose"))
	{
		Fcb = FltObjects->FileObject->FsContext;
		KdBreakPoint();
	}
	
#endif
	FsRtlEnterFileSystem();

	if (!IsMyFakeFcb(FltObjects->FileObject))
	{
		FsRtlExitFileSystem();
 		return FLT_PREOP_SUCCESS_NO_CALLBACK;
	}	
	DbgPrint("PtPreClose......\n");

	bTopLevelIrp = IsTopLevelIRP(Data);
	if (FLT_IS_IRP_OPERATION(Data))
	{
		__try
		{
			Fcb = FltObjects->FileObject->FsContext;
			Ccb = FltObjects->FileObject->FsContext2;
			if (NULL == Fcb)
			{
				__leave;
			}
			DbgPrint("close:openCount=%d, uncleanup=%d...\n", Fcb->OpenCount, Fcb->UncleanCount);
			if (0 == Fcb->OpenCount)
			{
				if (FlagOn(Fcb->FcbState, FCB_STATE_REAME_INFO))
				{
					if (Fcb->CcFileObject)
					{
						ObDereferenceObject(Fcb->CcFileObject);
						FltClose(Fcb->CcFileHandle);
						Fcb->CcFileObject = NULL;
						Fcb->CcFileHandle = NULL;
					}
					ClearFlag(Fcb->FcbState, FCB_STATE_REAME_INFO);
				}
				if (FlagOn(Fcb->FcbState, FCB_STATE_DELETE_ON_CLOSE))
				{
					ClearFlag(Fcb->FcbState, FCB_STATE_REAME_INFO);
					FsFreeFcb(Fcb, NULL);
					FltObjects->FileObject->FsContext = NULL;
				}


				FsFreeCcb(Ccb);
				FltObjects->FileObject->FsContext2 = NULL;
				Fcb->Ccb = NULL;
			}
		}
		__finally
		{

		}
	}
	else if (FLT_IS_FASTIO_OPERATION(Data))
	{
		FltStatus = FLT_PREOP_DISALLOW_FASTIO;
	}
	else 
	{
		Data->IoStatus.Status = STATUS_UNSUCCESSFUL;
		Data->IoStatus.Information = 0;
		FltStatus = FLT_PREOP_COMPLETE;
	}

	if (bTopLevelIrp)
	{
		IoSetTopLevelIrp(NULL);
	}
	FsRtlExitFileSystem();
	return FltStatus;
}

FLT_POSTOP_CALLBACK_STATUS PtPostClose(__inout PFLT_CALLBACK_DATA Data, __in PCFLT_RELATED_OBJECTS FltObjects, __in_opt PVOID CompletionContext, __in FLT_POST_OPERATION_FLAGS Flags)
{
	UNREFERENCED_PARAMETER(Data);
	UNREFERENCED_PARAMETER(FltObjects);
	UNREFERENCED_PARAMETER(Flags);
	UNREFERENCED_PARAMETER(CompletionContext);

	return FLT_POSTOP_FINISHED_PROCESSING;
}
