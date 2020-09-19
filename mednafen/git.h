#ifndef __MDFN_GIT_H
#define __MDFN_GIT_H

#include <algorithm>
#include <string>
#include <vector>
#include <libretro.h>

#include "video.h"
#include "state.h"


enum
{
 MDFN_ROTATE0 = 0,
 MDFN_ROTATE90,
 MDFN_ROTATE180,
 MDFN_ROTATE270
};

typedef enum
{
 VIDSYS_NONE, // Can be used internally in system emulation code, but it is an error condition to let it continue to be
	      // after the Load() or LoadCD() function returns!
 VIDSYS_PAL,
 VIDSYS_PAL_M, // Same timing as NTSC, but uses PAL-style colour encoding
 VIDSYS_NTSC,
 VIDSYS_SECAM
} VideoSystems;

typedef enum
{
 GMT_NONE = 0,
 GMT_ARCADE,	// VS Unisystem, PC-10...
 GMT_PLAYER	// Music player(NSF, HES, GSF)
} GameMediumTypes;

#ifdef WANT_DEBUGGER
// #ifdef WANT_DEBUGGER
// typedef struct DebuggerInfoStruct;
// #else
#include "debug.h"

#endif

enum InputDeviceInputType : uint8
{
 IDIT_PADDING = 0,	// n-bit, zero

 IDIT_BUTTON,		// 1-bit
 IDIT_BUTTON_CAN_RAPID, // 1-bit

 IDIT_SWITCH,		// ceil(log2(n))-bit
			// Current switch position(default 0).
			// Persistent, and bidirectional communication(can be modified driver side, and Mednafen core and emulation module side)

 IDIT_STATUS,		// ceil(log2(n))-bit
			// emulation module->driver communication

 IDIT_AXIS,		// 16-bits; 0 through 65535; 32768 is centered position

 IDIT_POINTER_X,	// mouse pointer, 16-bits, signed - in-screen/window range before scaling/offseting normalized coordinates: [0.0, 1.0)
 IDIT_POINTER_Y,	// see: mouse_scale_x, mouse_scale_y, mouse_offs_x, mouse_offs_y

 IDIT_AXIS_REL,		// mouse relative motion, 16-bits, signed

 IDIT_BYTE_SPECIAL,

 IDIT_RESET_BUTTON,	// 1-bit

 IDIT_BUTTON_ANALOG,	// 16-bits, 0 - 65535

 IDIT_RUMBLE,		// 16-bits, lower 8 bits are weak rumble(0-255), next 8 bits are strong rumble(0-255), 0=no rumble, 255=max rumble.  Somewhat subjective, too...
			// It's a rather special case of game module->driver code communication.
};


enum : uint8
{
 IDIT_AXIS_FLAG_SQLR		= 0x01,	// Denotes analog data that may need to be scaled to ensure a more squareish logical range(for emulated analog sticks).
 IDIT_AXIS_FLAG_INVERT_CO	= 0x02,	// Invert config order of the two components(neg,pos) of the axis.
 IDIT_AXIS_REL_FLAG_INVERT_CO 	= IDIT_AXIS_FLAG_INVERT_CO,
 IDIT_FLAG_AUX_SETTINGS_UNDOC	= 0x80,
};

struct IDIIS_StatusState
{
	const char* ShortName;
	const char* Name;
	int32 Color;	// (msb)0RGB(lsb), -1 for unused.
};

struct IDIIS_SwitchPos
{
	const char* SettingName;
	const char* Name;
	const char* Description;
};

struct InputDeviceInputInfoStruct
{
	const char *SettingName;	// No spaces, shouldbe all a-z0-9 and _. Definitely no ~!
	const char *Name;
        int16 ConfigOrder;          	// Configuration order during in-game config process, -1 for no config.
	InputDeviceInputType Type;

	uint8 Flags;
	uint8 BitSize;
	uint16 BitOffset;

	union
	{
	 struct
	 {
	  const char *ExcludeName;	// SettingName of a button that can't be pressed at the same time as this button
					// due to physical limitations.
	 } Button;
	 //
	 //
	 //
	 struct
	 {
	  const char* sname_dir[2];
	  const char* name_dir[2];
	 } Axis;

