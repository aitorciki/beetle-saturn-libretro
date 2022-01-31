/******************************************************************************/
/* Mednafen Sega Saturn Emulation Module                                      */
/******************************************************************************/
/* db.cpp:
**  Copyright (C) 2016-20197 Mednafen Team
**
** This program is free software; you can redistribute it and/or
** modify it under the terms of the GNU General Public License
** as published by the Free Software Foundation; either version 2
** of the License, or (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software Foundation, Inc.,
** 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

/*
 Grandia could use full cache emulation to fix a hang at the end of disc 1, but
 it glitches graphically during gameplay with it enabled, possibly from
 VDP1 drawing completing too fast relative to the CPU...  Also it makes
 emulator CPU usage too high.

 Lost World(Jurassic Park) could use full cache emulation to fix some disappearing background graphics(at least mostly), but
 it makes emulator CPU usage borderline too high.
*/

#include "../FileStream.h"

#include "ss.h"
#include "smpc.h"
#include "cart.h"
#include "db.h"


static const struct
{
 uint8 id[16];
 unsigned area;
 const char* game_name;
} regiondb[] =
{
 { { 0x10, 0x8f, 0xe1, 0xaf, 0x55, 0x5a, 0x95, 0x42, 0x04, 0x85, 0x7e, 0x98, 0x8c, 0x53, 0x6a, 0x31, }, SMPC_AREA_EU_PAL,	"Preview Sega Saturn Vol. 1 (Europe)" },
 { { 0xed, 0x4c, 0x0b, 0x87, 0x35, 0x37, 0x86, 0x76, 0xa0, 0xf6, 0x32, 0xc6, 0xa4, 0xc3, 0x99, 0x88, }, SMPC_AREA_EU_PAL,	"Primal Rage (Europe)" },
 { { 0x15, 0xfc, 0x3a, 0x82, 0x16, 0xa9, 0x85, 0xa5, 0xa8, 0xad, 0x30, 0xaf, 0x9a, 0xff, 0x03, 0xa9, }, SMPC_AREA_JP,		"Race Drivin' (Japan)" },
 { { 0xe1, 0xdd, 0xfd, 0xa1, 0x8b, 0x47, 0x02, 0x21, 0x36, 0x1e, 0x5a, 0xae, 0x20, 0xc0, 0x59, 0x9f, }, SMPC_AREA_CSA_NTSC,	"Riven - A Sequencia de Myst (Brazil) (Disc 1)" },
 { { 0xbf, 0x5f, 0xf8, 0x5f, 0xf2, 0x0c, 0x35, 0xf6, 0xc9, 0x8d, 0x03, 0xbc, 0x34, 0xd9, 0xda, 0x7f, }, SMPC_AREA_CSA_NTSC,	"Riven - A Sequencia de Myst (Brazil) (Disc 2)" },
 { { 0x98, 0xb6, 0x6e, 0x09, 0xe6, 0xdc, 0x30, 0xe6, 0x55, 0xdb, 0x85, 0x01, 0x33, 0x0c, 0x0b, 0x9c, }, SMPC_AREA_CSA_NTSC,	"Riven - A Sequencia de Myst (Brazil) (Disc 3)" },
 { { 0xa2, 0x34, 0xb0, 0xb9, 0xaa, 0x47, 0x74, 0x1f, 0xd4, 0x1e, 0x35, 0xda, 0x3d, 0xe7, 0x4d, 0xe3, }, SMPC_AREA_CSA_NTSC,	"Riven - A Sequencia de Myst (Brazil) (Disc 4)" },
 { { 0xf7, 0xe9, 0x23, 0x0a, 0x9e, 0x92, 0xf1, 0x93, 0x16, 0x43, 0xf8, 0x6c, 0xe8, 0x21, 0x50, 0x66, }, SMPC_AREA_JP,		"Sega International Victory Goal (Japan)" },
 { { 0x64, 0x75, 0x25, 0x0c, 0xa1, 0x9b, 0x6c, 0x5e, 0x4e, 0xa0, 0x6d, 0x69, 0xd9, 0x0f, 0x32, 0xca, }, SMPC_AREA_EU_PAL,	"Virtua Racing (Europe)" },
 { { 0x0d, 0xe3, 0xfa, 0xfb, 0x2b, 0xb9, 0x6d, 0x79, 0xe0, 0x3a, 0xb7, 0x6d, 0xcc, 0xbf, 0xb0, 0x2c, }, SMPC_AREA_JP,		"Virtua Racing (Japan)" },
 { { 0x6b, 0x29, 0x33, 0xfc, 0xdd, 0xad, 0x8e, 0x0d, 0x95, 0x81, 0xa6, 0xee, 0xfd, 0x90, 0x4b, 0x43, }, SMPC_AREA_EU_PAL,	"Winter Heat (Europe) (Demo)" },
 { { 0x73, 0x91, 0x4b, 0xe1, 0xad, 0x4d, 0xaf, 0x69, 0xc3, 0xeb, 0xb8, 0x43, 0xee, 0x3e, 0xb5, 0x09, }, SMPC_AREA_EU_PAL,	"WWF WrestleMania - The Arcade Game (Europe) (Demo)" },
};

