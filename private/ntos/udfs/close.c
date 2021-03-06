/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    Close.c

Abstract:

    This module implements the File Close routine for Udfs called by the
    Fsd/Fsp dispatch routines.

    The close operation interacts with both the async and delayed close queues
    in the UdfData structure.  Since close may be called recursively we may
    violate the locking order in acquiring the Vcb or Fcb.  In this case
    we may move the request to the async close queue.  If this is the last
    reference on the Fcb and there is a chance the user may reopen this
    file again soon we would like to defer the close.  In this case we
    may move the request to the delayed close queue.

    Once we are past the decode file operation there is no need for the
    file object.  If we are moving the request to either of the work
    queues then we remember all of the information from the file object and
    complete the request with STATUS_SUCCESS.  The Io system can then
    reuse the file object and we can complete the request when convenient.

    The async close queue consists of requests which we would like to
    complete as soon as possible.  They are queued using the original
    IrpContext where some of the fields have been overwritten with
    information from the file object.  We will extract this information,
    cleanup the IrpContext and then call the close worker routine.

    The delayed close queue consists of requests which we would like to
    defer the close for.  We keep size of this list within a range
    determined by the size of the system.  We let it grow to some maximum
    value and then shrink to some minimum value.  We allocate a small
    structure which contains the key information from the file object
    and use this information along with an IrpContext on the stack
    to complete the request.

Author:

    Dan Lovinger    [DanLo]     04-Nov-1996

Revision History:

--*/

#include "UdfProcs.h"

//
//  The Bug check file id for this module
//

#define BugCheckFileId                   (UDFS_BUG_CHECK_CLOSE)

//
//  The local debug trace level
//

#define Dbg                              (UDFS_DEBUG_LEVEL_CLOSE)

//
//  Local support routines
//

BOOLEAN
UdfCommonClosePrivate (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    IN PFCB Fcb,
    IN ULONG UserReference,
    IN BOOLEAN FromFsd
    );

VOID
UdfQueueClose (
    IN PIRP_CONTEXT IrpContext,
    IN PFCB Fcb,
    IN ULONG UserReference,
    IN BOOLEAN DelayedClose
    );

PIRP_CONTEXT
UdfRemoveClose (
    IN PVCB Vcb OPTIONAL
    );

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, UdfCommonClose)
#pragma alloc_text(PAGE, UdfCommonClosePrivate)
#pragma alloc_text(PAGE, UdfQueueClose)
#pragma alloc_text(PAGE, UdfRemoveClose)
#endif


VOID
UdfFspClose (
    IN PVCB Vcb OPTIONAL
    )

/*++

Routine Description:

    This routine is called to process the close queues in the UdfData.  If the
    Vcb is passed then we want to remove all of the closes for this Vcb.
    Otherwise we will do as many of the delayed closes as we need to do.

Arguments:

    Vcb - If specified then we are looking for all of the closes for the
        given Vcb.

Return Value:

    None

--*/

