/************************************************************************************

PublicHeader:   None
Filename    :   main.cpp
Content     :   Utility functions for manipulating file paths
Created     :   May 8, 2014
Author      :   Jonathan E. Wright
Notes       :

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

************************************************************************************/

#include <stdlib.h>
#include <string>
#include <vector>
#include <algorithm>
#include <iostream>
#include <conio.h>
#include <ft2build.h>           
#include <Include\OVR.h>
#if 0 // JSON file moved into Kernel folder in newer SDKs
#include <Src\OVR_JSON.h>
#else 
#include <Src\Kernel\OVR_JSON.h>
#endif
#include "path_utils.h"
#include <sstream>
#include <gl/GL.h>

static int FNT_FILE_VERSION = 1;

#include FT_FREETYPE_H

#define USED( var__ ) (void)( var__ )

namespace OVR {

#if defined( OVR_CPU_X86_64 )
inline int ftoi( const float f )
{
    return _mm_cvtt_ss2si( _mm_set_ss( f ) );
}
#else
inline int ftoi(float f)
{
    int i;
    __asm
    {
        fld f
        fistp i
    }
    return i;
}
#endif

inline int32_t dtoi( const double d )
{
    return (int32_t)d;
}

static float ClampFloat( float const in, float const min, float const max )
{
	if ( in < min )
	{
		return min;
	}
	else if ( in > max )
	{
		return max;
	}
	return in;
}

#pragma pack( push )
#pragma pack(1)
struct ktxHeader
{
	static uint8_t FileIdentifier[12];

	ktxHeader() :
		endianness( 0x04030201 )
	{
		for ( int i = 0; i < sizeof( identifier ); ++i )
		{
			identifier[i] = FileIdentifier[i];
		}
	}

	uint8_t	identifier[12];
	int32_t	endianness;
	int32_t	glType;
	int32_t	glTypeSize;
	int32_t	glFormat;
	int32_t	glInternalFormat;
	int32_t	glBaseInternalFormat;
	int32_t	pixelWidth;
	int32_t	pixelHeight;
	int32_t	pixelDepth;
	int32_t	numberOfArrayElements;
	int32_t	numberOfFaces;
	int32_t	numberOfMipmapLevels;
	int32_t	bytesOfKeyValueData;
};
#pragma pack( pop )

uint8_t ktxHeader::FileIdentifier[12] = 
{
	0xAB, 0x4B, 0x54, 0x58, 0x20, 0x31, 0x31, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A
};

#define GL_R8 0x8229

static bool WriteKTX_R8( char const * outFileName, uint8_t const * buffer, int32_t const srcWidth, int32_t const srcHeight )
{
	ktxHeader header;
	header.glType = GL_UNSIGNED_BYTE;
	header.glTypeSize = 1;
	header.glFormat = GL_RED;
	header.glInternalFormat = GL_R8;
	header.glBaseInternalFormat = GL_RED;
	header.pixelWidth = srcWidth;
	header.pixelHeight = srcHeight;
	header.pixelDepth = 0;
	header.numberOfArrayElements = 0;
	header.numberOfFaces = 1;
	header.numberOfMipmapLevels = 1;
	header.bytesOfKeyValueData = 0;

	FILE * f = fopen( outFileName, "wb" );
	if ( f == NULL ) 
	{
		return false;
	}

	fwrite( &header, sizeof( ktxHeader ), 1, f );
	
	// write the size of the 1st mip
	uint32_t mipSize = srcWidth * srcHeight;
	fwrite( &mipSize, sizeof( mipSize ), 1, f );

	size_t srcRowSize = srcWidth * 4;
	size_t destRowSize = srcWidth;
	uint8_t * destRowBuffer = new uint8_t[destRowSize];
	for ( int y = 0; y < srcHeight; ++y )
	{
		size_t srcRowIdx = ( y * srcRowSize );
		for ( int x = 0; x < srcWidth; ++x )
		{
			destRowBuffer[x] = buffer[srcRowIdx + ( x * 4 )];
		}
		fwrite( destRowBuffer, destRowSize, 1, f );
	}
	fclose( f );
	
	return true;
}

static bool WriteTGA32( char const * outFileName, uint8_t const * buffer, int32_t const srcWidth, int32_t const srcHeight, 
		int32_t const sx, int32_t const sy, int32_t const dw, int32_t const dh )
{
// this needs special packing because the tga header is not aligned to machine word boundaries
#pragma pack( push )	
#pragma pack( 1)
	struct tgaHeader_t
	{
		tgaHeader_t() :
			Id( 0 ),			// 0 = no image id field
			ColorMapType( 0 ),	// 0 = no color map
			ImageTypeCode( 2 ),	// 2 = Unmapped RGB
			ColorMapOrigin( 0 ),
			ColorMapLength( 0 ),
			ColorMapEntrySize( 0 ),
			XOrigin ( 0 ),
			YOrigin( 0 ),
			Width( 0 ),
			Height( 0 ),
			ImagePixelSize( 32 ),
			ImageDescriptor( 8 | ( 1 << 5 ) ) // 8 attribute bits (bits 0 - 3 ) and upper left-hand origin (bit 5) 
		{
		}

		uint8_t		Id;
		uint8_t		ColorMapType;
		uint8_t		ImageTypeCode;
		uint16_t	ColorMapOrigin;
		uint16_t	ColorMapLength;
		uint8_t		ColorMapEntrySize;
		uint16_t	XOrigin;
		uint16_t	YOrigin;
		uint16_t	Width;
		uint16_t	Height;
		uint8_t		ImagePixelSize;
		uint8_t		ImageDescriptor;
	};
#pragma pack( pop )

	FILE * f = fopen( outFileName, "wb" );
	if ( f == NULL )
	{
		return false;
	}

	int32_t const destWidth = sx + dw > srcWidth ? srcWidth - sx : dw;
	int32_t const destHeight = sy + dh > srcHeight ? srcHeight - sy : dh;
	if ( destWidth > 65535 || destHeight > 65535 )
	{
		return false;
	}

	tgaHeader_t header;
	header.Width = static_cast< uint16_t >( destWidth );
	header.Height = static_cast< uint16_t >( destHeight );

	if ( fwrite( &header, sizeof( tgaHeader_t ), 1, f ) != 1 )
	{
		fclose( f );
		return false;
	}

	// TODO: allocate and write out a single row at a time
	// swap order to BGRA
	size_t bmSizePixels = (size_t)destWidth * (size_t)destHeight;
	size_t bmSizeBytes = bmSizePixels * 4;	// 32 bpp
	uint8_t * bgra = reinterpret_cast< uint8_t* >( OVR_ALLOC_ALIGNED( bmSizeBytes, 16 ) );
		
	for ( size_t y = 0; y < destHeight; ++y )
	{
		size_t srcRowStartIdx = size_t( srcWidth ) * 4 * ( y + sy );
		size_t destRowStartIdx = size_t( destWidth ) * 4 * y;
		for ( size_t x = 0; x < destWidth; ++x )
		{
			uint8_t b = buffer[srcRowStartIdx + ( ( x + sx ) * 4 ) + 0];
			uint8_t g = buffer[srcRowStartIdx + ( ( x + sx ) * 4 ) + 1];
			uint8_t r = buffer[srcRowStartIdx + ( ( x + sx ) * 4 ) + 2];
			uint8_t a = buffer[srcRowStartIdx + ( ( x + sx ) * 4 ) + 3];
			if ( b != 0 || g != 0 || r != 0 || a != 0 )
			{
				int foo = 1;
				foo = foo;
			}
			bgra[destRowStartIdx + ( x * 4 ) + 0] = b;
			bgra[destRowStartIdx + ( x * 4 ) + 1] = g;
			bgra[destRowStartIdx + ( x * 4 ) + 2] = r;
			bgra[destRowStartIdx + ( x * 4 ) + 3] = a;
		}
	}
/*
	for ( size_t i = 0; i < bmSizePixels; ++i )
	{
		bgra[i * 4 + 0] = buffer[i * 4 + 2 ];
		bgra[i * 4 + 1] = buffer[i * 4 + 1 ];
		bgra[i * 4 + 2] = buffer[i * 4 + 0 ];
		bgra[i * 4 + 3] = buffer[i * 4 + 3 ];
	}
*/
	size_t countWritten = fwrite( bgra, bmSizeBytes, 1, f );
	OVR_FREE_ALIGNED( bgra );
	bgra = NULL;

	fclose( f );

	return ( countWritten == 1 );
}

class CBitmap
{
public:
	CBitmap() :
		Buffer( NULL ),
		Width( 0 ),
		Height( 0 ),
		BytesPerPixel( 0 ),
		Pitch( 0 )
	{
	}
	~CBitmap()
	{
		OVR_FREE_ALIGNED( Buffer );
	}