static const struct
{
 const char* sgid;
 int cart_type;
 const char* game_name;
 const char* purpose;
 uint8 fd_id[16];
} cartdb[] =
{
 //
 //
 // NetLink Modem TODO:
 { "MK-81218", CART_NONE, "Daytona USA CCE Net Link Edition", "Reserved for future modem support." },
 { "MK-81071", CART_NONE, "Duke Nukem 3D", "Reserved for future modem support." },
 { "T-319-01H", CART_NONE, "PlanetWeb Browser (multiple versions)", "Reserved for future modem support." },
 { "MK-81070", CART_NONE, "Saturn Bomberman", "Reserved for future modem support." },
 { "MK-81215", CART_NONE, "Sega Rally Championship Plus NetLink Edition", "Reserved for future modem support." },
 { "MK-81072", CART_NONE, "Virtual On NetLink Edition", "Reserved for future modem support." },
 //
 //
 // Japanese modem TODO:
 { "GS-7106", CART_NONE, "Dennou Senki Virtual On (SegaNet)", "Reserved for future modem support." },
 { "GS-7114", CART_NONE, "Dragon's Dream (Japan)", "Reserved for future modem support." },
 { "GS-7105", CART_NONE, "Habitat II (Japan)", "Reserved for future modem support." },
 { "GS-7101", CART_NONE, "Pad Nifty (Japan)", "Reserved for future modem support." },
 { "GS-7113", CART_NONE, "Puzzle Bobble 3 (SegaNet)", "Reserved for future modem support." },
 { "T-14305G", CART_NONE, "Saturn Bomberman (SegaNet)", "Reserved for future modem support." },
 { "T-31301G", CART_NONE, "SegaSaturn Internet Vol. 1 (Japan)", "Reserved for future modem support." },
 //
 //
 // ROM carts:
 { "MK-81088", CART_KOF95, "King of Fighters '95, The (Europe)", "Game requirement." },
 { "T-3101G", CART_KOF95, "King of Fighters '95, The (Japan)", "Game requirement." },
 { "T-13308G", CART_ULTRAMAN, "Ultraman - Hikari no Kyojin Densetsu (Japan)", "Game requirement." },
 //
 //
 // 1MiB RAM cart:
 { "T-1521G", CART_EXTRAM_1M, "Astra Superstars (Japan)" },	// Would 4MiB be better?
 { "T-9904G", CART_EXTRAM_1M, "Cotton 2 (Japan)" },
 { "T-1217G", CART_EXTRAM_1M, "Cyberbots (Japan)" },
 { "GS-9107", CART_EXTRAM_1M, "Fighter's History Dynamite (Japan)", "Game requirement." },
 { "T-20109G", CART_EXTRAM_1M, "Friends (Japan)" },		// Would 4MiB be better?
 { "T-14411G", CART_EXTRAM_1M, "Groove on Fight (Japan)", "Game requirement." },
 { "T-7032H-50", CART_EXTRAM_1M, "Marvel Super Heroes (Europe)" },
 { "T-1215G", CART_EXTRAM_1M, "Marvel Super Heroes (Japan)" },
 { "T-3111G", CART_EXTRAM_1M, "Metal Slug (Japan)", "Game requirement." },
 { "T-22205G", CART_EXTRAM_1M, "NOël 3 (Japan)" },
 { "T-20114G", CART_EXTRAM_1M, "Pia Carrot e Youkoso!! 2 (Japan)" },
 { "T-3105G", CART_EXTRAM_1M, "Real Bout Garou Densetsu (Japan)", "Game requirement." }, //  Incompatible with 4MiB extended RAM cart.
 { "T-3119G", CART_EXTRAM_1M, "Real Bout Garou Densetsu Special (Japan)", "Game requirement." },
 { "T-3116G", CART_EXTRAM_1M, "Samurai Spirits - Amakusa Kourin (Japan)", "Game requirement." }, // Incompatible with 4MiB extended RAM cart.
 { "T-3104G", CART_EXTRAM_1M, "Samurai Spirits - Zankurou Musouken (Japan)", "Game requirement." },
 { "610636008",CART_EXTRAM_1M,"Tech Saturn 1997.6 (Japan)", "Required by \"Groove on Fight\" demo." },
 { "T-16509G", CART_EXTRAM_1M, "Super Real Mahjong P7 (Japan) (TODO: Test)" },
 { "T-16510G", CART_EXTRAM_1M, "Super Real Mahjong P7 (Japan)" },	// Would 4MiB be better?
 { "T-3108G", CART_EXTRAM_1M, "The King of Fighters '96 (Japan)", "Game requirement." },
 { "T-3121G", CART_EXTRAM_1M, "The King of Fighters '97 (Japan)", "Game requirement." },
 { "T-1515G", CART_EXTRAM_1M, "Waku Waku 7 (Japan)", "Game requirement." },
 //
 //
 // 4MiB RAM cart:
 { "T-1245G", CART_EXTRAM_4M, "Dungeons and Dragons Collection (Japan)", "Game requirement(\"Shadow over Mystara\")." },
 { "T-1248G", CART_EXTRAM_4M, "Final Fight Revenge (Japan)", "Game requirement." },
 { "T-1238G", CART_EXTRAM_4M, "Marvel Super Heroes vs. Street Fighter (Japan)", "Game requirement." },
 { "T-1230G", CART_EXTRAM_4M, "Pocket Fighter (Japan)" },
 { "T-1246G", CART_EXTRAM_4M, "Street Fighter Zero 3 (Japan)", "Game requirement." },
 { "T-1229G", CART_EXTRAM_4M, "Vampire Savior (Japan)", "Game requirement." },
 { "T-1226G", CART_EXTRAM_4M, "X-Men vs. Street Fighter (Japan)", "Game requirement." },
 //
 //
 //
 { nullptr, CART_CS1RAM_16M, "Heart of Darkness (Prototype)", "Game requirement(though it's probable the original dev cart was only around 6 to 8MiB).", { 0x4a, 0xf9, 0xff, 0x30, 0xea, 0x54, 0xfe, 0x3a, 0x79, 0xa7, 0x68, 0x69, 0xae, 0xde, 0x55, 0xbb } },
 { nullptr, CART_CS1RAM_16M, "Heart of Darkness (Prototype)", "Game requirement(though it's probable the original dev cart was only around 6 to 8MiB).", { 0xf1, 0x71, 0xc3, 0xe4, 0x69, 0xd5, 0x99, 0x93, 0x94, 0x09, 0x05, 0xfc, 0x29, 0xd3, 0x8a, 0x59 } },
 //
 //
 // Backup memory cart:
 { "T-16804G", CART_BACKUP_MEM, "Dezaemon 2 (Japan)", "Allows saving." },	// !
 { "GS-9123", CART_BACKUP_MEM,	"Die Hard Trilogy (Japan)", "Game will crash when running with a RAM expansion cart." }, // !
 { "T-16103H", CART_BACKUP_MEM,	"Die Hard Trilogy (Europe/USA)", "Game will crash when running with a RAM expansion cart." }, // !
 { "T-26104G", CART_BACKUP_MEM, "Kouryuu Sangoku Engi (Japan)" }, // !
 { "GS-9197", CART_BACKUP_MEM,	"Sega Ages - Galaxy Force II", "Allows saving replay data." }, // !
#if 0
 { "T-9527G", CART_BACKUP_MEM,	"Akumajou Dracula X - Gekka no Yasoukyoku (Japan)" },
 { "T-1507G", CART_BACKUP_MEM,	"Albert Odyssey (Japan)" },
 { "T-12705H", CART_BACKUP_MEM,	"Albert Odyssey (USA)" },
 { "T-1209G", CART_BACKUP_MEM,	"Arthur to Astaroth no Nazomakaimura - Incredible Toons (Japan)" },
 { "T-33901G", CART_BACKUP_MEM,	"Baroque (Japan)" },
 { "T-20113G", CART_BACKUP_MEM, "Black Matrix (Japan)" },
 { "T-20115G", CART_BACKUP_MEM, "Black Matrix (Japan)" },
 { "T-4315G", CART_BACKUP_MEM,	"Blue Breaker (Japan)" },
 { "GS-9174", CART_BACKUP_MEM,	"Burning Rangers (Japan)" },
 { "MK-81803", CART_BACKUP_MEM,	"Burning Rangers (Europe/USA)" },
 { "610-6431", CART_BACKUP_MEM,	"Christmas NiGHTS into Dreams (Japan)" },
 { "610-6483", CART_BACKUP_MEM,	"Christmas NiGHTS into Dreams (Europe)" },
 { "MK-81067", CART_BACKUP_MEM,	"Christmas NiGHTS into Dreams (USA)" },
 { "T-22101G", CART_BACKUP_MEM,	"Dark Savior (Japan)" },
 { "MK-81304", CART_BACKUP_MEM,	"Dark Savior (Europe/USA)" },
 { "GS-9028", CART_BACKUP_MEM,	"Dragon Force (Japan)" }, // ~
 { "T-12703H", CART_BACKUP_MEM,	"Dragon Force (USA)" }, // ~
 { "MK-8138250", CART_BACKUP_MEM,"Dragon Force (Europe)" }, // ~
 { "GS-9184", CART_BACKUP_MEM,	"Dragon Force II (Japan)" }, // ~
 { "T-31503G", CART_BACKUP_MEM,	"Falcom Classics (Japan)" },
 { "T-31504G", CART_BACKUP_MEM,	"Falcom Classics II Genteiban (Japan)" },
 { "T-31505G", CART_BACKUP_MEM,	"Falcom Classics II (Japan)" },
 { "T-9525G", CART_BACKUP_MEM,	"Gensou Suikoden (Japan)" },
 { "T-4507G", CART_BACKUP_MEM,	"Grandia (Japan)" },
 { "T-4512G", CART_BACKUP_MEM,	"Grandia - Digital Museum (Japan)" },
 { "T-19710G", CART_BACKUP_MEM,	"GunBlaze-S (Japan)" },
 { "T-18612G", CART_BACKUP_MEM,	"Hexen (Japan)", "Allows saving." }, // !
 { "T-25406H", CART_BACKUP_MEM,	"Hexen (USA)", "Allows saving." }, // !
 { "T-25405H50", CART_BACKUP_MEM,"Hexen (Europe)", "Allows saving." }, // !
 { "T-2502G", CART_BACKUP_MEM,	"Langrisser III (Japan)" },
 { "T-2505G", CART_BACKUP_MEM,	"Langrisser IV (Japan)" },
 { "T-2509G", CART_BACKUP_MEM,	"Langrisser V (Japan)" },
 { "T-37101G", CART_BACKUP_MEM,	"Legend of Heroes I & II, The - Eiyuu Densetsu (Japan)" },
 { "MK-81302", CART_BACKUP_MEM,	"Legend of Oasis, The (USA) / Story of Thor 2, The (Europe)" },
 { "GS-9053", CART_BACKUP_MEM,	"Thor - Seireioukiden (Japan)" },
 { "T-27901G", CART_BACKUP_MEM,	"Lunar - Silver Star Story (Japan)" },
 { "T-27904G", CART_BACKUP_MEM,	"Lunar - Silver Star Story MPEG (Japan)" },
 { "T-27906G", CART_BACKUP_MEM,	"Lunar 2 - Eternal Blue (Japan)" },
 { "T-6607G", CART_BACKUP_MEM,	"Madou Monogatari (Japan)" },
 { "GS-9018", CART_BACKUP_MEM,	"Magic Knight Rayearth (Japan)" },
 { "T-12706H", CART_BACKUP_MEM,	"Magic Knight Rayearth (USA)" },
 { "T-27902G", CART_BACKUP_MEM,	"Mahou Gakuen Lunar (Japan)" },
 { "T-1214G", CART_BACKUP_MEM,	"Rockman 8 (Japan)" },
 { "T-1216H", CART_BACKUP_MEM,	"Mega Man 8 (USA)" },
 { "T-1210G", CART_BACKUP_MEM,	"Rockman X3 (Japan)" },
 { "T-7029H-50", CART_BACKUP_MEM,"Mega Man X3 (Europe)" },
 { "T-1221G", CART_BACKUP_MEM,	"Rockman X4 (Japan)" },
 { "T-1219H", CART_BACKUP_MEM,	"Mega Man X4 (USA)" },
 { "T-1501G", CART_BACKUP_MEM,	"Myst (Japan)" },
 { "T-26801H08", CART_BACKUP_MEM,"Myst (Korea)" },
 { "T-8101H", CART_BACKUP_MEM,	"Myst (USA)" },
 { "MK-81081", CART_BACKUP_MEM,	"Myst (Europe)" },
 { "GS-9046", CART_BACKUP_MEM,	"NiGHTS into Dreams (Japan)" },
 { "MK-81020", CART_BACKUP_MEM,	"NiGHTS into Dreams (Europe/USA)" },
 { "GS-9076", CART_BACKUP_MEM,	"Panzer Dragoon RPG (Japan)" },
 { "MK-81307", CART_BACKUP_MEM,	"Panzer Dragoon Saga (Europe/USA)" },
 { "T-26112G", CART_BACKUP_MEM,	"Prisoner of Ice (Japan)" }, // ~
 { "T-1219G", CART_BACKUP_MEM,	"Bio Hazard (Japan)" },
 { "T-1221H", CART_BACKUP_MEM,	"Resident Evil (USA)" },
 { "MK-81092", CART_BACKUP_MEM,	"Resident Evil (Europe)" },
 { "MK-81383", CART_BACKUP_MEM,	"Shining Force III (Europe/USA)" }, // ~
 { "GS-9175", CART_BACKUP_MEM,	"Shining Force III - Scenario 1 (Japan)" }, // ~
 { "GS-9188", CART_BACKUP_MEM,	"Shining Force III - Scenario 2 (Japan)" }, // ~
 { "GS-9203", CART_BACKUP_MEM,	"Shining Force III - Scenario 3 (Japan)" }, // ~
 { "T-33101G", CART_BACKUP_MEM,	"Shining the Holy Ark (Japan)" },
 { "MK-81306", CART_BACKUP_MEM,	"Shining the Holy Ark (Europe/USA)" },
 { "GS-9057", CART_BACKUP_MEM,	"Shining Wisdom (Japan)" },
 { "T-12702H", CART_BACKUP_MEM,	"Shining Wisdom (USA)" },
 { "MK-81381", CART_BACKUP_MEM,	"Shining Wisdom (Europe)" },
 { "T-14322G", CART_BACKUP_MEM,	"Shiroki Majo - Mou Hitotsu no Eiyuu Densetsu (Japan)" },
 { "GS-9027", CART_BACKUP_MEM,	"SimCity 2000 (Japan)" }, // ~
 { "T-12601H", CART_BACKUP_MEM,	"SimCity 2000 (USA)" }, // ~
 { "MK-81580", CART_BACKUP_MEM,	"SimCity 2000 (Europe)" }, // ~
 { "T-27903G", CART_BACKUP_MEM,	"Slayers Royal (Japan)" },
 { "T-27907G", CART_BACKUP_MEM,	"Slayers Royal 2 (Japan)" },
 { "GS-9170", CART_BACKUP_MEM,	"Sonic R (Japan)" },
 { "MK-81800", CART_BACKUP_MEM,	"Sonic R (Europe/USA)" },
 { "T-16609G", CART_BACKUP_MEM,	"Sorvice (Japan)" },
 { "T-9526G", CART_BACKUP_MEM,	"Vandal Hearts - Ushinawareta Kodai Bunmei (Japan)" },
 { "T-10623G", CART_BACKUP_MEM,	"WarCraft II (Japan)" }, // ~
 { "T-5023H", CART_BACKUP_MEM,	"WarCraft II (USA)" }, // ~
 { "T-5023H-50", CART_BACKUP_MEM,"WarCraft II (Europe)" }, // ~
#endif
};

