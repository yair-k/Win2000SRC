/******************************************************************************

   Copyright (C) Microsoft Corporation 1985-1991. All rights reserved.

   Title:   drvproc.c - Multimedia Systems Media Control Interface
            driver for AVI.

*****************************************************************************/
#include <nt.h>
#include <ntrtl.h>
#include <nturtl.h>	// for {struct _TEB} defintion
#include "graphic.h"
#include "cnfgdlg.h"            // to get IDA_CONFIG
#include "avitask.h"            // to get mciaviTaskCleanup()

#ifndef _WIN32 // Not used in 32 bit world
void NEAR PASCAL AppExit(HTASK htask, BOOL fNormalExit);
#endif

#define CONFIG_ID   10000L  // Use the hiword of dwDriverID to identify
HANDLE ghModule;
extern HWND ghwndConfig;
extern const TCHAR szIni[];

/* Link to DefDriverProc in MMSystem explicitly, so we don't get the
** one in USER by mistake.
*/
#ifndef _WIN32
extern DWORD FAR PASCAL mmDefDriverProc(DWORD, HANDLE, UINT, DWORD, DWORD);
#else
#define mmDefDriverProc DefDriverProc
#endif

#ifndef _WIN32
BOOL FAR PASCAL LibMain (HANDLE hModule, int cbHeap, LPSTR lpchCmdLine)
{
    ghModule = hModule;
    return TRUE;
}
#else
#if 0
// Get the module handle on DRV_LOAD
BOOL DllInstanceInit(PVOID hModule, ULONG Reason, PCONTEXT pContext)
{
    if (Reason == DLL_PROCESS_ATTACH) {
        ghModule = hModule;  // All we need to save is our module handle...
    } else {
        if (Reason == DLL_PROCESS_DETACH) {
        }
    }
    return TRUE;
}

#endif
#endif // WIN16

/***************************************************************************
 *
 * @doc     INTERNAL
 *
 * @api     DWORD | DriverProc | The entry point for an installable driver.
 *
 * @parm    DWORD | dwDriverId | For most messages, dwDriverId is the DWORD
 *          value that the driver returns in response to a DRV_OPEN message.
 *          Each time that the driver is opened, through the DrvOpen API,
 *          the driver receives a DRV_OPEN message and can return an
 *          arbitrary, non-zero, value. The installable driver interface
 *          saves this value and returns a unique driver handle to the
 *          application. Whenever the application sends a message to the
 *          driver using the driver handle, the interface routes the message
 *          to this entry point and passes the corresponding dwDriverId.
 *
 *          This mechanism allows the driver to use the same or different
 *          identifiers for multiple opens but ensures that driver handles
 *          are unique at the application interface layer.
 *
 *          The following messages are not related to a particular open
 *          instance of the driver. For these messages, the dwDriverId
 *          will always be  ZERO.
 *
 *              DRV_LOAD, DRV_FREE, DRV_ENABLE, DRV_DISABLE, DRV_OPEN
 *
 * @parm    UINT | wMessage | The requested action to be performed. Message
 *          values below DRV_RESERVED are used for globally defined messages.
 *          Message values from DRV_RESERVED to DRV_USER are used for
 *          defined driver portocols. Messages above DRV_USER are used
 *          for driver specific messages.
 *
 * @parm    DWORD | dwParam1 | Data for this message.  Defined separately for
 *          each message
 *
 * @parm    DWORD | dwParam2 | Data for this message.  Defined separately for
 *          each message
 *
 * @rdesc Defined separately for each message.
 *
 ***************************************************************************/