	 struct
	 {
	  const char* sname_dir[2];
	  const char* name_dir[2];
	 } AxisRel;

         struct
         {
	  const IDIIS_SwitchPos* Pos;
	  uint32 NumPos;
         } Switch;

	 struct
	 {
	  const IDIIS_StatusState* States;
	  uint32 NumStates;
	 } Status;
	};
};

struct IDIISG : public std::vector<InputDeviceInputInfoStruct>
{
 IDIISG();
 IDIISG(std::initializer_list<InputDeviceInputInfoStruct> l);
 uint32 InputByteSize;
};

extern const IDIISG IDII_Empty;

static INLINE constexpr InputDeviceInputInfoStruct IDIIS_Button(const char* sname, const char* name, int16 co, const char* exn = nullptr)
{
 return { sname, name, co, IDIT_BUTTON, 0, 0, 0, { exn } };
}

static INLINE constexpr InputDeviceInputInfoStruct IDIIS_ButtonCR(const char* sname, const char* name, int16 co, const char* exn = nullptr)
{
 return { sname, name, co, IDIT_BUTTON_CAN_RAPID, 0, 0, 0, { exn } };
}

static INLINE constexpr InputDeviceInputInfoStruct IDIIS_AnaButton(const char* sname, const char* name, int16 co)
{
 return { sname, name, co, IDIT_BUTTON_ANALOG, 0, 0, 0 };
}

static INLINE constexpr InputDeviceInputInfoStruct IDIIS_Rumble(const char* sname = "rumble", const char* name = "Rumble")
{
 return { sname, name, -1, IDIT_RUMBLE, 0, 0, 0 };
}

static INLINE constexpr InputDeviceInputInfoStruct IDIIS_ResetButton(void)
{
 return { nullptr, nullptr, -1, IDIT_RESET_BUTTON, 0, 0, 0 };
}

template<unsigned nbits = 1>
static INLINE constexpr InputDeviceInputInfoStruct IDIIS_Padding(void)
{
 return { nullptr, nullptr, -1, IDIT_PADDING, 0, nbits, 0 };
}

static INLINE /*constexpr*/ InputDeviceInputInfoStruct IDIIS_Axis(const char* sname_pfx, const char* name_pfx, const char* sname_neg, const char* name_neg, const char* sname_pos, const char* name_pos, int16 co, bool co_invert = false, bool sqlr = false)
{
 InputDeviceInputInfoStruct ret = { sname_pfx, name_pfx, co, IDIT_AXIS, (uint8)((sqlr ? IDIT_AXIS_FLAG_SQLR : 0) | (co_invert ? IDIT_AXIS_FLAG_INVERT_CO : 0)), 0, 0 };

 ret.Axis.sname_dir[0] = sname_neg;
 ret.Axis.sname_dir[1] = sname_pos;
 ret.Axis.name_dir[0] = name_neg;
 ret.Axis.name_dir[1] = name_pos;

 return ret;
}

static INLINE /*constexpr*/ InputDeviceInputInfoStruct IDIIS_AxisRel(const char* sname_pfx, const char* name_pfx, const char* sname_neg, const char* name_neg, const char* sname_pos, const char* name_pos, int16 co, bool co_invert = false, bool sqlr = false)
{
 InputDeviceInputInfoStruct ret = { sname_pfx, name_pfx, co, IDIT_AXIS_REL, (uint8)(co_invert ? IDIT_AXIS_REL_FLAG_INVERT_CO : 0), 0, 0 };

 ret.AxisRel.sname_dir[0] = sname_neg;
 ret.AxisRel.sname_dir[1] = sname_pos;
 ret.AxisRel.name_dir[0] = name_neg;
 ret.AxisRel.name_dir[1] = name_pos;

 return ret;
}

template<uint32 spn_count>
static INLINE /*constexpr*/ InputDeviceInputInfoStruct IDIIS_Switch(const char* sname, const char* name, int16 co, const IDIIS_SwitchPos (&spn)[spn_count], bool undoc_defpos = true)
{
 InputDeviceInputInfoStruct ret = { sname, name, co, IDIT_SWITCH, (uint8)(undoc_defpos ? IDIT_FLAG_AUX_SETTINGS_UNDOC : 0), 0, 0 };

 ret.Switch.Pos = spn;
 ret.Switch.NumPos = spn_count;

 return ret;
}

