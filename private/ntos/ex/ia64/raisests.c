/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    raisests.c

Abstract:

    This module implements routines to raise a general exception from kernel
    mode or a noncontinuable exception from kernel mode.

Author:

    William K. Cheung (wcheung) 08-Mar-1996

    based on David N. Cutler (davec) 18-Oct-1990

Environment:

    Any mode.

Revision History:

--*/

#include "exp.h"

//
// Define private function prototypes.
//

VOID
ExpRaiseException (
    IN PEXCEPTION_RECORD ExceptionRecord
    );

VOID
ExpRaiseStatus (
    IN NTSTATUS ExceptionCode
    );

VOID
ExRaiseException (
    IN PEXCEPTION_RECORD ExceptionRecord
    )

/*++

Routine Description:

    This function raises a software exception by building a context record
    and calling the exception dispatcher directly.

    N.B. This routine is a shell routine that simply calls another routine
         to do the real work. The reason this is done is to avoid a problem
         in try/finally scopes where the last statement in the scope is a
         call to raise an exception.

Arguments:

    ExceptionRecord - Supplies a pointer to an exception record.

Return Value:

    None.

--*/

{

    ExpRaiseException(ExceptionRecord);
    return;
}

VOID
ExpRaiseException (
    IN PEXCEPTION_RECORD ExceptionRecord
    )

/*++

Routine Description:

    This function raises a software exception by building a context record
    and calling the exception dispatcher directly.

    N.B. Assuming that the call is made in the 3rd slot of a bundle;
         therefore, ControlPc is ORed with 0x8.

Arguments:

    ExceptionRecord - Supplies a pointer to an exception record.

Return Value:

    None.

--*/

{

    ULONGLONG ControlPc;
    CONTEXT ContextRecord;
    FRAME_POINTERS EstablisherFrame;
    PRUNTIME_FUNCTION FunctionEntry;
    BOOLEAN InFunction;
    ULONGLONG NextPc;
    NTSTATUS Status;
    ULONGLONG ImageBase;
    ULONGLONG TargetGp;

    //
    // Capture the current context, virtually unwind to the caller of this
    // routine, set the fault instruction address to that of the caller, and
    // call the exception dispatcher.
    //

    RtlCaptureContext(&ContextRecord);
    ControlPc = RtlIa64InsertIPSlotNumber((ContextRecord.BrRp-16), 2);
    FunctionEntry = RtlLookupFunctionEntry(ControlPc, &ImageBase, &TargetGp);
    NextPc = RtlVirtualUnwind(ImageBase,
                              ControlPc,
                              FunctionEntry,
                              &ContextRecord,
                              &InFunction,
                              &EstablisherFrame,
                              NULL);

    ContextRecord.StIIP = NextPc + 8;
    ContextRecord.StIPSR &= ~((ULONGLONG) 3 << PSR_RI);
    ExceptionRecord->ExceptionAddress = (PVOID)ContextRecord.StIIP;

    //
    // If the exception is successfully dispatched, then continue execution.
    // Otherwise, give the kernel debugger a chance to handle the exception.
    //

    if (RtlDispatchException(ExceptionRecord, &ContextRecord)) {
        Status = ZwContinue(&ContextRecord, FALSE);

    } else {
        Status = ZwRaiseException(ExceptionRecord, &ContextRecord, FALSE);
    }

    //
    // Either the attempt to continue execution or the attempt to give
    // the kernel debugger a chance to handle the exception failed. Raise
    // a noncontinuable exception.
    //

    ExRaiseStatus(Status);
}

VOID
ExRaiseStatus (
    IN NTSTATUS ExceptionCode
    )

/*++

Routine Description:

    This function raises an exception with the specified status value by
    building an exception record, building a context record, and calling the
    exception dispatcher directly. The exception is marked as noncontinuable
    with no parameters. There is no return from this function.

    N.B. This routine is a shell routine that simply calls another routine
         to do the real work. The reason this is done is to avoid a problem
         in try/finally scopes where the last statement in the scope is a
         call to raise an exception.

Arguments:

    ExceptionCode - Supplies the status value to be used as the exception
        code for the exception that is to be raised.

Return Value:

    None.

--*/

{

    ExpRaiseStatus(ExceptionCode);
    return;
}

VOID
ExpRaiseStatus (
    IN NTSTATUS ExceptionCode
    )

/*++

Routine Description:

    This function raises an exception with the specified status value by
    building an exception record, building a context record, and calling the
    exception dispatcher directly. The exception is marked as noncontinuable
    with no parameters. There is no return from this function.

    N.B. Assuming that the call is made in the 3rd slot of a bundle;
         therefore, ControlPc is ORed with 0x8.

Arguments:

    ExceptionCode - Supplies the status value to be used as the exception
        code for the exception that is to be raised.

Return Value:

    None.

--*/

{

    ULONGLONG ControlPc;
    CONTEXT ContextRecord;
    FRAME_POINTERS EstablisherFrame;
    EXCEPTION_RECORD ExceptionRecord;
    PRUNTIME_FUNCTION FunctionEntry;
    BOOLEAN InFunction;
    ULONGLONG NextPc;
    NTSTATUS Status;
    ULONGLONG ImageBase;
    ULONGLONG TargetGp;

    //
    // Construct an exception record.
    //

    ExceptionRecord.ExceptionCode = ExceptionCode;
    ExceptionRecord.ExceptionRecord = (PEXCEPTION_RECORD)NULL;
    ExceptionRecord.NumberParameters = 0;
    ExceptionRecord.ExceptionFlags = EXCEPTION_NONCONTINUABLE;

    //
    // Capture the current context, virtually unwind to the caller of this
    // routine, set the fault instruction address to that of the caller, and
    // call the exception dispatcher.
    //

    RtlCaptureContext(&ContextRecord);
    ControlPc = RtlIa64InsertIPSlotNumber((ContextRecord.BrRp-16), 2);
    FunctionEntry = RtlLookupFunctionEntry(ControlPc, &ImageBase, &TargetGp);
    NextPc = RtlVirtualUnwind(ImageBase,
                              ControlPc,
                              FunctionEntry,
                              &ContextRecord,
                              &InFunction,
                              &EstablisherFrame,
                              NULL);

    ContextRecord.StIIP = NextPc + 8;
    ContextRecord.StIPSR &= ~((ULONGLONG) 3 << PSR_RI);
    ExceptionRecord.ExceptionAddress = (PVOID)ContextRecord.StIIP;
    RtlDispatchException(&ExceptionRecord, &ContextRecord);

    //
    // An unwind was not initiated during the dispatching of a noncontinuable
    // exception. Give the kernel debugger a chance to handle the exception.
    //

    Status = ZwRaiseException(&ExceptionRecord, &ContextRecord, FALSE);

    //
    // The attempt to give the kernel debugger a chance to handle the exception
    // failed. Raise another noncontinuable exception.
    //

    ExRaiseStatus(Status);
}
