#include <xorg-server.h>
#include <xf86Xinput.h>

#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) >= 14
#define OPTTYPE XF86OptionPtr
#define CONST const
#else
#define OPTTYPE pointer
#define CONST
#endif

extern int xf86ReadSerial (int fd, void *buf, int count);
extern int xf86WriteSerial (int fd, const void *buf, int count);
extern int xf86CloseSerial (int fd);
extern int xf86WaitForInput (int fd, int timeout);
extern int xf86OpenSerial (pointer options);
extern int xf86SetSerialSpeed (int fd, int speed);

extern OPTTYPE xf86ReplaceIntOption(OPTTYPE optlist, const char *name, const int val);
extern OPTTYPE xf86AddNewOption(OPTTYPE head, const char *name, const char *val);
extern char* xf86OptionName(OPTTYPE opt);
extern CONST char* xf86FindOptionValue(OPTTYPE options, const char *name);
extern int xf86NameCmp(const char *s1, const char *s2);
extern char* xf86CheckStrOption(OPTTYPE optlist, const char *name, char *deflt);


extern char * xf86SetStrOption(OPTTYPE optlist, const char *name, CONST char *deflt);
extern int xf86SetBoolOption(OPTTYPE optlist, const char *name, int deflt);
extern OPTTYPE xf86AddNewOption(OPTTYPE head, const char *name, const char *val);
extern char* xf86OptionName(OPTTYPE opt);
extern char *xf86OptionValue(OPTTYPE opt);
extern int xf86NameCmp(const char *s1, const char *s2);
extern char * xf86CheckStrOption(OPTTYPE optlist, const char *name, char *deflt);
extern void xf86AddEnabledDevice(InputInfoPtr pInfo);
extern void xf86RemoveEnabledDevice(InputInfoPtr pInfo);
extern Atom XIGetKnownProperty(char *name);
extern void xf86AddInputDriver(InputDriverPtr driver, pointer module, int flags);
extern int
xf86ScaleAxis(int	Cx,
              int	to_max,
              int	to_min,
              int	from_max,
              int	from_min );

extern void DeleteInputDeviceRequest(DeviceIntPtr pDev);
#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) >= 11
extern void FreeInputAttributes(InputAttributes *attrs);
#endif
extern void
xf86PostButtonEvent(DeviceIntPtr	device,
                    int			is_absolute,
                    int			button,
                    int			is_down,
                    int			first_valuator,
                    int			num_valuators,
                    ...);
extern int Xasprintf(char ** ret, const char *format, ...);
extern int
XISetDevicePropertyDeletable(DeviceIntPtr dev, Atom property, Bool deletable);

extern InputInfoPtr xf86FirstLocalDevice(void);
extern void xf86DeleteInput(InputInfoPtr pInp, int flags);
extern OPTTYPE xf86OptionListDuplicate(OPTTYPE options);
extern Bool
InitButtonClassDeviceStruct(DeviceIntPtr dev, int numButtons, Atom* labels,
                            CARD8 *map);
extern void
InitValuatorAxisStruct(DeviceIntPtr dev, int axnum, Atom label, int minval, int maxval,
		       int resolution, int min_res, int max_res, int mode);
extern void
xf86PostKeyboardEvent(DeviceIntPtr      device,
                      unsigned int      key_code,
                      int               is_down);
extern int
xf86SetIntOption(OPTTYPE optlist, const char *name, int deflt);
extern void
xf86PostButtonEventP(DeviceIntPtr	device,
                     int		is_absolute,
                     int		button,
                     int		is_down,
                     int		first_valuator,
                     int		num_valuators,
#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) >= 12
                     const
#endif
                           int		*valuators);
extern Bool
InitPtrFeedbackClassDeviceStruct(DeviceIntPtr dev, PtrCtrlProcPtr controlProc);

extern int
XIChangeDeviceProperty (DeviceIntPtr dev, Atom property, Atom type,
                        int format, int mode, unsigned long len,
                        pointer value, Bool sendevent);
extern CARD32 GetTimeInMillis (void);

extern int
NewInputDeviceRequest (InputOption *options,
#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) >= 11
                       InputAttributes *attrs,
#endif
                       DeviceIntPtr *pdev);

extern Bool
InitLedFeedbackClassDeviceStruct (DeviceIntPtr dev, LedCtrlProcPtr controlProc);

#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) >= 11
extern InputAttributes* DuplicateInputAttributes(InputAttributes *attrs);
#endif
extern int ValidAtom(Atom atom);
extern Bool
InitKeyboardDeviceStruct(DeviceIntPtr dev, XkbRMLVOSet *rmlvo,
                         BellProcPtr bell_func, KbdCtrlProcPtr ctrl_func);
extern long
XIRegisterPropertyHandler(DeviceIntPtr         dev,
                          int (*SetProperty) (DeviceIntPtr dev,
                                              Atom property,
                                              XIPropertyValuePtr prop,
                                              BOOL checkonly),
                          int (*GetProperty) (DeviceIntPtr dev,
                                              Atom property),
                          int (*DeleteProperty) (DeviceIntPtr dev,
                                                 Atom property));
extern int InitProximityClassDeviceStruct(DeviceIntPtr dev);
extern void LogMessageVerbSigSafe(MessageType type, int verb, const char *format, ...);
extern void xf86MsgVerb(MessageType type, int verb, const char *format, ...);
extern void xf86Msg(MessageType type, const char *format, ...);

extern void
xf86PostMotionEventP(DeviceIntPtr	device,
                    int			is_absolute,
                    int			first_valuator,
                    int			num_valuators,
#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) >= 12
                    const
#endif
                          int		*valuators);

extern Bool
InitValuatorClassDeviceStruct(DeviceIntPtr dev, int numAxes, Atom *labels,
                              int numMotionEvents, int mode);

extern OPTTYPE
xf86ReplaceStrOption(OPTTYPE optlist, const char *name, const char* val);

extern OPTTYPE xf86NextOption(OPTTYPE list);

extern int
XIGetDeviceProperty (DeviceIntPtr dev, Atom property, XIPropertyValuePtr *value);

extern Atom MakeAtom(const char *string, unsigned len, Bool makeit);

extern int GetMotionHistorySize(void);

extern void
xf86PostProximityEventP(DeviceIntPtr	device,
                        int		is_in,
                        int		first_valuator,
                        int		num_valuators,
#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) >= 12
                        const
#endif
                              int	*valuators);

extern Bool InitFocusClassDeviceStruct(DeviceIntPtr dev);


#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) < 12
extern void
xf86ProcessCommonOptions(InputInfoPtr pInfo, pointer list);

extern void
xf86CollectInputOptions(InputInfoPtr pInfo,
                        const char **defaultOpts,
                        pointer extraOpts);

extern InputInfoPtr
xf86AllocateInput(InputDriverPtr drv, int flags);


extern ClientPtr serverClient;

extern Bool QueueWorkProc (
    Bool (*function)(ClientPtr /* pClient */, pointer /* closure */),
    ClientPtr client, pointer closure);

#endif

extern OsTimerPtr
TimerSet(OsTimerPtr timer, int flags, CARD32 millis,
         OsTimerCallback func, pointer arg);

extern void TimerFree(OsTimerPtr timer);

extern int xf86BlockSIGIO (void);
extern void xf86UnblockSIGIO (int wasset);
