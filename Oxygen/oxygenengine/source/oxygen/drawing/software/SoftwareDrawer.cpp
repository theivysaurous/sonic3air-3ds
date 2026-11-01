/*
*	Part of the Oxygen Engine / Sonic 3 A.I.R. software distribution.
*	Copyright (C) 2017-2026 by Eukaryot
*
*	Published under the GNU GPLv3 open source software license, see license.txt
*	or https://www.gnu.org/licenses/gpl-3.0.en.html
*/

#include "oxygen/pch.h"
#include "oxygen/drawing/software/SoftwareDrawer.h"
#include "oxygen/drawing/software/SoftwareDrawerTexture.h"
#include "oxygen/drawing/software/SoftwareRasterizer.h"
#include "oxygen/drawing/software/Blitter.h"
#include "oxygen/drawing/DrawCollection.h"
#include "oxygen/drawing/DrawCommand.h"
#include "oxygen/engine/EngineMain.h"
#include "oxygen/helper/Logging.h"
#include "oxygen/resources/PaletteCollection.h"
#include "oxygen/resources/SpriteCollection.h"

#if defined(PLATFORM_3DS)
	#include <3ds.h>
#endif


#if defined(PLATFORM_3DS)
namespace softwaredrawer
{
	// Direct presenting on the 3DS: One fused pass that converts the intermediate ABGR8888 buffer and writes it, rotated, straight into the 3DS framebuffer
	//  -> Skips SDL's own surface copy (which was a second full pass with cache-unfriendly writes)
	//  -> Optional test settings, read from "video.cfg" in the game folder on the SD card:  "format=24" (24-bit BGR8 instead of 32-bit RGBA8),  "alpha=0" (write alpha 0 instead of 255 in 32-bit mode)
	struct Present3DS
	{
		bool mInitialized = false;
		bool m24Bit = false;
		bool mAlphaZero = false;
		bool mBottomScreen = true;		// Show a blurred, dimmed version of the game screen on the bottom screen ("bottom=0" in video.cfg turns it off)
		int mClearFrames = 3;
		int mBottomCounter = 0;

		void initialize()
		{
			mInitialized = true;
			FILE* file = fopen("video.cfg", "r");
			if (nullptr != file)
			{
				char line[128];
				while (fgets(line, sizeof(line), file))
				{
					if (strncmp(line, "format=24", 9) == 0)
						m24Bit = true;
					else if (strncmp(line, "alpha=0", 7) == 0)
						mAlphaZero = true;
					else if (strncmp(line, "bottom=0", 8) == 0)
						mBottomScreen = false;
				}
				fclose(file);
			}
			if (m24Bit)
				gfxSetScreenFormat(GFX_TOP, GSP_BGR8_OES);
		}

		void present(const Bitmap& source)
		{
			if (!mInitialized)
				initialize();

			u16 fbWidth = 0, fbHeight = 0;		// 240 x 400 for the top screen (the framebuffer is stored rotated)
			u8* fb = gfxGetFramebuffer(GFX_TOP, GFX_LEFT, &fbWidth, &fbHeight);
			const int bytesPerPixel = m24Bit ? 3 : 4;
			const u32 fbSize = (u32)fbWidth * fbHeight * bytesPerPixel;
			if (mClearFrames > 0)
			{
				// Both buffers need to be cleared once, in case a format switch left old data in the unused rows
				memset(fb, 0, fbSize);
				--mClearFrames;
			}

			const int width = std::min<int>(source.getWidth(), fbHeight);
			const int height = std::min<int>(source.getHeight(), fbWidth);
			const int column = fbWidth;
			const uint32 alphaMask = mAlphaZero ? 0xffffff00u : 0xffffffffu;

			// Work in tiles of 8 columns, so that each source cache line is used fully and the destination lines stay in the cache
			for (int x0 = 0; x0 < width; x0 += 8)
			{
				const int n = std::min(8, width - x0);
				for (int y = 0; y < height; ++y)
				{
					const uint32* src = source.getPixelPointer(x0, y);
					const int dstIndex = x0 * column + (column - 1 - y);
					if (m24Bit)
					{
						u8* dst = fb + (size_t)dstIndex * 3;
						for (int i = 0; i < n; ++i)
						{
							const uint32 c = src[i];
							dst[0] = (u8)(c >> 16);		// B
							dst[1] = (u8)(c >> 8);		// G
							dst[2] = (u8)c;				// R
							dst += column * 3;
						}
					}
					else
					{
						uint32* dst = (uint32*)fb + dstIndex;
						for (int i = 0; i < n; ++i)
						{
							dst[i * column] = __builtin_bswap32(src[i]) & alphaMask;
						}
					}
				}
			}

			GSPGPU_FlushDataCache(fb, fbSize);
			gfxScreenSwapBuffers(GFX_TOP, false);

			updateBottomScreen(source);
		}