	static bool Create( CBitmap & bm, int32_t const width, int32_t const height, int32_t const bytesPerPixel, bool const fill )
	{
		size_t bmSizeInBytes = (size_t)width * (size_t)height * (size_t)bytesPerPixel;
		OVR_ASSERT( bmSizeInBytes > 0 );
		bm.Buffer = reinterpret_cast< uint8_t* >( OVR_ALLOC_ALIGNED( bmSizeInBytes, 16 ) );
		if ( bm.Buffer == NULL )
		{
			bm.Width = 0;
			bm.Height = 0;
			bm.BytesPerPixel = 0;
			bm.Pitch = 0;
			return false;
		}
		bm.Width = width;
		bm.Height = height;
		bm.BytesPerPixel = bytesPerPixel;
		bm.Pitch = bytesPerPixel * width;
		if ( fill )
		{
			memset( bm.Buffer, 0, bmSizeInBytes );
		}
		return true;
	}

	static CBitmap * Create( int32_t const width, int32_t const height, int32_t bytesPerPixel, bool const fill )
	{
		CBitmap * bm = new CBitmap;
		if ( bm == NULL )
		{
			return false;
		}
		if ( !Create( *bm, width, height, bytesPerPixel, fill ) )
		{
			delete bm;
			return NULL;
		}
		return bm;
	}

	void CopyInto( CBitmap & other, int32_t const x, int32_t const y  );

	void SetPixelRGBA( int32_t const x, int32_t const y, uint32_t rgba )
	{
		size_t ofs = ( size_t( y ) * size_t( Pitch ) ) + ( size_t( x ) * BytesPerPixel );
		Buffer[ofs + 0] = rgba >> 24;
		Buffer[ofs + 1] = ( rgba >> 16 ) & 0xFF;
		Buffer[ofs + 2] = ( rgba >> 8 ) & 0xFF;
		Buffer[ofs + 3] = ( rgba & 0xFF );
	}

	void SetPixelRGBA( int32_t const x, int32_t const y, uint8_t const r, uint8_t const g, uint8_t const b, uint8_t const a )
	{
		size_t ofs = ( size_t( y ) * size_t( Pitch ) ) + ( size_t( x ) * BytesPerPixel );
		Buffer[ofs + 0] = r;
		Buffer[ofs + 1] = g;
		Buffer[ofs + 2] = b;
		Buffer[ofs + 3] = a;
	}

	uint32_t GetPixelRGBA( int32_t x, int32_t y ) const
	{
		if ( x < 0 ) { x = 0; } else if ( x >= Width ) { x = Width - 1; }
		if ( y < 0 ) { y = 0; } else if ( y >= Height ) { y = Height - 1; }
		size_t ofs = ( size_t( y ) * size_t( Pitch ) ) + ( size_t( x ) * BytesPerPixel );
		return uint32_t ( ( Buffer[ofs + 0 ] << 24 ) | ( Buffer[ofs + 1 ] << 16 ) | ( Buffer[ofs + 2] << 8 ) | Buffer[ofs + 3] );
	}

	uint32_t GetPixelRGB( int32_t x, int32_t y ) const
	{
		if ( x < 0 ) { x = 0; } else if ( x >= Width ) { x = Width - 1; }
		if ( y < 0 ) { y = 0; } else if ( y >= Height ) { y = Height - 1; }
		size_t ofs = ( size_t( y ) * size_t( Pitch ) ) + ( size_t( x ) * BytesPerPixel );
		return uint32_t ( ( Buffer[ofs + 0 ] << 24 ) | ( Buffer[ofs + 1 ] << 16 ) | ( Buffer[ofs + 2] << 8 ) );
	}

	uint32_t SamplePixelRGBA( float const u, float const v )
	{
		float const u1 = ( u * Width );
		float const v1 = ( v * Height );
		int const x = static_cast< int >( floor( u1 ) );
		int const y = static_cast< int >( floor( v1 ) );
		float const uFrac = u1 - x;
		float const vFrac = v1 - y;
		float const invuFrac = 1.0f - uFrac;
		float const invvFrac = 1.0f - vFrac;
		uint32_t s0 = GetPixelRGBA( x, y );
		uint32_t s1 = GetPixelRGBA( x + 1, y );
		uint32_t s2 = GetPixelRGBA( x, y + 1 );
		uint32_t s3 = GetPixelRGBA( x + 1, y + 1 );
		float r = ( ( s0 >> 24 ) * uFrac + ( s1 >> 24 ) * invuFrac ) * invvFrac + 
			      ( ( s2 >> 24 ) * uFrac + ( s3 >> 24 ) * invuFrac ) * vFrac;
		float g = ( ( ( s0 >> 16 ) & 0xFF ) * uFrac + ( ( s1 >> 16 ) & 0xFF ) * invuFrac ) * invvFrac + 
			      ( ( ( s2 >> 16 ) & 0xFF ) * uFrac + ( ( s3 >> 16 ) & 0xFF ) * invuFrac ) * vFrac;
		float b = ( ( ( s0 >> 8 ) & 0xFF ) * uFrac + ( ( s1 >> 8 ) & 0xFF ) * invuFrac ) * invvFrac + 
			      ( ( ( s2 >> 8 ) & 0xFF ) * uFrac + ( ( s3 >> 8 ) & 0xFF ) * invuFrac ) * vFrac;
		float a = ( ( s0 & 0xFF ) * uFrac + ( s1 & 0xFF ) * invuFrac ) * invvFrac + 
			      ( ( s2 & 0xFF ) * uFrac + ( s3 & 0xFF ) * invuFrac ) * vFrac;
		uint32_t p = ( (int)r << 24 ) | ( (int)g << 16 ) | ( (int)b << 8 ) | (int)a;
		return p;
	}

	uint32_t SamplePixelRGB( float const u, float const v )
	{
		float const u1 = ( u * Width );
		float const v1 = ( v * Height );
		int const x = static_cast< int >( floor( u1 ) );
		int const y = static_cast< int >( floor( v1 ) );
		float const uFrac = u1 - x;
		float const vFrac = v1 - y;
		float const invuFrac = 1.0f - uFrac;
		float const invvFrac = 1.0f - vFrac;
		uint32_t s0 = GetPixelRGB( x, y );
		uint32_t s1 = GetPixelRGB( x + 1, y );
		uint32_t s2 = GetPixelRGB( x, y + 1 );
		uint32_t s3 = GetPixelRGB( x + 1, y + 1 );
		float r = ( ( s0 >> 24 ) * uFrac + ( s1 >> 24 ) * invuFrac ) * invvFrac + 
			      ( ( s2 >> 24 ) * uFrac + ( s3 >> 24 ) * invuFrac ) * vFrac;
		float g = ( ( ( s0 >> 16 ) & 0xFF ) * uFrac + ( ( s1 >> 16 ) & 0xFF ) * invuFrac ) * invvFrac + 
			      ( ( ( s2 >> 16 ) & 0xFF ) * uFrac + ( ( s3 >> 16 ) & 0xFF ) * invuFrac ) * vFrac;
		float b = ( ( ( s0 >> 8 ) & 0xFF ) * uFrac + ( ( s1 >> 8 ) & 0xFF ) * invuFrac ) * invvFrac + 
			      ( ( ( s2 >> 8 ) & 0xFF ) * uFrac + ( ( s3 >> 8 ) & 0xFF ) * invuFrac ) * vFrac;
		uint32_t p = ( (int)r << 24 ) | ( (int)g << 16 ) | ( (int)b << 8 );
		return p;
	}

	void WriteTGA32( char const * path ) const 
	{
		OVR::WriteTGA32( path, Buffer, Width, Height, 0, 0, Width, Height );
	}

	void WriteTGA32( char const * path, const int sx, const int sy, const int w, const int h ) const 
	{
		OVR::WriteTGA32( path, Buffer, Width, Height, sx, sy, w, h );
	}