static const struct
{
 const char* sgid;
 unsigned mode;
 const char* game_name;
 const char* purpose;
 uint8 fd_id[16];
} cemdb[] =
{
 { "T-9705H",	CPUCACHE_EMUMODE_DATA_CB,	"Area 51 (USA)", "Fixes game hang." },
 { "T-25408H",	CPUCACHE_EMUMODE_DATA_CB,	"Area 51 (Europe)", "Fixes game hang." },
 { "MK-81036",	CPUCACHE_EMUMODE_DATA_CB,	"Clockwork Knight 2 (USA)", "Fixes game hang that occurred when some FMVs were played." },
 { "T-30304G", CPUCACHE_EMUMODE_DATA_CB,	"DeJig - Lassen Art Collection (Japan)", "Fixes graphical glitches." },
 { "GS-9184", CPUCACHE_EMUMODE_DATA_CB,		"Dragon Force II (Japan)", "Fixes math and game logic errors during battles." },
 { "T-18504G", CPUCACHE_EMUMODE_DATA_CB,	"Father Christmas (Japan)", "Fixes stuck music and voice acting." },
 { "GS-9101",  CPUCACHE_EMUMODE_DATA_CB,	"Fighting Vipers (Japan)", "Fixes computer-controlled opponent turning into a ghost statue." },
 { "MK-81041",  CPUCACHE_EMUMODE_DATA_CB,	"Fighting Vipers (Europe/USA)", "Fixes computer-controlled opponent turning into a ghost statue." },
 { "T-7309G", CPUCACHE_EMUMODE_DATA_CB,		"Formula Grand Prix - Team Unei Simulation (Japan)", "Fixes game hang." },
 { "MK-81045", CPUCACHE_EMUMODE_DATA_CB,	"Golden Axe - The Duel (Europe/USA)", "Fixes flickering title screen." },
 { "GS-9041",	CPUCACHE_EMUMODE_DATA_CB,	"Golden Axe - The Duel (Japan)", "Fixes flickering title screen." },
 { "GS-9173", CPUCACHE_EMUMODE_DATA_CB,		"House of the Dead (Japan)", "Fixes game crash on lightgun calibration screen." },
 { "GS-9055", CPUCACHE_EMUMODE_DATA_CB,		"Linkle Liver Story (Japan)", "Fixes game crash when going to the world map." },
 { "T-14415G", CPUCACHE_EMUMODE_DATA_CB,	"Ronde (Japan)", "Fixes missing graphics on the title screen, main menu, and elsewhere." },
 { "81600",	CPUCACHE_EMUMODE_DATA_CB,	"Sega Saturn Choice Cuts (USA)", "Fixes FMV playback hangs and playback failures." },
 { "610680501",CPUCACHE_EMUMODE_DATA_CB,	"Segakore Sega Bible Mogitate SegaSaturn (Japan)", "" },	// ? ? ?
 { "T-18703G", CPUCACHE_EMUMODE_DATA_CB,	"Shunsai (Japan)", "Fixes various graphical glitches." },
 { "T-7001H",	CPUCACHE_EMUMODE_DATA_CB,	"Spot Goes to Hollywood (USA)", "Fixes hang at corrupted \"Burst\" logo." },
 { "T-7014G",	CPUCACHE_EMUMODE_DATA_CB,	"Spot Goes to Hollywood (Japan)", "Fixes hang at corrupted \"Burst\" logo." },
 // Nooo, causes glitches: { "T-7001H-50",CPUCACHE_EMUMODE_DATA_CB,	"Spot Goes to Hollywood (Europe)
 { "T-1206G",	CPUCACHE_EMUMODE_DATA_CB,	"Street Fighter Zero (Japan)", "Fixes weird color/palette issues during game startup." },
 { "T-1246G",	CPUCACHE_EMUMODE_DATA_CB,	"Street Fighter Zero 3 (Japan)", "" },	// ? ? ?
 { "T-1215H",	CPUCACHE_EMUMODE_DATA_CB,	"Super Puzzle Fighter II Turbo (USA)", "Fixes color/brightness and other graphical issues." },
 { "T-5001H",	CPUCACHE_EMUMODE_DATA_CB,	"Theme Park (Europe)", "Fixes hang during FMV." },
 { "T-1807G", CPUCACHE_EMUMODE_DATA_CB,		"Thunder Force Gold Pack 1 (Japan)", "Fixes explosion graphic glitches in \"Thunder Force III\"." },
 { "T-1808G", CPUCACHE_EMUMODE_DATA_CB,		"Thunder Force Gold Pack 2 (Japan)", "Fixes hang when pausing the game under certain conditions in \"Thunder Force AC\"." },
 { "GS-9113", CPUCACHE_EMUMODE_DATA_CB,		"Virtua Fighter Kids (Java Tea Original)", "Fixes malfunction of computer-controlled player." },
 { "T-2206G", CPUCACHE_EMUMODE_DATA_CB,		"Virtual Mahjong (Japan)", "Fixes graphical glitches on the character select screen." },
 { "T-15005G", CPUCACHE_EMUMODE_DATA_CB,	"Virtual Volleyball (Japan)", "Fixes invisible menu items and hang." },
 { "T-18601H", CPUCACHE_EMUMODE_DATA_CB,	"WipEout (USA)", "Fixes hang when trying to exit gameplay back to the main menu." },
 { "T-18603G", CPUCACHE_EMUMODE_DATA_CB,	"WipEout (Japan)", "Fixes hang when trying to exit gameplay back to the main menu." },
 { "T-11301H", CPUCACHE_EMUMODE_DATA_CB,	"WipEout (Europe)", "Fixes hang when trying to exit gameplay back to the main menu." },
 { "GS-9061", CPUCACHE_EMUMODE_DATA_CB,		"Hideo Nomo World Series Baseball (Japan)", "Fixes severe gameplay logic glitches." },
 { "MK-81109", CPUCACHE_EMUMODE_DATA_CB,	"World Series Baseball (Europe/USA)", "Fixes severe gameplay logic glitches." },

 //{ "MK-81019", CPUCACHE_EMUMODE_DATA },	// Astal (USA)
 //{ "GS-9019",  CPUCACHE_EMUMODE_DATA },	// Astal (Japan)

 { "T-1507G",	CPUCACHE_EMUMODE_FULL,		"Albert Odyssey (Japan)", "" },
 { "T-12705H",	CPUCACHE_EMUMODE_FULL,		"Albert Odyssey (USA)", "Fixes battle text truncation." },
 { "GS-9123", CPUCACHE_EMUMODE_FULL,		"Die Hard Trilogy (Japan)", "Fixes game hang." },
 { "T-16103H", CPUCACHE_EMUMODE_FULL,		"Die Hard Trilogy (Europe/USA)", "Fixes game hang." },
 { "T-13331G", CPUCACHE_EMUMODE_FULL,		"Digital Monster Version S (Japan)", "Fixes game hang." },
 //{ "T-20502G", CPUCACHE_EMUMODE_FULL,		"Discworld (Japan) (still broken...)" },
 { "T-13310G", CPUCACHE_EMUMODE_FULL,		"GeGeGe no Kitarou (Japan)", "Fixes game hang." },
 { "T-15904G", CPUCACHE_EMUMODE_FULL,		"Gex (Japan)",		"Fixes minor FMV glitches."  },
 { "T-15904H", CPUCACHE_EMUMODE_FULL,		"Gex (USA)",		"Fixes minor FMV glitches." },
 { "T-15904H50", CPUCACHE_EMUMODE_FULL,		"Gex (Europe)",		"Fixes minor FMV glitches." },
 { "T-27901G", CPUCACHE_EMUMODE_FULL,		"Lunar - Silver Star Story (Japan)", "Fixes FMV flickering with alternative BIOS." },
 { "T-7664G", CPUCACHE_EMUMODE_FULL,		"Nobunaga no Yabou Shouseiroku (Japan)", "Fixes game hang." },
 { "T-9510G", CPUCACHE_EMUMODE_FULL,		"Policenauts (Japan)",	"Fixes screen flickering on disc 2." },
 { "T-25416H50", CPUCACHE_EMUMODE_FULL,		"Rampage - World Tour (Europe)", "Fixes game hang." },
 { "T-159056", CPUCACHE_EMUMODE_FULL,		"Slam 'n Jam 96 (Japan)", "Fixes minor FMV glitches." },
 { "T-159028H", CPUCACHE_EMUMODE_FULL,		"Slam 'n Jam 96 (USA)",	"Fixes minor FMV glitches." },
 { "T-15902H50", CPUCACHE_EMUMODE_FULL,		"Slam 'n Jam 96 (Europe)", "Fixes minor FMV glitches." },
 { "T-8119G", CPUCACHE_EMUMODE_FULL,		"Space Jam (Japan)", 	"Fixes game crash." },
 { "T-8125H", CPUCACHE_EMUMODE_FULL,		"Space Jam (USA)", 	"Fixes game crash." },
 { "T-8125H-50", CPUCACHE_EMUMODE_FULL,		"Space Jam (Europe)", 	"Fixes game crash." },
 { "T-36102G",	CPUCACHE_EMUMODE_FULL,		"Whizz (Japan)", 	"Fixes quasi-random hangs during startup." },
 { "T-9515H-50", CPUCACHE_EMUMODE_FULL,		"Whizz (Europe)", 	"Fixes quasi-random hangs during startup." },
};