template<uint32 ss_count>
static INLINE /*constexpr*/ InputDeviceInputInfoStruct IDIIS_Status(const char* sname, const char* name, const IDIIS_StatusState (&ss)[ss_count])
{
 InputDeviceInputInfoStruct ret = { sname, name, -1, IDIT_STATUS, 0, 0, 0 };

 ret.Status.States = ss;
 ret.Status.NumStates = ss_count;

 return ret;
}

struct InputDeviceInfoStruct
{
 const char *ShortName;
 const char *FullName;
 const char *Description;

 const IDIISG& IDII;

 unsigned Flags;

 enum
 {
  FLAG_KEYBOARD = (1U << 0)
 };
};

struct InputPortInfoStruct
{
 const char *ShortName;
 const char *FullName;
 const std::vector<InputDeviceInfoStruct> &DeviceInfo;
 const char *DefaultDevice;	// Default device for this port.
};

struct MemoryPatch;

struct CheatFormatStruct
{
 const char *FullName;		//"Game Genie", "GameShark", "Pro Action Catplay", etc.
 const char *Description;	// Whatever?

 bool (*DecodeCheat)(const std::string& cheat_string, MemoryPatch* patch);	// *patch should be left as initialized by MemoryPatch::MemoryPatch(), unless this is the
										// second(or third or whatever) part of a multipart cheat.
										//
										// Will throw an std::exception(or derivative) on format error.
										//
										// Will return true if this is part of a multipart cheat.
};

extern const std::vector<CheatFormatStruct> CheatFormatInfo_Empty;

struct CheatInfoStruct
{
 //
 // InstallReadPatch and RemoveReadPatches should be non-NULL(even if only pointing to dummy functions) if the emulator module supports
 // read-substitution and read-substitution-with-compare style(IE Game Genie-style) cheats.
 //
 // See also "SubCheats" global stuff in mempatcher.h.
 //
 void (*InstallReadPatch)(uint32 address, uint8 value, int compare); // Compare is >= 0 when utilized.
 void (*RemoveReadPatches)(void);
 uint8 (*MemRead)(uint32 addr);
 void (*MemWrite)(uint32 addr, uint8 val);

 const std::vector<CheatFormatStruct>& CheatFormatInfo;

 bool BigEndian;	// UI default for cheat search and new cheats.
};

extern const CheatInfoStruct CheatInfo_Empty;

// Miscellaneous system/simple commands(power, reset, dip switch toggles, coin insert, etc.)
// (for DoSimpleCommand() )
enum
{
 MDFN_MSC_RESET = 0x01,
 MDFN_MSC_POWER = 0x02,

 MDFN_MSC_INSERT_COIN = 0x07,

 // If we ever support arcade systems, we'll abstract DIP switches differently...maybe.
 MDFN_MSC_TOGGLE_DIP0 = 0x10,
 MDFN_MSC_TOGGLE_DIP1,
 MDFN_MSC_TOGGLE_DIP2,
 MDFN_MSC_TOGGLE_DIP3,
 MDFN_MSC_TOGGLE_DIP4,
 MDFN_MSC_TOGGLE_DIP5,
 MDFN_MSC_TOGGLE_DIP6,
 MDFN_MSC_TOGGLE_DIP7,
 MDFN_MSC_TOGGLE_DIP8,
 MDFN_MSC_TOGGLE_DIP9,
 MDFN_MSC_TOGGLE_DIP10,
 MDFN_MSC_TOGGLE_DIP11,
 MDFN_MSC_TOGGLE_DIP12,
 MDFN_MSC_TOGGLE_DIP13,
 MDFN_MSC_TOGGLE_DIP14,
 MDFN_MSC_TOGGLE_DIP15,

 MDFN_MSC__LAST = 0x3F	// WARNING: Increasing(or having the enum'd value of a command greater than this :b) this will necessitate a change to the netplay protocol.
};

