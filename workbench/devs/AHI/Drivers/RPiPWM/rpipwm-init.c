
#include <config.h>

#include <proto/exec.h>
#include <proto/kernel.h>
#include <proto/dma.h>

#include "library.h"
#include "DriverData.h"
#include "rpipwm-hwaccess.h"

APTR KernelBase = NULL;
APTR DMABase = NULL;

/******************************************************************************
** Custom driver init *********************************************************
******************************************************************************/

BOOL DriverInit(struct DriverBase *AHIsubBase)
{
    struct RPiPWMBase *RPiPWMBase = (struct RPiPWMBase *) AHIsubBase;

    /*
     * No headphone jack, no driver 
     */
    if (!pwm_audio_present(AHIsubBase))
        return FALSE;

    RPiPWMBase->dosbase = (struct DosLibrary *) OpenLibrary(DOSNAME, 37);

    if (RPiPWMBase->dosbase == NULL) {
        Req("Unable to open 'dos.library' version 37.\n");
        return FALSE;
    }

    KernelBase = OpenResource("kernel.resource");

    if (KernelBase == NULL) {
        Req("Unable to open 'kernel.resource'.\n");
        return FALSE;
    }

    DMABase = OpenResource("dma.resource");

    if (DMABase == NULL) {
        Req("Unable to open 'dma.resource'.\n");
        return FALSE;
    }

    RPiPWMBase->periiobase = KrnGetSystemAttr(KATTR_PeripheralBase);

    if (RPiPWMBase->periiobase == 0) {
        Req("No BCM283x peripheral base found.\n");
        return FALSE;
    }

    /* The device tree describes a jack the emulator does not model - refuse,
     * so AHI keeps the void driver rather than a mode that stays silent. */
    if (KrnGetSystemAttr(KATTR_Emulated) == 1)
        return FALSE;

    return TRUE;
}


/******************************************************************************
** Custom driver clean-up *****************************************************
******************************************************************************/

VOID DriverCleanup(struct DriverBase *AHIsubBase)
{
    struct RPiPWMBase *RPiPWMBase = (struct RPiPWMBase *) AHIsubBase;

    CloseLibrary((struct Library *) DOSBase);
}
