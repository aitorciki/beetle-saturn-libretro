
#include "libretro.h"
#include "libretro_settings.h"
#include "mednafen/mednafen-types.h"
#include "mednafen/ss/ss.h"
#include "mednafen/ss/smpc.h"
#include "mednafen/state.h"
#include <math.h>
#include <stdio.h>

//------------------------------------------------------------------------------
// Locals
//------------------------------------------------------------------------------

static retro_environment_t environ_cb; // cached during input_set_env

#define MAX_CONTROLLERS		12 // 2x 6 player adapters

static unsigned players = 2;

static int astick_deadzone = 0;
static int trigger_deadzone = 0;
static float mouse_sensitivity = 1.0f;

typedef union
{
	uint8_t u8[ 32 ];
	uint16_t gun_pos[ 2 ];
	uint16_t buttons;
}
INPUT_DATA;

// Controller state buffer (per player)
static INPUT_DATA input_data[ MAX_CONTROLLERS ] = {0};

// Controller type (per player)
static uint32_t input_type[ MAX_CONTROLLERS ] = {0};


#define INPUT_MODE_3D_PAD_ANALOG		( 1 << 0 ) // Set means analog mode.
#define INPUT_MODE_3D_PAD_PREVIOUS_MASK	( 1 << 1 ) // Edge trigger helper.

#define INPUT_MODE_DEFAULT				0
#define INPUT_MODE_DEFAULT_3D_PAD		INPUT_MODE_3D_PAD_ANALOG

// Mode switch for 3D Control Pad (per player)
static uint16_t input_mode[ MAX_CONTROLLERS ] = {0};



//------------------------------------------------------------------------------
// Supported Devices
//------------------------------------------------------------------------------

#define RETRO_DEVICE_SS_PAD			RETRO_DEVICE_SUBCLASS( RETRO_DEVICE_JOYPAD, 0 )
#define RETRO_DEVICE_SS_3D_PAD		RETRO_DEVICE_SUBCLASS( RETRO_DEVICE_ANALOG, 0 )
#define RETRO_DEVICE_SS_WHEEL		RETRO_DEVICE_SUBCLASS( RETRO_DEVICE_ANALOG, 1 )
#define RETRO_DEVICE_SS_MOUSE		RETRO_DEVICE_SUBCLASS( RETRO_DEVICE_MOUSE,  0 )
#define RETRO_DEVICE_SS_GUN			RETRO_DEVICE_SUBCLASS( RETRO_DEVICE_LIGHTGUN, 0 )

enum { INPUT_DEVICE_TYPES_COUNT = 1 /*none*/ + 5 }; // <-- update me!

static const struct retro_controller_description input_device_types[ INPUT_DEVICE_TYPES_COUNT ] =
{
	{ "Control Pad", RETRO_DEVICE_JOYPAD },
	{ "3D Control Pad", RETRO_DEVICE_SS_3D_PAD },
	{ "Arcade Racer", RETRO_DEVICE_SS_WHEEL },
	{ "Mouse", RETRO_DEVICE_SS_MOUSE },
	{ "Virtua Gun / Stunner", RETRO_DEVICE_SS_GUN },
	{ NULL, 0 },
};


//------------------------------------------------------------------------------
// Mapping Helpers
//------------------------------------------------------------------------------

/* Control Pad (default) */
enum { INPUT_MAP_PAD_SIZE = 12 };
static const unsigned input_map_pad[ INPUT_MAP_PAD_SIZE ] =
{
	// libretro input				 at position	|| maps to Saturn		on bit
	//-----------------------------------------------------------------------------
	RETRO_DEVICE_ID_JOYPAD_L,		// L1			-> Z					0
	RETRO_DEVICE_ID_JOYPAD_X,		// X(top)		-> Y					1
	RETRO_DEVICE_ID_JOYPAD_Y,		// Y(left)		-> X					2
	RETRO_DEVICE_ID_JOYPAD_R2,		// R2			-> R					3
	RETRO_DEVICE_ID_JOYPAD_UP,		// Pad-Up		-> Pad-Up				4
	RETRO_DEVICE_ID_JOYPAD_DOWN,	// Pad-Down		-> Pad-Down				5
	RETRO_DEVICE_ID_JOYPAD_LEFT,	// Pad-Left		-> Pad-Left				6
	RETRO_DEVICE_ID_JOYPAD_RIGHT,	// Pad-Right	-> Pad-Right			7
	RETRO_DEVICE_ID_JOYPAD_A,		// A(right)		-> B					8
	RETRO_DEVICE_ID_JOYPAD_R,		// R1			-> C					9
	RETRO_DEVICE_ID_JOYPAD_B,		// B(down)		-> A					10
	RETRO_DEVICE_ID_JOYPAD_START,	// Start		-> Start				11
};