		// Bottom screen: A soft, dimmed copy of what's on the top screen, so it reflects the current stage or menu
		//  -> Updated only every few frames; the source gets averaged into 40x30 blocks, which are then smoothly scaled up to 320x240
		void updateBottomScreen(const Bitmap& source)
		{
			if (!mBottomScreen || ++mBottomCounter < 20)
				return;
			mBottomCounter = 0;

			const int srcWidth = source.getWidth();
			const int srcHeight = source.getHeight();
			if (srcWidth < 40 || srcHeight < 30)
				return;

			int blocks[30][40][3];
			const int blockW = srcWidth / 40;
			const int blockH = srcHeight / 30;
			const int step = 2;		// Only look at every second pixel in each direction
			for (int by = 0; by < 30; ++by)
			{
				for (int bx = 0; bx < 40; ++bx)
				{
					int sumR = 0, sumG = 0, sumB = 0, count = 0;
					for (int y = by * blockH; y < (by + 1) * blockH; y += step)
					{
						const uint32* row = source.getPixelPointer(bx * blockW, y);
						for (int x = 0; x < blockW; x += step)
						{
							const uint32 c = row[x];
							sumR += c & 0xff;
							sumG += (c >> 8) & 0xff;
							sumB += (c >> 16) & 0xff;
							++count;
						}
					}
					// Dim to about 60%
					blocks[by][bx][0] = sumR * 154 / (count * 256);
					blocks[by][bx][1] = sumG * 154 / (count * 256);
					blocks[by][bx][2] = sumB * 154 / (count * 256);
				}
			}

			u16 fbWidth = 0, fbHeight = 0;		// 240 x 320 for the bottom screen
			u8* fb = gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, &fbWidth, &fbHeight);
			if (gfxGetScreenFormat(GFX_BOTTOM) != GSP_RGBA8_OES || fbWidth != 240 || fbHeight != 320)
				return;
			uint32* dst = (uint32*)fb;

			// Calculate one color per 2x2 output pixels (that's smooth enough for a blurred image, and 4x cheaper)
			for (int x2 = 0; x2 < 160; ++x2)
			{
				// Horizontal interpolation position in 1/16 block units (each block is 8 output pixels wide), taken at the center of the 2x2 output pixels
				const int px = x2 * 4 - 6;
				int bx0 = px >> 4;
				const int fx = px & 15;
				int bx1 = bx0 + 1;
				bx0 = std::max(0, std::min(39, bx0));
				bx1 = std::max(0, std::min(39, bx1));
				for (int y2 = 0; y2 < 120; ++y2)
				{
					const int py = y2 * 4 - 6;
					int by0 = py >> 4;
					const int fy = py & 15;
					int by1 = by0 + 1;
					by0 = std::max(0, std::min(29, by0));
					by1 = std::max(0, std::min(29, by1));

					uint32 channel[3];
					for (int k = 0; k < 3; ++k)
					{
						const int top    = blocks[by0][bx0][k] * (16 - fx) + blocks[by0][bx1][k] * fx;
						const int bottom = blocks[by1][bx0][k] * (16 - fx) + blocks[by1][bx1][k] * fx;
						channel[k] = (uint32)((top * (16 - fy) + bottom * fy) >> 8);
					}
					const uint32 color = (channel[0] << 24) | (channel[1] << 16) | (channel[2] << 8) | 0xff;
					uint32* d = dst + (x2 * 2) * 240 + (239 - y2 * 2);
					d[0] = color;			// (x, y)
					d[-1] = color;			// (x, y+1)
					d[240] = color;			// (x+1, y)
					d[239] = color;			// (x+1, y+1)
				}
			}

			GSPGPU_FlushDataCache(fb, 240 * 320 * 4);
			gfxScreenSwapBuffers(GFX_BOTTOM, false);
		}
	};
}
#endif


