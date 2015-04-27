/************************************************************************************

Filename    :   main.cpp
Content     :   FBX converter
Created     :   May, 2014
Authors     :   J.M.P. van Waveren

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#include <stdint.h>
#if defined( __APPLE__ ) || defined( __linux__ )
#define fopen_s(a,b,c) *(a) = fopen(b,c)
#define _stricmp strcasecmp
#else
#include <conio.h>	// for _getch()
#endif

#define FBX_TOOL
#include "../../VRLib/jni/LibOVR/Include/OVR.h"
#include "../../VRLib/jni/LibOVR/Src/Kernel/OVR_Hash.h"
#include "../../VRLib/jni/LibOVR/Src/Kernel/OVR_String_Utils.h"
#include "../../VRLib/jni/LibOVR/Src/Kernel/OVR_JSON.h"

#include "../../VRLib/jni/3rdParty/stb/stb_image.h"
#include "../../VRLib/jni/3rdParty/stb/stb_image_write.h"

#include "File_Utils.h"

using namespace OVR;

#include "RawModel.h"
#include "Fbx2Raw.h"
#include "ModelData.h"
#include "Raw2RenderModel.h"
#include "Raw2CollisionModel.h"
#include "Raw2RayTraceModel.h"

void ParseModelsJsonTest( const char * jsonFileName, const char * binFileName );

#if defined( __APPLE__ ) || defined( __linux__ )
const char * mkDir = "mkdir";
const char * rmDir = "rm -rf";
const char * cpFile = "cp -r";
const char * rmFile = "rm";
const char * pckExe = "PVRTexTool/PVRTexToolCLI";
const char * zipExe = "7Zip/7z";
const char pathSeparator = '/';
#else
const char * mkDir = "mkdir";
const char * rmDir = "rmdir /s /q";
const char * cpFile = "copy /Y";
const char * rmFile = "del";
const char * pckExe = "PVRTexTool\\PVRTexToolCLI.exe";
const char * zipExe = "7Zip\\7z.exe";
const char * timeStampExe = "TimeStampx64.exe";
const char pathSeparator = '\\';
#endif
const char * jsonFileName = "models.json";
const char * binaryFileName = "models.bin";

enum TextureFormat
{
	TEXTURE_FORMAT_DEFAULT, // based on texture container
	TEXTURE_FORMAT_DXT,
	TEXTURE_FORMAT_ETC,
	TEXTURE_FORMAT_UNCOMPRESSED
};

enum TextureContainer
{
	TEXTURE_CONTAINER_KTX,
	TEXTURE_CONTAINER_PVR,
	TEXTURE_CONTAINER_DDS
};

enum LaunchApp
{
	APP_SCENE,
	APP_EXPO,
	APP_CINEMA
};

struct options_t
{
	options_t() :
		format( TEXTURE_FORMAT_DEFAULT ),
		container( TEXTURE_CONTAINER_KTX ),
		zip( 0 ),

		stripModoNumbers( false ),
		center( false ),
		repeat( false ),
		clean( false ),
		pack( false ),
		fullText( false ),
		allowAlpha( false ),
		tuscany( false ),

		pushToDevice( true ),
		testOnDevice( true ),
		launch( APP_SCENE ),

		cubeMapRotation( 0.0f ),
		cubeMapSwapXZ( false ),

		texLodFov( 0 ),
		texLodRes( 0 ),
		texLodAniso( 0.0f ),

		renderOrder( false )
	{
	}

	TextureFormat		format;
	TextureContainer	container;
	int					zip;				// 0 = none
											// 9 = ultra compression
	bool				stripModoNumbers;	// strip " (2)" etc. from mesh names
	bool				center;				// center the geometry
	bool				repeat;				// auto-repeat textures
	bool				clean;				// clean textures
	bool				pack;				// automatically run the pack batch file
	bool				fullText;			// include full geometry as text in JSON file
	bool				allowAlpha;			// alpha alpha blended/tested materials
	bool				tuscany;

	bool				pushToDevice;
	bool				testOnDevice;
	LaunchApp			launch;

	float				cubeMapRotation;
	bool				cubeMapSwapXZ;

	int					texLodFov;
	int					texLodRes;
	float				texLodAniso;

	bool				renderOrder;
};

struct jointAnim_t
{
	const char *			name;
	RawJointAnimation		animation;
	Vector3f				parameters;
	float					timeScale;
	float					timeOffset;
};

void Warning( const char * format, ... )
{
#if defined( __APPLE__ ) || defined( __linux__ )
	printf( "\x1B[33m" ); // yellow
	va_list args;
	va_start( args, format );
	vprintf( format, args );
	va_end( args );
	printf( "\033[0m" ); // reset
#else
	HANDLE hConsole = GetStdHandle( STD_OUTPUT_HANDLE );
    SetConsoleTextAttribute( hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY );

	va_list args;
	va_start( args, format );
	vprintf( format, args );
	va_end( args );

    SetConsoleTextAttribute( hConsole, FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_GREEN );
#endif
}

void Error( const char * format, ... )
{
#if defined( __APPLE__ ) || defined( __linux__ )
	printf( "\x1B[31m" ); // red
	va_list args;
	va_start( args, format );
	vprintf( format, args );
	va_end( args );
	printf( "\033[0m" ); // reset
#else
	HANDLE hConsole = GetStdHandle( STD_OUTPUT_HANDLE );
    SetConsoleTextAttribute( hConsole, FOREGROUND_RED | FOREGROUND_INTENSITY );

	va_list args;
	va_start( args, format );
	vprintf( format, args );
	va_end( args );

    SetConsoleTextAttribute( hConsole, FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_GREEN );
#endif
}

void PrintHighlight( const char * format, ... )
{
#if defined( __APPLE__ ) || defined( __linux__ )
	printf( "\x1B[37m" ); // white
	va_list args;
	va_start( args, format );
	vprintf( format, args );
	va_end( args );
	printf( "\033[0m" ); // reset
#else
	HANDLE hConsole = GetStdHandle( STD_OUTPUT_HANDLE );
    SetConsoleTextAttribute( hConsole, FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_INTENSITY );

	va_list args;
	va_start( args, format );
	vprintf( format, args );
	va_end( args );

    SetConsoleTextAttribute( hConsole, FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_GREEN );
#endif
}

static void ShowHelp()
{
	printf( "Usage: FbxConvert [<option> ... ] <visual.fbx>\n" );
	printf( "Options:\n" );

	// source data
	printf( "-o <output>                     : Specify the name for the .ovrscene file.\n" );
	printf( "                                  This is name is specified without extension.\n" );
	printf( "-render <model.fbx>             : Specify model for rendering.\n" );
	printf( "-collision <model.fbx|meshes>   : Specify model or meshes for wall collision.\n" );
	printf( "-ground <model.fbx|meshes>      : Specify model or meshes for floor collision.\n" );
	printf( "-raytrace <model.fbx|meshes>    : Specify model or meshes for focus tracking.\n" );

	// geometry transformations
	printf( "-translate <x> <y> <z>          : Translate the models by x,y,z.\n" );
	printf( "-rotate <degrees>               : Rotate the models about the Y axis.\n" );
	printf( "-scale <factor>                 : Scale the models by the given factor.\n" );
	printf( "-center                         : Center the geometry.\n" );
	printf( "-swapXZ                         : Swap the X and Z axis.\n" );
	printf( "-flipU                          : Flip the U texture coordinate.\n" );
	printf( "-flipV                          : Flip the V texture coordinate.\n" );
	printf( "-stripModoNumbers               : Strip duplicate name numbers Modo adds.\n" );
	printf( "-sort <+|-><X|Y|Z|origin>       : Sort geometry along axis or from origin.\n" );
	printf( "-expand <dist>                  : Expand collision walls by this distance.\n" );
	printf( "                                  Defaults to 0.5\n" );

	// geometry manipulation
	printf( "-remove <mesh1> [<mesh2> ...]   : Remove these source meshes for rendering.\n" );
	printf( "-atlas <mesh1> [<mesh2> ...]    : Create texture atlas for these meshes.\n" );
	printf( "-discrete <mesh1> [<mesh2> ...] : Keep these meshes separate for rendering.\n" );
	printf( "-skin <mesh1> [<mesh2> ...]     : Skin these source meshes rigidly to a joint.\n" );
	printf( "-tag <mesh1> [<mesh2> ...]      : Turn 1st triangles of these meshes into tags.\n" );
	printf( "-attrib <attr1> [<attr2> ...]   : Only keep these attributes: [position,\n" );
	printf( "                                  normal, tangent, binormal, color, uv0, uv1].\n" );

	// animations
	printf( "-anim <rotate> <pitch> <yaw> <roll> <timeoffset> <timescale> <joint1> [<joint2> ...]\n" );
	printf( "-anim <sway> <pitch> <yaw> <roll> <timeoffset> <timescale> <joint1> [<joint2> ...]\n" );
	printf( "-anim <bob> <X> <Y> <Z> <timeoffset> <timescale> <joint1> [<joint2>...]\n" );
	printf( "                                : Apply parametric animation to joints.\n" );

	// analysis
	printf( "-texlod <fov> <res> <aniso>     : Vertex color based on visible texture LOD.\n" );
	printf( "-renderorder                    : Vertex color based on render order.\n" );

	// compression / packing / testing
	printf( "-dxt                            : Use DXT texture compression format.\n" );
	printf( "-etc                            : Use ETC2 texture compression format.\n" );
	printf( "-uncompressed                   : Use uncompressed texture format (RGBA8).\n" );
	printf( "-ktx                            : Compress textures to KTX files (default).\n" );
	printf( "-pvr                            : Compress textures to PVR files.\n" );
	printf( "-dds                            : Compress textures to DDS files.\n" );
	printf( "-alpha                          : Keep texture alpha channels if present.\n" );
	printf( "-clean                          : Delete previously compressed textures.\n" );
	printf( "-include <file1> [<file2> ...]  : Include these files in the package.\n" );
	printf( "-pack                           : Automatically run <output>_pack.bat file.\n" );
	printf( "-zip <x>                        : 7-Zip compression level (0=none, 9=ultra).\n" );
	printf( "-fullText                       : Store binary data as text in JSON file.\n" );
	printf( "-noPush                         : Do not push to device in batch file.\n" );
	printf( "-noTest                         : Do not run a test scene from batch file.\n" );
	printf( "-cinema                         : Launch VrCinema instead of VrScene.\n" );
	printf( "-expo                           : Launch VrExpo instead of VrScene.\n" );
}

static bool LoadRawModel( RawModel & raw, const char * fbxFileName, const char * textureExtensions,
					const Matrix4f & geometryTransform, const Matrix3f & texturesTransform )
{
	printf( "loading '%s'...\n", fbxFileName );

	if ( !LoadFBXFile( fbxFileName, textureExtensions, raw ) )
	{
		Warning( "failed to load '%s'\n", fbxFileName );
		return false;
	}

	raw.TransformGeometry( geometryTransform );
	raw.TransformTextures( texturesTransform );

	return true;
}

/*
	Programmatically replace the ocean in Tuscany with a giant quad.
*/
static void TuscanyHack( RawModel & raw )
{
	RawVertex v0;
	v0.position = Vector3f( -210, -12, -456 );
	v0.uv0 = Vector2f( 0, 0 );

	RawVertex v1;
	v1.position = Vector3f( -210, -12,  456 );
	v1.uv0 = Vector2f( 8, 0 );

	RawVertex v2;
	v2.position = Vector3f(  466, -12,  456 );
	v2.uv0 = Vector2f( 8, 8 );

	RawVertex v3;
	v3.position = Vector3f(  466, -12, -456 );
	v3.uv0 = Vector2f( 0, 8 );

	const int i0 = raw.AddVertex( v0 );
	const int i1 = raw.AddVertex( v1 );
	const int i2 = raw.AddVertex( v2 );
	const int i3 = raw.AddVertex( v3 );

	int diffuseTexture = -1;
	for ( int i = 0; i < raw.GetTextureCount(); i++ )
	{
		if ( strstr( raw.GetTexture( i ).name, "Ocean.tga" ) )
		{
			diffuseTexture = i;
			break;
		}
	}

	const int surfaceIndex = raw.AddSurface( "Ocean", Matrix4f() );
	const int textures[RAW_TEXTURE_USAGE_MAX] = { diffuseTexture, -1, -1, -1 };
	const int materialIndex = raw.AddMaterial( RAW_MATERIAL_TYPE_OPAQUE, textures );
	raw.AddTriangle( i0, i1, i2, materialIndex, surfaceIndex );
	raw.AddTriangle( i0, i2, i3, materialIndex, surfaceIndex );
}