static const unsigned input_map_pad_left_shoulder =
	RETRO_DEVICE_ID_JOYPAD_L2;		// L2			-> L					15

/* 3D Control Pad */
enum { INPUT_MAP_3D_PAD_SIZE = 11 };
static const unsigned input_map_3d_pad[ INPUT_MAP_3D_PAD_SIZE ] =
{
	// libretro input				 at position	|| maps to Saturn		on bit
	//-----------------------------------------------------------------------------
	RETRO_DEVICE_ID_JOYPAD_UP,		// Pad-Up		-> Pad-Up				0
	RETRO_DEVICE_ID_JOYPAD_DOWN,	// Pad-Down		-> Pad-Down				1
	RETRO_DEVICE_ID_JOYPAD_LEFT,	// Pad-Left		-> Pad-Left				2
	RETRO_DEVICE_ID_JOYPAD_RIGHT,	// Pad-Right	-> Pad-Right			3
	RETRO_DEVICE_ID_JOYPAD_A,		// A(right)		-> B					4
	RETRO_DEVICE_ID_JOYPAD_R,		// R1			-> C					5
	RETRO_DEVICE_ID_JOYPAD_B,		// B(down)		-> A					6
	RETRO_DEVICE_ID_JOYPAD_START,	// Start		-> Start				7
	RETRO_DEVICE_ID_JOYPAD_L,		// L1			-> Z					8
	RETRO_DEVICE_ID_JOYPAD_X,		// X(top)		-> Y					9
	RETRO_DEVICE_ID_JOYPAD_Y,		// Y(left)		-> X					10
};

static const unsigned input_map_3d_pad_mode_switch =
	RETRO_DEVICE_ID_JOYPAD_SELECT;

/* Arcade Racer (wheel) */
enum { INPUT_MAP_WHEEL_BITSHIFT = 4 };
enum { INPUT_MAP_WHEEL_SIZE = 7 };
static const unsigned input_map_wheel[ INPUT_MAP_WHEEL_SIZE ] =
{
	// libretro input				 at position	|| maps to Saturn		on bit
	//-----------------------------------------------------------------------------
	RETRO_DEVICE_ID_JOYPAD_A,		// A(right)		-> B					4
	RETRO_DEVICE_ID_JOYPAD_R,		// R1			-> C					5
	RETRO_DEVICE_ID_JOYPAD_B,		// B(down)		-> A					6
	RETRO_DEVICE_ID_JOYPAD_START,	// Start		-> Start				7
	RETRO_DEVICE_ID_JOYPAD_L,		// L1			-> Z					8
	RETRO_DEVICE_ID_JOYPAD_X,		// X(top)		-> Y					9
	RETRO_DEVICE_ID_JOYPAD_Y,		// Y(left)		-> X					10
};

static const unsigned input_map_wheel_shift_left =
	RETRO_DEVICE_ID_JOYPAD_L2;
static const unsigned input_map_wheel_shift_right =
	RETRO_DEVICE_ID_JOYPAD_R2;



//------------------------------------------------------------------------------
// Local Functions
//------------------------------------------------------------------------------

static uint16_t apply_trigger_deadzone( uint16_t input )
{
	if ( trigger_deadzone > 0 )
	{
		static const int TRIGGER_MAX = 0x8000;
		const float scale = ((float)TRIGGER_MAX/(float)(TRIGGER_MAX - trigger_deadzone));

		if ( input > trigger_deadzone )
		{
			// Re-scale analog range
			float scaled = (input - trigger_deadzone)*scale;

			input = (int)round(scaled);
			if (input > +32767) {
				input = +32767;
			}
		}
		else
		{
			input = 0;
		}
	}

	return input;
}