	void WriteKTX_R8( char const * path )
	{
		OVR::WriteKTX_R8( path, Buffer, Width, Height );
	}

	void WriteRAW32( char const * path ) const
	{
		std::string outPath = path;
		PathUtils::SetExtension( outPath, "raw" );

		FILE * f = fopen( outPath.c_str(), "wb" );
		if ( f != NULL )
		{
			fwrite( Buffer, (size_t)Width * (size_t)Height * (size_t)BytesPerPixel, 1, f );
			fclose( f );
		}
	}

	void WriteRAW8( char const * path, int32_t const sx, int32_t const sy, int32_t dw, int32_t dh )
	{
		std::string outPath = path;
		PathUtils::SetExtension( outPath, "raw" );

		FILE * f = fopen( outPath.c_str(), "wb" );
		if ( f == NULL )
		{
			return;
		}

		int32_t const destWidth = sx + dw > Width ? Width - sx : dw;
		int32_t const destHeight = sy + dh > Height ? Height - sy : dh;

		size_t destRowSize = size_t( destWidth );
		uint8_t * destRowBuffer = new uint8_t [destRowSize];
		for ( int32_t y = 0; y < destHeight; ++y )
		{
			size_t srcRowStartIdx = (size_t)y * (size_t)Width * 4 + size_t( sx ) * 4;
			for ( int32_t x = 0; x < destWidth; ++x )
			{
				destRowBuffer[x] = Buffer[srcRowStartIdx + ( x * 4 )];
			}
			fwrite( destRowBuffer, destRowSize, 1, f );
		}
		fclose( f );
	}

	struct WalkType_t
	{
		//float	scale;
		int16_t	x;
		int16_t	y;
	};

	static int32_t const WALK_RADIUS = 256;
	

	static void CalcRadialWalkTable( int32_t const radius )
	{
		int32_t const radialWalkTableSize = ( radius * 2 + 1 ) * ( radius * 2 + 1 );
		WalkTable = new WalkType_t[radialWalkTableSize];
		NumWalk = 0;
		CalcRadialWalkTable( WALK_RADIUS, WalkTable, NumWalk );
	}

	// creates a table 8sed to walk in a circle around a single pixel in an image
	static void CalcRadialWalkTable( int32_t const radius, WalkType_t * walk, int32_t & numWalk )
	{
		numWalk = 0;
		for ( int r = 0; r < radius; r++ )
		{
			for ( int y = -r; y <= r; y++ )
			{
				for ( int x = -r; x <= r; x++ )
				{
					const int s = y * y + x * x;
					if ( s >= r * r && s < ( r + 1 ) * ( r + 1 ) )
					{
						WalkType_t newWalk;
						//newWalk.scale = 1.0f - (float)( x * x + y * y ) / ( radius * radius * radius );
						newWalk.x = (short) x;
						newWalk.y = (short) y;
						walk[numWalk] = newWalk;
						for ( int i = numWalk - 1; i >= 0; i-- )
						{
							if ( x * x + y * y >= walk[i].x * walk[i].x + walk[i].y * walk[i].y )
							{
								walk[i + 1] = newWalk;
								break;
							}
							walk[i + 1] = walk[i];
						}
						numWalk++;

					}
				}
			}
		}
	}

	float CalcDistance( int32_t const x, int32_t const y ) const
	{
		// get the color at x,y ... search for the closest opposite color
		uint32_t pixelColor = GetPixelRGBA( x, y ) & 0xff000000;
		if ( pixelColor == 0 )
		{
			// starting outside of a glyph -- walk until we find the closes outside pixel
			for ( int32_t i = 1; i < NumWalk; ++i )
			{
				int32_t xx = x + WalkTable[i].x;
				int32_t yy = y + WalkTable[i].y;
				if ( xx < 0 || yy < 0 || xx >= Width || yy >= Height )
				{
					continue;	// ignore out-of-bound pixels if we started outside
				}
				uint32_t curColor = GetPixelRGB( xx, yy ) & 0xff000000;	// ignore alpha
				if ( curColor != 0 )
				{
					return sqrtf( float( (int32_t)WalkTable[i].x * (int32_t)WalkTable[i].x + (int32_t)WalkTable[i].y * (int32_t)WalkTable[i].y ) ) / WALK_RADIUS;
				}
			}
			return 1.0f;
		}
		// starting inside of a glyph -- walk until we find the closest inside pixel
		for ( int32_t i = 1; i < NumWalk; ++i )
		{
			int32_t xx = x + WalkTable[i].x;
			int32_t yy = y + WalkTable[i].y;
			uint32_t curColor;
			if ( xx < 0 || yy < 0 || xx >= Width || yy >= Height )
			{
				curColor = 0;	// if we started inside, consider out-of-bound pixels as outside
			}
			else
			{
				curColor = GetPixelRGB( xx, yy ) & 0xff000000;	// ignore alpha
			}
			if ( curColor == 0 )
			{
				return ( sqrtf( float( (int32_t)WalkTable[i].x * (int32_t)WalkTable[i].x + (int32_t)WalkTable[i].y * (int32_t)WalkTable[i].y ) ) / WALK_RADIUS ) * -1.0f;
			}
		}
		return -1.0f;
	}

	uint8_t *	Buffer;			// the bitmap data
	int32_t		Width;			// width of the bitmap
	int32_t		Height;			// height of the bitmap
	int32_t		BytesPerPixel;	// bits per pixel
	uint32_t	Pitch;			// width * bytes per pixel (memory offset to advance to next row of the bitmap image)

	static WalkType_t *			WalkTable;
	static int32_t				NumWalk;
};

CBitmap::WalkType_t * CBitmap::WalkTable = NULL;
int32_t CBitmap::NumWalk = 0;

void CBitmap::CopyInto( CBitmap & other, int32_t const x, int32_t const y )
{
	for ( size_t r = 0; r < Height; ++r )
	{
		size_t outR = y + r;
		OVR_ASSERT( outR < other.Height );
		size_t outOfs = outR * size_t( other.Pitch ) + size_t( x ) * 4;
		size_t inOfs = r * size_t( Pitch );
		for ( size_t c = 0; c < Width; ++c )
		{
			other.Buffer[outOfs + c * 4 + 0] = Buffer[inOfs + c * 4 + 0];
			other.Buffer[outOfs + c * 4 + 1] = Buffer[inOfs + c * 4 + 1];
			other.Buffer[outOfs + c * 4 + 2] = Buffer[inOfs + c * 4 + 2];
			other.Buffer[outOfs + c * 4 + 3] = Buffer[inOfs + c * 4 + 3];
		}
	}
}

// =============================================
// CGlyph
// this is just a temporary structure for holding glyph info until it's written to disk
// =============================================
class CGlyph
{
public:
	CGlyph( uint32_t const charCode ) :
		CharCode( charCode ),
		X( -1 ),
		Y( -1 ),
		Width( 0 ),
		Height( 0 ),
		Area( 0 ),
		AdvanceX( 0 ),
		AdvanceY( 0 ),
		BearingX( 0 ),
		BearingY( 0 ),
		Bitmap( NULL )
	{
	}

	static bool SortByHeightThenWidth( CGlyph const * a, CGlyph const * b );

	uint32_t	CharCode;	// character code for this glyph
	int32_t		X;			// x offset in the bitmap
	int32_t		Y;			// y offset in the bitmap
	int32_t		Width;		// width of the character in pixels (doesn't include any empty pixels to the left or right -- advanceX does that)
	int32_t		Height;		// height of the character (doesn't include any empty pixels to the top or bottom -- advanceY does that)
	int32_t		Area;		// area of the character (used for sorting)
	int32_t		AdvanceX;	// horizontal increment to the next character
	int32_t		AdvanceY;	// vertical increment to the next character
	int32_t		BearingX;	//
	int32_t		BearingY;	//
	CBitmap *	Bitmap;		// bitmap of the glyph
};

bool CGlyph::SortByHeightThenWidth( CGlyph const * a, CGlyph const * b ) 
{
	if ( a->Height != b->Height )
	{
		return a->Height < b->Height;
	}
	return a->Width < b->Width;
}

// =============================================
// CRectangle
//
// We start out with a single rectangle representing the image and then we split it as we
// add glyphs.
// =============================================
class CRectangle
{
public:
	CRectangle() :
		GlyphIndex( -1 ),
		X( 0 ),
		Y( 0 ),
		Width( 0 ),
		Height( 0 )
	{
	}