typedef struct
{
	// Pitch(32-bit) must be equal to width and >= the "fb_width" specified in the MDFNGI struct for the emulated system.
	// Height must be >= to the "fb_height" specified in the MDFNGI struct for the emulated system.
	// The framebuffer pointed to by surface->pixels is written to by the system emulation code.
	MDFN_Surface *surface;

	// Will be set to true if the video pixel format has changed since the last call to Emulate(), false otherwise.
	// Will be set to true on the first call to the Emulate() function/method
	bool VideoFormatChanged;

	// Set by the system emulation code every frame, to denote the horizontal and vertical offsets of the image, and the size
	// of the image.  If the emulated system sets the elements of LineWidths, then the width(w) of this structure
	// is ignored while drawing the image.
	MDFN_Rect DisplayRect;

	// Pointer to an array of int32, number of elements = fb_height, set by the driver code.  Individual elements written
	// to by system emulation code.  If the emulated system doesn't support multiple screen widths per frame, or if you handle
	// such a situation by outputting at a constant width-per-frame that is the least-common-multiple of the screen widths, then
	// you can ignore this.  If you do wish to use this, you must set all elements every frame.
	int32 *LineWidths;

	// Pointer to an array of uint8, 3 * CustomPaletteEntries.
	// CustomPalette must be NULL and CustomPaletteEntries mujst be 0 if no custom palette is specified/available;
	// otherwise, CustomPalette must be non-NULL and CustomPaletteEntries must be equal to a non-zero "num_entries" member of a CustomPalette_Spec
	// entry of MDFNGI::CPInfo.
	//
	// Set and used internally, driver-side code needn't concern itself with this.
	//
	uint8 *CustomPalette;
	uint32 CustomPaletteNumEntries;

	// Set(optionally) by emulation code.  If InterlaceOn is true, then assume field height is 1/2 DisplayRect.h, and
	// only every other line in surface (with the start line defined by InterlacedField) has valid data
	// (it's up to internal Mednafen code to deinterlace it).
	bool InterlaceOn;
	bool InterlaceField;

	// Skip rendering this frame if true.  Set by the driver code.
	int skip;

	//
	// If sound is disabled, the driver code must set SoundRate to false, SoundBuf to NULL, SoundBufMaxSize to 0.

        // Will be set to true if the sound format(only rate for now, at least) has changed since the last call to Emulate(), false otherwise.
        // Will be set to true on the first call to the Emulate() function/method
	bool SoundFormatChanged;

	// Sound rate.  Set by driver side.
	double SoundRate;

	// Number of frames currently in internal sound buffer.  Set by the system emulation code, to be read by the driver code.
	int32 SoundBufSize;
	int32 SoundBufSizeALMS;	// SoundBufSize value at last MidSync(), 0
				// if mid sync isn't implemented for the emulation module in use.

	// Number of cycles that this frame consumed, using MDFNGI::MasterClock as a time base.
	// Set by emulation code.
	int64 MasterCycles;
	int64 MasterCyclesALMS;	// MasterCycles value at last MidSync(), 0
				// if mid sync isn't implemented for the emulation module in use.

	// Current sound volume(0.000...<=volume<=1.000...).  If, after calling Emulate(), it is still != 1, Mednafen will handle it internally.
	// Emulation modules can handle volume themselves if they like, for speed reasons.  If they do, afterwards, they should set its value to 1.
	double SoundVolume;

	// Current sound speed multiplier.  Set by the driver code.  If, after calling Emulate(), it is still != 1, Mednafen will handle it internally
	// by resampling the audio.  This means that emulation modules can handle(and set the value to 1 after handling it) it if they want to get the most
	// performance possible.  HOWEVER, emulation modules must make sure the value is in a range(with minimum and maximum) that their code can handle
	// before they try to handle it.
	double soundmultiplier;

	// True if we want to rewind one frame.  Set by the driver code.
	bool NeedRewind;

	// Sound reversal during state rewinding is normally done in mednafen.cpp, but
        // individual system emulation code can also do it if this is set, and clear it after it's done.
        // (Also, the driver code shouldn't touch this variable)
	bool NeedSoundReverse;

} EmulateSpecStruct;

typedef enum
{
 MODPRIO_INTERNAL_EXTRA_LOW = 0,	// For "cdplay" module, mostly.

 MODPRIO_INTERNAL_LOW = 10,
 MODPRIO_EXTERNAL_LOW = 20,
 MODPRIO_INTERNAL_HIGH = 30,
 MODPRIO_EXTERNAL_HIGH = 40
} ModPrio;

