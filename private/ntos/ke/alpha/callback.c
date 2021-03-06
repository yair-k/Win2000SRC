/*++

Copyright (c) 1994  Microsoft Corporation

Module Name:

    callback.c

Abstract:

    This module implements stubs for the user mode call back services.

Author:

    David N. Cutler (davec) 29-Oct-1994

Environment:

    Kernel mode only.

Revision History:

--*/

#include "ki.h"

NTSTATUS
KeUserModeCallback (
    IN ULONG ApiNumber,
    IN PVOID InputBuffer,
    IN ULONG InputLength,
    OUT PVOID *OutputBuffer,
    IN PULONG OutputLength
    )

/*++

Routine Description:

    This function call out from kernel mode to a user mode function.

Arguments:

    ApiNumber - Supplies the API number.

    InputBuffer - Supplies a pointer to a structure that is copied
        to the user stack.

    InputLength - Supplies the length of the input structure.

    Outputbuffer - Supplies a pointer to a variable that receives
        the address of the output buffer.

    Outputlength - Supplies a pointer to a variable that receives
        the length of the output buffer.

Return Value:

    If the callout cannot be executed, then an error status is
    returned. Otherwise, the status returned by the callback function
    is returned.

--*/

{

    PUCALLOUT_FRAME CalloutFrame;
    ULONG Length;
    ULONGLONG OldStack;
    NTSTATUS Status;
    PKTRAP_FRAME TrapFrame;
    PVOID ValueBuffer;
    ULONG ValueLength;

    ASSERT(KeGetPreviousMode() == UserMode);

    //
    // Get the user mode stack pointer and attempt to copy input buffer
    // to the user stack.
    //

    TrapFrame = KeGetCurrentThread()->TrapFrame;
    OldStack = TrapFrame->IntSp;
    try {

        //
        // Compute new user mode stack address, probe for writability,
        // and copy the input buffer to the user stack.
        //
        // N.B. Alpha requires stacks to be 16-byte aligned, therefore
        //      the input length must be rounded up to a 16-byte boundary.
        //

        Length =  (InputLength +
                16 - 1 + sizeof(UCALLOUT_FRAME)) & ~(16 - 1);

        CalloutFrame = (PUCALLOUT_FRAME)(OldStack - Length);
        ProbeForWrite(CalloutFrame, Length, sizeof(QUAD));
        RtlCopyMemory(CalloutFrame + 1, InputBuffer, InputLength);

        //
        // Allocate stack frame and fill in callout arguments.
        //

        CalloutFrame->Buffer = (PVOID)(CalloutFrame + 1);
        CalloutFrame->Length = InputLength;
        CalloutFrame->ApiNumber = ApiNumber;
        CalloutFrame->Sp = OldStack;
        CalloutFrame->Ra = TrapFrame->IntRa;

    //
    // If an exception occurs during the probe of the user stack, then
    // always handle the exception and return the exception code as the
    // status value.
    //

    } except (EXCEPTION_EXECUTE_HANDLER) {
        return GetExceptionCode();
    }

    //
    // Call user mode.
    //

    TrapFrame->IntSp = (ULONGLONG)(LONG_PTR)CalloutFrame;
    Status = KiCallUserMode(OutputBuffer, OutputLength);

    //
    // When returning from user mode, any drawing done to the GDI TEB
    // batch must be flushed.
    //

    if (((PTEB)KeGetCurrentThread()->Teb)->GdiBatchCount > 0) {
        TrapFrame->IntSp -= 256;
        KeGdiFlushUserBatch();
    }

    TrapFrame->IntSp = OldStack;
    return Status;
}

NTSTATUS
NtW32Call (
    IN ULONG ApiNumber,
    IN PVOID InputBuffer,
    IN ULONG InputLength,
    OUT PVOID *OutputBuffer,
    OUT PULONG OutputLength
    )

/*++

Routine Description:

    This function calls a W32 function.

Arguments:

    ApiNumber - Supplies the API number.

    InputBuffer - Supplies a pointer to a structure that is copied to
        the user stack.

    InputLength - Supplies the length of the input structure.

    Outputbuffer - Supplies a pointer to a variable that recevies the
        output buffer address.

    Outputlength - Supplies a pointer to a variable that recevies the
        output buffer length.

Return Value:

    TBS.

--*/

{

    PVOID ValueBuffer;
    ULONG ValueLength;
    NTSTATUS Status;

    ASSERT(KeGetPreviousMode() == UserMode);

    //
    // If the current thread is not a GUI thread, then fail the service
    // since the thread does not have a large stack.
    //

    if (KeGetCurrentThread()->Win32Thread == (PVOID)&KeServiceDescriptorTable[0]) {
        return STATUS_NOT_IMPLEMENTED;
    }

    //
    // Probe the output buffer address and length for writeability.
    //

    try {
        ProbeForWriteUlong((PULONG)OutputBuffer);
        ProbeForWriteUlong(OutputLength);

    //
    // If an exception occurs during the probe of the output buffer or
    // length, then always handle the exception and return the exception
    // code as the status value.
    //

    } except(EXCEPTION_EXECUTE_HANDLER) {
        return GetExceptionCode();
    }

    //
    // Call out to user mode specifying the input buffer and API number.
    //

    Status = KeUserModeCallback(ApiNumber,
                                InputBuffer,
                                InputLength,
                                &ValueBuffer,
                                &ValueLength);

    //
    // If the callout is successful, then the output buffer address and
    // length.
    //

    if (NT_SUCCESS(Status)) {
        try {
            *OutputBuffer = ValueBuffer;
            *OutputLength = ValueLength;

        } except(EXCEPTION_EXECUTE_HANDLER) {
        }
    }

    return Status;
}
