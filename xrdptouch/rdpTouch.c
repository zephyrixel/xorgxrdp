/*
 * xorgxrdp native direct-touch input driver.
 *
 * Touch contacts arrive through the xrdp/xup private input protocol and are
 * posted as XInput2 direct-touch events. Pointer emulation is intentionally
 * disabled: applications must consume the native XI2 touch events.
 */

#if defined(HAVE_CONFIG_H)
#include "config_ac.h"
#endif

#include <stdint.h>

#include <xorg-server.h>
#include <xorgVersion.h>
#include <xf86.h>
#include <xf86_OSproc.h>
#include <xf86Xinput.h>
#include <X11/extensions/XI.h>
#include <X11/extensions/XI2.h>
#include <exevents.h>
#include <xserver-properties.h>

#include "rdp.h"
#include "rdpDraw.h"
#include "rdpInput.h"
#include "rdpMisc.h"

#define XRDP_TOUCH_MAX_CONTACTS 10
#define XRDP_TOUCH_AXES 2

static char g_touch_type[] = XI_TOUCHSCREEN;
static char g_touch_name[] = "XRDPTouch";
static DeviceIntPtr g_touch_device;

/******************************************************************************/
static int
rdptouchInput(rdpPtr dev, uint32_t contact_id, uint32_t state,
              int32_t x, int32_t y)
{
    ValuatorMask *mask;
    uint16_t event_type;
    uint32_t event_flags = TOUCH_CLIENT_ID;

    if (g_touch_device == NULL || !((DevicePtr)g_touch_device)->on ||
            contact_id >= 256)
    {
        return 0;
    }

    x = RDPCLAMP(x, 0, dev->width > 0 ? dev->width - 1 : 0);
    y = RDPCLAMP(y, 0, dev->height > 0 ? dev->height - 1 : 0);
    mask = valuator_mask_new(XRDP_TOUCH_AXES);
    if (mask == NULL)
    {
        return 1;
    }
    valuator_mask_set(mask, 0, x);
    valuator_mask_set(mask, 1, y);

    switch (state)
    {
        case XRDP_TOUCH_CONTACT_DOWN:
            event_type = XI_TouchBegin;
            break;
        case XRDP_TOUCH_CONTACT_UPDATE:
            event_type = XI_TouchUpdate;
            break;
        case XRDP_TOUCH_CONTACT_UP:
            event_type = XI_TouchEnd;
            event_flags |= TOUCH_END;
            break;
        default:
            valuator_mask_free(&mask);
            return 1;
    }

    xf86PostTouchEvent(g_touch_device, contact_id, event_type,
                       event_flags, mask);
    valuator_mask_free(&mask);
    return 0;
}

/******************************************************************************/
static int
rdptouchControlDevice(DeviceIntPtr device, int what)
{
    DevicePtr p_dev;
    rdpPtr dev;
    Atom axes_labels[XRDP_TOUCH_AXES];
    CARD8 button_map[] = { 0, 1 };

    p_dev = (DevicePtr)device;
    switch (what)
    {
        case DEVICE_INIT:
            /* Xorg's touch event state machine requires ButtonClass, but no
             * pointer-emulation flag is sent, so this never creates mouse
             * events for the remote touch contacts. */
            if (!InitButtonClassDeviceStruct(device, 1, NULL, button_map))
            {
                return BadAlloc;
            }
            axes_labels[0] = XIGetKnownProperty(AXIS_LABEL_PROP_ABS_X);
            axes_labels[1] = XIGetKnownProperty(AXIS_LABEL_PROP_ABS_Y);
            if (!InitValuatorClassDeviceStruct(device, XRDP_TOUCH_AXES,
                                                axes_labels,
                                                GetMotionHistorySize(), Absolute))
            {
                return BadAlloc;
            }
            if (!InitTouchClassDeviceStruct(device, XRDP_TOUCH_MAX_CONTACTS,
                                            XIDirectTouch, XRDP_TOUCH_AXES))
            {
                return BadAlloc;
            }

            dev = rdpGetDevFromScreen(NULL);
            xf86InitValuatorAxisStruct(device, 0, axes_labels[0], 0,
                                       dev->width - 1, 1, 1, 1, Absolute);
            xf86InitValuatorAxisStruct(device, 1, axes_labels[1], 0,
                                       dev->height - 1, 1, 1, 1, Absolute);
            g_touch_device = device;
            rdpRegisterTouchCallback(rdptouchInput);
            break;

        case DEVICE_ON:
            p_dev->on = 1;
            break;

        case DEVICE_OFF:
            p_dev->on = 0;
            break;

        case DEVICE_CLOSE:
            if (p_dev->on)
            {
                p_dev->on = 0;
            }
            break;
    }
    return Success;
}

#if XORG_VERSION_CURRENT < XORG_VERSION_NUMERIC(1, 9, 0, 1, 0)
static InputInfoPtr
rdptouchPreInit(InputDriverPtr drv, IDevPtr dev, int flags)
{
    InputInfoPtr info = xf86AllocateInput(drv, 0);
    (void) flags;
    info->name = dev->identifier;
    info->device_control = rdptouchControlDevice;
    info->flags = XI86_CONFIGURED;
    info->type_name = g_touch_type;
    info->fd = -1;
    info->conf_idev = dev;
    return info;
}
#else
static int
rdptouchPreInit(InputDriverPtr drv, InputInfoPtr info, int flags)
{
    (void) drv;
    (void) flags;
    info->device_control = rdptouchControlDevice;
    info->type_name = g_touch_type;
    return 0;
}
#endif

/******************************************************************************/
static void
rdptouchUnInit(InputDriverPtr drv, InputInfoPtr info, int flags)
{
    (void) drv;
    (void) info;
    (void) flags;
    rdpUnregisterTouchCallback(rdptouchInput);
    g_touch_device = NULL;
}

static InputDriverRec rdptouch =
{
    .driverVersion = PACKAGE_VERSION_MAJOR,
    .driverName = g_touch_name,
    .PreInit = rdptouchPreInit,
    .UnInit = rdptouchUnInit
};

static pointer
rdptouchPlug(pointer module, pointer options, int *errmaj, int *errmin)
{
    (void) options;
    (void) errmaj;
    (void) errmin;
    xf86AddInputDriver(&rdptouch, module, 0);
    return module;
}

static void
rdptouchUnplug(pointer module)
{
    (void) module;
}

static XF86ModuleVersionInfo rdptouchVersionRec =
{
    .modname = "xrdptouch",
    .vendor = MODULEVENDORSTRING,
    ._modinfo1_ = MODINFOSTRING1,
    ._modinfo2_ = MODINFOSTRING2,
    .xf86version = XORG_VERSION_CURRENT,
    .majorversion = PACKAGE_VERSION_MAJOR,
    .minorversion = PACKAGE_VERSION_MINOR,
    .patchlevel = PACKAGE_VERSION_PATCHLEVEL,
    .abiclass = ABI_CLASS_XINPUT,
    .abiversion = ABI_XINPUT_VERSION,
    .moduleclass = MOD_CLASS_XINPUT
};

_X_EXPORT XF86ModuleData xrdptouchModuleData =
{
    .vers = &rdptouchVersionRec,
    .setup = rdptouchPlug,
    .teardown = rdptouchUnplug
};