static void RemoveDuplicateTriangles( RawModel & raw )
{
	int duplicates = raw.RemoveDuplicateTriangles();
	printf( "%7d duplicate triangles\n", duplicates );
}

static int WildCardICmp( const char * str, const char * match )
{
	if ( match[0] == '\0' && str[0] == '\0' )
	{
		return 0;
	}

	if ( match[0] == '*' && match[1] != '\0' && str[0] == '\0' )
	{
		return match[0];
	}

	if ( match[0] == '?' || tolower( match[0] ) == tolower( str[0] ) )
	{
		return WildCardICmp( str + 1, match + 1 );
	}

	if ( match[0] == '*' )
	{
		if ( WildCardICmp( str, match + 1 ) == 0 )
		{
			return 0;
		}
		return WildCardICmp( str + 1, match );
	}

	return match[0] - str[0];
}

static void RemoveRawModelSurfaces( RawModel & raw, const Array< const char * > & removeSurfaces )
{
	for ( int i = 0; i < raw.GetSurfaceCount(); i++ )
	{
		for ( int j = 0; j < removeSurfaces.GetSizeI(); j++ )
		{
			if ( WildCardICmp( raw.GetSurface( i ).name, removeSurfaces[j] ) == 0 )
			{
				printf( "Removed render surface '%s'\n", raw.GetSurface( i ).name.ToCStr() );
				raw.RemoveSurface( i );
				i--;
				break;
			}
		}
	}
}

static void KeepSelectedSurfaces( RawModel & raw, const Array< const char * > & keepSurfaces, const char * label )
{
	for ( int i = 0; i < raw.GetSurfaceCount(); i++ )
	{
		bool keep = false;
		for ( int j = 0; j < keepSurfaces.GetSizeI(); j++ )
		{
			if ( WildCardICmp( raw.GetSurface( i ).name, keepSurfaces[j] ) == 0 )
			{
				keep = true;
				break;
			}
		}
		if ( !keep )
		{
			printf( "Removed %s surface '%s'\n", label, raw.GetSurface( i ).name.ToCStr() );
			raw.RemoveSurface( i );
			i--;
		}
	}
}

