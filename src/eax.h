#ifndef EAX_H
#define EAX_H

#include <windows.h>


inline constexpr GUID DSPROPSETID_EAX10_ListenerProperties{
    0x4A4E6FC1,
    0xC341, 0x11D1,
    {0xB7, 0x3A, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00}
};

inline constexpr GUID DSPROPSETID_EAX10_BufferProperties{
    0x4A4E6FC0,
    0xC341, 0x11D1,
    {0xB7, 0x3A, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00}
};

inline constexpr GUID DSPROPSETID_EAX20_ListenerProperties{
    0x306A6A8,
    0xB224, 0x11D2,
    {0x99, 0xE5, 0x00, 0x00, 0xE8, 0xD8, 0xC7, 0x22}
};

inline constexpr GUID DSPROPSETID_EAX20_BufferProperties{
    0x306A6A7,
    0xB224, 0x11D2,
    {0x99, 0xE5, 0x00, 0x00, 0xE8, 0xD8, 0xC7, 0x22}
};

inline constexpr GUID DSPROPSETID_EAX30_ListenerProperties{
    0xA8FA6882,
    0xB476, 0x11D3,
    {0xBD, 0xB9, 0x00, 0xC0, 0xF0, 0x2D, 0xDF, 0x87}
};

inline constexpr GUID DSPROPSETID_EAX30_BufferProperties {
    0xA8FA6881,
    0xB476, 0x11D3,
    {0xBD, 0xB9, 0x00, 0xC0, 0xF0, 0x2D, 0xDF, 0x87}
};

inline constexpr GUID EAXPROPERTYID_EAX40_Context{
    0x1D4870AD,
    0x0DEF, 0x43C0,
    {0xA4, 0xC, 0x52, 0x36, 0x32, 0x29, 0x63, 0x42}
};

inline constexpr GUID EAXPROPERTYID_EAX40_FXSlot0{
    0xC4D79F1E,
    0xF1AC, 0x436B,
    {0xA8, 0x1D, 0xA7, 0x38, 0xE7, 0x04, 0x54, 0x69}
};

inline constexpr GUID EAXPROPERTYID_EAX40_FXSlot1{
    0x8C00E96,
    0x74BE, 0x4491,
    {0x93, 0xAA, 0xE8, 0xAD, 0x35, 0xA4, 0x91, 0x17}
};

inline constexpr GUID EAXPROPERTYID_EAX40_FXSlot2{
    0x1D433B88,
    0xF0F6, 0x4637,
    {0x91, 0x9F, 0x60, 0xE7, 0xE0, 0x6B, 0x5E, 0xDD}
};

inline constexpr GUID EAXPROPERTYID_EAX40_FXSlot3{
    0xEFFF08EA,
    0xC7D8, 0x44AB,
    {0x93, 0xAD, 0x6D, 0xBD, 0x5F, 0x91, 0x00, 0x64}
};

inline constexpr GUID EAXPROPERTYID_EAX40_Source{
    0x1B86B823,
    0x22DF, 0x4EAE,
    {0x8B, 0x3C, 0x12, 0x78, 0xCE, 0x54, 0x42, 0x27}
};

inline constexpr auto EAX_MAX_FXSLOTS = 4;

inline constexpr auto EAX40_MAX_ACTIVE_FXSLOTS = 2;

// Source object properties
enum EAXSOURCE_PROPERTY : unsigned int {
    // EAX30
    EAXSOURCE_NONE,
    EAXSOURCE_ALLPARAMETERS,
    EAXSOURCE_OBSTRUCTIONPARAMETERS,
    EAXSOURCE_OCCLUSIONPARAMETERS,
    EAXSOURCE_EXCLUSIONPARAMETERS,
    EAXSOURCE_DIRECT,
    EAXSOURCE_DIRECTHF,
    EAXSOURCE_ROOM,
    EAXSOURCE_ROOMHF,
    EAXSOURCE_OBSTRUCTION,
    EAXSOURCE_OBSTRUCTIONLFRATIO,
    EAXSOURCE_OCCLUSION,
    EAXSOURCE_OCCLUSIONLFRATIO,
    EAXSOURCE_OCCLUSIONROOMRATIO,
    EAXSOURCE_OCCLUSIONDIRECTRATIO,
    EAXSOURCE_EXCLUSION,
    EAXSOURCE_EXCLUSIONLFRATIO,
    EAXSOURCE_OUTSIDEVOLUMEHF,
    EAXSOURCE_DOPPLERFACTOR,
    EAXSOURCE_ROLLOFFFACTOR,
    EAXSOURCE_ROOMROLLOFFFACTOR,
    EAXSOURCE_AIRABSORPTIONFACTOR,
    EAXSOURCE_FLAGS,

    // EAX40
    EAXSOURCE_SENDPARAMETERS,
    EAXSOURCE_ALLSENDPARAMETERS,
    EAXSOURCE_OCCLUSIONSENDPARAMETERS,
    EAXSOURCE_EXCLUSIONSENDPARAMETERS,
    EAXSOURCE_ACTIVEFXSLOTID,

    // EAX50
    EAXSOURCE_MACROFXFACTOR,
    EAXSOURCE_SPEAKERLEVELS,
    EAXSOURCE_ALL2DPARAMETERS,
};


DWORD EAX4Context_Query(DWORD propid);
DWORD EAX4Slot_Query(DWORD propid);
DWORD EAX4Source_Query(DWORD propid);

DWORD EAX3_Query(DWORD propid);
DWORD EAX3Buffer_Query(DWORD propid);

DWORD EAX2_Query(DWORD propid);
DWORD EAX2Buffer_Query(DWORD propid);

DWORD EAX1_Query(DWORD propid);
DWORD EAX1Buffer_Query(DWORD propid);

#endif // EAX_H