static uint16_t get_analog_trigger( retro_input_state_t input_state_cb,
									int player_index,
									int id )
{
	uint16_t trigger;

	// NOTE: Analog triggers were added Nov 2017. Not all front-ends support this
	// feature (or pre-date it) so we need to handle this in a graceful way.

	// First, try and get an analog value using the new libretro API constant
	trigger = input_state_cb( player_index,
							  RETRO_DEVICE_ANALOG,
							  RETRO_DEVICE_INDEX_ANALOG_BUTTON,
							  id );

	if ( trigger == 0 )
	{
		// If we got exactly zero, we're either not pressing the button, or the front-end
		// is not reporting analog values. We need to do a second check using the classic
		// digital API method, to at least get some response - better than nothing.

		// NOTE: If we're really just not holding the trigger, we're still going to get zero.

		trigger = input_state_cb( player_index,
								  RETRO_DEVICE_JOYPAD,
								  0,
								  id ) ? 0x7FFF : 0;
	}
	else
	{
		// We got something, which means the front-end can handle analog buttons.
		// So we apply a deadzone to the input and use it.

		trigger = apply_trigger_deadzone( trigger );
	}

	return trigger;
}


//------------------------------------------------------------------------------
// Global Functions
//------------------------------------------------------------------------------

void input_init_env( retro_environment_t _environ_cb )
{
	// Cache this
	environ_cb = _environ_cb;

#define RETRO_DESCRIPTOR_BLOCK( _user )																				\
		{ _user, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP,    "D-Pad Up" },								\
		{ _user, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN,  "D-Pad Down" },								\
		{ _user, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT,  "D-Pad Left" },								\
		{ _user, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "D-Pad Right" },								\
		{ _user, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B,     "A Button" },								\
		{ _user, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A,     "B Button" },								\
		{ _user, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R,     "C Button" },								\
		{ _user, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y,     "X Button" },								\
		{ _user, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X,     "Y Button" },								\
		{ _user, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L,     "Z Button" },								\
		{ _user, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L2,    "L Button" },								\
		{ _user, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R2,    "R Button" },								\
		{ _user, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START, "Start Button" },							\
		{ _user, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT, "Mode Switch" },							\
		{ _user, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_X, "Analog X" },		\
		{ _user, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_Y, "Analog Y" },		\
		{ _user, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_TRIGGER, "Gun Trigger" },						\
		{ _user, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_START, "Gun Start" },							\
		{ _user, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_RELOAD, "Gun Reload" }

	struct retro_input_descriptor desc[] =
	{
		RETRO_DESCRIPTOR_BLOCK( 0 ),
		RETRO_DESCRIPTOR_BLOCK( 1 ),
		RETRO_DESCRIPTOR_BLOCK( 2 ),
		RETRO_DESCRIPTOR_BLOCK( 3 ),
		RETRO_DESCRIPTOR_BLOCK( 4 ),
		RETRO_DESCRIPTOR_BLOCK( 5 ),
		RETRO_DESCRIPTOR_BLOCK( 6 ),
		RETRO_DESCRIPTOR_BLOCK( 7 ),
		RETRO_DESCRIPTOR_BLOCK( 8 ),
		RETRO_DESCRIPTOR_BLOCK( 9 ),
		RETRO_DESCRIPTOR_BLOCK( 10 ),
		RETRO_DESCRIPTOR_BLOCK( 11 ),

		{ 0 },
	};

#undef RETRO_DESCRIPTOR_BLOCK

	// Send to front-end
	environ_cb( RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS, desc );
}

void input_set_env( retro_environment_t environ_cb )
{
	static const struct retro_controller_info ports[ MAX_CONTROLLERS + 1 ] =
	{
		{ input_device_types, INPUT_DEVICE_TYPES_COUNT },
		{ input_device_types, INPUT_DEVICE_TYPES_COUNT },
		{ input_device_types, INPUT_DEVICE_TYPES_COUNT },
		{ input_device_types, INPUT_DEVICE_TYPES_COUNT },
		{ input_device_types, INPUT_DEVICE_TYPES_COUNT },
		{ input_device_types, INPUT_DEVICE_TYPES_COUNT },
		{ input_device_types, INPUT_DEVICE_TYPES_COUNT },
		{ input_device_types, INPUT_DEVICE_TYPES_COUNT },
		{ input_device_types, INPUT_DEVICE_TYPES_COUNT },
		{ input_device_types, INPUT_DEVICE_TYPES_COUNT },
		{ input_device_types, INPUT_DEVICE_TYPES_COUNT },
		{ input_device_types, INPUT_DEVICE_TYPES_COUNT },

		{ 0 },
	};

	// Send to front-end
	environ_cb( RETRO_ENVIRONMENT_SET_CONTROLLER_INFO, (void*)ports );
}