void DB_Lookup(const char* path, const char* sgid, const uint8* fd_id, unsigned* const region, int* const cart_type, unsigned* const cpucache_emumode)
{
 for(auto& re : regiondb)
 {
  if(!memcmp(re.id, fd_id, 16))
  {
   *region = re.area;
   break;
  }
 }

 for(auto& ca : cartdb)
 {
  if((ca.sgid && !strcmp(ca.sgid, sgid)) || (!ca.sgid && !memcmp(ca.fd_id, fd_id, 16)))
  {
   *cart_type = ca.cart_type;
   break;
  }
 }

 for(auto& c : cemdb)
 {
  if((c.sgid && !strcmp(c.sgid, sgid)) || (!c.sgid && !memcmp(c.fd_id, fd_id, 16)))
  {
   *cpucache_emumode = c.mode;
   break;
  }
 }
}

static const struct
{
 const char* sgid;
 unsigned horrible_hacks;
 const char* game_name;
 const char* purpose;
 uint8 fd_id[16];
} hhdb[] =
{
 { "GS-9126", HORRIBLEHACK_NOSH2DMAPENALTY,	"Fighters Megamix (Japan)", "Fixes hang after watching or aborting FMV playback." },
 { "MK-81073", HORRIBLEHACK_NOSH2DMAPENALTY,	"Fighters Megamix (Europe/USA)", "Fixes hang after watching or aborting FMV playback." },
 { "T-22403G", HORRIBLEHACK_NOSH2DMAPENALTY,	"Irem Arcade Classics (Japan)", "Fixes hang when trying to start \"Zippy Race\"." }, // (way too finicky...)

 { "T-4507G", HORRIBLEHACK_VDP1VRAM5000FIX,	"Grandia (Japan)", "Fixes hang at end of first disc." },

 { "T-1507G",	HORRIBLEHACK_VDP1RWDRAWSLOWDOWN,"Albert Odyssey (Japan)", "Partially fixes battle text truncation." },
 { "T-12705H",	HORRIBLEHACK_VDP1RWDRAWSLOWDOWN,"Albert Odyssey (USA)", "Partially fixes battle text truncation." },
 { "T-8150H", HORRIBLEHACK_VDP1RWDRAWSLOWDOWN,	"All-Star Baseball 97 (USA)", "Fixes texture glitches." },
 { "T-9703H", HORRIBLEHACK_VDP1RWDRAWSLOWDOWN,	"Arcade's Greatest Hits (USA)", "Fixes flickering credits text." },
 { "T-9706H", HORRIBLEHACK_VDP1RWDRAWSLOWDOWN,	"Arcade's Greatest Hits - Atari Collection 1 (USA)", "Fixes flickering credits text." },
 { "6106856", HORRIBLEHACK_VDP1RWDRAWSLOWDOWN,	"Burning Rangers Taikenban (Japan)", "Fixes flickering rescue text." },
 { "GS-9174", HORRIBLEHACK_VDP1RWDRAWSLOWDOWN,	"Burning Rangers (Japan)", "Fixes flickering rescue text." },
 { "MK-81803", HORRIBLEHACK_VDP1RWDRAWSLOWDOWN,	"Burning Rangers (Europe/USA)", "Fixes flickering rescue text." },
 { "T-8111G", HORRIBLEHACK_VDP1RWDRAWSLOWDOWN, "Frank Thomas Big Hurt Baseball (Japan)", "Reduces graphical glitches." },
 { "T-8138H", HORRIBLEHACK_VDP1RWDRAWSLOWDOWN, "Frank Thomas Big Hurt Baseball (USA)", "Reduces graphical glitches." }, // Probably need more-accurate VDP1 draw timings to fix the glitches completely.
 { "T-36102G", HORRIBLEHACK_VDP1RWDRAWSLOWDOWN, "Whizz (Japan)", "Fixes major graphical issues during gameplay." },
 { "T-9515H-50", HORRIBLEHACK_VDP1RWDRAWSLOWDOWN,"Whizz (Europe)", "Fixes major graphical issues during gameplay." },

 // Still random hangs...wtf is this game doing...
 { "T-6006G", HORRIBLEHACK_NOSH2DMALINE106 | HORRIBLEHACK_VDP1INSTANT, "Thunderhawk II (Japan)", "Fixes hangs just before and during gameplay." },
 { "T-11501H00", HORRIBLEHACK_NOSH2DMALINE106 | HORRIBLEHACK_VDP1INSTANT, "Thunderstrike II (USA)", "Fixes hangs just before and during gameplay." },
};

uint32 DB_LookupHH(const char* sgid, const uint8* fd_id)
{
 for(auto& hh : hhdb)
 {
  if((hh.sgid && !strcmp(hh.sgid, sgid)) || (!hh.sgid && !memcmp(hh.fd_id, fd_id, 16)))
  {
   return hh.horrible_hacks;
  }
 }

 return 0;
}