	CRectangle( int32_t const x, int32_t const y, int32_t const w, int32_t const h ) :
		GlyphIndex( -1 ),
		X( x ),
		Y( y ),
		Width( w ),
		Height( h ) 
	{
	}

	CRectangle *	SplitHorizontally( int32_t const splitHeight );
	CRectangle *	SplitVertically( int32_t const splitWidth );

	bool			CanFitGlyph( CGlyph const & glyph ) const { return Width >= glyph.Width && Height >= glyph.Height; }

	int32_t			GetGlyphIndex() const { return GlyphIndex; }
	void			SetGlyphIndex( int32_t const i ) { GlyphIndex = i; }

	int32_t			GetX() const { return X; }
	void			SetX( int32_t const x ) { X = x; }

	int32_t			GetY() const { return Y; }
	void			SetY( int32_t const y ) { Y = y; }

	int32_t			GetWidth() const { return Width; }
	void			SetWidth( int32_t const w ) { Width = w; }

	int32_t			GetHeight() const { return Height; }
	void			SetHeight( int32_t const h ) { Height = h; }

	int32_t			GetArea() const { return Width * Height; }

	void			SetDims( int32_t const w, int32_t const h ) { Width = w; Height = h; }

	static bool		CompareGlyphArea( CRectangle const * a, CRectangle const * b );
	static bool		CompareGlyphHeight( CRectangle const * a, CRectangle const * b );

private:
	int32_t		GlyphIndex;	// index of the glyph occupying this bin (-1 if not occupied)
	int32_t		X;			// x location of the bin
	int32_t		Y;			// y location of the bin
	int32_t		Width;		// width of the bin
	int32_t		Height;		// height of the bin
};

bool CRectangle::CompareGlyphArea( CRectangle const * a, CRectangle const * b ) 
{
	return a->GetArea() < b->GetArea();
}

bool CRectangle::CompareGlyphHeight( CRectangle const * a, CRectangle const * b ) 
{
	return a->GetHeight() < b->GetHeight();
}

CRectangle * CRectangle::SplitHorizontally( int32_t const splitHeight )
{
	if ( splitHeight > Height )
	{
		OVR_ASSERT( splitHeight <= Height );
		return NULL;
	}
	if ( splitHeight == Height )
	{
		// if it's the same height we don't need to do anything
		return NULL;
	}
	CRectangle * r = new CRectangle( X, Y + splitHeight, Width, Height - splitHeight );
	// adjust this rectangle's dimensions since it was just sliced!
	Height = splitHeight;
	return r;
}

CRectangle * CRectangle::SplitVertically( int32_t const splitWidth )
{
	if ( splitWidth > Width )
	{
		OVR_ASSERT( splitWidth <= Width );
		return NULL;
	}
	if ( splitWidth == Width )
	{
		// same width so nothing to do
		return NULL;
	}
	CRectangle * r = new CRectangle( X + splitWidth, Y, Width - splitWidth, Height );
	// adjust width since this rectangle is now cut into two
	Width = splitWidth;
	return r;
}

static int32_t NextPowerOfTwo( int32_t n )
{
	n--;
	n |= ( n >> 1 );
	n |= ( n >> 2 );
	n |= ( n >> 4 );
	n |= ( n >> 8 );
	n |= ( n >> 16 );
	n++;
	return n;
}

static void FreeRectList( std::vector< CRectangle* > & list )
{
	for ( int i = 0; i < list.size(); ++i )
	{
		delete list[i];
		list[i] = NULL;
	}
	list.resize( 0 );
}

static bool PlaceGlyphs(	std::vector< CGlyph* > const & glyphs, 
		int32_t const sideX, int32_t const sideY,
		std::vector< CRectangle* > & usedRects )
{
	// make an initial rectangle 
	CRectangle * initialRect = new CRectangle( 0, 0, sideX, sideY );

	std::vector< CRectangle* > emptyRects;
	emptyRects.push_back( initialRect );

	// make a list of glyphs indices that are not used
	std::vector< int32_t > unusedGlyphs;
	unusedGlyphs.resize( glyphs.size() );
	for ( int i = 0; i < glyphs.size(); ++i ) 
	{
		unusedGlyphs[i] = i;
	}

	// while we have unused glyphs and an empty bin
	// - place the largest glyph in the empty rect and remove the glyph from the unused list
	// - split the empty rect (it may split into anywhere from 0 to 3 new rects depending on 
	//   where the cut is made)
	// - put the rect with the glyph in it into the used list
	// - add new rects to the empty list
	while ( unusedGlyphs.size() )
	{
		int32_t glyphIndex = unusedGlyphs.back();
		unusedGlyphs.pop_back();

		CGlyph const & g = *( glyphs[glyphIndex] );
		if ( g.Width <= 0 || g.Height <= 0 )
		{
			continue;	// some glyphs can actually take up no space... like space
		}

		CRectangle * emptyRect = NULL;
		if ( emptyRects.size() == 0 )
		{
			// ran out of free rectangles -- we need a larger initial rectangle to allocate from
			FreeRectList( emptyRects );
			FreeRectList( usedRects );
			return false;
		}
		emptyRect = emptyRects.back();
		emptyRects.pop_back();

		// if the glyph won't fit, see if there's 
		if ( !emptyRect->CanFitGlyph( g ) )
		{
			// the glyph doesn't fit in the current empty rect, so search the list for one that does
			int32_t i = static_cast< int32_t >( emptyRects.size() ) - 1;
			for ( ; i >= 0; --i )
			{
				CRectangle * rect = emptyRects[i];
				if ( rect->CanFitGlyph( g ) )
				{
					emptyRects[i] = emptyRect;
					emptyRect = rect;
					std::sort( emptyRects.begin(), emptyRects.end(), CRectangle::CompareGlyphHeight );
					break;
				}
			}
			if ( i < 0 )
			{
				// couldn't fit in any of the remaining rects -- we need to start with a larger
				// initial rectangle
				delete emptyRect;
				FreeRectList( emptyRects );
				FreeRectList( usedRects );
				return false;
			}
		}
		OVR_ASSERT( emptyRect->CanFitGlyph( g ) );
		emptyRect->SetGlyphIndex( glyphIndex );
		usedRects.push_back( emptyRect );
		if ( g.Width != emptyRect->GetWidth() || g.Height != emptyRect->GetHeight() )
		{
			CRectangle * hRect = emptyRect->SplitHorizontally( g.Height );
			CRectangle * vRect = emptyRect->SplitVertically( g.Width );
			if ( hRect != NULL )
			{
				emptyRects.push_back( hRect );
			}
			if ( vRect != NULL )
			{
				emptyRects.push_back( vRect );
			}			
		}
	}

	// clean up empty rects and total the unused area for metrics
	int32_t emptyArea = 0;
	for ( size_t i = 0; i < emptyRects.size(); ++i )
	{
		emptyArea += emptyRects[i]->GetArea();
		delete emptyRects[i];
		emptyRects[i] = NULL;
	}
	emptyRects.resize( 0 );

	// total the used area
	int32_t usedArea = 0;
	for ( size_t i = 0; i < usedRects.size(); ++i )
	{
		usedArea += usedRects[i]->GetArea();
	}

	std::cout << "Empty area: " << emptyArea << "\n";
	std::cout << "Used area: " << usedArea << "\n";

	return true;
}

#define ASSIGN_NEXT_ARG( _var_ )	\
if ( i + 1 >= argc ) { \
	std::cout << "Expected an argument after option " << argv[i] << ".\n"; \
	return -1; \
}

#define ASSIGN_NEXT_ARGI( _var_ )	\
ASSIGN_NEXT_ARG( _var_ )	\
_var_ = atoi( argv[++i] );

#define ASSIGN_NEXT_ARGF( _var_ )	\
ASSIGN_NEXT_ARG( _var_ ) \
_var_ = static_cast< float >( atof( argv[++i] ) );

#define ASSIGN_NEXT_ARGSTR( _var_ ) \
ASSIGN_NEXT_ARG( _var_ ) \
_var_ = argv[++i];


bool IsHexString( std::string const & s )
{
	return ( s.length() > 2 && s[0] == '0' && s[1] == 'x' );
}

bool CharIsInString( char const ch, char const * s )
{
	if ( s[0] == '\0' )
	{
		return false;
	}
	for ( int i = 0; s[i] != '\0'; ++i )
	{
		if ( s[i] == ch )
		{
			return true;
		}
	}
	return false;
}

std::string StripLeadingChars( std::string const & s, char const * chars )
{
	char const * p = s.c_str();
	while( CharIsInString( *p, chars ) ) 
	{
		p++;
	}
	
	std::string out( p );
	return out;
}

std::string StripTrailingChars( std::string const & s, char const * chars )
{
	size_t len = s.length();
	if ( len == 0 )
	{
		std::string empty;
		return empty;
	}

	char const * start = s.c_str();
	char const * end = start + len - 1;
	while( CharIsInString( *end, chars ) && end >= start )
	{
		end--;
	}
	size_t const tempLen = ( end - start ) + 1;
	char * temp = new char[tempLen + 1];
	OVR_strncpy( temp, tempLen + 1, start, tempLen );
	std::string out( temp );
	delete [] temp;
	return out;
}

bool StringToHex( std::string const & s, int32_t & value )
{
	std::string temp = StripLeadingChars( s, " \n\t\r" );
	temp = StripTrailingChars( s, " \n\t\r" );
	value = 0;
	int32_t len = static_cast< int32_t >( s.length() );
	for ( int32_t i = 0; i < len; ++i ) 
	{
		char const c = s.c_str()[len - 1 - i];
		int placeIdx;
		if ( c == 'x' && s[0] == '0' && len - 1 - i == 1 )
		{
			// if we got an 'x' in the second character spot and the first character is '0', we're done.
			return true;
		}
		if ( c >= 48 && c <= 57 )
		{
			placeIdx = c - 48;
		}
		else if ( c >= 97 && c <= 102 )
		{
			placeIdx = c - 87;
		}
		else if ( c >= 65 && c <= 70 )
		{
			placeIdx = c - 55;
		}
		else
		{
			return false;
		}
		int32_t placeValue = ( 1 << ( i * 4 ) ) * placeIdx;
		value += placeValue;
	}
	return true;
}

class UnicodeRange
{
public:
	UnicodeRange() : StartCode( 0 ), EndCode( 0 ) { }
	UnicodeRange( uint32_t const s, uint32_t const e ) : StartCode( s ), EndCode( e ) { }