void input_init()
{
	// Initialise to default pad type and bind input buffers to SMPC emulation.
	for ( unsigned i = 0; i < MAX_CONTROLLERS; ++i )
	{
		input_type[ i ] = RETRO_DEVICE_JOYPAD;
		input_mode[ i ] = INPUT_MODE_DEFAULT;

		SMPC_SetInput( i, "gamepad", (uint8*)&input_data[ i ] );
	}
}

void input_set_deadzone_stick( int percent )
{
	if ( percent >= 0 && percent <= 100 )
		astick_deadzone = (int)( percent * 0.01f * 0x8000);
}

void input_set_deadzone_trigger( int percent )
{
	if ( percent >= 0 && percent <= 100 )
		trigger_deadzone = (int)( percent * 0.01f * 0x8000);
}

void input_set_mouse_sensitivity( int percent )
{
	if ( percent > 0 && percent <= 200 ) {
		mouse_sensitivity = (float)percent / 100.0f;
	}
}

void input_update( retro_input_state_t input_state_cb )
{
	// For each player (logical controller)
	for ( unsigned iplayer = 0; iplayer < players; ++iplayer )
	{
		INPUT_DATA* p_input = &(input_data[ iplayer ]);

		// reset input
		p_input->buttons = 0;

		// What kind of controller is connected?
		switch ( input_type[ iplayer ] )
		{

		case RETRO_DEVICE_JOYPAD:
		case RETRO_DEVICE_SS_PAD:

			{
				//
				// -- standard control pad buttons + d-pad

				// input_map_pad is configured to quickly map libretro buttons to the correct bits for the Saturn.
				for ( int i = 0; i < INPUT_MAP_PAD_SIZE; ++i ) {
					p_input->buttons |= input_state_cb( iplayer, RETRO_DEVICE_JOYPAD, 0, input_map_pad[ i ] ) ? ( 1 << i ) : 0;
				}
				// .. the left trigger on the Saturn is a special case since there's a gap in the bits.
				p_input->buttons |=
					input_state_cb( iplayer, RETRO_DEVICE_JOYPAD, 0, input_map_pad_left_shoulder ) ? ( 1 << 15 ) : 0;
			}
			break;

		case RETRO_DEVICE_SS_3D_PAD:

			{
				//
				// -- 3d control pad buttons

				// input_map_3d_pad is configured to quickly map libretro buttons to the correct bits for the Saturn.
				for ( int i = 0; i < INPUT_MAP_3D_PAD_SIZE; ++i ) {
					p_input->buttons |= input_state_cb( iplayer, RETRO_DEVICE_JOYPAD, 0, input_map_3d_pad[ i ] ) ? ( 1 << i ) : 0;
				}

				//
				// -- analog stick

				int analog_x = input_state_cb( iplayer, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT,
					RETRO_DEVICE_ID_ANALOG_X );

				int analog_y = input_state_cb( iplayer, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT,
					RETRO_DEVICE_ID_ANALOG_Y );

				// Analog stick deadzone (borrowed code from parallel-n64 core)
				if ( astick_deadzone > 0 )
				{
					static const int ASTICK_MAX = 0x8000;

					// Convert cartesian coordinate analog stick to polar coordinates
					double radius = sqrt(analog_x * analog_x + analog_y * analog_y);
					double angle = atan2(analog_y, analog_x);

					if (radius > astick_deadzone)
					{
						// Re-scale analog stick range to negate deadzone (makes slow movements possible)
						radius = (radius - astick_deadzone)*((float)ASTICK_MAX/(ASTICK_MAX - astick_deadzone));

						// Convert back to cartesian coordinates
						analog_x = (int)round(radius * cos(angle));
						analog_y = (int)round(radius * sin(angle));

						// Clamp to correct range
						if (analog_x > +32767) analog_x = +32767;
						if (analog_x < -32767) analog_x = -32767;
						if (analog_y > +32767) analog_y = +32767;
						if (analog_y < -32767) analog_y = -32767;
					}
					else
					{
						analog_x = 0;
						analog_y = 0;
					}
				}


				//
				// -- triggers

				uint16_t l_trigger, r_trigger;
				l_trigger = get_analog_trigger( input_state_cb, iplayer, RETRO_DEVICE_ID_JOYPAD_L2 );
				r_trigger = get_analog_trigger( input_state_cb, iplayer, RETRO_DEVICE_ID_JOYPAD_R2 );

				//
				// -- mode switch

				{
					// Handle MODE button as a switch
					uint16_t prev = ( input_mode[iplayer] & INPUT_MODE_3D_PAD_PREVIOUS_MASK );
					uint16_t held = input_state_cb( iplayer, RETRO_DEVICE_JOYPAD, 0, input_map_3d_pad_mode_switch )
						? INPUT_MODE_3D_PAD_PREVIOUS_MASK : 0;

					// Rising edge trigger
					if ( !prev && held )
					{
						// Toggle 'state' bit: analog/digital mode
						input_mode[ iplayer ] ^= INPUT_MODE_3D_PAD_ANALOG;

						// Tell user
						char text[ 256 ];
						if ( input_mode[iplayer] & INPUT_MODE_3D_PAD_ANALOG ) {
							sprintf( text, "Controller %u: Analog Mode", (iplayer+1) );
						} else {
							sprintf( text, "Controller %u: Digital Mode", (iplayer+1) );
						}
						struct retro_message msg = { text, 180 };
						environ_cb( RETRO_ENVIRONMENT_SET_MESSAGE, &msg );
					}

					// Store held state in 'previous' bit.
					input_mode[ iplayer ] = ( input_mode[ iplayer ] & ~INPUT_MODE_3D_PAD_PREVIOUS_MASK ) | held;
				}

				//
				// -- format input data

				// Convert analog values into direction values.
				uint16_t right = analog_x > 0 ?  analog_x : 0;
				uint16_t left  = analog_x < 0 ? -analog_x : 0;
				uint16_t down  = analog_y > 0 ?  analog_y : 0;
				uint16_t up    = analog_y < 0 ? -analog_y : 0;

				// Apply analog/digital mode switch bit.
				if ( input_mode[iplayer] & INPUT_MODE_3D_PAD_ANALOG ) {
					p_input->buttons |= 0x1000; // set bit 12
				}

				p_input->u8[0x2] = ((left  >> 0) & 0xff);
				p_input->u8[0x3] = ((left  >> 8) & 0xff);
				p_input->u8[0x4] = ((right >> 0) & 0xff);
				p_input->u8[0x5] = ((right >> 8) & 0xff);
				p_input->u8[0x6] = ((up    >> 0) & 0xff);
				p_input->u8[0x7] = ((up    >> 8) & 0xff);
				p_input->u8[0x8] = ((down  >> 0) & 0xff);
				p_input->u8[0x9] = ((down  >> 8) & 0xff);
				p_input->u8[0xa] = ((r_trigger >> 0) & 0xff);
				p_input->u8[0xb] = ((r_trigger >> 8) & 0xff);
				p_input->u8[0xc] = ((l_trigger >> 0) & 0xff);
				p_input->u8[0xd] = ((l_trigger >> 8) & 0xff);
			}

			break;

		case RETRO_DEVICE_SS_WHEEL:

			{
				//
				// -- Wheel buttons

				// input_map_wheel is configured to quickly map libretro buttons to the correct bits for the Saturn.
				for ( int i = 0; i < INPUT_MAP_WHEEL_SIZE; ++i ) {
					const uint16_t bit = ( 1 << ( i + INPUT_MAP_WHEEL_BITSHIFT ) );
					p_input->buttons |= input_state_cb( iplayer, RETRO_DEVICE_JOYPAD, 0, input_map_wheel[ i ] ) ? bit : 0;
				}

				// shift-paddles
				p_input->buttons |= input_state_cb( iplayer, RETRO_DEVICE_JOYPAD, 0, input_map_wheel_shift_left ) ? ( 1 << 0 ) : 0;
				p_input->buttons |= input_state_cb( iplayer, RETRO_DEVICE_JOYPAD, 0, input_map_wheel_shift_right ) ? ( 1 << 1 ) : 0;

				//
				// -- analog wheel

				int analog_x = input_state_cb( iplayer, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT,
					RETRO_DEVICE_ID_ANALOG_X );

				// Analog stick deadzone
				if ( astick_deadzone > 0 )
				{
					static const int ASTICK_MAX = 0x8000;
					const float scale = ((float)ASTICK_MAX/(float)(ASTICK_MAX - astick_deadzone));

					if ( analog_x < -astick_deadzone )
					{
						// Re-scale analog stick range
						float scaled = (-analog_x - astick_deadzone)*scale;

						analog_x = (int)round(-scaled);
						if (analog_x < -32767) {
							analog_x = -32767;
						}
					}
					else if ( analog_x > astick_deadzone )
					{
						// Re-scale analog stick range
						float scaled = (analog_x - astick_deadzone)*scale;

						analog_x = (int)round(scaled);
						if (analog_x > +32767) {
							analog_x = +32767;
						}
					}
					else
					{
						analog_x = 0;
					}
				}

				//
				// -- format input data

				// Convert analog values into direction values.
				uint16_t right = analog_x > 0 ?  analog_x : 0;
				uint16_t left  = analog_x < 0 ? -analog_x : 0;

				p_input->u8[0x2] = ((left  >> 0) & 0xff);
				p_input->u8[0x3] = ((left  >> 8) & 0xff);
				p_input->u8[0x4] = ((right >> 0) & 0xff);
				p_input->u8[0x5] = ((right >> 8) & 0xff);
			}

			break;

		case RETRO_DEVICE_SS_MOUSE:

			{
				// mouse buttons
				p_input->u8[0x8] = 0;

				if ( input_state_cb( iplayer, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_LEFT ) ) {
					p_input->u8[0x8] |= ( 1 << 0 ); // A
				}

				if ( input_state_cb( iplayer, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_RIGHT ) ) {
					p_input->u8[0x8] |= ( 1 << 1 ); // B
				}

				if ( input_state_cb( iplayer, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_MIDDLE ) ) {
					p_input->u8[0x8] |= ( 1 << 2 ); // C
				}

				if ( input_state_cb( iplayer, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START ) ||
					 input_state_cb( iplayer, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_BUTTON_4 ) ||
					 input_state_cb( iplayer, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_BUTTON_5 ) ) {
					p_input->u8[0x8] |= ( 1 << 3 ); // Start
				}

				// mouse input
				int dx_raw, dy_raw;
				dx_raw = input_state_cb( iplayer, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_X );
				dy_raw = input_state_cb( iplayer, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_Y );

				int *delta;
				delta = (int*)p_input;
				delta[ 0 ] = (int)roundf( dx_raw * mouse_sensitivity );
				delta[ 1 ] = (int)roundf( dy_raw * mouse_sensitivity );
			}

			break;

		case RETRO_DEVICE_SS_GUN:

			{
				uint8_t shot_type;
				int gun_x, gun_y;
				int forced_reload;

				forced_reload = input_state_cb( iplayer, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_RELOAD );

				// off-screen?
				if ( input_state_cb( iplayer, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_IS_OFFSCREEN ) || forced_reload )
				{
					shot_type = 0x4; // off-screen shot

					gun_x = -16384; // magic position to disable cross-hair drawing.
					gun_y = -16384;
				}
				else
				{
					shot_type = 0x1; // on-screen shot

					int gun_x_raw, gun_y_raw;
					gun_x_raw = input_state_cb( iplayer, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_SCREEN_X );
					gun_y_raw = input_state_cb( iplayer, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_SCREEN_Y );

					// .. scale into screen space:
					// NOTE: the scaling here is semi-guesswork, need to re-write.
					// TODO: Test with PAL games.

					const int scale_x = 21472;
					const int offset_x = 60;
					const int scale_y = 240;

					gun_x = ( ( gun_x_raw + offset_x + 0x7fff ) * scale_x ) / (0x7fff << 1);
					gun_y = ( ( gun_y_raw + 0x7fff ) * scale_y ) / (0x7fff << 1);
				}

				// position
				p_input->gun_pos[ 0 ] = gun_x;
				p_input->gun_pos[ 1 ] = gun_y;

				// buttons
				p_input->u8[ 4 ] = 0;

				// trigger
				if ( input_state_cb( iplayer, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_TRIGGER ) || forced_reload ) {
					p_input->u8[ 4 ] |= shot_type;
				}

				// start
				if ( input_state_cb( iplayer, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_START ) ) {
					p_input->u8[ 4 ] |= 0x2;
				}
			}

			break;

		}; // switch ( input_type[ iplayer ] )

	}; // for each player
}