namespace softwaredrawer
{
	const char* getSDLPixelFormatText(uint32 format)
	{
		switch (format)
		{
			case SDL_PIXELFORMAT_INDEX1LSB:		return "SDL_PIXELFORMAT_INDEX1LSB";
			case SDL_PIXELFORMAT_INDEX1MSB:		return "SDL_PIXELFORMAT_INDEX1MSB";
			case SDL_PIXELFORMAT_INDEX4LSB:		return "SDL_PIXELFORMAT_INDEX4LSB";
			case SDL_PIXELFORMAT_INDEX4MSB:		return "SDL_PIXELFORMAT_INDEX4MSB";
			case SDL_PIXELFORMAT_INDEX8:		return "SDL_PIXELFORMAT_INDEX8";
			case SDL_PIXELFORMAT_RGB332:		return "SDL_PIXELFORMAT_RGB332";
		#ifdef RMX_USE_SDL3
			case SDL_PIXELFORMAT_XRGB4444:		return "SDL_PIXELFORMAT_XRGB4444";
			case SDL_PIXELFORMAT_XRGB1555:		return "SDL_PIXELFORMAT_XRGB1555";
			case SDL_PIXELFORMAT_XBGR1555:		return "SDL_PIXELFORMAT_XBGR1555";
		#else
			case SDL_PIXELFORMAT_RGB444:		return "SDL_PIXELFORMAT_RGB444";
			case SDL_PIXELFORMAT_RGB555:		return "SDL_PIXELFORMAT_RGB555";
			case SDL_PIXELFORMAT_BGR555:		return "SDL_PIXELFORMAT_BGR555";
		#endif
			case SDL_PIXELFORMAT_ARGB4444:		return "SDL_PIXELFORMAT_ARGB4444";
			case SDL_PIXELFORMAT_RGBA4444:		return "SDL_PIXELFORMAT_RGBA4444";
			case SDL_PIXELFORMAT_ABGR4444:		return "SDL_PIXELFORMAT_ABGR4444";
			case SDL_PIXELFORMAT_BGRA4444:		return "SDL_PIXELFORMAT_BGRA4444";
			case SDL_PIXELFORMAT_ARGB1555:		return "SDL_PIXELFORMAT_ARGB1555";
			case SDL_PIXELFORMAT_RGBA5551:		return "SDL_PIXELFORMAT_RGBA5551";
			case SDL_PIXELFORMAT_ABGR1555:		return "SDL_PIXELFORMAT_ABGR1555";
			case SDL_PIXELFORMAT_BGRA5551:		return "SDL_PIXELFORMAT_BGRA5551";
			case SDL_PIXELFORMAT_RGB565:		return "SDL_PIXELFORMAT_RGB565";
			case SDL_PIXELFORMAT_BGR565:		return "SDL_PIXELFORMAT_BGR565";
			case SDL_PIXELFORMAT_RGB24:			return "SDL_PIXELFORMAT_RGB24";
			case SDL_PIXELFORMAT_BGR24:			return "SDL_PIXELFORMAT_BGR24";
		#ifdef RMX_USE_SDL3
			case SDL_PIXELFORMAT_XRGB8888:		return "SDL_PIXELFORMAT_XRGB8888";
		#else
			case SDL_PIXELFORMAT_RGB888:		return "SDL_PIXELFORMAT_RGB888";
		#endif
			case SDL_PIXELFORMAT_RGBX8888:		return "SDL_PIXELFORMAT_RGBX88881";
		#ifdef RMX_USE_SDL3
			case SDL_PIXELFORMAT_XBGR8888:		return "SDL_PIXELFORMAT_XBGR8888";
		#else
			case SDL_PIXELFORMAT_BGR888:		return "SDL_PIXELFORMAT_BGR888";
		#endif
			case SDL_PIXELFORMAT_BGRX8888:		return "SDL_PIXELFORMAT_BGRX8888";
			case SDL_PIXELFORMAT_ARGB8888:		return "SDL_PIXELFORMAT_ARGB8888";
			case SDL_PIXELFORMAT_RGBA8888:		return "SDL_PIXELFORMAT_RGBA8888";
			case SDL_PIXELFORMAT_ABGR8888:		return "SDL_PIXELFORMAT_ABGR8888";
			case SDL_PIXELFORMAT_BGRA8888:		return "SDL_PIXELFORMAT_BGRA8888";
			case SDL_PIXELFORMAT_ARGB2101010:	return "SDL_PIXELFORMAT_ARGB2101010";
			case SDL_PIXELFORMAT_YV12:			return "SDL_PIXELFORMAT_YV12";
			case SDL_PIXELFORMAT_IYUV:			return "SDL_PIXELFORMAT_IYUV";
			case SDL_PIXELFORMAT_YUY2:			return "SDL_PIXELFORMAT_YUY2";
			case SDL_PIXELFORMAT_UYVY:			return "SDL_PIXELFORMAT_UYVY";
			case SDL_PIXELFORMAT_YVYU:			return "SDL_PIXELFORMAT_YVYU";
			case SDL_PIXELFORMAT_NV12:			return "SDL_PIXELFORMAT_NV12";
			case SDL_PIXELFORMAT_NV21:			return "SDL_PIXELFORMAT_NV21";
		}
		return "<unresolved>";
	}

	void applyTintToBitmap(Bitmap& destBitmap, const Color& tintColor)
	{
		const uint16 multiplicators[4] =
		{
			(uint16)roundToInt(tintColor.r * 256.0f),
			(uint16)roundToInt(tintColor.g * 256.0f),
			(uint16)roundToInt(tintColor.b * 256.0f),
			(uint16)roundToInt(tintColor.a * 256.0f)
		};

		const uint32 numPixels = destBitmap.getPixelCount();
		uint8* dst = (uint8*)destBitmap.getData();

		for (uint32 i = 0; i < numPixels; ++i)
		{
			dst[0] = (dst[0] * multiplicators[0]) >> 8;
			dst[1] = (dst[1] * multiplicators[1]) >> 8;
			dst[2] = (dst[2] * multiplicators[2]) >> 8;
			dst[3] = (dst[3] * multiplicators[3]) >> 8;
			dst += 4;
		}
	}

	void mirrorBitmapX(Bitmap& destBitmap, int& destReservedSize, const Bitmap& sourceBitmap)
	{
		destBitmap.createReusingMemory(sourceBitmap.getSize(), destReservedSize);
		for (int y = 0; y < sourceBitmap.getHeight(); ++y)
		{
			const uint32* src = sourceBitmap.getPixelPointer(sourceBitmap.getWidth() - 1, y);
			uint32* dst = destBitmap.getPixelPointer(0, y);

			for (int x = 0; x < sourceBitmap.getWidth(); ++x)
			{
				*dst = *src;
				--src;
				++dst;
			}
		}
	}


	struct Internal
	{
	public:
		Internal()
		{
		}

		~Internal()
		{
		}