{
    PIRP_CONTEXT IrpContext;
    IRP_CONTEXT StackIrpContext;

    THREAD_CONTEXT ThreadContext;

    PFCB Fcb;
    ULONG UserReference;

    ULONG VcbHoldCount = 0;
    PVCB CurrentVcb = NULL;

    BOOLEAN ReleaseUdfData = FALSE;

    PAGED_CODE();

    //
    //  Check input.
    //

    ASSERT_OPTIONAL_VCB( Vcb );

    FsRtlEnterFileSystem();

    //
    //  Continue processing until there are no more closes to process.
    //

    while (IrpContext = UdfRemoveClose( Vcb )) {

        //
        //  If we don't have an IrpContext then use the one on the stack.
        //  Initialize it for this request.
        //

        if (SafeNodeType( IrpContext ) != UDFS_NTC_IRP_CONTEXT ) {

            //
            //  Update the local values from the IrpContextLite.
            //

            Fcb = ((PIRP_CONTEXT_LITE) IrpContext)->Fcb;
            UserReference = ((PIRP_CONTEXT_LITE) IrpContext)->UserReference;

            //
            //  Update the stack irp context with the values from the
            //  IrpContextLite.
            //

            UdfInitializeStackIrpContext( &StackIrpContext,
                                          (PIRP_CONTEXT_LITE) IrpContext );

            //
            //  Free the IrpContextLite.
            //

            UdfFreeIrpContextLite( (PIRP_CONTEXT_LITE) IrpContext );

            //
            //  Remember we have the IrpContext from the stack.
            //

            IrpContext = &StackIrpContext;

        //
        //  Otherwise cleanup the existing IrpContext.
        //

        } else {

            //
            //  Remember the Fcb and user reference count.
            //

            Fcb = (PFCB) IrpContext->Irp;
            IrpContext->Irp = NULL;

            UserReference = (ULONG) IrpContext->ExceptionStatus;
            IrpContext->ExceptionStatus = STATUS_SUCCESS;
        }

        //
        //  We have an IrpContext.  Now we need to set the top level thread
        //  context.
        //

        SetFlag( IrpContext->Flags, IRP_CONTEXT_FSP_FLAGS );

        //
        //  If we were given a Vcb then there is a request on top of this.
        //

        if (ARGUMENT_PRESENT( Vcb )) {

            ClearFlag( IrpContext->Flags,
                       IRP_CONTEXT_FLAG_TOP_LEVEL | IRP_CONTEXT_FLAG_TOP_LEVEL_UDFS );
        }

        UdfSetThreadContext( IrpContext, &ThreadContext );

        //
        //  If we have hit the maximum number of requests to process without
        //  releasing the Vcb then release the Vcb now.  If we are holding
        //  a different Vcb to this one then release the previous Vcb.
        //
        //  In either case acquire the current Vcb.
        //
        //  We use the MinDelayedCloseCount from the UdfData since it is
        //  a convenient value based on the system size.  Only thing we are trying
        //  to do here is prevent this routine starving other threads which
        //  may need this Vcb exclusively.
        //

        ReleaseUdfData = !ARGUMENT_PRESENT( Vcb ) &&
                         (Fcb->Vcb->VcbCondition != VcbMounted) &&
                         (Fcb->Vcb->VcbCondition != VcbMountInProgress) &&
                         (Fcb->Vcb->VcbCleanup == 0);

        if (ReleaseUdfData ||
            (VcbHoldCount > UdfData.MinDelayedCloseCount) ||
            (Fcb->Vcb != CurrentVcb)) {

            if (CurrentVcb != NULL) {

                UdfReleaseVcb( IrpContext, CurrentVcb );
            }

            if (ReleaseUdfData) {

                UdfAcquireUdfData( IrpContext );
            }

            CurrentVcb = Fcb->Vcb;
            UdfAcquireVcbShared( IrpContext, CurrentVcb, FALSE );

            VcbHoldCount = 0;

        } else {

            VcbHoldCount += 1;
        }

        DebugTrace(( +1, Dbg,
                     "UdfFspClose, Fcb %08x %4s Vcb %d/%d Fcb %d/%d\n",
                     Fcb,
                     ( UserReference? "USER" : "SYS" ),
                     CurrentVcb->VcbReference,
                     CurrentVcb->VcbUserReference,
                     Fcb->FcbReference,
                     Fcb->FcbUserReference ));

        //
        //  Call our worker routine to perform the close operation.
        //

        UdfCommonClosePrivate( IrpContext, CurrentVcb, Fcb, UserReference, FALSE );

        //
        //  If the reference count on this Vcb is below our residual reference
        //  then check if we should dismount the volume.
        //

        if (ReleaseUdfData) {

            UdfReleaseVcb( IrpContext, CurrentVcb );
            UdfCheckForDismount( IrpContext, CurrentVcb, FALSE );

            CurrentVcb = NULL;

            UdfReleaseUdfData( IrpContext );
            ReleaseUdfData = FALSE;
        }

        //
        //  Complete the current request to cleanup the IrpContext.
        //

        UdfCompleteRequest( IrpContext, NULL, STATUS_SUCCESS );

        DebugTrace(( -1, Dbg, "UdfFspClose -> VOID\n" ));
    }

    //
    //  Release any Vcb we may still hold.
    //

    if (CurrentVcb != NULL) {

        UdfReleaseVcb( IrpContext, CurrentVcb );
    }

    FsRtlExitFileSystem();
    return;
}