// save state function for input
int input_StateAction( StateMem* sm, const unsigned load, const bool data_only )
{
	int success;

	SFORMAT StateRegs[] =
	{
		SFARRAY16N( input_mode, MAX_CONTROLLERS, "pad-mode" ),
		SFEND
	};

	success = MDFNSS_StateAction( sm, load, data_only, StateRegs, "LIBRETRO-INPUT" );

	// ok?
	return success;
}

//------------------------------------------------------------------------------
// Libretro Interface
//------------------------------------------------------------------------------

void retro_set_controller_port_device( unsigned in_port, unsigned device )
{
	if ( in_port < MAX_CONTROLLERS )
	{
		// Store input type
		input_type[ in_port ] = device;
		input_mode[ in_port ] = INPUT_MODE_DEFAULT;

		switch ( device )
		{

		case RETRO_DEVICE_NONE:
			log_cb( RETRO_LOG_INFO, "Controller %u: Unplugged\n", (in_port+1) );
			SMPC_SetInput( in_port, "none", (uint8*)&input_data[ in_port ] );
			break;

		case RETRO_DEVICE_JOYPAD:
		case RETRO_DEVICE_SS_PAD:
			log_cb( RETRO_LOG_INFO, "Controller %u: Control Pad\n", (in_port+1) );
			SMPC_SetInput( in_port, "gamepad", (uint8*)&input_data[ in_port ] );
			break;

		case RETRO_DEVICE_SS_3D_PAD:
			log_cb( RETRO_LOG_INFO, "Controller %u: 3D Control Pad\n", (in_port+1) );
			SMPC_SetInput( in_port, "3dpad", (uint8*)&input_data[ in_port ] );
			input_mode[ in_port ] = INPUT_MODE_DEFAULT_3D_PAD;
			break;

		case RETRO_DEVICE_SS_WHEEL:
			log_cb( RETRO_LOG_INFO, "Controller %u: Arcade Racer\n", (in_port+1) );
			SMPC_SetInput( in_port, "wheel", (uint8*)&input_data[ in_port ] );
			break;

		case RETRO_DEVICE_SS_MOUSE:
			log_cb( RETRO_LOG_INFO, "Controller %u: Mouse\n", (in_port+1) );
			SMPC_SetInput( in_port, "mouse", (uint8*)&input_data[ in_port ] );
			break;

		case RETRO_DEVICE_SS_GUN:
			log_cb( RETRO_LOG_INFO, "Controller %u: Virtua Gun\n", (in_port+1) );
			SMPC_SetInput( in_port, "gun", (uint8*)&input_data[ in_port ] );
			break;

		default:
			log_cb( RETRO_LOG_WARN, "Controller %u: Unsupported Device (%u)\n", (in_port+1), device );
			SMPC_SetInput( in_port, "none", (uint8*)&input_data[ in_port ] );
			break;

		}; // switch ( device )

	}; // valid port?
}

