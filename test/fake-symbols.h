#include <xorg-server.h>
#include <dix.h>
#include <os.h>
#include <exevents.h>
#include <Xprintf.h>
#include <xf86.h>
#include <xf86Xinput.h>
#include <xf86_OSproc.h>

#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) >= 14
#define OPTTYPE XF86OptionPtr
#define CONST const
#else
#define OPTTYPE pointer
#define CONST
#endif