static void AtlasRawModelSurfaces( RawModel & raw, const Array< const char * > atlasSurfaces[MAX_ATLASES],
									const int numAtlases, const char * textureFolder, const bool repeatTextures )
{
	for ( int atlas = 0; atlas < numAtlases; atlas++ )
	{
		for ( int i = 0; i < raw.GetSurfaceCount(); i++ )
		{
			for ( int j = 0; j < atlasSurfaces[atlas].GetSizeI(); j++ )
			{
				if ( WildCardICmp( raw.GetSurface( i ).name, atlasSurfaces[atlas][j] ) == 0 )
				{
					printf( "Atlas %d surface '%s'\n", atlas, raw.GetSurface( i ).name.ToCStr() );
					raw.SetSurfaceAtlas( i, atlas );
					break;
				}
			}
		}
	}
	raw.CreateTextureAtlases( textureFolder, repeatTextures );
}

static void DiscretizeRawModelSurfaces( RawModel & raw, const Array< const char * > & discreteSurfaces )
{
	for ( int i = 0; i < raw.GetSurfaceCount(); i++ )
	{
		for ( int j = 0; j < discreteSurfaces.GetSizeI(); j++ )
		{
			if ( WildCardICmp( raw.GetSurface( i ).name, discreteSurfaces[j] ) == 0 )
			{
				printf( "Discrete surface '%s'\n", raw.GetSurface( i ).name.ToCStr() );
				raw.SetSurfaceDiscrete( i, true );
				break;
			}
		}
	}
}

static void SkinRawModelSurfaces( RawModel & raw, const Array< const char * > & skinSurfaces )
{
	for ( int i = 0; i < raw.GetSurfaceCount(); i++ )
	{
		for ( int j = 0; j < skinSurfaces.GetSizeI(); j++ )
		{
			if ( WildCardICmp( raw.GetSurface( i ).name, skinSurfaces[j] ) == 0 )
			{
				printf( "Skinned surface '%s'\n", raw.GetSurface( i ).name.ToCStr() );
				raw.SetSurfaceSkinned( i, true );
				break;
			}
		}
	}
	raw.SkinRigidSurfaces();
}

static void SetJointAnimations( RawModel & raw, const Array< jointAnim_t > & jointAnimations )
{
	for ( int i = 0; i < jointAnimations.GetSizeI(); i++ )
	{
		if ( raw.SetJointAnimation( jointAnimations[i].name, jointAnimations[i].animation, jointAnimations[i].parameters, jointAnimations[i].timeOffset, jointAnimations[i].timeScale ) )
		{
			printf( "Animated joint '%s'\n", jointAnimations[i].name );
		}
		else
		{
			Warning( "joint '%s' not found\n", jointAnimations[i].name );
		}
	}
}

static void TagRawModelSurfaces( RawModel & raw, const Array< const char * > & tagSurfaces )
{
	for ( int i = 0; i < raw.GetSurfaceCount(); i++ )
	{
		for ( int j = 0; j < tagSurfaces.GetSizeI(); j++ )
		{
			if ( WildCardICmp( raw.GetSurface( i ).name, tagSurfaces[j] ) == 0 )
			{
				printf( "Tagged surface '%s'\n", raw.GetSurface( i ).name.ToCStr() );
				raw.AddTag( i );
				break;
			}
		}
	}
}

static void AddRawModelTextures( RawModel & raw, const Array< const char * > & includeFiles, const char * textureExtensions )
{
	for ( int i = 0; i < includeFiles.GetSizeI(); i++ )
	{
		char ext[ 100 ];
		StringUtils::GetFileExtension( ext, includeFiles[i] );
		if ( FileUtils::MatchExtension( ext, textureExtensions ) )
		{
			raw.AddTexture( includeFiles[i], RAW_TEXTURE_USAGE_DIFFUSE );
		}
	}
}

bool WriteBinary( const char * binaryPath,
					const ModelData * data_render_model,
					const ModelData * data_collision_model,
					const ModelData * data_ground_collision_model,
					const ModelData * data_raytrace_model )
{
	FILE * fp = fopen( binaryPath, "wb" );
	if ( fp == NULL )
	{
		return false;
	}
	const unsigned int header = 0x6272766F;	// little endian "ovrb"
	if ( fwrite( &header, sizeof( header ), 1, fp ) != 1 )
	{
		fclose( fp );
		return false;
	}
	if ( data_render_model != NULL && data_render_model->binary.GetSize() != 0 )
	{
		if ( fwrite( data_render_model->binary.GetDataPtr(), data_render_model->binary.GetSize(), 1, fp ) != 1 )
		{
			fclose( fp );
			return false;
		}
	}
	if ( data_collision_model != NULL && data_collision_model->binary.GetSize() != 0 )
	{
		if ( fwrite( data_collision_model->binary.GetDataPtr(), data_collision_model->binary.GetSize(), 1, fp ) != 1 )
		{
			fclose( fp );
			return false;
		}
	}
	if ( data_ground_collision_model != NULL && data_ground_collision_model->binary.GetSize() != 0 )
	{
		if ( fwrite( data_ground_collision_model->binary.GetDataPtr(), data_ground_collision_model->binary.GetSize(), 1, fp ) != 1 )
		{
			fclose( fp );
			return false;
		}
	}
	if ( data_raytrace_model != NULL && data_raytrace_model->binary.GetSize() != 0 )
	{
		if ( fwrite( data_raytrace_model->binary.GetDataPtr(), data_raytrace_model->binary.GetSize(), 1, fp ) != 1 )
		{
			fclose( fp );
			return false;
		}
	}
	fclose( fp );
	return true;
}

void RotateImage( const char * texturePath, const char * rotatedTexturePath, const int rotateDegrees )
{
	int angle = rotateDegrees;
	while ( angle < 0 ) { angle += 360; }
	while ( angle > 360 ) { angle -= 360; }

	int rawSourceWidth;
	int rawSourceHeight;
	int rawSourceComponents;
	stbi_uc * rawSource = stbi_load( texturePath, &rawSourceWidth, &rawSourceHeight, &rawSourceComponents, 4 );

	int rawDestinationWidth = rawSourceWidth;
	int rawDestinationHeight = rawSourceHeight;
	int rawDestinationComponents = 4;
	stbi_uc * rawDestination = (stbi_uc *) malloc( rawSourceWidth * rawSourceHeight * 4 );

	switch( angle )
	{
		case 0:
		{
			// do nothing
			break;
		}
		case 90:
		{
			rawDestinationWidth = rawSourceHeight;
			rawDestinationHeight = rawSourceWidth;
			for ( int y = 0; y < rawSourceHeight; y++ )
			{
				for ( int x = 0; x < rawSourceWidth; x++ )
				{
					memcpy( rawDestination + ( x * rawDestinationWidth + rawDestinationWidth - 1 - y ) * 4, rawSource + ( y * rawSourceWidth + x ) * 4, 4 );
				}
			}
			break;
		}
		case 180:
		{
			for ( int y = 0; y < rawSourceHeight; y++ )
			{
				for ( int x = 0; x < rawSourceWidth; x++ )
				{
					memcpy( rawDestination + ( ( rawDestinationHeight - 1 - y ) * rawDestinationWidth + rawDestinationWidth - 1 - x ) * 4, rawSource + ( y * rawSourceWidth + x ) * 4, 4 );
				}
			}
			break;
		}
		case 270:
		{
			rawDestinationWidth = rawSourceHeight;
			rawDestinationHeight = rawSourceWidth;
			for ( int y = 0; y < rawSourceHeight; y++ )
			{
				for ( int x = 0; x < rawSourceWidth; x++ )
				{
					memcpy( rawDestination + ( ( rawDestinationHeight - 1 - x ) * rawDestinationWidth + y ) * 4, rawSource + ( y * rawSourceWidth + x ) * 4, 4 );
				}
			}
			break;
		}
		default:
		{
			Warning( "unsupported image rotation of %d degrees\n", angle );
		}
	}

	stbi_write_png( rotatedTexturePath, rawDestinationWidth, rawDestinationHeight, 4, rawDestination, rawDestinationWidth * 4 );

	free( rawSource );
	free( rawDestination );
}