void input_multitap( int port, bool enabled )
{
	switch ( port )
	{
		case 1: // PORT 1
			if ( enabled != setting_multitap_port1 ) {
				setting_multitap_port1 = enabled;
				if ( setting_multitap_port1 ) {
					log_cb( RETRO_LOG_INFO, "Connected 6Player Adaptor to Port 1\n" );
				} else {
					log_cb( RETRO_LOG_INFO, "Removed 6Player Adaptor from Port 1\n" );
				}
				SMPC_SetMultitap( 0, setting_multitap_port1 );
			}
			break;

		case 2: // PORT 2
			if ( enabled != setting_multitap_port2 ) {
				setting_multitap_port2 = enabled;
				if ( setting_multitap_port2 ) {
					log_cb( RETRO_LOG_INFO, "Connected 6Player Adaptor to Port 2\n" );
				} else {
					log_cb( RETRO_LOG_INFO, "Removed 6Player Adaptor from Port 2\n" );
				}
				SMPC_SetMultitap( 1, setting_multitap_port2 );
			}
			break;

	}; // switch ( port )

	// Update players count
	players = 2;
	if ( setting_multitap_port1 ) {
		players += 5;
	}
	if ( setting_multitap_port2 ) {
		players += 5;
	}
}

//==============================================================================