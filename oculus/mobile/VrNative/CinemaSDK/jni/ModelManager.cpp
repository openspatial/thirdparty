/************************************************************************************

Filename    :   ModelManager.cpp
Content     :
Created     :	7/3/2014
Authors     :   Jim Dos�

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the Cinema/ directory. An additional grant 
of patent rights can be found in the PATENTS file in the same directory.

*************************************************************************************/

#include <dirent.h>
#include "LibOVR/Src/Kernel/OVR_String_Utils.h"
#include "ModelManager.h"
#include "CinemaApp.h"
#include "PackageFiles.h"


namespace OculusCinema {

static const char * TheatersDirectory = "Oculus/Cinema/Theaters";

//=======================================================================================

ModelManager::ModelManager( CinemaApp &cinema ) :
	Cinema( cinema ),
	Theaters(),
	BoxOffice( NULL ),
	VoidScene( NULL ),
	LaunchIntent(),
	DefaultSceneModel( NULL )

{
}

ModelManager::~ModelManager()
{
}

void ModelManager::OneTimeInit( const char * launchIntent )
{
	LOG( "ModelManager::OneTimeInit" );
	const double start = ovr_GetTimeInSeconds();
	LaunchIntent = launchIntent;

	DefaultSceneModel = new ModelFile( "default" );

	LoadModels();

	LOG( "ModelManager::OneTimeInit: %i theaters loaded, %3.1f seconds", Theaters.GetSizeI(), ovr_GetTimeInSeconds() - start );
}

void ModelManager::OneTimeShutdown()
{
	LOG( "ModelManager::OneTimeShutdown" );

	// Free GL resources

	for( UPInt i = 0; i < Theaters.GetSize(); i++ )
	{
		delete Theaters[ i ];
	}
}

void ModelManager::LoadModels()
{
	LOG( "ModelManager::LoadModels" );
	const double start = ovr_GetTimeInSeconds();

	BoxOffice = LoadScene( "assets/scenes/BoxOffice.ovrscene", false, true, true );
	BoxOffice->UseSeats = false;
	BoxOffice->LobbyScreen = true;

	if ( LaunchIntent.GetLength() > 0 )
	{
		Theaters.PushBack( LoadScene( LaunchIntent.ToCStr(), true, true, false ) );
	}
	else
	{
		// we want our theaters to show up first
		Theaters.PushBack( LoadScene( "assets/scenes/home_theater.ovrscene", true, false, true ) );

		// create void scene
		VoidScene = new SceneDef();
		VoidScene->SceneModel = new ModelFile( "Void" );
		VoidScene->UseSeats = false;
		VoidScene->UseDynamicProgram = false;
		VoidScene->UseScreenGeometry = false;
		VoidScene->UseFreeScreen = true;

		int width = 0, height = 0;
		VoidScene->IconTexture = LoadTextureFromApplicationPackage( "assets/VoidTheater.png",
				TextureFlags_t( TEXTUREFLAG_NO_DEFAULT ), width, height );

		BuildTextureMipmaps( VoidScene->IconTexture );
		MakeTextureTrilinear( VoidScene->IconTexture );
		MakeTextureClamped( VoidScene->IconTexture );

		Theaters.PushBack( VoidScene );

		// load all scenes on startup, so there isn't a delay when switching theaters
		ScanDirectoryForScenes( Cinema.ExternalRetailDir( TheatersDirectory ), true, false, Theaters );
		ScanDirectoryForScenes( Cinema.RetailDir( TheatersDirectory ), true, false, Theaters );
		ScanDirectoryForScenes( Cinema.SDCardDir( TheatersDirectory ), true, false, Theaters );
	}

	LOG( "ModelManager::LoadModels: %i theaters loaded, %3.1f seconds", Theaters.GetSizeI(), ovr_GetTimeInSeconds() - start );
}

void ModelManager::ScanDirectoryForScenes( const char * directory, bool useDynamicProgram, bool useScreenGeometry, Array<SceneDef *> &scenes ) const
{
	DIR * dir = opendir( directory );
	if ( dir != NULL )
	{
		struct dirent * entry;
		while( ( entry = readdir( dir ) ) != NULL ) {
			String filename = entry->d_name;
			String ext = filename.GetExtension().ToLower();
			if ( ( ext == ".ovrscene" ) )
			{
				String fullpath = directory;
				fullpath.AppendString( "/" );
				fullpath.AppendString( filename );
				SceneDef *def = LoadScene( fullpath.ToCStr(), useDynamicProgram, useScreenGeometry, false );
				scenes.PushBack( def );
			}
		}

		closedir( dir );
	}
}

SceneDef * ModelManager::LoadScene( const char *sceneFilename, bool useDynamicProgram, bool useScreenGeometry, bool loadFromApplicationPackage ) const
{
	String filename;

	if ( loadFromApplicationPackage && !ovr_PackageFileExists( sceneFilename ) )
	{
		LOG( "Scene %s not found in application package.  Checking sdcard.", sceneFilename );
		loadFromApplicationPackage = false;
	}

	if ( loadFromApplicationPackage )
	{
		filename = sceneFilename;
	}
	else if ( ( sceneFilename != NULL ) && ( *sceneFilename == '/' ) ) 	// intent will have full path for scene file, so check for /
	{
		filename = sceneFilename;
	}
	else if ( Cinema.FileExists( Cinema.ExternalRetailDir( sceneFilename ) ) )
	{
		filename = Cinema.ExternalRetailDir( sceneFilename );
	}
	else if ( Cinema.FileExists( Cinema.RetailDir( sceneFilename ) ) )
	{
		filename = Cinema.RetailDir( sceneFilename );
	}
	else
	{
		filename = Cinema.SDCardDir( sceneFilename );
	}

	LOG( "Adding scene: %s, %s", filename.ToCStr(), sceneFilename );

	SceneDef *def = new SceneDef();
	def->Filename = sceneFilename;
	def->UseSeats = true;
	def->UseDynamicProgram = useDynamicProgram;

	MaterialParms materialParms;

	// This may be called during init, before the FramebufferIsSrgb is set,
	// so use WantsSrgbFramebuffer instead.
	materialParms.UseSrgbTextureFormats = Cinema.app->GetAppInterface()->GetWantSrgbFramebuffer();

	// Improve the texture quality with anisotropic filtering.
	materialParms.EnableDiffuseAniso = true;

	// The emissive texture is used as a separate lighting texture and should not be LOD clamped.
	materialParms.EnableEmissiveLodClamp = false;

	ModelGlPrograms glPrograms = ( useDynamicProgram ) ? Cinema.ShaderMgr.DynamicPrograms : Cinema.ShaderMgr.DefaultPrograms;

	String iconFilename = StringUtils::SetFileExtensionString( filename.ToCStr(), "png" );

	int textureWidth = 0, textureHeight = 0;

	if ( loadFromApplicationPackage )
	{
		def->SceneModel = LoadModelFileFromApplicationPackage( filename.ToCStr(), glPrograms, materialParms );
		def->IconTexture = LoadTextureFromApplicationPackage( iconFilename.ToCStr(), TextureFlags_t( TEXTUREFLAG_NO_DEFAULT ), textureWidth, textureHeight );
	}
	else
	{
		def->SceneModel = LoadModelFile( filename.ToCStr(), glPrograms, materialParms );
		def->IconTexture = LoadTextureFromBuffer( iconFilename.ToCStr(), MemBufferFile( iconFilename.ToCStr() ),
				TextureFlags_t( TEXTUREFLAG_NO_DEFAULT ), textureWidth, textureHeight );
	}

	if ( def->IconTexture != 0 )
	{
		LOG( "Loaded external icon for theater: %s", iconFilename.ToCStr() );
	}
	else
	{
		const ModelTexture * iconTexture = def->SceneModel->FindNamedTexture( "icon" );
		if ( iconTexture != NULL )
		{
			def->IconTexture = iconTexture->texid;
		}
		else
		{
			LOG( "No icon in scene.  Loading default." );

			int	width = 0, height = 0;
			def->IconTexture = LoadTextureFromApplicationPackage( "assets/noimage.png",
				TextureFlags_t( TEXTUREFLAG_NO_DEFAULT ), width, height );
		}
	}

	BuildTextureMipmaps( def->IconTexture );
	MakeTextureTrilinear( def->IconTexture );
	MakeTextureClamped( def->IconTexture );

	def->UseScreenGeometry = useScreenGeometry;
	def->UseFreeScreen = false;

	return def;
}

const SceneDef & ModelManager::GetTheater( UPInt index ) const
{
	if ( index < Theaters.GetSize() )
	{
		return *Theaters[ index ];
	}

	// default to the Void Scene
	return *VoidScene;
}

/*
 * Command
 *
 * Actions that need to be performed on the render thread.
 */
bool ModelManager::Command( const char * msg )
{
	return false;
}

} // namespace OculusCinema