bool CreateBatchFile( const char * batchFilePath, const char * exePath, const char * outputName, const RawModel & raw, const options_t & options )
{
	FILE * f = NULL;
	fopen_s( &f, batchFilePath, "w" );
	if ( f == NULL )
	{
		printf( "failed to create '%s'\n", batchFilePath );
		return false;
	}

	// cube map transform
	int cubeMapUpDownRotate = 180;
	if ( options.cubeMapRotation != 0.0f )
	{
		if ( options.cubeMapRotation == 90.0f )
		{
			printf( "Rotating cube maps by 90 degrees\n" );
			Alg::Swap( cubeMapSidesFileBaseExtensions[0], cubeMapSidesFileBaseExtensions[1] );
			Alg::Swap( cubeMapSidesFileBaseExtensions[0], cubeMapSidesFileBaseExtensions[4] );
			Alg::Swap( cubeMapSidesFileBaseExtensions[1], cubeMapSidesFileBaseExtensions[5] );
			cubeMapUpDownRotate += 90;
		}
		else if ( options.cubeMapRotation == 180.0f )
		{
			printf( "Rotating cube maps by 180 degrees\n" );
			Alg::Swap( cubeMapSidesFileBaseExtensions[0], cubeMapSidesFileBaseExtensions[1] );
			Alg::Swap( cubeMapSidesFileBaseExtensions[4], cubeMapSidesFileBaseExtensions[5] );
			cubeMapUpDownRotate -= 180;
		}
		else if ( options.cubeMapRotation == 270.0f )
		{
			printf( "Rotating cube maps by 270 degrees\n" );
			Alg::Swap( cubeMapSidesFileBaseExtensions[4], cubeMapSidesFileBaseExtensions[5] );
			Alg::Swap( cubeMapSidesFileBaseExtensions[0], cubeMapSidesFileBaseExtensions[4] );
			Alg::Swap( cubeMapSidesFileBaseExtensions[1], cubeMapSidesFileBaseExtensions[5] );
			cubeMapUpDownRotate -= 90;
		}
		else
		{
			Warning( "Cube map rotation of %1.0f degrees not supported.\n", options.cubeMapRotation );
		}
	}
	if ( options.cubeMapSwapXZ ) {
		printf( "Swapping cube maps X-Z\n" );
		Alg::Swap( cubeMapSidesFileBaseExtensions[4], cubeMapSidesFileBaseExtensions[5] );
		Alg::Swap( cubeMapSidesFileBaseExtensions[0], cubeMapSidesFileBaseExtensions[4] );
		Alg::Swap( cubeMapSidesFileBaseExtensions[1], cubeMapSidesFileBaseExtensions[5] );
		cubeMapUpDownRotate -= 90;
	}

	const String exeFolder = StringUtils::GetCleanPathString( StringUtils::GetFolderString( exePath ), pathSeparator );
	const String curFolder = StringUtils::GetCleanPathString( FileUtils::GetCurrentFolder(), pathSeparator );
	const String absFolder = ( strncmp( exeFolder,  "..", 2 ) == 0 ) ? StringUtils::GetCleanPathString( curFolder + exeFolder, pathSeparator ) : exeFolder;

	const String tmpFolder = (const char *)StringUtils::Va( "%s%s_tmp%c", curFolder.ToCStr(), outputName, pathSeparator );
	const String pckFolder = (const char *)StringUtils::Va( "%s%s_tmp%cpack", curFolder.ToCStr(), outputName, pathSeparator );
#if defined( __APPLE__ ) || defined( __linux__ )
	fprintf( f, "echo Generated by FbxConvert\n" );
	if ( options.clean )
	{
		fprintf( f, "rm -rf \"%s\"\n", pckFolder.ToCStr() );
	}
	fprintf( f, "mkdir \"%s\"\n", pckFolder.ToCStr() );
	fprintf( f, "cp -r \"%s%s\" \"%s\"\n", tmpFolder.ToCStr(), jsonFileName, pckFolder.ToCStr() );
	fprintf( f, "cp -r \"%s%s\" \"%s\"\n", tmpFolder.ToCStr(), binaryFileName, pckFolder.ToCStr() );
#else
	fprintf( f, "rem Generated by FbxConvert\n" );
	if ( options.clean )
	{
		fprintf( f, "rmdir /s /q \"%s\"\n", pckFolder.ToCStr() );
	}
	fprintf( f, "mkdir \"%s\"\n", pckFolder.ToCStr() );
	fprintf( f, "copy /Y \"%s%s\" \"%s\"\n", tmpFolder.ToCStr(), jsonFileName, pckFolder.ToCStr() );
	fprintf( f, "copy /Y \"%s%s\" \"%s\"\n", tmpFolder.ToCStr(), binaryFileName, pckFolder.ToCStr() );
#endif

	for ( int i = 0; i < raw.GetTextureCount(); i++ )
	{
		const RawTexture & texture = raw.GetTexture( i );
		const String sourceTexture = StringUtils::GetCleanPathString( texture.name, pathSeparator );

		const String textureFolder = StringUtils::GetFolderString( sourceTexture );
		const String textureBase = StringUtils::GetFileBaseString( sourceTexture );
		const String textureExt = StringUtils::GetFileExtensionString( sourceTexture );

		const char * compressedExt =	( options.container == TEXTURE_CONTAINER_KTX ? ".ktx" :
										( options.container == TEXTURE_CONTAINER_PVR ? ".pvr" :
										( options.container == TEXTURE_CONTAINER_DDS ? ".dds" :
										".ktx" ) ) );
		const String compressedSource = StringUtils::SetFileExtensionString( sourceTexture, compressedExt );

		// Check if the source texture exists.
		if ( !FileUtils::FileExists( sourceTexture ) )
		{
			Warning( "WARNING: missing texture \"%s\"\n", sourceTexture.ToCStr() );
		}

		// If a compressed texture exists in the source folder.
		if ( FileUtils::FileExists( compressedSource ) )
		{
			// Just copy the compressed texture to the pack folder.
			fprintf( f, "%s \"%s\" \"%s\\%s%s\"\n", cpFile, compressedSource.ToCStr(), pckFolder.ToCStr(), textureBase.ToCStr(), compressedExt );
			continue;
		}

		bool needsAlpha = texture.usage == RAW_TEXTURE_USAGE_DIFFUSE && options.allowAlpha && texture.occlusion != RAW_TEXTURE_OCCLUSION_OPAQUE;

		const char * format;
		switch ( options.format )
		{
		case TEXTURE_FORMAT_DXT: 
			format = needsAlpha ? "BC3" : "BC1";
			break;
		case TEXTURE_FORMAT_ETC:
			format = needsAlpha ? "ETC2_RGBA" : "ETC2_RGB";
			break;
		case TEXTURE_FORMAT_UNCOMPRESSED:
			format = needsAlpha ? "r8g8b8a8" : "r8g8b8";
			break;
		default:
			// should assert here
			break;
		}
		String textureInput = String( "\"" ) + sourceTexture + String( "\"" );
		String textureOutput = String( "\"" ) + pckFolder + String( &pathSeparator, 1 ) + textureBase + compressedExt + String( "\"" );
		String cubeMapOption = "";

		if ( texture.usage == RAW_TEXTURE_USAGE_REFLECTION )
		{
			const String cubeMapBase = StripCubeMapSidesFileBaseExtensions( textureBase );
			textureInput.Clear();
			for ( int i = 0; i < 6; i++ )
			{
				if ( i > 0 )
				{
					textureInput += ",";
				}
				const String texturePath = textureFolder + cubeMapBase + cubeMapSidesFileBaseExtensions[i] + "." + textureExt;
				if ( cubeMapUpDownRotate != 0 && _stricmp( cubeMapSidesFileBaseExtensions[i], "_up" ) == 0 )
				{
					const String rotatedTexturePath = tmpFolder + cubeMapBase + cubeMapSidesFileBaseExtensions[i] + ".png";
					RotateImage( texturePath, rotatedTexturePath, cubeMapUpDownRotate );
					textureInput += String( "\"" ) + rotatedTexturePath + String( "\"" );
				}
				else if ( cubeMapUpDownRotate != 0 && _stricmp( cubeMapSidesFileBaseExtensions[i], "_down" ) == 0 )
				{
					const String rotatedTexturePath = tmpFolder + cubeMapBase + cubeMapSidesFileBaseExtensions[i] + ".png";
					RotateImage( texturePath, rotatedTexturePath, -cubeMapUpDownRotate );
					textureInput += String( "\"" ) + rotatedTexturePath + String( "\"" );
				}
				else
				{
					textureInput += String( "\"" ) + texturePath + String( "\"" );
				}
			}
			textureOutput = pckFolder + String( &pathSeparator, 1 ) + cubeMapBase + cubeMapFileBaseExtension + compressedExt;
			cubeMapOption = " -cube -flip x";
		}

#if defined( __APPLE__ ) || defined( __linux__ )
		fprintf( f, "if [ \"%s\" -nt %s ]\nthen\n",
						sourceTexture.ToCStr(),
						textureOutput.ToCStr() );
		fprintf( f, "\t\"%s%s\" -i %s -o %s -f %s -q etcfast -m%s\n",
						absFolder.ToCStr(),		// exe
						pckExe,					// exe
						textureInput.ToCStr(),	// -i
						textureOutput.ToCStr(),	// -o
						format,					// -f
						cubeMapOption.ToCStr()	// -cube
						);
		fprintf( f, "fi\n");
#else
		fprintf( f, "\"%s%s\" \"%s\" %s\n",
						absFolder.ToCStr(),
						timeStampExe,
						sourceTexture.ToCStr(),
						textureOutput.ToCStr() );
		fprintf( f, "if %%ERRORLEVEL%% == 1 \"%s%s\" -i %s -o %s -f %s -q etcfast -m%s\n",
						absFolder.ToCStr(),		// exe
						pckExe,					// exe
						textureInput.ToCStr(),	// -i
						textureOutput.ToCStr(),	// -o
						format,					// -f
						cubeMapOption.ToCStr()	// -cube
						);
#endif
	}

	fprintf( f, "%s \"%s.ovrscene\"\n", rmFile, outputName );
	// Note that the ./<dir>/* file format (with forward slashes only!) makes 7z not include the path with the filename in the archive.
	fprintf( f, "\"%s%s\" a -tzip -mx%d %s.ovrscene ./%s/*\n", absFolder.ToCStr(), zipExe, options.zip, outputName,
			StringUtils::GetRelativePathString( StringUtils::GetCleanPathString( pckFolder, '/' ), curFolder, '/' ).ToCStr() );

	if ( options.pushToDevice && options.container != TEXTURE_CONTAINER_DDS )
	{
		const char * intentFolder =	( options.launch == APP_SCENE  ) ? "" :
									( options.launch == APP_EXPO   ) ? "Expo/" :
									( options.launch == APP_CINEMA ) ? "Cinema/Theaters/" : "";
								
		fprintf( f, "adb push \"%s.ovrscene\" /sdcard/oculus/%s%s.ovrscene\n", outputName, intentFolder, outputName );
		if ( options.testOnDevice )
		{
			const char * appName =	( options.launch == APP_SCENE  ) ? "com.oculusvr.vrscene" :
									( options.launch == APP_EXPO   ) ? "com.oculusvr.vrexpo" :
									( options.launch == APP_CINEMA ) ? "com.oculus.cinema" : "com.oculusvr.vrscene";
			fprintf( f, "adb shell am force-stop com.oculusvr.%s/%s.MainActivity\n", appName, appName );
			fprintf( f, "adb shell am start -d file:/sdcard/oculus/%s%s.ovrscene -t model/vnd.oculusvr.ovrscene -n %s/%s.MainActivity\n", intentFolder, outputName, appName, appName );
		}
	}
	fclose( f );

	return true;
}