		void setupScreenSurface(SDL_Window* window)
		{
			mOutputWindow = window;
			mSurfaceReverseBytes = false;

			mScreenSurface = SDL_GetWindowSurface(window);
			RMX_CHECK(nullptr != mScreenSurface, "Could not get SDL screen surface", return);

			bool formatSupported = false;
		#ifdef RMX_USE_SDL3
			switch (mScreenSurface->format)
		#else
			switch (mScreenSurface->format->format)
		#endif
			{
			#ifdef RMX_USE_SDL3
				case SDL_PIXELFORMAT_XRGB8888:		// Used in my Windows 10
			#else
				case SDL_PIXELFORMAT_RGB888:
			#endif
				case SDL_PIXELFORMAT_ARGB8888:		// Used in my Linux Mint
				{
					formatSupported = true;
					mSurfaceSwapRedBlue = true;
					break;
				}

			#ifdef RMX_USE_SDL3
				case SDL_PIXELFORMAT_XBGR8888:
			#else
				case SDL_PIXELFORMAT_BGR888:
			#endif
				case SDL_PIXELFORMAT_ABGR8888:
				{
					formatSupported = true;
					mSurfaceSwapRedBlue = false;
					break;
				}

			#ifndef RMX_USE_SDL3
				case SDL_PIXELFORMAT_RGBA8888:		// Used on Nintendo 3DS (framebuffer format GSP_RGBA8_OES)
				{
					// Draw into an intermediate buffer in our native format (ABGR8888), then reverse the byte order of each pixel when presenting
					formatSupported = true;
					mSurfaceSwapRedBlue = false;
					mSurfaceReverseBytes = true;
					break;
				}
			#endif
			}

			if (formatSupported)
			{
				// Success, format is supported
				mScissorRect.set(0, 0, mScreenSurface->w, mScreenSurface->h);
			}
			else
			{
				// Fallback when not able to use the SDL surface directly
				if (!mDisplayedFormatWarning)
				{
				#ifdef RMX_USE_SDL3
					RMX_ERROR("Unsupported screen surface format " << getSDLPixelFormatText(mScreenSurface->format) << " (" << rmx::hexString(mScreenSurface->format, 8) << ")", );
				#else
					RMX_ERROR("Unsupported screen surface format " << getSDLPixelFormatText(mScreenSurface->format->format) << " (" << rmx::hexString(mScreenSurface->format->format, 8) << ")", );
				#endif
					mDisplayedFormatWarning = true;
				}

				mScreenSurface = nullptr;
				mOutputWrapper = BitmapViewMutable<uint32>();
			}
		}

		SoftwareDrawerTexture* createTexture(DrawerTexture& outTexture)
		{
			SoftwareDrawerTexture* texture = new SoftwareDrawerTexture(outTexture);
			return texture;
		}

		void setCurrentRenderTarget(DrawerTexture* renderTarget)
		{
			RMX_ASSERT(mScissorStack.empty(), "Changed render targets with non-empty scissor stack");
			if (nullptr == mCurrentRenderTarget)
			{
				unlockScreenSurface();
			}

			mCurrentRenderTarget = renderTarget;

			if (nullptr == mCurrentRenderTarget)
			{
				// Next call to "getOutputWrapper" will update the output wrapper accordingly
				mScissorRect.set(0, 0, mScreenSurface->w, mScreenSurface->h);
			}
			else
			{
				mOutputWrapper = BitmapViewMutable<uint32>(mCurrentRenderTarget->accessBitmap());
				mScissorRect.set(Vec2i(), mCurrentRenderTarget->getSize());
			}
		}

		DrawerTexture* getCurrentRenderTarget() const
		{
			return mCurrentRenderTarget;
		}

		BitmapViewMutable<uint32>& getOutputWrapper()
		{
			if (nullptr == mCurrentRenderTarget)
			{
				if (nullptr != mScreenSurface)
				{
					if (!mIsScreenSurfaceLocked)
					{
						if (mSurfaceReverseBytes)
						{
							// Not touching the SDL surface until presenting, instead draw into the intermediate buffer
							if (mIntermediateBuffer.getWidth() != mScreenSurface->w || mIntermediateBuffer.getHeight() != mScreenSurface->h)
								mIntermediateBuffer.create(mScreenSurface->w, mScreenSurface->h, 0xff000000);
							mIsScreenSurfaceLocked = true;
							mOutputWrapper = BitmapViewMutable<uint32>(mIntermediateBuffer.getData(), Vec2i(mScreenSurface->w, mScreenSurface->h));
						}
						else
						{
							SDL_LockSurface(mScreenSurface);
							mIsScreenSurfaceLocked = true;
							mOutputWrapper = BitmapViewMutable<uint32>((uint32*)mScreenSurface->pixels, Vec2i(mScreenSurface->w, mScreenSurface->h));
						}
					}
				}
				else
				{
					// Handle invalid screen surface
					mOutputWrapper = BitmapViewMutable<uint32>();
				}
			}
			return mOutputWrapper;
		}

		bool needSwapRedBlueChannels()
		{
			return (nullptr == mCurrentRenderTarget && mSurfaceSwapRedBlue);
		}

		void swapRedBlueChannels(uint32* dst, const uint32* src, int numPixels)
		{
			// Copy over data and swap red and blue channels
			//  -> Note that input and output may be identical, or must not overlap otherwise
			int k = 0;
			if constexpr (sizeof(void*) == 8)
			{
			#if !defined(PLATFORM_VITA)
				// On 64-bit architectures: Process 2 pixels at once
				for (; k < numPixels; k += 2)
				{
					const uint64 colors = *(uint64*)&src[k];
					*(uint64*)&dst[k] = ((colors & 0x00ff000000ff0000ull) >> 16) | (colors & 0xff00ff00ff00ff00ull) | ((colors & 0x000000ff000000ffull) << 16);
				}
			#endif
			}
			// Process single pixels
			for (; k < numPixels; ++k)
			{
				const uint32 color = src[k];
				dst[k] = ((color & 0x00ff0000) >> 16) | (color & 0xff00ff00) | ((color & 0x000000ff) << 16);
			}
		}