NTSTATUS
UdfCommonClose (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine is the Fsd entry for the close operation.  We decode the file
    object to find the UDFS structures and type of open.  We call our internal
    worker routine to perform the actual work.  If the work wasn't completed
    then we post to one of our worker queues.  The Ccb isn't needed after this
    point so we delete the Ccb and return STATUS_SUCCESS to our caller in all
    cases.

Arguments:

    Irp - Supplies the Irp to process

Return Value:

    STATUS_SUCCESS

--*/

{
    TYPE_OF_OPEN TypeOfOpen;

    PVCB Vcb;
    PFCB Fcb;
    PCCB Ccb;
    ULONG UserReference = 0;

    BOOLEAN DelayedClose;
    BOOLEAN ReleaseUdfData = FALSE;

    PAGED_CODE();

    //
    //  Check input.
    //

    ASSERT_IRP_CONTEXT( IrpContext );
    ASSERT_IRP( Irp );

    //
    //  If we were called with our file system device object instead of a
    //  volume device object, just complete this request with STATUS_SUCCESS.
    //

    if (IrpContext->Vcb == NULL) {

        UdfCompleteRequest( IrpContext, Irp, STATUS_SUCCESS );
        return STATUS_SUCCESS;
    }

    //
    //  Decode the file object to get the type of open and Fcb/Ccb.
    //

    TypeOfOpen = UdfDecodeFileObject( IoGetCurrentIrpStackLocation( Irp )->FileObject,
                                      &Fcb,
                                      &Ccb );

    //
    //  No work to do for unopened file objects.
    //

    if (TypeOfOpen == UnopenedFileObject) {

        UdfCompleteRequest( IrpContext, Irp, STATUS_SUCCESS );

        return STATUS_SUCCESS;
    }

    Vcb = Fcb->Vcb;

    //
    //  Call the worker routine to perform the actual work.  This routine
    //  should never raise except for a fatal error.
    //

    if (Ccb != NULL) {

        UserReference = 1;

        //
        //  We can always deallocate the Ccb if present.
        //

        UdfDeleteCcb( IrpContext, Ccb );
    }

    //
    //  If this is a user file or directory then check if we should post
    //  this to the delayed close queue.  This has to be the last reference
    //  for a user file or directory open.
    //

    if ((Vcb->VcbCondition == VcbMounted) &&
        (Fcb->FcbReference == 1) &&
        ((TypeOfOpen == UserFileOpen) ||
         (TypeOfOpen == UserDirectoryOpen))) {

        UdfQueueClose( IrpContext, Fcb, UserReference, TRUE );
        IrpContext = NULL;

    //
    //  Otherwise try to process this close.  Post to the async close queue
    //  if we can't acquire all of the resources.
    //

    } else {

        //
        //  If we may be dismounting this volume then acquire the UdfData
        //  resource.
        //
	//  Since we now must make volumes go away as soon as reasonable after
	//  the last user handles closes, key off of the cleanup count.  It is
	//  OK to do this more than neccesary.  Since this Fcb could be holding
	//  a number of other Fcbs (and thus their references), a simple check
	//  on reference count is not appropriate.
        //

        if ((Vcb->VcbCleanup == 0) &&
            (Vcb->VcbCondition != VcbMounted) &&
            (Vcb->VcbCondition != VcbMountInProgress) &&
            FlagOn( IrpContext->Flags, IRP_CONTEXT_FLAG_TOP_LEVEL_UDFS )) {

            UdfAcquireUdfData( IrpContext );
            ReleaseUdfData = TRUE;
        }

        if (!UdfCommonClosePrivate( IrpContext, Vcb, Fcb, UserReference, TRUE )) {

            //
            //  If we didn't complete the request then post the request as needed.
            //

            UdfQueueClose( IrpContext, Fcb, UserReference, FALSE );
            IrpContext = NULL;

        //
        //  Check whether we should be dismounting the volume and then complete
        //  the request.
        //

        } else if (ReleaseUdfData) {

            UdfCheckForDismount( IrpContext, Vcb, FALSE );
        }
    }

    //
    //  Always complete this request with STATUS_SUCCESS.
    //

    UdfCompleteRequest( IrpContext, Irp, STATUS_SUCCESS );

    if (ReleaseUdfData) {

        UdfReleaseUdfData( IrpContext );
    }

    //
    //  Always return STATUS_SUCCESS for closes.
    //

    return STATUS_SUCCESS;
}


//
//  Local support routine
//

BOOLEAN
UdfCommonClosePrivate (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    IN PFCB Fcb,
    IN ULONG UserReference,
    IN BOOLEAN FromFsd
    )

/*++

Routine Description:

    This is the worker routine for the close operation.  We can be called in
    an Fsd thread or from a worker Fsp thread.  If called from the Fsd thread
    then we acquire the resources without waiting.  Otherwise we know it is
    safe to wait.

    We check to see whether we should post this request to the delayed close
    queue.  If we are to process the close here then we acquire the Vcb and
    Fcb.  We will adjust the counts and call our teardown routine to see
    if any of the structures should go away.

Arguments:

    Vcb - Vcb for this volume.

    Fcb - Fcb for this request.

    UserReference - Number of user references for this file object.  This is
        zero for an internal stream.

    FromFsd - This request was called from an Fsd thread.  Indicates whether
        we should wait to acquire resources.

Return Value:

    BOOLEAN - TRUE if this thread processed the close, FALSE otherwise.

--*/

{
    BOOLEAN CompletedClose;
    BOOLEAN RemovedFcb;

    PAGED_CODE();

    //
    //  Check inputs.
    //
    
    ASSERT_IRP_CONTEXT( IrpContext );
    ASSERT_VCB( Vcb );
    ASSERT_FCB( Fcb );

    //
    //  Try to acquire the Vcb and Fcb.  If we can't acquire them then return
    //  and let our caller know he should post the request to the async
    //  queue.
    //

    if (UdfAcquireVcbShared( IrpContext, Vcb, FromFsd )) {

        if (!UdfAcquireFcbExclusive( IrpContext, Fcb, FromFsd )) {

            //
            //  We couldn't get the Fcb.  Release the Vcb and let our caller
            //  know to post this request.
            //

            UdfReleaseVcb( IrpContext, Vcb );
            return FALSE;
        }

    //
    //  We didn't get the Vcb.  Let our caller know to post this request.
    //

    } else {

        return FALSE;
    }

    //
    //  Lock the Vcb and decrement the reference counts.
    //

    UdfLockVcb( IrpContext, Vcb );
    
    DebugTrace(( +1, Dbg,
                 "UdfCommonClosePrivate, Fcb %08x %4s Vcb %d/%d Fcb %d/%d\n", Fcb,
                 ( UserReference? "USER" : "SYS" ),
                 Vcb->VcbReference,
                 Vcb->VcbUserReference,
                 Fcb->FcbReference,
                 Fcb->FcbUserReference ));

    UdfDecrementReferenceCounts( IrpContext, Fcb, 1, UserReference );

    DebugTrace(( +0, Dbg,
                 "UdfCommonClosePrivate, Vcb %d/%d Fcb %d/%d\n",
                 Vcb->VcbReference,
                 Vcb->VcbUserReference,
                 Fcb->FcbReference,
                 Fcb->FcbUserReference ));

    UdfUnlockVcb( IrpContext, Vcb );

    //
    //  Call our teardown routine to see if this object can go away.
    //  If we don't remove the Fcb then release it.
    //

    UdfTeardownStructures( IrpContext, Fcb, FALSE, &RemovedFcb );

    if (!RemovedFcb) {

        UdfReleaseFcb( IrpContext, Fcb );
    }

    DebugTrace(( -1, Dbg,
                 "UdfCommonClosePrivate, RemovedFcb %08x -> %c\n",
                 Fcb,
                 ( RemovedFcb? 'T' : 'F' )));

    //
    //  Release the Vcb and return to our caller.  Let him know we completed
    //  this request.
    //

    UdfReleaseVcb( IrpContext, Vcb );

    return TRUE;
}


VOID
UdfQueueClose (
    IN PIRP_CONTEXT IrpContext,
    IN PFCB Fcb,
    IN ULONG UserReference,
    IN BOOLEAN DelayedClose
    )

/*++

Routine Description:

    This routine is called to queue a request to either the async or delayed
    close queue.  For the delayed queue we need to allocate a smaller
    structure to contain the information about the file object.  We do
    that so we don't put the larger IrpContext structures into this long
    lived queue.  If we can allocate this structure then we put this
    on the async queue instead.

Arguments:

    Fcb - Fcb for this file object.

    UserReference - Number of user references for this file object.  This is
        zero for an internal stream.

    DelayedClose - Indicates whether this should go on the async or delayed
        close queue.

Return Value:

    None

--*/

{
    PIRP_CONTEXT_LITE IrpContextLite = NULL;
    BOOLEAN StartWorker = FALSE;

    PAGED_CODE();

    //
    //  Check inputs.
    //
    
    ASSERT_IRP_CONTEXT( IrpContext );
    ASSERT_FCB( Fcb );

    //
    //  Start with the delayed queue request.  We can move this to the async
    //  queue if there is an allocation failure.
    //

    if (DelayedClose) {

        //
        //  Try to allocate non-paged pool for the IRP_CONTEXT_LITE.
        //

        IrpContextLite = UdfCreateIrpContextLite( IrpContext );
    }

    //
    //  We want to clear the top level context in this thread if
    //  necessary.  Call our cleanup routine to do the work.
    //

    SetFlag( IrpContext->Flags, IRP_CONTEXT_FLAG_MORE_PROCESSING );
    UdfCleanupIrpContext( IrpContext, TRUE );

    //
    //  Synchronize with the UdfData lock.
    //

    UdfLockUdfData();

    DebugTrace(( +1, Dbg,
                 "UdfQueueClose, Fcb %08x %4s %5s\n",
                 Fcb,
                 ( UserReference? "USER" : "SYS" ),
                 ( IrpContextLite? "DELAY" : "ASYNC" )));


    //
    //  If we have an IrpContext then put the request on the delayed close queue.
    //

    if (IrpContextLite != NULL) {

        //
        //  Initialize the IrpContextLite.
        //

        IrpContextLite->NodeTypeCode = UDFS_NTC_IRP_CONTEXT_LITE;
        IrpContextLite->NodeByteSize = sizeof( IRP_CONTEXT_LITE );
        IrpContextLite->Fcb = Fcb;
        IrpContextLite->UserReference = UserReference;
        IrpContextLite->RealDevice = IrpContext->RealDevice;

        //
        //  Add this to the delayed close list and increment
        //  the count.
        //

        InsertTailList( &UdfData.DelayedCloseQueue,
                        &IrpContextLite->DelayedCloseLinks );

        UdfData.DelayedCloseCount += 1;

        //
        //  If we are above our threshold then start the delayed
        //  close operation.
        //

        if (UdfData.DelayedCloseCount > UdfData.MaxDelayedCloseCount) {

            UdfData.ReduceDelayedClose = TRUE;

            if (!UdfData.FspCloseActive) {

                UdfData.FspCloseActive = TRUE;
                StartWorker = TRUE;
            }
        }

        //
        //  Unlock the global data.
        //

        UdfUnlockUdfData();

        //
        //  Cleanup the IrpContext.
        //

        UdfCompleteRequest( IrpContext, NULL, STATUS_SUCCESS );

    //
    //  Otherwise drop into the async case below.
    //

    } else {

        //
        //  Store the information about the file object into the IrpContext.
        //

        IrpContext->Irp = (PIRP) Fcb;
        IrpContext->ExceptionStatus = (NTSTATUS) UserReference;

        //
        //  Add this to the async close list and increment the count.
        //

        InsertTailList( &UdfData.AsyncCloseQueue,
                        &IrpContext->WorkQueueItem.List );

        UdfData.AsyncCloseCount += 1;

        //
        //  Remember to start the Fsp close thread if not currently started.
        //

        if (!UdfData.FspCloseActive) {

            UdfData.FspCloseActive = TRUE;
            StartWorker = TRUE;
        }

        //
        //  Unlock the global data.
        //

        UdfUnlockUdfData();
    }

    //
    //  Start the FspClose thread if we need to.
    //

    if (StartWorker) {

        ExQueueWorkItem( &UdfData.CloseItem, CriticalWorkQueue );
    }

    DebugTrace(( -1, Dbg, "UdfQueueClose -> VOID\n" ));

    //
    //  Return to our caller.
    //

    return;
}


//
//  Local support routine
//

PIRP_CONTEXT
UdfRemoveClose (
    IN PVCB Vcb OPTIONAL
    )

/*++

Routine Description:

Arguments:

    This routine is called to scan the async and delayed close queues looking
    for a suitable entry.  If the Vcb is specified then we scan both queues
    looking for an entry with the same Vcb.  Otherwise we will look in the
    async queue first for any close item.  If none found there then we look
    in the delayed close queue provided that we have triggered the delayed
    close operation.

Return Value:

    PIRP_CONTEXT - NULL if no work item found.  Otherwise it is the pointer to
        either the IrpContext or IrpContextLite for this request.

--*/

{
    PIRP_CONTEXT IrpContext = NULL;
    PIRP_CONTEXT NextIrpContext;
    PIRP_CONTEXT_LITE NextIrpContextLite;

    PLIST_ENTRY Entry;

    PAGED_CODE();

    ASSERT_OPTIONAL_VCB( Vcb );

    //
    //  Lock the UdfData to perform the scan.
    //

    UdfLockUdfData();

    //
    //  First check the list of async closes.
    //

    Entry = UdfData.AsyncCloseQueue.Flink;

    while (Entry != &UdfData.AsyncCloseQueue) {

        //
        //  Extract the IrpContext.
        //

        NextIrpContext = CONTAINING_RECORD( Entry,
                                            IRP_CONTEXT,
                                            WorkQueueItem.List );

        //
        //  If no Vcb was specified or this Vcb is for our volume
        //  then perform the close.
        //

        if (!ARGUMENT_PRESENT( Vcb ) || (NextIrpContext->Vcb == Vcb)) {

            RemoveEntryList( Entry );
            UdfData.AsyncCloseCount -= 1;

            IrpContext = NextIrpContext;
            break;
        }

        //
        //  Move to the next entry.
        //

        Entry = Entry->Flink;
    }

    //
    //  If we didn't find anything look through the delayed close
    //  queue.
    //
    //  We will only check the delayed close queue if we were given
    //  a Vcb or the delayed close operation is active.
    //

    if ((IrpContext == NULL) &&
        (ARGUMENT_PRESENT( Vcb ) ||
         (UdfData.ReduceDelayedClose &&
          (UdfData.DelayedCloseCount > UdfData.MinDelayedCloseCount)))) {

        Entry = UdfData.DelayedCloseQueue.Flink;

        while (Entry != &UdfData.DelayedCloseQueue) {

            //
            //  Extract the IrpContext.
            //

            NextIrpContextLite = CONTAINING_RECORD( Entry,
                                                    IRP_CONTEXT_LITE,
                                                    DelayedCloseLinks );

            //
            //  If no Vcb was specified or this Vcb is for our volume
            //  then perform the close.
            //

            if (!ARGUMENT_PRESENT( Vcb ) || (NextIrpContextLite->Fcb->Vcb == Vcb)) {

                RemoveEntryList( Entry );
                UdfData.DelayedCloseCount -= 1;

                IrpContext = (PIRP_CONTEXT) NextIrpContextLite;
                break;
            }

            //
            //  Move to the next entry.
            //

            Entry = Entry->Flink;
        }
    }

    //
    //  If the Vcb wasn't specified and we couldn't find an entry
    //  then turn off the Fsp thread.
    //

    if (!ARGUMENT_PRESENT( Vcb ) && (IrpContext == NULL)) {

        UdfData.FspCloseActive = FALSE;
        UdfData.ReduceDelayedClose = FALSE;
    }

    //
    //  Unlock the global data.
    //

    UdfUnlockUdfData();

    return IrpContext;
}