class CDIF;

struct RMD_Media
{
 std::string Name;
 unsigned MediaType;	// Index into RMD_Layout::MediaTypes
 std::vector<std::string> Orientations;	// The vector may be empty.
};

struct RMD_MediaType
{
 std::string Name;
};

struct RMD_State
{
 std::string Name;

 bool MediaPresent;
 bool MediaUsable;	// Usually the same as MediaPresent.
 bool MediaCanChange;
};

struct RMD_Drive
{
 std::string Name;

 std::vector<RMD_State> PossibleStates;	// Ideally, only one state will have MediaPresent == true.
 std::vector<unsigned> CompatibleMedia;	// Indexes into RMD_Layout::MediaTypes
 unsigned MediaMtoPDelay;		// Recommended minimum delay, in milliseconds, between a MediaPresent == false state and a MediaPresent == true state; to be enforced
					// by the media changing user interface.
};

struct RMD_DriveDefaults
{
 uint32 State;
 uint32 Media;
 uint32 Orientation;
};

struct RMD_Layout
{
 std::vector<RMD_Drive> Drives;
 std::vector<RMD_MediaType> MediaTypes;
 std::vector<RMD_Media> Media;
 std::vector<RMD_DriveDefaults> DrivesDefaults;
};

struct CustomPalette_Spec
{
 const char* description;
 const char* name_override;

 unsigned valid_entry_count[32];	// 0-terminated
};

//===========================================

typedef struct
{
 // Time base for EmulateSpecStruct::MasterCycles
 // MasterClock must be >= MDFN_MASTERCLOCK_FIXED(1.0)
 // All or part of the fractional component may be ignored in some timekeeping operations in the emulator to prevent integer overflow,
 // so it is unwise to have a fractional component when the integral component is very small(less than say, 10000).
 #define MDFN_MASTERCLOCK_FIXED(n)	((int64)((double)(n) * (1LL << 32)))
 int64 MasterClock;

 // Nominal frames per second * 65536 * 256, truncated.
 // May be deprecated in the future due to many systems having slight frame rate programmability.
 uint32 fps;

 // multires is a hint that, if set, indicates that the system has fairly programmable video modes(particularly, the ability
 // to display multiple horizontal resolutions, such as the PCE, PC-FX, or Genesis).  In practice, it will cause the driver
 // code to set the linear interpolation on by default.
 //
 // lcm_width and lcm_height are the least common multiples of all possible
 // resolutions in the frame buffer as specified by DisplayRect/LineWidths(Ex for PCE: widths of 256, 341.333333, 512,
 // lcm = 1024)
 //
 // nominal_width and nominal_height specify the resolution that Mednafen should display
 // the framebuffer image in at 1x scaling, scaled from the dimensions of DisplayRect, and optionally the LineWidths array
 // passed through espec to the Emulate() function.
 //
 bool multires;

 int lcm_width;
 int lcm_height;

 void *dummy_separator;	//

 int nominal_width;
 int nominal_height;

 int fb_width;		// Width of the framebuffer(not necessarily width of the image).  MDFN_Surface width should be >= this.
 int fb_height;		// Height of the framebuffer passed to the Emulate() function(not necessarily height of the image)

 int soundchan; 	// Number of output sound channels.  Only values of 1 and 2 are currently supported.


 int rotated;

   uint8_t MD5[16];

   int soundrate;  /* For Ogg Vorbis expansion sound wacky support.  0 for default. */

 VideoSystems VideoSystem;
 GameMediumTypes GameType;	// Deprecated.

 RMD_Layout* RMD;

 std::vector<const char *>DesiredInput; // Desired input device for the input ports, NULL for don't care



 //
 // For absolute coordinates(IDIT_X_AXIS and IDIT_Y_AXIS), usually mapped to a mouse(hence the naming).
 //
 float mouse_scale_x, mouse_scale_y;
 float mouse_offs_x, mouse_offs_y; 
} MDFNGI;

//===========================================

int StateAction(StateMem *sm, int load, int data_only);

extern retro_log_printf_t log_cb;

#endif