		void setupRedBlueSwappedBitmapWrapper(BitmapViewMutable<uint32>& bitmapWrapper)
		{
			// Setup temp buffer with the right size
			mTempBuffer.createReusingMemory(bitmapWrapper.getSize().x, bitmapWrapper.getSize().y, mTempReservedSize);

			// Copy over data and swap red and blue channels
			swapRedBlueChannels(mTempBuffer.getData(), bitmapWrapper.getData(), mTempBuffer.getPixelCount());

			// Use the temp buffer as replacement for the bitmap wrapper
			bitmapWrapper = mTempBuffer;
		}

		void unlockScreenSurface()
		{
			if (nullptr != mScreenSurface && mIsScreenSurfaceLocked)
			{
			#if defined(PLATFORM_3DS)
				if (mSurfaceReverseBytes)
				{
					// Nothing to do here, the intermediate buffer gets converted and written to the 3DS framebuffer in one go when presenting
				}
				else
			#endif
				if (mSurfaceReverseBytes)
				{
					// Convert from ABGR8888 (intermediate buffer) to RGBA8888 (SDL surface)
					SDL_LockSurface(mScreenSurface);
					const int width = std::min(mScreenSurface->w, mIntermediateBuffer.getWidth());
					const int height = std::min(mScreenSurface->h, mIntermediateBuffer.getHeight());
					for (int y = 0; y < height; ++y)
					{
						const uint32* src = mIntermediateBuffer.getPixelPointer(0, y);
						uint32* dst = (uint32*)((uint8*)mScreenSurface->pixels + y * mScreenSurface->pitch);
						for (int x = 0; x < width; ++x)
						{
							const uint32 color = src[x];
							dst[x] = (color >> 24) | ((color >> 8) & 0x0000ff00) | ((color << 8) & 0x00ff0000) | (color << 24);
						}
					}
					SDL_UnlockSurface(mScreenSurface);
				}
				else
				{
					SDL_UnlockSurface(mScreenSurface);
				}
				mIsScreenSurfaceLocked = false;
			}
		}

		const Recti& getScissorRect() const
		{
			return mScissorRect;
		}

		bool useAlphaBlending() const
		{
			return (mCurrentBlendMode == BlendMode::ALPHA);
		}

		void drawRect(Recti targetRect, Bitmap* inputBitmap, Color color, Vec2f uv0 = Vec2f(0.0f, 0.0f), Vec2f uv1 = Vec2f(1.0f, 1.0f))
		{
			// Target rect to fill in the output
			bool mirrorX = false;
			if (targetRect.width < 0)
			{
				// Mirror horizontally
				mirrorX = true;
				targetRect.x += targetRect.width;
				targetRect.width = -targetRect.width;
			}
			const Recti uncroppedRect = targetRect;
			targetRect.intersect(targetRect, getScissorRect());

			if (targetRect.empty())
				return;

			if (nullptr != inputBitmap)
			{
				// Consider mirroring
				if (mirrorX)
				{
					softwaredrawer::mirrorBitmapX(mTempBuffer, mTempReservedSize, *inputBitmap);
					inputBitmap = &mTempBuffer;
				}

				BitmapViewMutable<uint32> inputWrapper(*inputBitmap);

				const Vec2f originalUVStart = uv0;
				const Vec2f originalUVRange = uv1 - uv0;
				Recti inputRect;
				bool useUVs = false;
				{
					// Calculate actual UVs that also take cropping into account
					if (targetRect != uncroppedRect)
					{
						const Vec2f relativeStart = Vec2f(targetRect.getPos() - uncroppedRect.getPos()) / Vec2f(uncroppedRect.getSize());
						const Vec2f relativeEnd   = Vec2f(targetRect.getPos() - uncroppedRect.getPos() + targetRect.getSize()) / Vec2f(uncroppedRect.getSize());
						uv0 = originalUVStart + relativeStart * originalUVRange;
						uv1 = originalUVStart + relativeEnd * originalUVRange;
					}

					useUVs = (uv0.x < 0.0f || uv0.x > uv1.x || uv1.x > 1.0f || uv0.y < 0.0f || uv0.y > uv1.y || uv1.y > 1.0f);

					// Get the part from the input that will get drawn
					const Vec2f inputStart = uv0 * Vec2f(inputWrapper.getSize());
					const Vec2f inputEnd = uv1 * Vec2f(inputWrapper.getSize());
					inputRect.x = roundToInt(inputStart.x);
					inputRect.y = roundToInt(inputStart.y);
					inputRect.width = roundToInt(inputEnd.x) - inputRect.x;
					inputRect.height = roundToInt(inputEnd.y) - inputRect.y;
				}

				if (useUVs || targetRect.getSize() != inputRect.getSize())
				{
					if (needSwapRedBlueChannels())
					{
						setupRedBlueSwappedBitmapWrapper(inputWrapper);
					}

					Blitter::Options options;
					options.mBlendMode = useAlphaBlending() ? BlendMode::ALPHA : BlendMode::OPAQUE;
					options.mTintColor = (color == Color::WHITE) ? nullptr : &color;

					if (useUVs)
					{
						mBlitter.blitRectWithUVs(getOutputWrapper(), targetRect, inputWrapper, inputRect, options);
					}
					else
					{
						mBlitter.blitRectWithScaling(getOutputWrapper(), targetRect, inputWrapper, inputRect, options);
					}
				}
				else
				{
					// Get the part from the input that will get drawn
					//  -> Calculated here again, as we need to use the original UVs in this case
					const Vec2f inputStart = originalUVStart * Vec2f(inputWrapper.getSize());
					const Vec2f inputEnd = (originalUVStart + originalUVRange) * Vec2f(inputWrapper.getSize());
					inputRect.x = roundToInt(inputStart.x);
					inputRect.y = roundToInt(inputStart.y);
					inputRect.width = roundToInt(inputEnd.x) - inputRect.x;
					inputRect.height = roundToInt(inputEnd.y) - inputRect.y;

					Blitter::Options blitterOptions;
					blitterOptions.mBlendMode = useAlphaBlending() ? BlendMode::ALPHA : BlendMode::OPAQUE;
					blitterOptions.mTintColor = (color != Color::WHITE) ? &color : nullptr;
					blitterOptions.mSwapRedBlueChannels = needSwapRedBlueChannels();

					mBlitter.blitSprite(Blitter::OutputWrapper(getOutputWrapper(), getScissorRect()), Blitter::SpriteWrapper(inputWrapper.getData(), inputWrapper.getSize(), Vec2i(), inputRect), uncroppedRect.getPos() - inputRect.getPos(), blitterOptions);
				}
			}
			else
			{
				if (needSwapRedBlueChannels())
					color.swapRedBlue();

				const BitmapViewMutable<uint32> outputView(getOutputWrapper(), targetRect);
				mBlitter.blitColor(outputView, color, useAlphaBlending() ? BlendMode::ALPHA : BlendMode::OPAQUE);
			}
		}