	uint32_t	StartCode;
	uint32_t	EndCode;
};

bool AddCodePointsFromFile( char const * charFile, std::vector< uint32_t > & codePoints, bool const verifyAscii, OVR::String & asciiErrors, OVR::String & nonAsciiChars )
{
	FILE * f = fopen( charFile, "rb" );
	if ( f == NULL ) 
	{
		return false;
	}

	fseek( f, 0, SEEK_END );
	size_t flen = ftell( f );
	fseek( f, 0, SEEK_SET );

	char * buff = new char[flen + 1 ];
	size_t numRead = fread( buff, flen, 1, f );
	fclose( f );
	if ( numRead != 1 )
	{
		delete [] buff;
		return false;
	}

	buff[flen] = '\0';
	char const * p = buff;
	uint32_t curChar;
	curChar = OVR::UTF8Util::DecodeNextChar( &p );
	int32_t count = 0;
	while( curChar )
	{
		if ( verifyAscii == true && curChar > 256 )
		{
			bool alreadyLogged = false;
			size_t len = nonAsciiChars.GetLength();
			for ( size_t i = 0; i < len; ++i )
			{
				if ( nonAsciiChars.GetCharAt( i ) == curChar )
				{
					alreadyLogged = true;
					break;
				}
			}
			if ( !alreadyLogged )
			{
				nonAsciiChars.AppendChar( curChar );
			}
			asciiErrors.AppendString( "ERROR: Non-ASCII character detected.\n" );
			asciiErrors.AppendString( "Offending file: " );
			asciiErrors.AppendString( charFile );
			asciiErrors.AppendChar( '\n' );
			asciiErrors.AppendString( "Offending text: \n" );
			asciiErrors.AppendChar( curChar );
			char const * p2 = p;
			int numOutChars = 0;
			uint32_t outChar = OVR::UTF8Util::DecodeNextChar( &p2 );
			while( outChar && numOutChars < 40 )
			{
				asciiErrors.AppendChar( outChar );
				numOutChars++;
				outChar = OVR::UTF8Util::DecodeNextChar( &p2 );
			}
			asciiErrors.AppendChar( '\n' );
		}
		size_t i = 0;
		for ( ; i < codePoints.size(); ++i )
		{
			if ( codePoints[i] == curChar )
			{
				break;
			}
		}
		if ( i >= codePoints.size() )
		{
			count++;
			codePoints.push_back( curChar );
		}

		curChar = OVR::UTF8Util::DecodeNextChar( &p );
	}

	delete [] buff;

	// codePoints should now be a list of unique Unicode code points
	std::cout << "Read " << count << " unique characters from file.\n";

	return true;
}

static int CreateBitmapFont( int const argc, char const * argv[], FT_Library & ftLibrary )
{
	int32_t	fontIdx = 0;
	int32_t charSize = 32;
	int32_t pixelSize = 0;
	int32_t faceIdx = -1;
	int32_t hpad = 32;
	int32_t vpad = 32;
	int32_t targetWidth = -1;	// for sdf only
	int32_t targetHeight = -1;	// for sdf only
	bool signedDistanceField = false;
	bool grid = false;
	bool raw = false;
	bool verifyAscii = false;
	OVR::String asciiErrors;	// using OVR::String because of its easy UTF-8 support
	OVR::String nonAsciiChars;
	std::string asciiErrorFile;
	int numCfCommands = 0;
	float scaleBias = 1.0f;
	float centerOffset = 0.0f;
	int32_t alphaValue = -1;	// -1 means set alpha to RGB

	std::vector< uint32_t > glyphToUnicodeMap;
	std::vector< UnicodeRange > unicodeRanges;
	UnicodeRange defaultRange( 0, 255 );
	unicodeRanges.push_back( defaultRange );

	for ( int i = 3; i < argc; ++i )
	{
		if ( _stricmp( argv[i], "-hpad" ) == 0 )
		{
			ASSIGN_NEXT_ARGI( hpad );
		}
		else if ( _stricmp( argv[i], "-vpad" ) == 0 )
		{
			ASSIGN_NEXT_ARGI( vpad );
		}
		else if ( _stricmp( argv[i], "-charSize" ) == 0 )
		{
			ASSIGN_NEXT_ARGI( pixelSize );
		}
		else if ( _stricmp( argv[i], "-pixelSize" ) == 0 )
		{
			ASSIGN_NEXT_ARGI( pixelSize );
		}
		else if ( _stricmp( argv[i], "-idx" ) == 0 )
		{
			ASSIGN_NEXT_ARGI( fontIdx );
		}
		else if ( _stricmp( argv[i], "-faceIdx" ) == 0 )
		{
			ASSIGN_NEXT_ARGI( faceIdx );
		}
		else if ( _stricmp( argv[i], "-sdf" ) == 0 )
		{
			signedDistanceField = true;
			ASSIGN_NEXT_ARGI( pixelSize );
			ASSIGN_NEXT_ARGI( targetWidth );
			ASSIGN_NEXT_ARGI( targetHeight );
		}
		else if ( _stricmp( argv[i], "-grid" ) == 0 )
		{
			grid = true;
		}
		else if ( _stricmp( argv[i], "-raw" ) == 0 )
		{
			raw = true;
		}
		else if ( _stricmp( argv[i], "-bias" ) == 0 )
		{
			ASSIGN_NEXT_ARGF( scaleBias );
		}
		else if ( _stricmp( argv[i], "-centerOffset" ) == 0 || _stricmp( argv[i], "-co" ) == 0 )
		{
			ASSIGN_NEXT_ARGF( centerOffset );
		}
		else if ( _stricmp( argv[i], "-a" ) == 0 || _stricmp( argv[i], "-alpha" ) == 0 )
		{
			// defaults to negative, which means the alpha should be the same as the RGB channels
			ASSIGN_NEXT_ARGI( alphaValue );
			if ( alphaValue > 255 )
			{
				alphaValue = 255;
			}
		}
		else if ( _stricmp( argv[i], "-ur" ) == 0 )
		{
			// add a range of unicode characters
			std::string startStr;
			std::string endStr;
			ASSIGN_NEXT_ARGSTR( startStr );
			ASSIGN_NEXT_ARGSTR( endStr );
			int32_t start;
			int32_t end;  
			if ( !StringToHex( startStr, start ) )
			{
				start = atoi( startStr.c_str() );
			}
			if ( !StringToHex( endStr, end ) )
			{
				end = atoi( endStr.c_str() );
			}
			UnicodeRange r( start, end );
			unicodeRanges.push_back( r );
		}
		else if ( _stricmp( argv[i], "-uc" ) == 0 )
		{
			// add a single unicode character
			int32_t c;
			ASSIGN_NEXT_ARGI( c );
			UnicodeRange r( c, c );
			unicodeRanges.push_back( r );
		}
		else if ( _stricmp( argv[i], "-cf" ) == 0 || _stricmp( argv[i], "-charFile" ) == 0 )
		{
			numCfCommands++;
			std::string charFile;
			ASSIGN_NEXT_ARGSTR( charFile );
			if ( !AddCodePointsFromFile( charFile.c_str(), glyphToUnicodeMap, verifyAscii, asciiErrors, nonAsciiChars ) )
			{
				std::cout << "Error reading file " << charFile << " for code points.\n";
				return -1;
			}
		}
		else if ( _stricmp( argv[i], "-verifyAscii" ) == 0 )
		{
			verifyAscii = true;
			ASSIGN_NEXT_ARGSTR( asciiErrorFile );
			if ( numCfCommands > 0 )
			{
				std::cout << "-verifyAscii must come before any -cf commands!\n";
				return -1;
			}
		}
		else
		{
			std::cout << "Unknown option " << argv[i] << "!\n";
			return -1;
		}
	}

	char const * inFileName = argv[1];
	char const * outFileName = argv[2];

	FT_Face ftFace;
	FT_Error error = FT_New_Face( ftLibrary, inFileName, fontIdx, &ftFace );
	if ( error )
	{
		std::cout << "Error loading font " << fontIdx << " from font file " << inFileName << ".\n";
		return -1;
	}

	if ( faceIdx >= 0 )
	{
		error = FT_Select_Size( ftFace, faceIdx );
		if ( error )
		{
			std::cout << "Error selecting face " << faceIdx << " from font file " << inFileName << ".\n";
			return -1;
		}
	}
	else
	{
		if ( !FT_IS_SCALABLE( ftFace ) )
		{
			std::cout << "Failed to set pixel size. Font is not scalable.\n";
			return -1;
		}
		if ( pixelSize > 0 )
		{
			error = FT_Set_Pixel_Sizes( ftFace, pixelSize, pixelSize );
		}
		else
		{
			error = FT_Set_Char_Size( ftFace, 0, charSize * 72, 0, 72 );
		}
		if ( error )
		{
			if ( ( ftFace->face_flags & FT_FACE_FLAG_FIXED_SIZES ) != 0 &&
				 ( ftFace->face_flags & FT_FACE_FLAG_SCALABLE ) == 0 )
			{
				std::cout << "Font is bitmap-only. Use -faceIdx to select a size. Available sizes are:\n";
				for ( int i = 0; i < ftFace->num_fixed_sizes; ++i )
				{
					FT_Bitmap_Size & size = ftFace->available_sizes[i];
					std::cout << "Index: " << i << ", size: " << size.size << ", width: " << size.width << ", height: " << size.height << "\n";
				}
				std::cout << "Failed to set " << ( pixelSize > 0 ? "pixelSize" : "charSize" ) << ( pixelSize > 0 ? pixelSize : charSize ) << ".\n";
				return -1;
			}
		}
	}

	// determine the font height in scaled space
	int fontHeight = ( ftFace->size->metrics.ascender - ftFace->size->metrics.descender ) >> 6;

	std::cout << "Loaded font file " << inFileName << ".\n";

	// calculate the number of glyphs and build a flat list of all glyphs
	for ( int i = 0; i < static_cast< int >( unicodeRanges.size() ); ++i )
	{
		UnicodeRange const & r = unicodeRanges[i];
		for ( uint32_t charCode = r.StartCode; charCode <= r.EndCode; ++charCode )
		{
			// don't add duplicate glyphs
			int j = 0;
			for ( ; j < static_cast< int >( glyphToUnicodeMap.size() ); ++j )
			{
				if ( glyphToUnicodeMap[j] == charCode )
				{
					break;
				}
			}
			if ( j >= glyphToUnicodeMap.size() )
			{
				glyphToUnicodeMap.push_back( charCode );
			}
		}
	}

	if ( asciiErrors.GetLength() > 0 )
	{
		std::cout << glyphToUnicodeMap.size() << " unique glyphs were found.\n";
		std::cout << "Non-ASCII characters were detected. Writing errors to '" << asciiErrorFile << "'.\n";
		FILE * outf = fopen( asciiErrorFile.c_str(), "wb" );
		if ( outf == NULL )
		{
			std::cout << "Error could not open file '" << asciiErrorFile << ".\n";
			return -1;
		}
		char msg[128];
		OVR_sprintf( msg, sizeof( msg ), "%i Non-ASCII characters found: ", nonAsciiChars.GetLength() );
		fwrite( msg, strlen( msg ), 1, outf );
		nonAsciiChars.AppendChar( '\n' );
		fwrite( nonAsciiChars.ToCStr(), nonAsciiChars.GetSize(), 1, outf );
		fwrite( asciiErrors.ToCStr(), asciiErrors.GetSize(), 1, outf );
		fclose( outf );
		return -1;
	}

	int32_t const numGlyphs = static_cast< int >( glyphToUnicodeMap.size() );
	std::cout << "Rendering " << glyphToUnicodeMap.size() << " glyphs.\n";

	// build a list of all 

	// render the glyphs to individual bitmaps
	int32_t totalArea = 0;
	int32_t numUndefined = 0;
	int32_t emptyChars = 0;
	std::vector< CGlyph* > glyphs;
	glyphs.resize( numGlyphs );	// some glyphs may not be assigned, depending 
	int32_t highest = -1;
	for ( int32_t i = 0; i < static_cast< int32_t >(glyphToUnicodeMap.size() ); ++i )
	{
		FT_ULong charCode = static_cast< FT_ULong >( glyphToUnicodeMap[i] );
		glyphs[i] = new CGlyph( charCode );
		CGlyph & g = *( glyphs[i] );
		FT_UInt glyphIdx = FT_Get_Char_Index( ftFace, charCode );
		if ( glyphIdx == 0 )
		{
			// this is very common for character codes < 32 and > 127
			if ( ( charCode >= 32 && charCode <= 127 ) || charCode > 255 )
			{
				if ( numUndefined < 10 )
				{
					std::cout << "Undefined character code " << charCode << ".\n";
				}
				else if ( numUndefined == 10 )
				{
					std::cout << "Multiple undefined characters were found.\n";
				}
				numUndefined++;
			}
			continue;
		}
		const int32_t loadFlags = FT_LOAD_DEFAULT;
		error = FT_Load_Glyph( ftFace, glyphIdx, loadFlags );
		if ( error )
		{
			std::cout << "Error loading glyph for character code " << charCode << ".\n";
			continue;
		}
		if ( ftFace->glyph == NULL )
		{
			continue;
		}
		if ( ftFace->glyph->format != FT_GLYPH_FORMAT_BITMAP )
		{
			error = FT_Render_Glyph( ftFace->glyph, FT_RENDER_MODE_NORMAL );
			if ( error )
			{
				std::cout << "Failed to render glyph for character code " << charCode << ".\n";
				continue;
			}
		}

		FT_Bitmap & ftBitmap = ftFace->glyph->bitmap;
		FT_Glyph_Metrics & ftMetrics = ftFace->glyph->metrics;

		if ( ftFace->glyph->metrics.height > highest )
		{
			highest = ftFace->glyph->metrics.height;
		}

		g.Width = ftBitmap.width + hpad;
		g.Height = ftBitmap.rows + vpad;
		// shifts are for 26.6 fixed point, so we're just truncating the fractional part
		//int extraHPad = ( hpad >> 1 ) - ( ftMetrics.horiBearingX >> 6 );
		//int extraVPad = ( vpad >> 1 ) - ( ftMetrics.horiBearingY >> 6 );
		g.AdvanceX = ( ftMetrics.horiAdvance >> 6 );// + extraHPad;	
		g.AdvanceY = ( ftMetrics.vertAdvance >> 6 );// + extraVPad;
		g.BearingX = ( ftMetrics.horiBearingX >> 6 ) - ( hpad >> 1 );
		g.BearingY = ( ftMetrics.horiBearingY >> 6 ) + ( vpad >> 1 );

		if ( ftBitmap.width == 0 || ftBitmap.rows == 0 )
		{
			if ( charCode > ' ' )
			{
				std::cout << "Character " << charCode << " has zero area!\n";
				emptyChars++;
			}
			continue;
		}

		totalArea += g.Width * g.Height;

		if ( ftBitmap.pixel_mode == FT_PIXEL_MODE_GRAY )
		{
			if ( ftBitmap.num_grays != 256 )
			{
				std::cout << "Wrong bitmap format (!256 grays) for glyph character code " << charCode << ".\n";
				continue;
			}
		
			g.Bitmap = CBitmap::Create( g.Width, g.Height, 4, true );

			// copy from rendered glyph to the bitmap
			uint8_t * buffer = static_cast< uint8_t* >( ftBitmap.buffer );
			
			for ( int y = 0; y < ftBitmap.rows; ++y )
			{
				for ( int x = 0; x < ftBitmap.width; ++x )
				{
					uint8_t gray = buffer[y * ftBitmap.pitch + x];
					uint8_t alpha = alphaValue < 0 ? gray : static_cast< uint8_t >( alphaValue );
					g.Bitmap->SetPixelRGBA( x + ( hpad >> 1 ), y + ( vpad >> 1 ), gray, gray, gray, alpha );
				}
			}
			if ( grid )
			{
				uint8_t alpha = alphaValue < 0 ? 255 : static_cast< uint8_t >( alphaValue );
				// put a grid around each char
				for ( int y = 0; y < ftBitmap.rows; ++y )
				{
					int py = y + ( vpad >> 1 );
					if ( y == 0 || y == ftBitmap.rows - 1 )
					{
						// set entire row
						for ( int x = 0; x < ftBitmap.width; ++x )
						{
							int px = x + ( hpad >> 1 );
							g.Bitmap->SetPixelRGBA( px, py, 255, 255, 255, alpha );
						}
					}
					else
					{
						// set first and last
						int px = ( hpad >> 1 );
						g.Bitmap->SetPixelRGBA( px, py, 255, 255, 255, alpha );
						px = ( ftBitmap.width - 1 ) + ( hpad >> 1 );
						g.Bitmap->SetPixelRGBA( px, py, 255, 255, 255, alpha );
					}
				}						
			}
			// for debugging individual glyphs -- RAW format can be loaded with IrfanView + plugins.
			if ( raw && charCode == 'a' )
			{
				char glyphName[MAX_PATH];
				sprintf( glyphName, "glyph_%u.tga", charCode );
				//g.Bitmap->WriteRAW32( glyphName );
				g.Bitmap->WriteTGA32( glyphName );
			}
		}
		else
		{
			std::cout << "Unsupported bitmap format for glyph character code " << charCode << ".\n";
			continue;
		}
	}

	highest >>= 6;

	if ( numUndefined > 0 )
	{
		std::cout << numUndefined << " undefined characters were found.\n";
	}

	if ( emptyChars > 0 )
	{
		std::cout << emptyChars << " were empty (no glyph data to render).\n";
	}

	if ( totalArea == 0 )
	{
		std::cout << "No glyphs were rendered.\n";
		return -1;
	}

	std::sort( glyphs.begin(), glyphs.end(), CGlyph::SortByHeightThenWidth );

	// split a square up into separate rectangles for each glyph. If we run out of space before
	// all glyphs are assigned to a sub rectangle, then increase the smallest dimension x or y
	// to the next power of two and try again.
	std::vector< CRectangle* > usedRects;
	const int32_t MAX_TEXTURE_DIM = 65536;

	// get the square root of the area, then find the next power of two larger than the square root
	// and use that as our starting side dimensions
	int32_t sqRoot = (int32_t)sqrtf( (float)totalArea );
	int32_t nextPowerOfTwoFromSqRoot = NextPowerOfTwo( sqRoot );
	int32_t sidex = nextPowerOfTwoFromSqRoot;
	int32_t sidey = nextPowerOfTwoFromSqRoot >> 1;
	if ( sidey == 0 )
	{
		sidey = sidex;
	}

	while( !PlaceGlyphs( glyphs, sidex, sidey, usedRects ) )
	{
		if ( sidex > MAX_TEXTURE_DIM && sidey > MAX_TEXTURE_DIM )
		{
			std::cout << "Aborting. The glyphs could not be packed into the max texture size of " << MAX_TEXTURE_DIM << "x" << MAX_TEXTURE_DIM << ".\n";
			return 1;
		}
		// increase the shorter side and try again
		if ( sidex <= sidey )
		{
			sidex <<= 1;
		} 
		else 
		{
			sidey <<= 1;
		}
	}

	// render each glyph rectangle into its place in the final bitmap
	CBitmap * fontBitmap = CBitmap::Create( sidex, sidey, 4, true );
	std::cout << "Rendering to " << sidex << "x" << sidey << " bitmap... ";
	int32_t maxY = 0;
	for ( int i = 0; i < usedRects.size(); ++i )
	{
		CRectangle const * r = usedRects[i];
		CGlyph & g = *( glyphs[r->GetGlyphIndex()] );
		g.X = r->GetX();
		g.Y = r->GetY();
		OVR_ASSERT( g.X >= 0 && g.Y >= 0 );
		OVR_ASSERT( g.X + g.Width < sidex );
		OVR_ASSERT( g.Y + g.Height < sidey );
		if ( g.Bitmap != NULL )	// glyph may have no bitmap if it is a space
		{
			g.Bitmap->CopyInto( *fontBitmap, g.X, g.Y );
		}
		if ( g.Y + g.Height > maxY )
		{
			maxY = g.Y + g.Height;
		}
		delete r;
		usedRects[i] = NULL;
	}

	// make sure that the maxY includes vertical padding so that we don't have a bottom row with
	// no distance falloff at the edge.
	maxY = OVR::Alg::Clamp( maxY + ( vpad >> 1 ), 0, fontBitmap->Height );

	std::cout << "done.\n";

	usedRects.resize( 0 );

	// output the glyph image as a TGA and the glyph info as a JSON file
	std::string path;
	std::string filename;
	std::string ext;
	PathUtils::SplitPath( outFileName, path, filename, ext );

	// write out the bitmap as a .tga
	std::string tgaFileName = filename;
	PathUtils::SetExtension( tgaFileName, "tga" );
	std::string tgaPath = path;
	PathUtils::AppendPath( tgaPath, tgaFileName);

	std::cout << "Writing " << fontBitmap->Width << "x" << fontBitmap->Height << " image file '" << tgaPath << "'...\n";
//	fontBitmap->WriteTGA32( tgaPath.c_str(), 0, fontBitmap->Height - 1024, 1024, 1024 );
	fontBitmap->WriteTGA32( tgaPath.c_str(), 0, 0, 1024, 1024 );
/*
	for ( int i = 0; i < 32; ++i )
	{
		std::string fname( filename );
		fname += "_";
		fname += std::to_string( i );
		fname += ".tga";
		std::string filePath = path;
		PathUtils::AppendPath( filePath, fname );
		fontBitmap->WriteTGA32( filePath.c_str(), 0, i * 1024, 1024, 1024 );
	}
*/
	//fontBitmap->WriteRAW8( tgaPath.c_str() );

	time_t sdfStart;
	time( &sdfStart );

	double scale = 1.0f;
	std::string sdfFileName;
	PathUtils::StripExtension( filename.c_str(), sdfFileName );
	sdfFileName += "_sdf";
	PathUtils::SetExtension( sdfFileName, "tga" );
	std::string sdfPath = path;
	PathUtils::AppendPath( sdfPath, sdfFileName );

	if ( signedDistanceField )
	{
		if ( targetHeight == 0 )
		{
			targetHeight = maxY;
			targetWidth = sidex;
		}

		// convert the bitmap to a signed distane field representation
		int32_t actualHeight = ftoi( targetHeight * ( (float)maxY / (float)sidex ) );
		CBitmap * sdfBitmap = CBitmap::Create( targetWidth, actualHeight, 4, true );

		scale = (float)targetWidth / (float)sidex;
		double const invScale = (float)sidex / (float)targetWidth;
		double const halfPixelInSource = invScale * 0.5f;

		std::cout << "Generating " << targetWidth << "x" << actualHeight << " signed distance field... ";

		CBitmap::CalcRadialWalkTable( 256 );
		for ( int32_t y = 0; y < sdfBitmap->Height; ++y )
		{
			char msg[ 64 ];
			sprintf( msg, "row %i of %i", y, sdfBitmap->Height );
			size_t msgLen = strlen( msg );
			printf( msg );

			// calculate distances in the corresponding row in the font bitmap
			int32_t fr = dtoi( ( invScale * y ) + halfPixelInSource );
			for ( int32_t x = 0; x < sdfBitmap->Width; ++x )
			{
				// calculate the corresponding column in the font bitmap
				int32_t fc = dtoi( ( invScale * x ) + halfPixelInSource );
				float distance = fontBitmap->CalcDistance( fc, fr );
				OVR_ASSERT( distance >= -1.0f && distance <= 1.0f );
				float s;
				if ( distance < 0.0f )
				{
					distance *= 128.0f + 128;
				}
				else
				{
					distance *= 127.0f + 128;
				}
				s = ClampFloat( distance + 128.0f, 0.0f, 255.0f );
				uint8_t b = 255 - (uint8_t)s;
				sdfBitmap->SetPixelRGBA( x, y, b, b, b, alphaValue < 0 ? b : static_cast< uint8_t >( alphaValue ) );
			}

			for ( size_t i = 0; i < msgLen; ++i )
			{
				printf( "\b" );
			}
		}

		std::cout << "done.            \n";  // extra spaces are intentional to clear previous output

		std::cout << "Writing " << targetWidth << "x" << actualHeight << " image file '" << sdfPath << "'...\n";
		sdfBitmap->WriteTGA32( sdfPath.c_str(), 0, 0, targetWidth, actualHeight );
		std::string ktxPath = sdfPath;
		PathUtils::SetExtension( ktxPath, "ktx" );
		sdfBitmap->WriteKTX_R8( ktxPath.c_str() );

		std::string rawPath = sdfPath;
		PathUtils::SetExtension( rawPath, "raw" );
		sdfBitmap->WriteRAW8( rawPath.c_str(), 0, 0, targetWidth, actualHeight );

		delete  sdfBitmap;
	}

	time_t sdfEnd;
	time( &sdfEnd );
	std::cout << "Sampling took " << ( sdfEnd - sdfStart ) << " seconds.\n";

	delete fontBitmap;
	fontBitmap = NULL;

	// output the glyph data as JSON
	std::string fontFileName = filename;
	PathUtils::SetExtension( fontFileName, "fnt" );

	JSON * joFont = JSON::CreateObject();
	joFont->AddStringItem( "FontName", fontFileName.c_str() );
	std::string commandLine;
	for ( int i = 1; i < argc; ++i )
	{
		commandLine += argv[i];
		if ( i < argc - 1 )
		{
			commandLine += " ";
		}
	}
	joFont->AddStringItem( "CommandLine", commandLine.c_str() );
	std::string imageFileName;
	PathUtils::GetFilenameWithExtension( signedDistanceField ? sdfFileName.c_str() : tgaFileName.c_str(), imageFileName );
	PathUtils::SetExtension( imageFileName, "ktx" );
	joFont->AddNumberItem( "Version", FNT_FILE_VERSION );
	joFont->AddStringItem( "ImageFileName", imageFileName.c_str() );
	joFont->AddNumberItem( "NaturalWidth", sidex );
	joFont->AddNumberItem( "NaturalHeight", maxY );
	joFont->AddNumberItem( "HorizontalPad", hpad );
	joFont->AddNumberItem( "VerticalPad", vpad );
	joFont->AddNumberItem( "FontHeight", fontHeight );
	joFont->AddNumberItem( "CenterOffset", centerOffset );
	joFont->AddNumberItem( "NumGlyphs", static_cast< int32_t >( glyphs.size() ) );
	JSON * joGlyphsArray = JSON::CreateArray();
	joFont->AddItem( "Glyphs", joGlyphsArray );
	// add all glyphs
	for ( int i = 0; i < glyphs.size(); ++i )
	{
		CGlyph const & g = *(glyphs[i]);
		JSON * joGlyph = JSON::CreateObject();
		if ( g.CharCode == '2' || g.CharCode == 'd' )
		{
			int foo = 0;
			foo = foo;
		}
		joGlyph->AddNumberItem( "CharCode", g.CharCode );
		joGlyph->AddNumberItem( "X", g.X );	// now in natural units because the JSON writer loses precision
		joGlyph->AddNumberItem( "Y", g.Y );		// now in natural units because the JSON writer loses precision
		joGlyph->AddNumberItem( "Width", g.Width );
		joGlyph->AddNumberItem( "Height", g.Height );
		joGlyph->AddNumberItem( "AdvanceX", g.AdvanceX );
		joGlyph->AddNumberItem( "AdvanceY", g.AdvanceY );
		joGlyph->AddNumberItem( "BearingX", g.BearingX );
		joGlyph->AddNumberItem( "BearingY", g.BearingY );
		joGlyphsArray->AddArrayElement( joGlyph );
	}

	std::string fontPath = path;
	PathUtils::AppendPath( fontPath, fontFileName );
	std::cout << "Writing font file '" << fontPath << "'...\n";
	joFont->Save( fontPath.c_str() );

	return 0;
}

static void PrintTitle()
{
	std::cout << "Fontue version 0.0\n";
	std::cout << "by Jonathan E. Wright\n";
	std::cout << "Copyright (C) 2014 by Oculus VR, LLC.\n\n";
	std::cout << "Portions of this software are copyright (C) 1996-2014 The FreeType\n";
    std::cout << "Project (www.freetype.org).  All rights reserved.\n\n";
}

static void PrintUsage()
{
	std::cout << "Fontue takes a TrueType font as input and uses the FreeType library to\n";
	std::cout << "create a bitmapped or signed distance field font image and glyph file that\n";
	std::cout << "can be used to render a variable-spaced bitmap font in high-performance\n";
	std::cout << "applications.\n\n";
	std::cout << "USAGE: fontue <in font file> <out font file > [[-option], ... ]\n";
	std::cout << "The output file name will be used to output both the font info (.fnt) and the\n";
	std::cout << "image file (.tga), so no extension is necessary.\n";
	std::cout << "Options:\n";
	std::cout << "-hpad <n> : horizontal pad around each glyph\n";
	std::cout << "-vpad <n> : vertical pad around each glyph\n";
	std::cout << "-pixelSize <n> : pixel size of the font.\n";
	std::cout << "-idx <n> : index of the font to load from the input font file.\n";
	std::cout << "-faceIdx <n> : index of the font face to use from the input font file.\n";
	std::cout << "-sdf <pixelSize> <targetWidth> <targetHeight>: output the bitmap as a signed distance field.\n";
	std::cout << "-ur <n1> <n2> : specify a range of unicode character codes to render, from n1 to n2.\n";
	std::cout << "-uc <n> : specify a single unicode character code to render.\n";
	std::cout << "-charFile <file name> : specify a UTF8 file that contains character codes to render.\n";
	std::cout << "-verifyAscii <out file>: verifies that all characters are within ASCII range (i.e. < 256) must come before ALL -cf commands!\n";
	std::cout << "-cf <file name> : shortcut for charFile.\n";
	std::cout << "\nPress Any Key\n";
	_getch();
}

} // namespace OVR

int main( int const argc, char const * argv[] )
{
	OVR::System::Init();

	OVR::PrintTitle();

	if ( argc == 1 )
	{
		OVR::PrintUsage();
		return 0;
	}

	FT_Library ftLibrary;
	FT_Error error = FT_Init_FreeType( &ftLibrary );
	if ( error )
	{
		std::cout << "An error occurred while initializing the FreeType library!\n";
		return -1;
	}

	int rc = OVR::CreateBitmapFont( argc, argv, ftLibrary );

	FT_Done_FreeType( ftLibrary );

	if ( rc == 0 )
	{
		std::cout << "Success.\n";
	}
	else
	{
		std::cout << "Failed.\n";
	}

	_getch();
	return rc;
}