DWORD_PTR FAR PASCAL _LOADDS DriverProc (DWORD_PTR dwDriverID, HANDLE hDriver, UINT wMessage,
    LPARAM dwParam1, LPARAM dwParam2)
{
    DWORD_PTR dwRes = 0L;


    /*
     * critical sections are now per-device. This means they
     * cannot be held around the whole driver-proc, since until we open
     * the device, we don't have a critical section to hold.
     * The critical section is allocated in mciSpecial on opening. It is
     * also held in mciDriverEntry, in GraphicWndProc, and around
     * all worker thread draw functions.
     */


    switch (wMessage)
        {

        // Standard, globally used messages.

        case DRV_LOAD:
	{
	    struct _TEB *pteb;

#ifdef _WIN32
            if (ghModule) {
                Assert(!"Did not expect ghModule to be non-NULL");
            }
            ghModule = GetDriverModuleHandle(hDriver);  // Remember

	    // The WOW guys say that the proper method of detecting
	    // whether we're talking to a 16-bit app is to test
	    // NtCurrentTeb()->WOW32Reserved; if it's nonzero, it's 16-bit.
	    // In the (unlikely) event that we can't test that, default
	    // to using the old detection method.
	    //
	    if ((pteb = NtCurrentTeb()) != NULL) {
                runningInWow = (pteb->WOW32Reserved != 0) ? TRUE : FALSE;
	    } else {
                #define GET_MAPPING_MODULE_NAME         TEXT("wow32.dll")
                runningInWow = (GetModuleHandle(GET_MAPPING_MODULE_NAME) != NULL);
            }
#endif
            if (GraphicInit())       // Initialize graphic mgmt.
                dwRes = 1L;
            else
                dwRes = 0L;

            break;
	}

        case DRV_FREE:

            GraphicFree();
            dwRes = 1L;
            DPF(("Returning from DRV_FREE\n"));
            Assert(npMCIList == NULL);
            ghModule = NULL;
            break;

        case DRV_OPEN:

            if (!dwParam2)
                dwRes = CONFIG_ID;
            else
                dwRes = GraphicDrvOpen((LPMCI_OPEN_DRIVER_PARMS)dwParam2);

            break;

        case DRV_CLOSE:
	    /* If we have a configure dialog up, fail the close.
	    ** Otherwise, we'll be unloaded while we still have the
	    ** configuration window up.
	    */
	    if (ghwndConfig)
		dwRes = 0L;
	    else
		dwRes = 1L;
            break;

        case DRV_ENABLE:

            dwRes = 1L;
            break;

        case DRV_DISABLE:

            dwRes = 1L;
            break;

        case DRV_QUERYCONFIGURE:

            dwRes = 1L;	/* Yes, we can be configured */
            break;

        case DRV_CONFIGURE:
            ConfigDialog((HWND)(UINT)dwParam1, NULL);
            dwRes = 1L;
            break;

#ifndef _WIN32
        //
        //  sent when a application is terminating
        //
        //  lParam1:
        //      DRVEA_ABNORMALEXIT
        //      DRVEA_NORMALEXIT
        //
        case DRV_EXITAPPLICATION:
            AppExit(GetCurrentTask(), (BOOL)dwParam1 == DRVEA_NORMALEXIT);
            break;
#endif

        default:

            if (!HIWORD(dwDriverID) &&
                wMessage >= DRV_MCI_FIRST &&
                wMessage <= DRV_MCI_LAST)

                dwRes = mciDriverEntry ((UINT) (UINT_PTR) dwDriverID,
                                        wMessage,
                                        (UINT) dwParam1,
                                        (LPMCI_GENERIC_PARMS)dwParam2);
            else
                dwRes = mmDefDriverProc(dwDriverID,
                                      hDriver,
                                      wMessage,
                                      dwParam1,
                                      dwParam2);
            break;
        }

    return dwRes;
}

#ifndef _WIN32
/*****************************************************************************
 * @doc INTERNAL
 *
 * @func void | AppExit |
 *      a application is exiting
 *
 ****************************************************************************/

void NEAR PASCAL AppExit(HTASK htask, BOOL fNormalExit)
{
    //
    //  walk the list of open MCIAVI instances and see if
    //  the dying task is the background task and do cleanup.
    //
    NPMCIGRAPHIC npMCI;

    // Note: we do not have EnterList/LeaveList macros here as this is
    // explicitly NOT Win32 code.
    for (npMCI=npMCIList; npMCI; npMCI = npMCI->npMCINext) {

        if (npMCI->hTask == htask) {
            DPF(("Calling mciaviTaskCleanup()\n"));
            mciaviTaskCleanup(npMCI);
            return;
        }
    }
}
#endif