		void drawIndexed(Vec2i position, PaletteBitmap& inputBitmap, const PaletteBase& palette, Color color)
		{
			const Blitter::OutputWrapper outputWrapper(getOutputWrapper(), getScissorRect());
			const Blitter::IndexedSpriteWrapper inputWrapper(inputBitmap.getData(), inputBitmap.getSize(), Vec2i());
			const Blitter::PaletteWrapper paletteWrapper(palette.getRawColors(), palette.getSize());

			Blitter::Options blitterOptions;
			blitterOptions.mBlendMode = useAlphaBlending() ? BlendMode::ALPHA : BlendMode::OPAQUE;
			blitterOptions.mTintColor = (color != Color::WHITE) ? &color : nullptr;
			blitterOptions.mSwapRedBlueChannels = needSwapRedBlueChannels();

			mBlitter.blitIndexed(outputWrapper, inputWrapper, paletteWrapper, position, blitterOptions);
		}

		void printText(Font& font, const StringReader& text, const Recti& rect, const DrawerPrintOptions& printOptions)
		{
			Bitmap& bufferBitmap = mTempBuffer;

			Vec2i drawPosition;
			font.printBitmap(bufferBitmap, drawPosition, rect, text, printOptions.mAlignment, printOptions.mSpacing, &mTempReservedSize);

			Blitter::Options blitterOptions;
			blitterOptions.mBlendMode = BlendMode::ALPHA;
			blitterOptions.mTintColor = (printOptions.mTintColor != Color::WHITE) ? &printOptions.mTintColor : nullptr;
			blitterOptions.mSwapRedBlueChannels = needSwapRedBlueChannels();
			mBlitter.blitSprite(Blitter::OutputWrapper(getOutputWrapper(), getScissorRect()), Blitter::SpriteWrapper(bufferBitmap, Vec2i()), drawPosition, blitterOptions);
		}

	public:
		SDL_Window* mOutputWindow = nullptr;
		SDL_Surface* mScreenSurface = nullptr;
		bool mSurfaceSwapRedBlue = false;
		bool mSurfaceReverseBytes = false;		// True if the surface is RGBA8888 and we need an intermediate buffer in ABGR8888
		Bitmap mIntermediateBuffer;
	#if defined(PLATFORM_3DS)
		Present3DS mPresent3DS;
	#endif

		BlendMode mCurrentBlendMode = BlendMode::OPAQUE;
		SamplingMode mCurrentSamplingMode = SamplingMode::POINT;
		TextureWrapMode mCurrentWrapMode = TextureWrapMode::CLAMP;

		Recti mScissorRect;
		std::vector<Recti> mScissorStack;

		Blitter mBlitter;
		Bitmap mTempBuffer;
		int mTempReservedSize = 0;

	private:
		DrawerTexture* mCurrentRenderTarget = nullptr;
		BitmapViewMutable<uint32> mOutputWrapper;
		bool mIsScreenSurfaceLocked = false;
		bool mDisplayedFormatWarning = false;
	};
}


SoftwareDrawer::SoftwareDrawer() :
	mInternal(*new softwaredrawer::Internal())
{
}

SoftwareDrawer::~SoftwareDrawer()
{
	delete &mInternal;
}

void SoftwareDrawer::createTexture(DrawerTexture& outTexture)
{
	outTexture.setImplementation(mInternal.createTexture(outTexture));
}

