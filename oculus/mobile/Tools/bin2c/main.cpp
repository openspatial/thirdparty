
#define _CRT_SECURE_NO_WARNINGS

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <io.h>
#include <conio.h>
#include <direct.h>
#include <math.h>

#include "../../VRLib/jni/3rdParty/stb/stb_image.h"
#include "../../VRLib/jni/3rdParty/stb/stb_image_write.h"

class CBuffer {
public:
    CBuffer() :
        buffer( NULL ),
        size( 0 ) {
    }
    ~CBuffer() {
        delete [] buffer;
        size = 0;
    }
    unsigned char * buffer;
    size_t          size;
};

bool LoadFile( char const * filename, CBuffer & buffer ) {
    FILE * f = NULL;
    errno_t error = fopen_s( &f, filename, "rb" );
    if ( error != 0 ) {
        return false;
    }

    fseek( f, 0, SEEK_END );
    buffer.size = ftell( f );

    buffer.buffer = new unsigned char[buffer.size];

    fseek( f, 0, SEEK_SET );
    size_t countRead = fread_s( buffer.buffer, buffer.size, buffer.size, 1, f ); 

    fclose( f );
    f = NULL;

    if ( countRead != 1 ) {
        delete [] buffer.buffer;
        buffer.buffer = NULL;
        buffer.size = 0;
        return false;
    }

    return true;
}

bool WriteCFile( char const * outFileName, CBuffer const & buffer ) {
    // delete if it exists
    if ( _access( outFileName, 0 ) == 0 ) {
        if ( _access( outFileName, 02 ) != 0 )  {
            printf( "Access violation writing '%s'\n", outFileName );
            return false;
        }
    }

    remove( outFileName );

    FILE * f = NULL;
    errno_t error = fopen_s( &f, outFileName, "wb" );
    if ( error != 0 ) {
        return false;
    }

    fprintf( f, "static const size_t bufferSize = %u;\n", buffer.size );
    fprintf( f, "static unsigned char bufferData[] = {\n" );

    const int MAX_LINE_WIDTH = 96;
    const int TAB_SIZE = 4;
    int charCount = TAB_SIZE;
    for ( size_t i = 0; i < buffer.size; i++ ) {
        bool const lastChar = ( i == buffer.size - 1 );
        if ( buffer.buffer[i] == 0 ) {
            fprintf( f, "%s0x00%s",
                     charCount == TAB_SIZE ? "\t" : "",
                     lastChar ? "" : ", " );
        } else {
            fprintf( f, "%s0x%02x%s", 
                     charCount == TAB_SIZE ? "\t" : "",
                     buffer.buffer[ i ], 
                     lastChar ? "" : ", " );
        }
        charCount += 6;
        if ( lastChar || charCount + 6 > MAX_LINE_WIDTH ) {
            fprintf( f, "\n" );
            charCount = TAB_SIZE;
        }
    }

    fprintf( f, "};\n" );

    fclose( f );

    return true;
}

#define TOFLOAT( _x_ ) *(float*)( &_x_ )
#define TOINT( _x_ ) *(unsigned int*)( &_x_ )

int main( int const argc, char const * argv[] ) {
/*
    // just some code for testing floating point bit magic
    const unsigned int FLT_MANTISSA_SHIFT = 0;
    const unsigned int FLT_MANTISSA_BITS = 23;
    const unsigned int FLT_MANTISSA_MASK = ( ( 1 << FLT_MANTISSA_BITS ) - 1 ) << 0;
    const unsigned int FLT_EXP_BITS = 8;
    const unsigned int FLT_EXP_SHIFT = FLT_MANTISSA_BITS;
    const unsigned int FLT_EXP_MASK = ( ( 1 << FLT_EXP_BITS ) - 1 ) << FLT_EXP_SHIFT;
    const unsigned int FLT_SIGN_BITS = 1;
    const unsigned int FLT_SIGN_SHIFT = FLT_EXP_BITS + FLT_MANTISSA_BITS;
    const unsigned int FLT_SIGN_MASK = ( ( FLT_SIGN_BITS << FLT_SIGN_SHIFT ) - 1 ) << FLT_SIGN_SHIFT;

    unsigned int nonDenormal = 1 << FLT_MANTISSA_BITS;
    float nonDenormalf = TOFLOAT( nonDenormal );
    int t = int( nonDenormalf );
    float f = 1734.0f;
    int fi = TOINT( f );
    unsigned char b = (unsigned char)( f );

    unsigned int intMantissaMask = FLT_MANTISSA_MASK;
    unsigned int intExpMask = FLT_EXP_MASK;
    unsigned int intSignMask = FLT_SIGN_MASK;    

    
    float onef = 1.0f;
    float nonDenormalf = TOFLOAT( nonDenormal );
    float nonDenormalfSqrt = sqrt( nonDenormalf );
    unsigned int nonDenormalfSqrti = TOINT( nonDenormalfSqrt );
    unsigned int one = TOINT( onef );
    float onef_again = TOFLOAT( one );
    float smallf = 1e-18f;
    unsigned int smalli = TOINT( smallf );

    unsigned int tempi = ( 64 << FLT_EXP_SHIFT );
    float tempf = TOFLOAT( tempi );
    float tempfsq = tempf * tempf;
    unsigned int tempfsqi = TOINT( tempfsq );
*/

    if ( argc < 3 ) {
        printf( "bin2c v1.0 by Nelno the Amoeba\n" );
        printf( "Reads in a binary file and outputs a C file for embedding the binary file\n" );
		printf( "in code.\n" );
        printf( "USAGE: bin2c <binary file name> <out file name> [options]\n" );
		printf( "options:\n" );
		printf( "-image     : First convert image to RGBA raw data.\n" );
		return 0;
    }

    char const * binaryFileName = argv[1];
    const char * outFileName = argv[2];

    CBuffer buffer;

    if ( !LoadFile( binaryFileName, buffer ) ) {
        printf( "Error loading binary file '%s'\n", binaryFileName );
        return 1;
    }

	if ( argc >= 4 )
	{
		if ( strcmp( argv[3], "-image" ) == 0 )
		{
			printf( "Converting %s to raw RGBA data...\n", binaryFileName );
			int w, h, c;
			stbi_uc * image = stbi_load_from_memory( buffer.buffer, buffer.size, &w, &h, &c, 4 );
			delete [] buffer.buffer;
			buffer.size = w * h * 4 * sizeof( unsigned char );
			buffer.buffer = new unsigned char[buffer.size];
			memcpy( buffer.buffer, image, buffer.size );
			stbi_image_free( image );
		}
	}

    if ( !WriteCFile( outFileName, buffer ) ) {
        printf( "Error writing C file '%s'\n", outFileName );
        return 1;
    }

    return 0;
}