bool CheckFile( const char * fileName )
{
	if ( FileUtils::FileExists( fileName ) )
	{
		printf( "found %s\n", fileName );
		return true;
	}
	Error( "couldn't find %s\n", fileName );
	return false;
}

int main( int argc, char * argv[] )
{
	PrintHighlight( "FbxConvert version 1.0\n" );
	PrintHighlight( "Copyright (c) 2015 Oculus VR, LLC.\n" );

	OVR::Allocator::setInstance( OVR::DefaultAllocator::InitSystemSingleton() );

#if !defined( _DEBUG )
	const String exeFolder = StringUtils::GetCleanPathString( StringUtils::GetFolderString( argv[0] ), pathSeparator );
	const String curFolder = StringUtils::GetCleanPathString( FileUtils::GetCurrentFolder(), pathSeparator );
	const String absFolder = ( strncmp( exeFolder,  "..", 2 ) == 0 ) ? StringUtils::GetCleanPathString( curFolder + exeFolder, pathSeparator ) : exeFolder;

	printf( "exe folder = %s\n", exeFolder.ToCStr() );
	printf( "cur folder = %s\n", curFolder.ToCStr() );
	printf( "abs folder = %s\n", absFolder.ToCStr() );

	if (	!CheckFile( absFolder + pckExe ) ||
			!CheckFile( absFolder + zipExe ) )
	{
		ShowHelp();
		return -1;
	}
#endif

	if ( argc <= 1 )
	{
		ShowHelp();
		return -1;
	}

	const char * renderFbxFileName = NULL;
	const char * wallCollisionFbxFileName = NULL;
	const char * groundCollisionFbxFileName = NULL;
	const char * rayTraceFbxFileName = NULL;
	const char * outputName = "default";
	options_t options;
	Array< const char * > removeSurfaces;
	Array< const char * > discreteSurfaces;
	Array< const char * > skinSurfaces;
	Array< const char * > tagSurfaces;
	Array< const char * > atlasSurfaces[MAX_ATLASES];
	Array< const char * > wallCollisionSurfaces;
	Array< const char * > groundCollisionSurfaces;
	Array< const char * > rayTraceSurfaces;
	Array< const char * > includeFiles;
	Array< jointAnim_t > jointAnimations;
	int numAtlases = 0;
	Matrix4f geometryTransform;
	Matrix3f texturesTransform;
	int sortAxis = 0;
	int keepAttribs = -1;
	float expandCollision = 0.0f;

	for ( int i = 1; i < argc; i++ )
	{
		const char * s = argv[i];
		if ( s[0] == '-' )
		{
			s++;
		}
		else
		{
			renderFbxFileName = argv[i];
			printf( "render: %s\n", renderFbxFileName );
			break;
		}
		if ( _stricmp( s, "o" ) == 0 )
		{
			i++;
			outputName = argv[i];
			printf( "output: %s.ovrscene\n", outputName );
		}
		else if ( _stricmp( s, "render" ) == 0 )
		{
			i++;
			renderFbxFileName = argv[i];
			printf( "render: %s\n", renderFbxFileName );
		}
		else if ( _stricmp( s, "collision" ) == 0 )
		{
			if ( StringUtils::GetFileExtensionString( argv[i + 1] ).CompareNoCase( "fbx" ) == 0 )
			{
				wallCollisionFbxFileName = argv[++i];
				printf( "collision: %s\n", wallCollisionFbxFileName );
			}
			else
			{
				for ( ; i + 1 < argc && argv[i + 1][0] != '-'; i++ )
				{
					wallCollisionSurfaces.PushBack( argv[i + 1] );
				}
			}
		}
		else if ( _stricmp( s, "ground" ) == 0 )
		{
			if ( StringUtils::GetFileExtensionString( argv[i + 1] ).CompareNoCase( "fbx" ) == 0 )
			{
				groundCollisionFbxFileName = argv[++i];
				printf( "ground: %s\n", groundCollisionFbxFileName );
			}
			else
			{
				for ( ; i + 1 < argc && argv[i + 1][0] != '-'; i++ )
				{
					groundCollisionSurfaces.PushBack( argv[i + 1] );
				}
			}
		}
		else if ( _stricmp( s, "raytrace" ) == 0 )
		{
			if ( StringUtils::GetFileExtensionString( argv[i + 1] ).CompareNoCase( "fbx" ) == 0 )
			{
				rayTraceFbxFileName = argv[++i];
				printf( "raytrace: %s\n", rayTraceFbxFileName );
			}
			else
			{
				for ( ; i + 1 < argc && argv[i + 1][0] != '-'; i++ )
				{
					rayTraceSurfaces.PushBack( argv[i + 1] );
				}
			}
		}
		else if ( _stricmp( s, "center" ) == 0 )
		{
			options.center = true;
			printf( "centering geometry\n" );
		}
		else if ( _stricmp( s, "translate" ) == 0 )
		{
			i += 3;
			const float translate[3] = { (float)atof( argv[i - 2] ), (float)atof( argv[i - 1] ), (float)atof( argv[i - 0] ) };
			geometryTransform = Matrix4f::Translation( translate[0], translate[1], translate[2] ) * geometryTransform;
			printf( "translating by (%f, %f, %f)\n", translate[0], translate[1], translate[2] );
		}
		else if ( _stricmp( s, "rotate" ) == 0 )
		{
			i++;
			float degrees = (float)atof( argv[i] );
			while ( degrees < 0.0f ) { degrees += 360.0f; }
			while ( degrees > 360.0f ) { degrees -= 360.0f; }
			geometryTransform = Matrix4f::RotationY( degrees * Math<float>::DegreeToRadFactor ) * geometryTransform;
			printf( "rotating about Y by %f degrees\n", degrees );
			options.cubeMapRotation = degrees;
		}
		else if ( _stricmp( s, "scale" ) == 0 )
		{
			i++;
			const float scaleFactor = (float)atof( argv[i] );
			geometryTransform = Matrix4f::Scaling( scaleFactor ) * geometryTransform;
			printf( "scaling by %f\n", scaleFactor );
		}
		else if ( _stricmp( s, "swapXZ" ) == 0 )
		{
			const Matrix4f swapXZ(	0, 0, -1, 0,		0, 1, 0, 0,		1, 0, 0, 0,		0, 0, 0, 1 );
			geometryTransform = swapXZ * geometryTransform;
			printf( "swapping world X-Z\n" );
			options.cubeMapSwapXZ = true;
		}
		else if ( _stricmp( s, "flipU" ) == 0 )
		{
			texturesTransform.M[0][0] = -1.0f;
			texturesTransform.M[0][2] = 1.0f;
			printf( "flipping texture U\n" );
		}
		else if ( _stricmp( s, "flipV" ) == 0 )
		{
			texturesTransform.M[1][1] = -1.0f;
			texturesTransform.M[1][2] = 1.0f;
			printf( "flipping texture V\n" );
		}
		else if ( _stricmp( s, "repeat" ) == 0 )
		{
			options.repeat = true;
			printf( "repeat textures\n" );
		}
		else if ( _stricmp( s, "stripModoNumbers" ) == 0 )
		{
			options.stripModoNumbers = true;
			printf( "strip Modo numbers\n" );
		}
		else if ( _stricmp( s, "sort" ) == 0 )
		{
			i++;
			int dir = 1;
			const char * axis = argv[i];
			if ( axis[0] == '+' ) { dir = 1; axis++; }
			else if ( axis[0] == '-' ) { dir = -1; axis++; }
			if ( _stricmp( axis, "X" ) == 0 ) { sortAxis = SORT_AXIS_POS_X; }
			else if ( _stricmp( axis, "Y" ) == 0 ) { sortAxis = SORT_AXIS_POS_Y; }
			else if ( _stricmp( axis, "Z" ) == 0 ) { sortAxis = SORT_AXIS_POS_Z; }
			else if ( _stricmp( axis, "origin" ) == 0 ) { sortAxis = SORT_ORIGIN_POS; }
			else { Warning( "unknown sort option: %s\n", argv[i] ); }
			sortAxis = dir * sortAxis;
			printf( "sorting %s\n",
					( sortAxis == SORT_AXIS_NEG_X ? "along X-axis in negative direction" :
					( sortAxis == SORT_AXIS_NEG_Y ? "along Y-axis in negative direction" :
					( sortAxis == SORT_AXIS_NEG_Z ? "along Z-axis in negative direction" :
					( sortAxis == SORT_AXIS_POS_X ? "along X-axis in positive direction" :
					( sortAxis == SORT_AXIS_POS_Y ? "along Y-axis in positive direction" :
					( sortAxis == SORT_AXIS_POS_Z ? "along Z-axis in positive direction" :
					( sortAxis == SORT_ORIGIN_NEG ? "from origin back-to-front" :
					( sortAxis == SORT_ORIGIN_POS ? "from origin front-to-back" :
					"unknown" ) ) ) ) ) ) ) ) );
		}
		else if ( _stricmp( s, "attrib" ) == 0 )
		{
			keepAttribs = RAW_VERTEX_ATTRIBUTE_JOINT_INDICES | RAW_VERTEX_ATTRIBUTE_JOINT_WEIGHTS;
			printf( "keep attributes:" );
			for ( ; i + 1 < argc && argv[i + 1][0] != '-'; i++ )
			{
				if ( _stricmp( argv[i + 1], "position" ) == 0 )			{ keepAttribs |= RAW_VERTEX_ATTRIBUTE_POSITION; }
				else if ( _stricmp( argv[i + 1], "normal" ) == 0 )		{ keepAttribs |= RAW_VERTEX_ATTRIBUTE_NORMAL; }
				else if ( _stricmp( argv[i + 1], "tangent" ) == 0 )		{ keepAttribs |= RAW_VERTEX_ATTRIBUTE_TANGENT; }
				else if ( _stricmp( argv[i + 1], "binormal" ) == 0 )	{ keepAttribs |= RAW_VERTEX_ATTRIBUTE_BINORMAL; }
				else if ( _stricmp( argv[i + 1], "color" ) == 0 )		{ keepAttribs |= RAW_VERTEX_ATTRIBUTE_COLOR; }
				else if ( _stricmp( argv[i + 1], "uv0" ) == 0 )			{ keepAttribs |= RAW_VERTEX_ATTRIBUTE_UV0; }
				else if ( _stricmp( argv[i + 1], "uv1" ) == 0 )			{ keepAttribs |= RAW_VERTEX_ATTRIBUTE_UV1; }
				else if ( _stricmp( argv[i + 1], "auto" ) == 0 )		{ keepAttribs |= RAW_VERTEX_ATTRIBUTE_AUTO; }
				else
				{
					Error( "invalid vertex attribute '%s'\n", argv[i + 1] );
					return -1;
				}
				printf( " %s", argv[i + 1] );
			}
			printf( "\n" );
		}
		else if ( _stricmp( s, "expand" ) == 0 )
		{
			i++;
			expandCollision = (float)atof( argv[i] );
			printf( "expand collision by %f\n", expandCollision );
		}
		else if ( _stricmp( s, "remove" ) == 0 )
		{
			for ( ; i + 1 < argc && argv[i + 1][0] != '-'; i++ )
			{
				removeSurfaces.PushBack( argv[i + 1] );
			}
		}
		else if ( _stricmp( s, "discrete" ) == 0 )
		{
			for ( ; i + 1 < argc && argv[i + 1][0] != '-'; i++ )
			{
				discreteSurfaces.PushBack( argv[i + 1] );
			}
		}
		else if ( _stricmp( s, "skin" ) == 0 )
		{
			for ( ; i + 1 < argc && argv[i + 1][0] != '-'; i++ )
			{
				skinSurfaces.PushBack( argv[i + 1] );
			}
		}
		else if ( _stricmp( s, "tag" ) == 0 )
		{
			for ( ; i + 1 < argc && argv[i + 1][0] != '-'; i++ )
			{
				tagSurfaces.PushBack( argv[i + 1] );
			}
		}
		else if ( _stricmp( s, "atlas" ) == 0 )
		{
			if ( numAtlases >= MAX_ATLASES )
			{
				Error( "more than %d atlases\n", MAX_ATLASES );
				return -1;
			}
			for ( ; i + 1 < argc && argv[i + 1][0] != '-'; i++ )
			{
				atlasSurfaces[numAtlases].PushBack( argv[i + 1] );
			}
			numAtlases++;
		}
		else if ( _stricmp( s, "anim" ) == 0 )
		{
			jointAnim_t anim;
			if ( _stricmp( argv[i + 1], "rotate" ) == 0 )
			{
				anim.animation = RAW_JOINT_ANIMATION_ROTATE;
				anim.parameters.x	= (float)atof( argv[i + 2] );	// pitch
				anim.parameters.y	= (float)atof( argv[i + 3] );	// yaw
				anim.parameters.z	= (float)atof( argv[i + 4] );	// roll
				anim.timeOffset		= (float)atof( argv[i + 5] );	// time offset
				anim.timeScale		= (float)atof( argv[i + 6] );	// time scale
			}
			else if ( _stricmp( argv[i + 1], "sway" ) == 0 )
			{
				anim.animation = RAW_JOINT_ANIMATION_SWAY;
				anim.parameters.x	= (float)atof( argv[i + 2] );	// pitch
				anim.parameters.y	= (float)atof( argv[i + 3] );	// yaw
				anim.parameters.z	= (float)atof( argv[i + 4] );	// roll
				anim.timeOffset		= (float)atof( argv[i + 5] );	// time offset
				anim.timeScale		= (float)atof( argv[i + 6] );	// time scale
			}
			else if ( _stricmp( argv[i + 1], "bob" ) == 0 )
			{
				anim.animation = RAW_JOINT_ANIMATION_BOB;
				anim.parameters.x	= (float)atof( argv[i + 2] );	// X
				anim.parameters.y	= (float)atof( argv[i + 3] );	// Y
				anim.parameters.z	= (float)atof( argv[i + 4] );	// Z
				anim.timeOffset		= (float)atof( argv[i + 5] );	// time offset
				anim.timeScale		= (float)atof( argv[i + 6] );	// time scale
			}
			for ( i += 6; i + 1 < argc && argv[i + 1][0] != '-'; i++ )
			{
				anim.name = argv[i + 1];
				jointAnimations.PushBack( anim );
			}
		}
		else if ( _stricmp( s, "texlod" ) == 0 )
		{
			options.texLodFov = atoi( argv[i + 1] );
			options.texLodRes = atoi( argv[i + 2] );
			options.texLodAniso = (float)atof( argv[i + 3] );
			i += 3;
			printf( "texlod %d %d %3.1f\n", options.texLodFov, options.texLodRes, options.texLodAniso );
		}
		else if ( _stricmp( s, "renderorder" ) == 0 )
		{
			options.renderOrder = true;
			printf( "render order vertex colors\n" );
		}
		else if ( _stricmp ( s, "dxt" ) == 0 )
		{
			options.format = TEXTURE_FORMAT_DXT;
		}
		else if ( _stricmp ( s, "etc" ) == 0 )
		{
			options.format = TEXTURE_FORMAT_ETC;
		}
		else if ( _stricmp ( s, "uncompressed" ) == 0 )
		{
			options.format = TEXTURE_FORMAT_UNCOMPRESSED;
		}
		else if ( _stricmp ( s, "pvr" ) == 0 )
		{
			options.container = TEXTURE_CONTAINER_PVR;
		}
		else if ( _stricmp ( s, "ktx" ) == 0 )
		{
			options.container = TEXTURE_CONTAINER_KTX;
		}
		else if ( _stricmp( s, "dds" ) == 0 )
		{
			options.container = TEXTURE_CONTAINER_DDS;
		}
		else if ( _stricmp( s, "alpha" ) == 0 )
		{
			options.allowAlpha = true;
		}
		else if ( _stricmp( s, "clean" ) == 0 )
		{
			options.clean = true;
		}
		else if ( _stricmp( s, "include" ) == 0 )
		{
			for ( ; i + 1 < argc && argv[i + 1][0] != '-'; i++ )
			{
				includeFiles.PushBack( argv[i + 1] );
			}
		}
		else if ( _stricmp( s, "pack" ) == 0 )
		{
			options.pack = true;
		}
		else if ( _stricmp( s, "zip" ) == 0 )
		{
			i++;
			options.zip = atoi( argv[i] );
			printf( "zip level %d\n", options.zip );
		}
		else if ( _stricmp( s, "fullText" ) == 0 )
		{
			options.fullText = true;
		}
		else if ( _stricmp( s, "noPush" ) == 0 )
		{
			options.pushToDevice = false;
		}
		else if ( _stricmp( s, "noTest" ) == 0 )
		{
			options.testOnDevice = false;
		}
		else if ( _stricmp( s, "expo" ) == 0 )
		{
			options.launch = APP_EXPO;
		}
		else if ( _stricmp( s, "cinema" ) == 0 )
		{
			options.launch = APP_CINEMA;
		}
		else if ( _stricmp( s, "tuscany" ) == 0 )
		{
			options.tuscany = true;
		}
		else
		{
			Error( "unknown option: '%s'\n", s );
			ShowHelp();
			return -1;
		}
	}

	if ( options.format == TEXTURE_FORMAT_DEFAULT )
	{
		options.format = options.container == TEXTURE_CONTAINER_DDS ? TEXTURE_FORMAT_DXT : TEXTURE_FORMAT_ETC;
	}

	for ( int i = 0; i < includeFiles.GetSizeI(); i++ )
	{
		if ( !FileUtils::FileExists( includeFiles[i] ) )
		{
			Error( "missing include file '%s'\n", includeFiles[i] );
			return -1;
		}
	}

	const String tempFolder = (const char *)StringUtils::Va( "%s_tmp/", outputName );
	if ( !FileUtils::CreatePath( tempFolder ) )
	{
		Error( "failed to create folder '%s'\n", tempFolder.ToCStr() );
		return -1;
	}

	if ( wallCollisionSurfaces.GetSizeI() > 0 )
	{
		wallCollisionFbxFileName = renderFbxFileName;
	}
	if ( groundCollisionSurfaces.GetSizeI() > 0 )
	{
		groundCollisionFbxFileName = renderFbxFileName;
	}
	if ( rayTraceSurfaces.GetSizeI() > 0 )
	{
		rayTraceFbxFileName = renderFbxFileName;
	}

#if defined( __APPLE__ ) || defined( __linux__ )
	const String batchPath = (const char *)StringUtils::Va( "%s_pack.sh", outputName );
#else
	const String batchPath = (const char *)StringUtils::Va( "%s_pack.bat", outputName );
#endif

	const char * textureExtensions =	( options.container == TEXTURE_CONTAINER_PVR ? "tga;bmp;png;jpg;pvr" :
										( options.container == TEXTURE_CONTAINER_KTX ? "tga;bmp;png;jpg;ktx" :
										( options.container == TEXTURE_CONTAINER_DDS ? "tga;bmp;png;jpg;dds" :
										"tga;bmp;png;jpg" ) ) );

	ModelData * data_render_model = NULL;
	ModelData * data_collision_model = NULL;
	ModelData * data_ground_collision_model = NULL;
	ModelData * data_raytrace_model = NULL;

	Matrix4f centerTranslation;

	if ( renderFbxFileName != NULL )
	{
		RawModel raw;
		if ( LoadRawModel( raw, renderFbxFileName, textureExtensions, geometryTransform, texturesTransform ) )
		{
			if ( options.stripModoNumbers )
			{
				raw.StripModoNumbers();
			}
			SkinRawModelSurfaces( raw, skinSurfaces );
			SetJointAnimations( raw, jointAnimations );
			TagRawModelSurfaces( raw, tagSurfaces );
			RemoveRawModelSurfaces( raw, removeSurfaces );
			AtlasRawModelSurfaces( raw, atlasSurfaces, numAtlases, FileUtils::GetCurrentFolder() + tempFolder, options.repeat );
			DiscretizeRawModelSurfaces( raw, discreteSurfaces );
			raw.Condense();
			if ( options.center )
			{
				centerTranslation = Matrix4f::Translation( - raw.GetBounds().GetCenter() );
				printf( "translating by (%f, %f, %f) to center\n", centerTranslation.M[0][3], centerTranslation.M[1][3], centerTranslation.M[2][3] );
				raw.TransformGeometry( centerTranslation );
			}
			if ( !options.allowAlpha )
			{
				raw.ForceOpaque();
			}
			if ( sortAxis != 0 )
			{
				raw.SortGeometry( sortAxis );
			}
			if ( options.tuscany )
			{
				TuscanyHack( raw );
			}
			AddRawModelTextures( raw, includeFiles, textureExtensions );
			if ( options.texLodFov > 0 && options.texLodRes > 0 )
			{
				raw.TexLod( Vector3f( 0.0f, 1.8f, 0.0f ), Vector2f( (float)options.texLodFov ), Vector2i( options.texLodRes ), options.texLodAniso );
				keepAttribs =	RAW_VERTEX_ATTRIBUTE_POSITION |
								RAW_VERTEX_ATTRIBUTE_COLOR |
								RAW_VERTEX_ATTRIBUTE_JOINT_INDICES |
								RAW_VERTEX_ATTRIBUTE_JOINT_WEIGHTS;
			}
			if ( options.renderOrder )
			{
				keepAttribs =	RAW_VERTEX_ATTRIBUTE_POSITION |
								RAW_VERTEX_ATTRIBUTE_COLOR |
								RAW_VERTEX_ATTRIBUTE_JOINT_INDICES |
								RAW_VERTEX_ATTRIBUTE_JOINT_WEIGHTS;
			}
			data_render_model = Raw2RenderModel( raw, keepAttribs, options.renderOrder, options.fullText );
			CreateBatchFile( batchPath, argv[0], outputName, raw, options );
		}
	}

	if ( wallCollisionFbxFileName != NULL )
	{
		RawModel raw;
		if ( LoadRawModel( raw, wallCollisionFbxFileName, textureExtensions, geometryTransform, texturesTransform ) )
		{
			if ( options.center )
			{
				raw.TransformGeometry( centerTranslation );
			}
			if ( options.stripModoNumbers )
			{
				raw.StripModoNumbers();
			}
			if ( wallCollisionSurfaces.GetSizeI() > 0 )
			{
				KeepSelectedSurfaces( raw, wallCollisionSurfaces, "wall collision" );
			}
			data_collision_model = Raw2CollisionModel( raw, expandCollision );
		}
	}

	if ( groundCollisionFbxFileName != NULL )
	{
		RawModel raw;
		if ( LoadRawModel( raw, groundCollisionFbxFileName, textureExtensions, geometryTransform, texturesTransform ) )
		{
			if ( options.center )
			{
				raw.TransformGeometry( centerTranslation );
			}
			if ( options.stripModoNumbers )
			{
				raw.StripModoNumbers();
			}
			if ( groundCollisionSurfaces.GetSizeI() > 0 )
			{
				KeepSelectedSurfaces( raw, groundCollisionSurfaces, "ground collision" );
			}
			data_ground_collision_model = Raw2CollisionModel( raw, 0.0f );
		}
	}

	if ( rayTraceFbxFileName != NULL )
	{
		RawModel raw;
		if ( LoadRawModel( raw, rayTraceFbxFileName, textureExtensions, geometryTransform, texturesTransform ) )
		{
			if ( options.center )
			{
				raw.TransformGeometry( centerTranslation );
			}
			if ( options.stripModoNumbers )
			{
				raw.StripModoNumbers();
			}
			if ( rayTraceSurfaces.GetSizeI() > 0 )
			{
				KeepSelectedSurfaces( raw, rayTraceSurfaces, "ray trace" );
			}
			data_raytrace_model = Raw2RayTraceModel( raw, options.fullText );
		}
	}

	const String jsonPath = tempFolder + jsonFileName;
	printf( "writing %s\n", jsonPath.ToCStr() );
	JSON * JSON_scene = JSON::CreateObject();
	if ( data_render_model != NULL )
	{
		JSON_scene->AddItem( "render_model", data_render_model->json );
	}
	if ( data_collision_model != NULL )
	{
		JSON_scene->AddItem( "collision_model", data_collision_model->json );
	}
	if ( data_ground_collision_model != NULL )
	{
		JSON_scene->AddItem( "ground_collision_model", data_ground_collision_model->json );
	}
	if ( data_raytrace_model != NULL )
	{
		JSON_scene->AddItem( "raytrace_model", data_raytrace_model->json );
	}
	if ( !JSON_scene->Save( jsonPath ) )
	{
		Error( "failed to write '%s'\n", jsonPath.ToCStr() );
	}
	JSON_scene->Release();

	const String binaryPath = tempFolder + binaryFileName;
	printf( "writing %s\n", binaryPath.ToCStr() );
	if ( !WriteBinary( binaryPath,
						data_render_model,
						data_collision_model,
						data_ground_collision_model,
						data_raytrace_model ) )
	{
		Error( "failed to write '%s'\n", binaryPath.ToCStr() );
	}

	delete data_render_model;
	delete data_collision_model;
	delete data_ground_collision_model;
	delete data_raytrace_model;

#if !defined( __APPLE__ ) && !defined( __linux__ )
	ParseModelsJsonTest( jsonPath, binaryPath );
#endif

	printf( "-----------------------\n" );
	if ( options.pack )
	{
		FileUtils::Execute( batchPath );
	}
	else
	{
		PrintHighlight( "Run the '%s' batch file to compress textures and pack the data.\n", batchPath.ToCStr() );
	}

#if defined( _DEBUG )
	printf( "Press any key to continue.\n" );
	_getch();
#endif

    return 0;
}