void SoftwareDrawer::refreshTexture(DrawerTexture& texture)
{
	createTexture(texture);
}

void SoftwareDrawer::setupRenderWindow(SDL_Window* window)
{
	mInternal.setupScreenSurface(window);
}

void SoftwareDrawer::performRendering(const DrawCollection& drawCollection)
{
	for (DrawCommand* drawCommand : drawCollection.getDrawCommands())
	{
		switch (drawCommand->getType())
		{
			case DrawCommand::Type::UNDEFINED:
			{
				RMX_ERROR("Got invalid draw command", );
				continue;
			}

			case DrawCommand::Type::SET_WINDOW_RENDER_TARGET:
			{
				//SetWindowRenderTargetDrawCommand& dc = drawCommand->as<SetWindowRenderTargetDrawCommand>();
				if (nullptr != mInternal.getCurrentRenderTarget())
				{
					mInternal.getCurrentRenderTarget()->bitmapUpdated();
					mInternal.setCurrentRenderTarget(nullptr);
				}
				break;
			}

			case DrawCommand::Type::SET_RENDER_TARGET:
			{
				SetRenderTargetDrawCommand& dc = drawCommand->as<SetRenderTargetDrawCommand>();
				if (nullptr != mInternal.getCurrentRenderTarget())
				{
					mInternal.getCurrentRenderTarget()->bitmapUpdated();
				}
				mInternal.setCurrentRenderTarget(dc.mTexture);
				break;
			}

			case DrawCommand::Type::RECT:
			{
				RectDrawCommand& dc = drawCommand->as<RectDrawCommand>();
				Bitmap* inputBitmap = (nullptr == dc.mTexture) ? nullptr : &dc.mTexture->accessBitmap();

				mInternal.drawRect(dc.mRect, inputBitmap, dc.mColor, dc.mUV0, dc.mUV1);
				break;
			}

			case DrawCommand::Type::UPSCALED_RECT:
			{
				UpscaledRectDrawCommand& dc = drawCommand->as<UpscaledRectDrawCommand>();
				if (nullptr != dc.mTexture)
				{
					BitmapViewMutable<uint32>& outputWrapper = mInternal.getOutputWrapper();
					BitmapViewMutable<uint32> inputWrapper(dc.mTexture->accessBitmap());

					if (mInternal.needSwapRedBlueChannels())
					{
						mInternal.setupRedBlueSwappedBitmapWrapper(inputWrapper);
					}

					mInternal.mBlitter.blitRectWithScaling(outputWrapper, dc.mRect, inputWrapper, Recti(0, 0, inputWrapper.getSize().x, inputWrapper.getSize().y), Blitter::Options());
				}
				break;
			}

			case DrawCommand::Type::SPRITE:
			{
				SpriteDrawCommand& sc = drawCommand->as<SpriteDrawCommand>();
				const SpriteCollection::Item* item = SpriteCollection::instance().getSprite(sc.mSpriteKey);
				if (nullptr == item)
					break;

				const PaletteBase* palette = nullptr;
				if (!item->mUsesComponentSprite)
				{
					palette = PaletteCollection::instance().getPalette(sc.mPaletteKey, 0);
					if (nullptr == palette)
						break;
				}

				SpriteBase& sprite = *item->mSprite;
				Vec2i offset = sprite.mOffset;

				// TODO: No support for bilinear sampling here...

				if (item->mUsesComponentSprite)
				{
					Vec2i size = sprite.getSize();
					if (sc.mScale.x != 1.0f || sc.mScale.y != 1.0f)
					{
						offset.x = roundToInt((float)offset.x * sc.mScale.x);
						offset.y = roundToInt((float)offset.y * sc.mScale.y);
						size.x = roundToInt((float)size.x * sc.mScale.x);
						size.y = roundToInt((float)size.y * sc.mScale.y);
					}
					const Recti targetRect(sc.mPosition + offset, size);

					ComponentSprite& componentSprite = static_cast<ComponentSprite&>(sprite);
					mInternal.drawRect(targetRect, &componentSprite.accessBitmap(), sc.mTintColor);
				}
				else
				{
					// TODO: Support scaling here as well

					PaletteSprite& paletteSprite = static_cast<PaletteSprite&>(sprite);
					mInternal.drawIndexed(sc.mPosition + offset, paletteSprite.accessBitmap(), *palette, sc.mTintColor);
				}
				break;
			}

			case DrawCommand::Type::SPRITE_RECT:
			{
				SpriteRectDrawCommand& sc = drawCommand->as<SpriteRectDrawCommand>();
				const SpriteCollection::Item* item = SpriteCollection::instance().getSprite(sc.mSpriteKey);
				if (nullptr == item)
					break;
				if (!item->mUsesComponentSprite)
					break;

				ComponentSprite& sprite = *static_cast<ComponentSprite*>(item->mSprite);

				// TODO: No support for bilinear sampling here...

				mInternal.drawRect(sc.mRect, &sprite.accessBitmap(), sc.mTintColor);
				break;
			}

			case DrawCommand::Type::MESH:
			{
				MeshDrawCommand& dc = drawCommand->as<MeshDrawCommand>();
				if (nullptr != dc.mTexture)
				{
					BitmapViewMutable<uint32> outputView(mInternal.getOutputWrapper().getData(), mInternal.getOutputWrapper().getSize());
					Bitmap& inputBitmap = dc.mTexture->accessBitmap();

					Blitter::Options options;
					options.mBlendMode = mInternal.useAlphaBlending() ? BlendMode::ALPHA : BlendMode::OPAQUE;
					options.mSamplingMode = (mInternal.mCurrentSamplingMode == SamplingMode::BILINEAR) ? SamplingMode::BILINEAR : SamplingMode::POINT;
					// Note that this does not support red-blue channel swap

					SoftwareRasterizer rasterizer(outputView, options);
					SoftwareRasterizer::Vertex triangle[3];

					const int numTriangles = (int)dc.mTriangles.size() / 3;
					for (int i = 0; i < numTriangles; ++i)
					{
						DrawerMeshVertex* input = &dc.mTriangles[i * 3];
						for (int k = 0; k < 3; ++k)
						{
							triangle[k].mPosition = input[k].mPosition;
							triangle[k].mUV = input[k].mTexcoords;
						}
						rasterizer.drawTriangle(triangle, inputBitmap, false);
					}
				}
				break;
			}

			case DrawCommand::Type::MESH_VERTEX_COLOR:
			{
				MeshVertexColorDrawCommand& dc = drawCommand->as<MeshVertexColorDrawCommand>();
				BitmapViewMutable<uint32> outputView(mInternal.getOutputWrapper().getData(), mInternal.getOutputWrapper().getSize());

				Blitter::Options options;
				options.mBlendMode = mInternal.useAlphaBlending() ? BlendMode::ALPHA : BlendMode::OPAQUE;

				SoftwareRasterizer rasterizer(outputView, options);
				SoftwareRasterizer::Vertex triangle[3];
				const bool swapRedBlue = mInternal.needSwapRedBlueChannels();

				const int numTriangles = (int)dc.mTriangles.size() / 3;
				for (int i = 0; i < numTriangles; ++i)
				{
					DrawerMeshVertex_P2_C4* input = &dc.mTriangles[i * 3];
					for (int k = 0; k < 3; ++k)
					{
						triangle[k].mPosition = input[k].mPosition;
						triangle[k].mColor = input[k].mColor;
					}
					if (swapRedBlue)
					{
						for (int k = 0; k < 3; ++k)
							triangle[k].mColor.swapRedBlue();
					}
					rasterizer.drawTriangle(triangle);
				}
				break;
			}

			case DrawCommand::Type::SET_BLEND_MODE:
			{
				SetBlendModeDrawCommand& dc = drawCommand->as<SetBlendModeDrawCommand>();
				mInternal.mCurrentBlendMode = dc.mBlendMode;
				break;
			}

			case DrawCommand::Type::SET_SAMPLING_MODE:
			{
				SetSamplingModeDrawCommand& dc = drawCommand->as<SetSamplingModeDrawCommand>();
				mInternal.mCurrentSamplingMode = dc.mSamplingMode;
				break;
			}

			case DrawCommand::Type::SET_WRAP_MODE:
			{
				SetWrapModeDrawCommand& dc = drawCommand->as<SetWrapModeDrawCommand>();
				mInternal.mCurrentWrapMode = dc.mWrapMode;
				break;
			}

			case DrawCommand::Type::PRINT_TEXT:
			{
				PrintTextDrawCommand& dc = drawCommand->as<PrintTextDrawCommand>();
				mInternal.printText(*dc.mFont, dc.mText, dc.mRect, dc.mPrintOptions);
				break;
			}

			case DrawCommand::Type::PRINT_TEXT_W:
			{
				PrintTextWDrawCommand& dc = drawCommand->as<PrintTextWDrawCommand>();
				mInternal.printText(*dc.mFont, dc.mText, dc.mRect, dc.mPrintOptions);
				break;
			}

			case DrawCommand::Type::PUSH_SCISSOR:
			{
				PushScissorDrawCommand& dc = drawCommand->as<PushScissorDrawCommand>();

				mInternal.mScissorRect.intersect(dc.mRect);
				mInternal.mScissorStack.emplace_back(mInternal.mScissorRect);
				break;
			}

			case DrawCommand::Type::POP_SCISSOR:
			{
				mInternal.mScissorStack.pop_back();
				if (mInternal.mScissorStack.empty())
				{
					if (nullptr != mInternal.mScreenSurface)
					{
						mInternal.mScissorRect.set(0, 0, mInternal.mScreenSurface->w, mInternal.mScreenSurface->h);
					}
				}
				else
				{
					mInternal.mScissorRect = mInternal.mScissorStack.back();
				}
				break;
			}
		}
	}
}

void SoftwareDrawer::presentScreen()
{
	if (nullptr == mInternal.mScreenSurface)
		return;

	mInternal.unlockScreenSurface();
#if defined(PLATFORM_3DS)
	if (mInternal.mSurfaceReverseBytes)
	{
		mInternal.mPresent3DS.present(mInternal.mIntermediateBuffer);
		return;
	}
#endif
	SDL_UpdateWindowSurface(mInternal.mOutputWindow);
}

const BitmapViewMutable<uint32>& SoftwareDrawer::getRenderTarget() const
{
	return mInternal.getOutputWrapper();
}

bool SoftwareDrawer::needSwapRedBlueChannels() const
{
	return mInternal.needSwapRedBlueChannels();
}
