/************************************************************************************

Filename    :   Image_Utils.cpp
Content     :   Image utility functions.
Created     :   May, 2014
Authors     :   Jonathan Wright

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/
#include <stdint.h>
#include <sys/stat.h>

#define FBX_TOOL
#include "../../VRLib/jni/LibOVR/Include/OVR.h"
#include "../../VRLib/jni/LibOVR/Src/Kernel/OVR_Hash.h"
#include "../../VRLib/jni/LibOVR/Src/Kernel/OVR_String_Utils.h"
#include "../../VRLib/jni/LibOVR/Src/Kernel/OVR_JSON.h"

#include "File_Utils.h"
#include "Image_Utils.h"

using namespace OVR;

const int PNG_IHDR_CHUNK_START	= 8;

struct PNG_IHDR_t
{
	unsigned int	Width;
	unsigned int	Height;
	unsigned char	BitDepth;
	unsigned char	ColorType;
	unsigned char	CompressionMethod;
	unsigned char	FilterMethod;
	unsigned char	InterlaceMethod;
};

const int PNG_HEADER_SIZE = PNG_IHDR_CHUNK_START + 8 + sizeof( PNG_IHDR_t );

enum PNG_ColorType
{
	Grayscale		= 0,
	RGB				= 2,
	Palette			= 3,
	GrayscaleAlpha	= 4,
	RGBA			= 6
};

static bool ImageIsPNG( char const * fileName, unsigned char const * buffer )
{
	if ( ( buffer[0] == 0x89 && buffer[1] == 0x50 && buffer[2] == 0x4E && buffer[3] == 0x47 ) )
	{
		// first chunk must be an IHDR
		return ( buffer[PNG_IHDR_CHUNK_START + 4] == 'I' &&
				 buffer[PNG_IHDR_CHUNK_START + 5] == 'H' &&
				 buffer[PNG_IHDR_CHUNK_START + 6] == 'D' &&
				 buffer[PNG_IHDR_CHUNK_START + 7] == 'R' );
	}
	return false;
}

static ImageProperties PNGProperties( unsigned char const * buffer )
{
	const PNG_IHDR_t * IHDR = reinterpret_cast< const PNG_IHDR_t * >( &(buffer[PNG_IHDR_CHUNK_START + 8]) );

	ImageProperties properties;
	properties.width = Alg::Max( Alg::ByteUtil::BEToSystem( IHDR->Width ), 1U );
	properties.height = Alg::Max( Alg::ByteUtil::BEToSystem( IHDR->Height ), 1U );
	properties.occlusion = ( IHDR->ColorType == RGBA ) ? IMAGE_TRANSPARENT : IMAGE_OPAQUE;

	return properties;
}

// header is broken into multiple structs because TGA headers are packed
struct TGA_HeaderStart_t
{
	char		IDLength;
	char		ColorMapType;
	char		DataTypeCode;
};

struct TGA_HeaderColor_t
{
	short int	ColorMapOrigin;
	short int	ColorMapLength;
	char		ColorMapDepth;
};

struct TGA_HeaderOrigin_t
{
	short int	XOrigin;
	short int	YOrigin;
	short		Width;
	short		Height;
	char		BitsPerPixel;
	char		ImageDescriptor;
};

const int TGA_HEADER_SIZE = 8 + sizeof( TGA_HeaderOrigin_t );

static bool ImageIsTGA( char const * fileName, unsigned char const * buffer )
{
	// TGA's have pretty ambiguous header so we simply check their file extension
	size_t len = strlen( fileName );
	if ( len < 4 )
	{
		return false;
	}
#if defined( __APPLE__ ) || defined( __linux__ )
	return strcasecmp( fileName + len - 4, ".tga" ) == 0;
#else
	return _stricmp( fileName + len - 4, ".tga" ) == 0;
#endif
}

static ImageProperties TGAProperties( unsigned char const * buffer )
{
	const TGA_HeaderOrigin_t * header = reinterpret_cast< const TGA_HeaderOrigin_t * >( &(buffer[8]) );

	ImageProperties properties;
	properties.width = Alg::Max( Alg::ByteUtil::LEToSystem( header->Width ), (short)1 );
	properties.height = Alg::Max( Alg::ByteUtil::LEToSystem( header->Height ), (short)1 );
	properties.occlusion = ( header->BitsPerPixel == 32 ) ? IMAGE_TRANSPARENT : IMAGE_OPAQUE;

	return properties;
}

ImageProperties GetImageProperties( char const * filePath )
{
	ImageProperties defaultProperties;
	defaultProperties.width = 1;
	defaultProperties.height = 1;
	defaultProperties.occlusion = IMAGE_OPAQUE;

	FILE * f = fopen( filePath, "rb" );
	if ( f == NULL )
	{
		return defaultProperties;
	}

	// This assumes every image file is at least as large as the largest header.
	const int maxHeaderSize = Alg::Max( PNG_HEADER_SIZE, TGA_HEADER_SIZE );
	unsigned char * buffer = new unsigned char[maxHeaderSize];
	if ( buffer == NULL )
	{
		return defaultProperties;
	}
	if ( fread( buffer, (size_t)maxHeaderSize, 1, f ) != 1 )
	{
		delete [] buffer;
		return defaultProperties;
	}

	if ( ImageIsPNG( filePath, buffer ) )
	{
		return PNGProperties( buffer );
	}
	else if ( ImageIsTGA( filePath, buffer ) )
	{
		return TGAProperties( buffer );
	}
	delete [] buffer;

	return defaultProperties;
}
