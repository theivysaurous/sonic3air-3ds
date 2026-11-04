/*
*	Part of the Oxygen Engine / Sonic 3 A.I.R. software distribution.
*	Copyright (C) 2017-2026 by Eukaryot
*
*	Published under the GNU GPLv3 open source software license, see license.txt
*	or https://www.gnu.org/licenses/gpl-3.0.en.html
*/

#pragma once

#include <rmxmedia.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <algorithm>


// Settings and shared state for the Nintendo 3DS port's screen output
//  -> The settings are read once from an optional "video.cfg" in the game folder on the SD card, one "key=value" per line
//  -> The bottom screen state is set by the game code (menus, title screen, ...) and used when presenting
struct Video3DS
{
	struct Config
	{
		bool mLoaded = false;
		bool m24Bit = true;				// "format=24" (default) or "format=32": 24-bit BGR8 or 32-bit RGBA8 framebuffers (NTR screen streaming needs 24-bit to show correct colors)
		bool mAlphaZero = false;		// "alpha=0": In 32-bit mode, write alpha 0 instead of 255
		bool mBottomEnabled = true;		// "bottom=0" turns the bottom screen content off
		bool mBottomScene = false;		// "bottom=background" (default): In levels, show only the background plane on the bottom screen; "bottom=scene": Show the level without sprites (all planes), which costs more
		int mBottomRate = 2;			// "bottom_rate=N": Update the background image on the bottom screen on every N-th frame (1 to 10)
		int mFpsLimit = 30;				// "fps=30" (default) or "fps=60": Frame rate limit

		void load()
		{
			if (mLoaded)
				return;
			mLoaded = true;

			FILE* file = fopen("video.cfg", "r");
			if (nullptr == file)
				return;

			char line[128];
			while (fgets(line, sizeof(line), file))
			{
				if (strncmp(line, "format=24", 9) == 0)
					m24Bit = true;
				else if (strncmp(line, "format=32", 9) == 0)
					m24Bit = false;
				else if (strncmp(line, "alpha=0", 7) == 0)
					mAlphaZero = true;
				else if (strncmp(line, "bottom=0", 8) == 0)
					mBottomEnabled = false;
				else if (strncmp(line, "bottom=scene", 12) == 0)
				{
					mBottomEnabled = true;
					mBottomScene = true;
				}
				else if (strncmp(line, "bottom=", 7) == 0)
				{
					mBottomEnabled = true;			// "bottom=background" or anything else: on
					mBottomScene = false;
				}
				else if (strncmp(line, "bottom_rate=", 12) == 0)
					mBottomRate = std::max(1, std::min(10, atoi(line + 12)));
				else if (strncmp(line, "fps=", 4) == 0)
					mFpsLimit = (atoi(line + 4) >= 60) ? 60 : 30;
			}
			fclose(file);
		}
	};

	static Config& config()
	{
		static Config instance;
		instance.load();
		return instance;
	}

	// What is shown on the bottom screen
	enum class BottomMode : uint8
	{
		BACKGROUND,		// The scrolling background layer (plane B) of the game
		SOLID,			// One color; either a fixed one, or the color of the top left pixel of the game screen (to follow fades)
		IMAGE			// A fixed image
	};

	static inline BottomMode sBottomMode = BottomMode::BACKGROUND;
	static inline uint32 sBottomSolidColor = 0xff000000;		// ABGR
	static inline bool sBottomSolidFollowsScreen = false;
	static inline const Bitmap* sBottomImage = nullptr;

	// Profiling of draw commands by type, see "perf.log"
	static inline uint64 sCommandTicks[16] = {};
	static inline uint32 sCommandCount[16] = {};

	// Set by the presenter: The renderer should capture the background layer in the frame it's about to render
	static inline bool sCaptureBackground = false;
	static inline bool sCaptureAllPlanes = true;				// Capture all planes (the level without sprites), not only plane B; avoids showing parts of plane B that are normally covered by plane A
	static inline bool sCaptureNoPlanes = false;				// Capture only the background sprites, no planes (title screen: the background without Sonic, who is on a plane)
	static inline uint16 sCaptureQueueLimit = 0x2000;			// Background sprites are everything with a render queue below this value
	static inline bool sCaptureBackgroundSprites = false;		// Also capture everything below the first game layer (render queue < 0x2000), e.g. the background image of the title screen

	static void setBottomSolid(uint32 colorABGR)
	{
		sBottomMode = BottomMode::SOLID;
		sBottomSolidColor = colorABGR;
		sBottomSolidFollowsScreen = false;
	}

	static void setBottomSolidFollowingScreen()
	{
		sBottomMode = BottomMode::SOLID;
		sBottomSolidFollowsScreen = true;
	}

	static void setBottomImage(const Bitmap* image)
	{
		if (nullptr == image || image->empty())
		{
			setBottomSolid(0xff000000);
			return;
		}
		sBottomMode = BottomMode::IMAGE;
		sBottomImage = image;
	}

	// The usual content in the game: Level background (or the level without sprites, depending on the "bottom=" setting)
	static void setBottomDefault()
	{
		sBottomMode = BottomMode::BACKGROUND;
		sCaptureAllPlanes = config().mBottomScene;
		sCaptureBackgroundSprites = false;
		sCaptureNoPlanes = false;
		sCaptureQueueLimit = 0x2000;
	}

	// Title screen and main menu: The whole scene without sprites, including custom background sprites
	static void setBottomScene(uint16 queueLimit = 0x2000)
	{
		sBottomMode = BottomMode::BACKGROUND;
		sCaptureAllPlanes = true;
		sCaptureBackgroundSprites = true;
		sCaptureNoPlanes = false;
		sCaptureQueueLimit = queueLimit;
	}

	// Title screen: Only the background sprites (sky and ocean), without Sonic and the logo
	static void setBottomTitleBackground()
	{
		sBottomMode = BottomMode::BACKGROUND;
		sCaptureAllPlanes = false;
		sCaptureBackgroundSprites = true;
		sCaptureNoPlanes = true;
		sCaptureQueueLimit = 0x2000;
	}

	// Used by scripts via "Game.setBottomScreen"
	enum ScriptMode
	{
		SCRIPT_DEFAULT = 0,
		SCRIPT_SCENE   = 1,
		SCRIPT_WHITE   = 2,
		SCRIPT_BLACK   = 3,
		SCRIPT_FOLLOW  = 4,		// One color that follows the color in the top left corner of the top screen, e.g. during fades
		SCRIPT_TITLE   = 5,		// Title screen background without Sonic
		SCRIPT_DATASELECT = 6	// Fixed image: "dataselect_bg.png" (set by the game code)
	};

	static void setBottomFromScript(int mode)
	{
		switch (mode)
		{
			default:
			case SCRIPT_DEFAULT:  setBottomDefault();  break;
			case SCRIPT_SCENE:    setBottomScene();  break;
			case SCRIPT_WHITE:    setBottomSolid(0xffffffff);  break;
			case SCRIPT_BLACK:    setBottomSolid(0xff000000);  break;
			case SCRIPT_FOLLOW:   setBottomSolidFollowingScreen();  break;
			case SCRIPT_TITLE:    setBottomTitleBackground();  break;
		}
	}
};
