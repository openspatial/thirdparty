/************************************************************************************

Filename    :   Oculus360Photos.cpp
Content     :   
Created     :   
Authors     :   

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the Oculus360Photos/ directory. An additional grant 
of patent rights can be found in the PATENTS file in the same directory.

*************************************************************************************/

#include "Oculus360Photos.h"
#include <android/keycodes.h>
#include "LibOVR/Src/Kernel/OVR_Threads.h"
#include "VRMenu/GuiSys.h"
#include "PanoBrowser.h"
#include "PanoMenu.h"
#include "FileLoader.h"
#include "ImageData.h"
#include "PackageFiles.h"
#include "PhotosMetaData.h"

static const char * DEFAULT_PANO = "assets/placeholderBackground.jpg";

// Comment out to disable all VRMenus - renders only the startup pano and nothing else
#define ENABLE_MENU

extern "C" {

long Java_com_oculus_oculus360photossdk_MainActivity_nativeSetAppInterface( JNIEnv *jni, jclass clazz, jobject activity,
	jstring fromPackageName, jstring commandString, jstring uriString )
{
	// This is called by the java UI thread.
	LOG( "nativeSetAppInterface" );
	return (new Oculus360Photos())->SetActivity( jni, clazz, activity, fromPackageName, commandString, uriString );
}

} // extern "C"

namespace OVR
{

Oculus360Photos::DoubleBufferedTextureData::DoubleBufferedTextureData() 
	: CurrentIndex( 0 )
{
	for ( int i = 0; i < 2; ++i )
	{
		TexId[ i ] = 0;
		Width[ i ] = 0;
		Height[ i ] = 0;
	}
}

Oculus360Photos::DoubleBufferedTextureData::~DoubleBufferedTextureData()
{
	FreeTexture( TexId[ 0 ] );
	FreeTexture( TexId[ 1 ] );
}

GLuint Oculus360Photos::DoubleBufferedTextureData::GetRenderTexId() const
{
	return TexId[ CurrentIndex ^ 1 ];
}

GLuint Oculus360Photos::DoubleBufferedTextureData::GetLoadTexId() const
{
	return TexId[ CurrentIndex ];
}

void Oculus360Photos::DoubleBufferedTextureData::SetLoadTexId( const GLuint texId )
{
	TexId[ CurrentIndex ] = texId;
}

void Oculus360Photos::DoubleBufferedTextureData::Swap()
{
	CurrentIndex ^= 1;
}

void Oculus360Photos::DoubleBufferedTextureData::SetSize( const int width, const int height )
{
	Width[ CurrentIndex ] = width;
	Height[ CurrentIndex ] = height;
}

bool Oculus360Photos::DoubleBufferedTextureData::SameSize( const int width, const int height ) const
{
	return ( Width[ CurrentIndex ] == width && Height[ CurrentIndex ] == height );
}

Oculus360Photos::Oculus360Photos() 
	: Fader( 0.0f )	
	, MetaData( NULL )
	, PanoMenu( NULL )
	, Browser( NULL )
	, ActivePano( NULL )
	, CurrentPanoIsCubeMap( false )
	, MenuState( MENU_NONE )
	, FadeOutRate( 1.0f / 0.45f )
	, FadeInRate( 1.0f / 0.5f )
	, PanoMenuVisibleTime( 7.0f )
	, CurrentFadeRate( FadeOutRate )
	, CurrentFadeLevel( 0.0f )
	, PanoMenuTimeLeft( -1.0f )
	, BrowserOpenTime( 0.0f )
	, UseOverlay( true )
	, UseSrgb( true )
	, BackgroundCommands( 100 )	
	, EglClientVersion( 0 )
	, EglDisplay( 0 )
	, EglConfig( 0 )
	, EglPbufferSurface( 0 )
	, EglShareContext( 0 )
{
	ShutdownRequest.SetState( false );
}

Oculus360Photos::~Oculus360Photos()
{
}

//============================================================================================

void Oculus360Photos::OneTimeInit( const char * fromPackage, const char * launchIntentJSON, const char * launchIntentURI )
{
	// This is called by the VR thread, not the java UI thread.
	LOG( "--------------- Oculus360Photos OneTimeInit ---------------" );

	//-------------------------------------------------------------------------
	TexturedMvpProgram = BuildProgram(
		"uniform mat4 Mvpm;\n"
		"attribute vec4 Position;\n"
		"attribute vec4 VertexColor;\n"
		"attribute vec2 TexCoord;\n"
		"uniform mediump vec4 UniformColor;\n"
		"varying  lowp vec4 oColor;\n"
		"varying highp vec2 oTexCoord;\n"
		"void main()\n"
		"{\n"
		"   gl_Position = Mvpm * Position;\n"
		"	oTexCoord = TexCoord;\n"
		"   oColor = /* VertexColor * */ UniformColor;\n"
		"}\n"
		,
		"uniform sampler2D Texture0;\n"
		"varying highp vec2 oTexCoord;\n"
		"varying lowp vec4	oColor;\n"
		"void main()\n"
		"{\n"
		"	gl_FragColor = oColor * texture2D( Texture0, oTexCoord );\n"
		"}\n"
		);

	CubeMapPanoProgram = BuildProgram(
		"uniform mat4 Mvpm;\n"
		"attribute vec4 Position;\n"
		"uniform mediump vec4 UniformColor;\n"
		"varying  lowp vec4 oColor;\n"
		"varying highp vec3 oTexCoord;\n"
		"void main()\n"
		"{\n"
		"   gl_Position = Mvpm * Position;\n"
		"	oTexCoord = Position.xyz;\n"
		"   oColor = UniformColor;\n"
		"}\n"
		,
		"uniform samplerCube Texture0;\n"
		"varying highp vec3 oTexCoord;\n"
		"varying lowp vec4	oColor;\n"
		"void main()\n"
		"{\n"
		"	gl_FragColor = oColor * textureCube( Texture0, oTexCoord );\n"
		"}\n"
		);

	PanoramaProgram = BuildProgram(
		"uniform highp mat4 Mvpm;\n"
		"uniform highp mat4 Texm;\n"
		"attribute vec4 Position;\n"
		"attribute vec2 TexCoord;\n"
		"varying  highp vec2 oTexCoord;\n"
		"void main()\n"
		"{\n"
		"   gl_Position = Mvpm * Position;\n"
		"   oTexCoord = vec2( Texm * vec4( TexCoord, 0, 1 ) );\n"
		"}\n"
		,
		"#extension GL_OES_EGL_image_external : require\n"
		"uniform samplerExternalOES Texture0;\n"
		"uniform lowp vec4 UniformColor;\n"
		"uniform lowp vec4 ColorBias;\n"
		"varying highp vec2 oTexCoord;\n"
		"void main()\n"
		"{\n"
		"	gl_FragColor = ColorBias + UniformColor * texture2D( Texture0, oTexCoord );\n"
		"}\n"
		);

	// launch cube pano -should always exist!
	StartupPano = DEFAULT_PANO;

	LOG( "Creating Globe" );
	Globe = BuildGlobe();

	// Stay exactly at the origin, so the panorama globe is equidistant
	// Don't clear the head model neck length, or swipe view panels feel wrong.
	VrViewParms viewParms = app->GetVrViewParms();
	viewParms.EyeHeight = 0.0f;
	app->SetVrViewParms( viewParms );

	// Optimize for 16 bit depth in a modest globe size
	Scene.Znear = 0.1f;
	Scene.Zfar = 200.0f;

	InitFileQueue( app, this );
	
#ifdef ENABLE_MENU	
	// meta file used by OvrMetaData 
	const char * relativePath = "Oculus/360Photos/";
	const char * metaFile = "meta.json";

	// Get package name
	const char * packageName = NULL;
	JNIEnv * jni = app->GetVrJni();
	jstring result;
	jmethodID getPackageNameId = jni->GetMethodID( app->GetVrActivityClass(), "getPackageName", "()Ljava/lang/String;" );
	if ( getPackageNameId != NULL )
	{
		result = ( jstring )jni->CallObjectMethod( app->GetJavaObject(), getPackageNameId );
		if ( !jni->ExceptionOccurred() )
		{
			jboolean isCopy;
			packageName = app->GetVrJni()->GetStringUTFChars( result, &isCopy );
		}
	}
	else
	{
		FAIL( "Oculus360Photos::OneTimeInit getPackageName failed" );
	}
	OVR_ASSERT( packageName );

	MetaData = new OvrPhotosMetaData();
	if ( MetaData == NULL )
	{
		FAIL( "Oculus360Photos::OneTimeInit failed to create MetaData" );
	}

	OvrMetaDataFileExtensions fileExtensions;

	fileExtensions.GoodExtensions.PushBack( ".jpg" );

	fileExtensions.BadExtensions.PushBack( ".jpg.x" );
	fileExtensions.BadExtensions.PushBack( "_px.jpg" );
	fileExtensions.BadExtensions.PushBack( "_py.jpg" );
	fileExtensions.BadExtensions.PushBack( "_pz.jpg" );
	fileExtensions.BadExtensions.PushBack( "_nx.jpg" );
	fileExtensions.BadExtensions.PushBack( "_ny.jpg" );

	const OvrStoragePaths & storagePaths = app->GetStoragePaths();
	storagePaths.PushBackSearchPathIfValid( EST_SECONDARY_EXTERNAL_STORAGE, EFT_ROOT, "RetailMedia/", SearchPaths );
	storagePaths.PushBackSearchPathIfValid( EST_SECONDARY_EXTERNAL_STORAGE, EFT_ROOT, "", SearchPaths );
	storagePaths.PushBackSearchPathIfValid( EST_PRIMARY_EXTERNAL_STORAGE, EFT_ROOT, "RetailMedia/", SearchPaths );
	storagePaths.PushBackSearchPathIfValid( EST_PRIMARY_EXTERNAL_STORAGE, EFT_ROOT, "", SearchPaths );

	LOG( "360 PHOTOS using %d searchPaths", SearchPaths.GetSizeI() );

	const double startTime = ovr_GetTimeInSeconds();
	MetaData->InitFromDirectoryMergeMeta( relativePath, SearchPaths, fileExtensions, metaFile, packageName );
	LOG( "META DATA INIT TIME: %f", ovr_GetTimeInSeconds() - startTime );

	jni->ReleaseStringUTFChars( result, packageName );

	// Start building the PanoMenu
	PanoMenu = ( OvrPanoMenu * )app->GetGuiSys().GetMenu( OvrPanoMenu::MENU_NAME );
	if ( PanoMenu == NULL )
	{
		PanoMenu = OvrPanoMenu::Create( 
			app, this, app->GetVRMenuMgr(), app->GetDefaultFont(), *MetaData, 2.0f, 2.0f );
		OVR_ASSERT( PanoMenu );

		app->GetGuiSys().AddMenu( PanoMenu );
	}

	PanoMenu->SetFlags( VRMenuFlags_t( VRMENU_FLAG_PLACE_ON_HORIZON ) | VRMENU_FLAG_SHORT_PRESS_HANDLED_BY_APP );

	// Start building the FolderView
	Browser = ( PanoBrowser * )app->GetGuiSys().GetMenu( OvrFolderBrowser::MENU_NAME );
	if ( Browser == NULL )
	{
		Browser = PanoBrowser::Create(
			app,
			*MetaData,
			256, 20.0f,
			160, 180.0f,
			7,
			5.3f );
		OVR_ASSERT( Browser );

		app->GetGuiSys().AddMenu( Browser );
	}

	Browser->SetFlags( VRMenuFlags_t( VRMENU_FLAG_PLACE_ON_HORIZON ) | VRMENU_FLAG_BACK_KEY_EXITS_APP );
	Browser->SetFolderTitleSpacingScale( 0.35f );
	Browser->SetScrollBarSpacingScale( 0.82f );
	Browser->SetScrollBarRadiusScale( 0.97f );
	Browser->SetPanelTextSpacingScale( 0.28f );
	Browser->OneTimeInit( *MetaData );
	Browser->BuildDirtyMenu( *MetaData );
	Browser->ReloadFavoritesBuffer();
#endif // ENABLE_MENU

	//---------------------------------------------------------
	// OpenGL initialization for shared context for 
	// background loading thread done on the main thread
	//---------------------------------------------------------

	// Get values for the current OpenGL context
	EglDisplay = eglGetCurrentDisplay();
	if ( EglDisplay == EGL_NO_DISPLAY )
	{
		FAIL( "EGL_NO_DISPLAY" );
	}

	EglShareContext = eglGetCurrentContext();
	if ( EglShareContext == EGL_NO_CONTEXT )
	{
		FAIL( "EGL_NO_CONTEXT" );
	}

	EGLint attribList[] =
	{
			EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
			EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
			EGL_NONE
	};

	EGLint numConfigs;
	if ( !eglChooseConfig( EglDisplay, attribList, &EglConfig, 1, &numConfigs ) )
	{
		FAIL( "eglChooseConfig failed" );
	}

	if ( EglConfig == NULL )
	{
		FAIL( "EglConfig NULL" );
	}
	if ( !eglQueryContext( EglDisplay, EglShareContext, EGL_CONTEXT_CLIENT_VERSION, ( EGLint * )&EglClientVersion ) )
	{
		FAIL( "eglQueryContext EGL_CONTEXT_CLIENT_VERSION failed" );
	}
	LOG( "Current EGL_CONTEXT_CLIENT_VERSION:%i", EglClientVersion );

	EGLint SurfaceAttribs [ ] =
	{
		EGL_WIDTH, 1,
		EGL_HEIGHT, 1,
		EGL_NONE
	};

	EglPbufferSurface = eglCreatePbufferSurface( EglDisplay, EglConfig, SurfaceAttribs );
	if ( EglPbufferSurface == EGL_NO_SURFACE ) {
		FAIL( "eglCreatePbufferSurface failed: %s", EglErrorString() );
	}

	EGLint bufferWidth, bufferHeight;
	if ( !eglQuerySurface( EglDisplay, EglPbufferSurface, EGL_WIDTH, &bufferWidth ) ||
		!eglQuerySurface( EglDisplay, EglPbufferSurface, EGL_HEIGHT, &bufferHeight ) )
	{
		FAIL( "eglQuerySurface failed:  %s", EglErrorString() );
	}

	// spawn the background loading thread with the command list
	pthread_attr_t loadingThreadAttr;
	pthread_attr_init( &loadingThreadAttr );
	sched_param sparam;
	sparam.sched_priority = Thread::GetOSPriority( Thread::NormalPriority );
	pthread_attr_setschedparam( &loadingThreadAttr, &sparam );

	pthread_t loadingThread;
	const int createErr = pthread_create( &loadingThread, &loadingThreadAttr, &BackgroundGLLoadThread, this );
	if ( createErr != 0 )
	{
		LOG( "pthread_create returned %i", createErr );
	}

	// We might want to save the view state and position for perfect recall
}

//============================================================================================

void Oculus360Photos::OneTimeShutdown()
{
	// This is called by the VR thread, not the java UI thread.
	LOG( "--------------- Oculus360Photos OneTimeShutdown ---------------" );

	// Shut down background loader
	ShutdownRequest.SetState( true );

	Globe.Free();

	if ( MetaData )
	{
		delete MetaData;
	}

	DeleteProgram( TexturedMvpProgram );
	DeleteProgram( CubeMapPanoProgram );
	DeleteProgram( PanoramaProgram );
	
	if ( eglDestroySurface( EglDisplay, EglPbufferSurface ) == EGL_FALSE )
	{
		FAIL( "eglDestroySurface: shutdown failed" );
	}
}

void * Oculus360Photos::BackgroundGLLoadThread( void * v )
{
	pthread_setname_np( pthread_self(), "BackgrndGLLoad" );

	Oculus360Photos * photos = ( Oculus360Photos * )v;

	// Create a new GL context on this thread, sharing it with the main thread context
	// so the loaded background texture can be passed.
	EGLint loaderContextAttribs [ ] =
	{
		EGL_CONTEXT_CLIENT_VERSION, photos->EglClientVersion,
		EGL_NONE, EGL_NONE,
		EGL_NONE
	};

	EGLContext EglBGLoaderContext = eglCreateContext( photos->EglDisplay, photos->EglConfig, photos->EglShareContext, loaderContextAttribs );
	if ( EglBGLoaderContext == EGL_NO_CONTEXT )
	{
		FAIL( "eglCreateContext failed: %s", EglErrorString() );
	}

	// Make the context current on the window, so no more makeCurrent calls will be needed
	if ( eglMakeCurrent( photos->EglDisplay, photos->EglPbufferSurface, photos->EglPbufferSurface, EglBGLoaderContext ) == EGL_FALSE )
	{
		FAIL( "BackgroundGLLoadThread eglMakeCurrent failed: %s", EglErrorString() );
	}

	// run until Shutdown requested
	for ( ;; )
	{
		if ( photos->ShutdownRequest.GetState() )
		{
			LOG( "BackgroundGLLoadThread ShutdownRequest received" );
			break;
		}

		photos->BackgroundCommands.SleepUntilMessage();
		const char * msg = photos->BackgroundCommands.GetNextMessage();
		LOG( "BackgroundGLLoadThread Commands: %s", msg );
		if ( MatchesHead( "pano ", msg ) )
		{
			unsigned char * data;
			int		width, height;
			sscanf( msg, "pano %p %i %i", &data, &width, &height );

			const double start = ovr_GetTimeInSeconds( );

			// Resample oversize images so gl can load them.
			// We could consider resampling to GL_MAX_TEXTURE_SIZE exactly for better quality.
			GLint maxTextureSize = 0;
			glGetIntegerv( GL_MAX_TEXTURE_SIZE, &maxTextureSize );

			while ( width > maxTextureSize || width > maxTextureSize )
			{
				LOG( "Quartering oversize %ix%i image", width, height );
				unsigned char * newBuf = QuarterImageSize( data, width, height, true );
				free( data );
				data = newBuf;
				width >>= 1;
				height >>= 1;
			}

			photos->LoadRgbaTexture( data, width, height, true );
			free( data );

			// Add a sync object for uploading textures
			EGLSyncKHR GpuSync = eglCreateSyncKHR_( photos->EglDisplay, EGL_SYNC_FENCE_KHR, NULL );
			if ( GpuSync == EGL_NO_SYNC_KHR ) {
				FAIL( "BackgroundGLLoadThread eglCreateSyncKHR_():EGL_NO_SYNC_KHR" );
			}

			// Force it to flush the commands and wait until the textures are fully uploaded
			if ( EGL_FALSE == eglClientWaitSyncKHR_( photos->EglDisplay, GpuSync, EGL_SYNC_FLUSH_COMMANDS_BIT_KHR, 
				EGL_FOREVER_KHR ) )
			{
				LOG( "BackgroundGLLoadThread eglClientWaitSyncKHR returned EGL_FALSE" );
			}

			photos->app->GetMessageQueue( ).PostPrintf( "%s", "loaded pano" );

			const double end = ovr_GetTimeInSeconds();
			LOG( "%4.2fs to load %ix%i res pano map", end - start, width, height );
		}
		else if ( MatchesHead( "cube ", msg ) )
		{
			unsigned char * data[ 6 ];
			int		size;
			sscanf( msg, "cube %i %p %p %p %p %p %p", &size, &data[ 0 ], &data[ 1 ], &data[ 2 ], &data[ 3 ], &data[ 4 ], &data[ 5 ] );

			const double start = ovr_GetTimeInSeconds( );

			photos->LoadRgbaCubeMap( size, data, true );
			for ( int i = 0; i < 6; i++ )
			{
				free( data[ i ] );
			}

			// Add a sync object for uploading textures
			EGLSyncKHR GpuSync = eglCreateSyncKHR_( photos->EglDisplay, EGL_SYNC_FENCE_KHR, NULL );
			if ( GpuSync == EGL_NO_SYNC_KHR ) {
				FAIL( "BackgroundGLLoadThread eglCreateSyncKHR_():EGL_NO_SYNC_KHR" );
			}

			// Force it to flush the commands and wait until the textures are fully uploaded
			if ( EGL_FALSE == eglClientWaitSyncKHR_( photos->EglDisplay, GpuSync, EGL_SYNC_FLUSH_COMMANDS_BIT_KHR,
				EGL_FOREVER_KHR ) )
			{
				LOG( "BackgroundGLLoadThread eglClientWaitSyncKHR returned EGL_FALSE" );
			}

			photos->app->GetMessageQueue( ).PostPrintf( "%s", "loaded cube" );
			
			const double end = ovr_GetTimeInSeconds();
			LOG( "%4.2fs to load %i res cube map", end - start, size );
		}
	}

	// release the window so it can be made current by another thread
	if ( eglMakeCurrent( photos->EglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT ) == EGL_FALSE )
	{
		FAIL( "BackgroundGLLoadThread eglMakeCurrent: shutdown failed" );
	}

	if ( eglDestroyContext( photos->EglDisplay, EglBGLoaderContext ) == EGL_FALSE )
	{
		FAIL( "BackgroundGLLoadThread eglDestroyContext: shutdown failed" );
	}
	return NULL;
}

void Oculus360Photos::Command( const char * msg )
{
	if ( MatchesHead( "loaded pano", msg ) )
	{
		BackgroundPanoTexData.Swap();
		CurrentPanoIsCubeMap = false;
		SetMenuState( MENU_PANO_FADEIN );
		app->GetGazeCursor().ClearGhosts();
		return;
	}

	if ( MatchesHead( "loaded cube", msg ) )
	{
		BackgroundCubeTexData.Swap();
		CurrentPanoIsCubeMap = true;
		SetMenuState( MENU_PANO_FADEIN );
		app->GetGazeCursor( ).ClearGhosts( );
		return;
	}
}

bool Oculus360Photos::GetUseOverlay() const {
	// Don't enable the overlay when in throttled state
	return ( UseOverlay && !ovr_GetPowerLevelStateThrottled() );
}

void Oculus360Photos::ConfigureVrMode( ovrModeParms & modeParms ) {
	// We need very little CPU for pano browsing, but a fair amount of GPU.
	// The CPU clock should ramp up above the minimum when necessary.
	LOG( "ConfigureClocks: Oculus360Photos only needs minimal clocks" );
	modeParms.CpuLevel = 1;	// jpeg loading is slow, but otherwise we need little
	modeParms.GpuLevel = 3;	// we need a fair amount for cube map overlay support

	// When the app is throttled, go to the platform UI and display a
	// dismissable warning. On return to the app, force 30hz timewarp.
	modeParms.AllowPowerSave = true;

	// No hard edged geometry, so no need for MSAA
	app->GetVrParms().multisamples = 1;

	app->GetVrParms().colorFormat = COLOR_8888;
	app->GetVrParms().depthFormat = DEPTH_16;
}

bool Oculus360Photos::OnKeyEvent( const int keyCode, const KeyState::eKeyEventType eventType )
{
#ifdef ENABLE_MENU
	if ( ( ( keyCode == AKEYCODE_BACK ) && ( eventType == KeyState::KEY_EVENT_SHORT_PRESS ) ) ||
		( ( keyCode == KEYCODE_B ) && ( eventType == KeyState::KEY_EVENT_UP ) ) )
	{
		if ( MenuState == MENU_PANO_LOADING )
		{
			return true;
		}

		// hide attribution menu
		if ( PanoMenu->IsOpen() )
		{
			app->GetGuiSys().CloseMenu( app, PanoMenu, false );
		}

		// if the menu is closed, open it
		if ( Browser->IsClosedOrClosing() )
		{
			app->GetGuiSys().OpenMenu( app, app->GetGazeCursor(), OvrFolderBrowser::MENU_NAME );
			return true;
		}

		// back out dir or prompt exit
		//return Swipe->OnKeyEvent(keyCode, eventType);
	}
#endif
	return false;
}

void Oculus360Photos::LoadRgbaCubeMap( const int resolution, const unsigned char * const rgba[ 6 ], const bool useSrgbFormat )
{
	GL_CheckErrors( "enter LoadRgbaCubeMap" );

	const GLenum glFormat = GL_RGBA;
	const GLenum glInternalFormat = useSrgbFormat ? GL_SRGB8_ALPHA8 : GL_RGBA;

	// Create texture storage once
	GLuint texId = BackgroundCubeTexData.GetLoadTexId();
	if ( texId == 0 || !BackgroundCubeTexData.SameSize( resolution, resolution ) )
	{
		FreeTexture( texId );
		glGenTextures( 1, &texId );
		glBindTexture( GL_TEXTURE_CUBE_MAP, texId );
		glTexStorage2D( GL_TEXTURE_CUBE_MAP, 1, glInternalFormat, resolution, resolution );
		for ( int side = 0; side < 6; side++ )
		{
			glTexSubImage2D( GL_TEXTURE_CUBE_MAP_POSITIVE_X + side, 0, 0, 0, resolution, resolution, glFormat, GL_UNSIGNED_BYTE, rgba[ side ] );
		}
		BackgroundCubeTexData.SetSize( resolution, resolution );
		BackgroundCubeTexData.SetLoadTexId( texId );
	}
	else // reuse the texture storage
	{
		glBindTexture( GL_TEXTURE_CUBE_MAP, texId );
		for ( int side = 0; side < 6; side++ )
		{
			glTexSubImage2D( GL_TEXTURE_CUBE_MAP_POSITIVE_X + side, 0, 0, 0, resolution, resolution, glFormat, GL_UNSIGNED_BYTE, rgba[ side ] );
		}

	}
	glGenerateMipmap( GL_TEXTURE_CUBE_MAP );

	glTexParameteri( GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	glTexParameteri( GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR );

	glBindTexture( GL_TEXTURE_CUBE_MAP, 0 );

	GL_CheckErrors( "leave LoadRgbaCubeMap" );
}

void Oculus360Photos::LoadRgbaTexture( const unsigned char * data, int width, int height, const bool useSrgbFormat )
{
	GL_CheckErrors( "enter LoadRgbaTexture" );

	const GLenum glFormat = GL_RGBA;
	const GLenum glInternalFormat = useSrgbFormat ? GL_SRGB8_ALPHA8 : GL_RGBA;

	// Create texture storage once
	GLuint texId = BackgroundPanoTexData.GetLoadTexId();
	if ( texId == 0 || !BackgroundPanoTexData.SameSize( width, height ) )
	{
		FreeTexture( texId );
		glGenTextures( 1, &texId );
		glBindTexture( GL_TEXTURE_2D, texId );
		glTexStorage2D( GL_TEXTURE_2D, 1, glInternalFormat, width, height );
		glTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, width, height, glFormat, GL_UNSIGNED_BYTE, data );
		BackgroundPanoTexData.SetSize( width, height );
		BackgroundPanoTexData.SetLoadTexId( texId );
	}
	else // reuse the texture storage
	{
		glBindTexture( GL_TEXTURE_2D, texId );
		glTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, width, height, glFormat, GL_UNSIGNED_BYTE, data );
	}

	BuildTextureMipmaps( texId );
	MakeTextureTrilinear( texId );
	glBindTexture( GL_TEXTURE_2D, texId );
	// Because equirect panos pinch at the poles so much,
	// they would pull in mip maps so deep you would see colors
	// from the opposite half of the pano.  Clamping the level avoids this.
	// A well filtered pano shouldn't have any high frequency texels
	// that alias near the poles.
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 2 );
	glBindTexture( GL_TEXTURE_2D, 0 );

	GL_CheckErrors( "leave LoadRgbaTexture" );
}

Matrix4f CubeMatrixForViewMatrix( const Matrix4f & viewMatrix )
{
	Matrix4f m = viewMatrix;
	// clear translation
	for ( int i = 0; i < 3; i++ )
	{
		m.M[ i ][ 3 ] = 0.0f;
	}
	return m.Inverted();
}

Matrix4f Oculus360Photos::DrawEyeView( const int eye, const float fovDegrees )
{
	// Don't draw the scene at all if it is faded out
	const bool drawScene = true;

	const Matrix4f view = drawScene ?
		Scene.DrawEyeView( eye, fovDegrees )
		: Scene.MvpForEye( eye, fovDegrees );

	const float color = CurrentFadeLevel;
	// Dim pano when browser open
	float fadeColor = color;
#ifdef ENABLE_MENU
	if ( Browser->IsOpenOrOpening() || MenuState == MENU_PANO_LOADING )
	{
		fadeColor *= 0.09f;
	}
#endif

	if ( GetUseOverlay() && CurrentPanoIsCubeMap )
	{
		// Clear everything to 0 alpha so the overlay plane shows through.
		glClearColor( 0, 0, 0, 0 );
		glClear( GL_COLOR_BUFFER_BIT );

		const Matrix4f	m( CubeMatrixForViewMatrix( Scene.CenterViewMatrix() ) );
		GLuint texId = BackgroundCubeTexData.GetRenderTexId();
		glBindTexture( GL_TEXTURE_CUBE_MAP, texId );
		if ( HasEXT_sRGB_texture_decode )
		{
			glTexParameteri( GL_TEXTURE_CUBE_MAP, GL_TEXTURE_SRGB_DECODE_EXT,
				UseSrgb ? GL_DECODE_EXT : GL_SKIP_DECODE_EXT );
		}
		glBindTexture( GL_TEXTURE_CUBE_MAP, 0 );

		app->GetSwapParms().WarpOptions = ( UseSrgb ? 0 : SWAP_OPTION_INHIBIT_SRGB_FRAMEBUFFER );
		app->GetSwapParms( ).Images[ eye ][ 1 ].TexId = texId;
		app->GetSwapParms().Images[ eye ][ 1 ].TexCoordsFromTanAngles = m;
		app->GetSwapParms().Images[ eye ][ 1 ].Pose = FrameInput.PoseState;
		app->GetSwapParms().WarpProgram = WP_CHROMATIC_MASKED_CUBE;
		for ( int i = 0; i < 4; i++ )
		{
			app->GetSwapParms().ProgramParms[ i ] = fadeColor;
		}
	}
	else
	{
		app->GetSwapParms().WarpOptions = UseSrgb ? 0 : SWAP_OPTION_INHIBIT_SRGB_FRAMEBUFFER;
		app->GetSwapParms().Images[ eye ][ 1 ].TexId = 0;
		app->GetSwapParms().WarpProgram = WP_CHROMATIC;
		for ( int i = 0; i < 4; i++ )
		{
			app->GetSwapParms().ProgramParms[ i ] = 1.0f;
		}

		glActiveTexture( GL_TEXTURE0 );
		if ( CurrentPanoIsCubeMap )
		{
			glBindTexture( GL_TEXTURE_CUBE_MAP, BackgroundCubeTexData.GetRenderTexId( ) );
			if ( HasEXT_sRGB_texture_decode )
			{
				glTexParameteri( GL_TEXTURE_CUBE_MAP, GL_TEXTURE_SRGB_DECODE_EXT,
					UseSrgb ? GL_DECODE_EXT : GL_SKIP_DECODE_EXT );
			}
		}
		else
		{
			glBindTexture( GL_TEXTURE_2D, BackgroundPanoTexData.GetRenderTexId( ) );
			if ( HasEXT_sRGB_texture_decode )
			{
				glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_SRGB_DECODE_EXT,
					UseSrgb ? GL_DECODE_EXT : GL_SKIP_DECODE_EXT );
			}
		}

		GlProgram & prog = CurrentPanoIsCubeMap ? CubeMapPanoProgram : TexturedMvpProgram;

		glUseProgram( prog.program );

		glUniform4f( prog.uColor, fadeColor, fadeColor, fadeColor, fadeColor );
		glUniformMatrix4fv( prog.uMvp, 1, GL_FALSE /* not transposed */,
			view.Transposed().M[ 0 ] );

		Globe.Draw();

		glBindTexture( GL_TEXTURE_CUBE_MAP, 0 );
		glBindTexture( GL_TEXTURE_2D, 0 );
	}

	GL_CheckErrors( "draw" );

	return view;
}


float Fade( double now, double start, double length )
{
	return OVR::Alg::Clamp( ( ( now - start ) / length ), 0.0, 1.0 );
}

void Oculus360Photos::StartBackgroundPanoLoad( const char * filename )
{
	LOG( "StartBackgroundPanoLoad( %s )", filename );

	// Queue1 will determine if this is a cube map and then post a message for each
	// cube face to the other queues.

	bool isCubeMap = strstr( filename, "_nz.jpg" );
	char const * command = isCubeMap ? "cube" : "pano";

	// Dump any load that hasn't started
	Queue1.ClearMessages();

	// Start a background load of the current pano image
	Queue1.PostPrintf( "%s %s", command, filename );
}

void Oculus360Photos::SetMenuState( const OvrMenuState state )
{
	OvrMenuState lastState = MenuState;
	MenuState = state;
	LOG( "%s to %s", MenuStateString( lastState ), MenuStateString( MenuState ) );
	switch ( MenuState )
	{
	case MENU_NONE:
		break;
	case MENU_BACKGROUND_INIT:
		StartBackgroundPanoLoad( StartupPano );
		break;
	case MENU_BROWSER:
#ifdef ENABLE_MENU
		app->GetGuiSys().CloseMenu( app, PanoMenu, false );
		app->GetGuiSys().OpenMenu( app, app->GetGazeCursor(), OvrFolderBrowser::MENU_NAME );
		BrowserOpenTime = 0.0f;
#endif
		break;
	case MENU_PANO_LOADING:
#ifdef ENABLE_MENU
		app->GetGuiSys().CloseMenu( app, Browser, false );
		app->GetGuiSys().OpenMenu( app, app->GetGazeCursor(), OvrPanoMenu::MENU_NAME );
#endif
		CurrentFadeRate = FadeOutRate;
		Fader.StartFadeOut();
		StartBackgroundPanoLoad( ActivePano->Url );

#ifdef ENABLE_MENU
		PanoMenu->UpdateButtonsState( ActivePano );
#endif
		break;
	// pano menu now to fully open
	case MENU_PANO_FADEIN:
	case MENU_PANO_REOPEN_FADEIN:
#ifdef ENABLE_MENU
		if ( lastState != MENU_BACKGROUND_INIT )
		{
			app->GetGuiSys().OpenMenu( app, app->GetGazeCursor(), OvrPanoMenu::MENU_NAME );
		}
		else
		{
			app->GetGuiSys( ).OpenMenu( app, app->GetGazeCursor( ), OvrFolderBrowser::MENU_NAME );
		}
#endif			
		Fader.Reset( );
		CurrentFadeRate = FadeInRate;
		Fader.StartFadeIn( );
		break;
	case MENU_PANO_FULLY_VISIBLE:
		PanoMenuTimeLeft = PanoMenuVisibleTime;
		break;
	case MENU_PANO_FADEOUT:
#ifdef ENABLE_MENU
		PanoMenu->StartFadeOut();
#endif		
		break;
	default:
		OVR_ASSERT( false );
		break;
	}
}

void Oculus360Photos::OnPanoActivated( const OvrMetaDatum * panoData )
{
	ActivePano = static_cast< const OvrPhotosMetaDatum * >( panoData );
	Browser->ReloadFavoritesBuffer();
	SetMenuState( MENU_PANO_LOADING );
}

Matrix4f Oculus360Photos::Frame( const VrFrame vrFrame )
{
	FrameInput = vrFrame;

	// if just starting up, begin loading a background image
	if ( !StartupPano.IsEmpty() )
	{
		SetMenuState( MENU_BACKGROUND_INIT );
		StartupPano.Clear();
	}

	// disallow player movement
	VrFrame vrFrameWithoutMove = vrFrame;
	vrFrameWithoutMove.Input.sticks[ 0 ][ 0 ] = 0.0f;
	vrFrameWithoutMove.Input.sticks[ 0 ][ 1 ] = 0.0f;
	Scene.Frame( app->GetVrViewParms(), vrFrameWithoutMove, app->GetSwapParms().ExternalVelocity );

#ifdef ENABLE_MENU
	// reopen PanoMenu when in pano
	if ( ActivePano && Browser->IsClosedOrClosing( ) && ( MenuState != MENU_PANO_LOADING ) )
	{
		// single touch 
		if ( MenuState > MENU_PANO_FULLY_VISIBLE && vrFrame.Input.buttonPressed & ( BUTTON_TOUCH_SINGLE | BUTTON_A ) )
		{
			SetMenuState( MENU_PANO_REOPEN_FADEIN );
		}

		// PanoMenu input - needs to swipe even when PanoMenu is closed and in pano
		const OvrPhotosMetaDatum * nextPano = NULL;
		
		if ( vrFrame.Input.buttonPressed & ( BUTTON_SWIPE_BACK | BUTTON_DPAD_LEFT | BUTTON_LSTICK_LEFT ) )
		{
			nextPano = static_cast< const OvrPhotosMetaDatum * >( Browser->NextFileInDirectory( -1 ) );
		}
		else if ( vrFrame.Input.buttonPressed & ( BUTTON_SWIPE_FORWARD | BUTTON_DPAD_RIGHT | BUTTON_LSTICK_RIGHT ) )
		{
			nextPano = static_cast< const OvrPhotosMetaDatum * >( Browser->NextFileInDirectory( 1 ) );
		}

		if ( nextPano && ( ActivePano != nextPano ) )
		{
			PanoMenu->RepositionMenu( app );
			app->PlaySound( "sv_release_active" );
			SetActivePano( nextPano );
			SetMenuState( MENU_PANO_LOADING );
		}
	}

	if ( Browser->IsOpenOrOpening() )
	{
		// Close the browser if a Pano is active and not gazing at menu - ie. between panels
		if ( ActivePano && !Browser->GazingAtMenu() && vrFrame.Input.buttonReleased & ( BUTTON_TOUCH_SINGLE | BUTTON_A ) )
		{
			app->GetGuiSys().CloseMenu( app, Browser, false );
		}
	}
#endif		

	// State transitions
	if ( Fader.GetFadeState() != Fader::FADE_NONE )
	{
		Fader.Update( CurrentFadeRate, vrFrame.DeltaSeconds );
		if ( MenuState != MENU_PANO_REOPEN_FADEIN )
		{
			CurrentFadeLevel = Fader.GetFinalAlpha();
		}
	}
	else if ( (MenuState == MENU_PANO_FADEIN || MenuState == MENU_PANO_REOPEN_FADEIN ) && 
		Fader.GetFadeAlpha() == 1.0 )
	{
		SetMenuState( MENU_PANO_FULLY_VISIBLE );
	}

	if ( MenuState == MENU_PANO_FULLY_VISIBLE )
	{
		if ( !PanoMenu->Interacting() )
		{
			if ( PanoMenuTimeLeft > 0.0f )
			{
				PanoMenuTimeLeft -= vrFrame.DeltaSeconds;
			}
			else
			{
				PanoMenuTimeLeft = 0.0f;
				SetMenuState( MENU_PANO_FADEOUT );
			}
		}
		else // Reset PanoMenuTimeLeft
		{
			PanoMenuTimeLeft = PanoMenuVisibleTime;
		}
	}

	// We could disable the srgb convert on the FBO. but this is easier
	app->GetVrParms().colorFormat = UseSrgb ? COLOR_8888_sRGB : COLOR_8888;

	// Draw both eyes
	app->DrawEyeViewsPostDistorted( Scene.CenterViewMatrix() );
	
	return Scene.CenterViewMatrix();
}

const char * menuStateNames[] = 
{
	"MENU_NONE",
	"MENU_BACKGROUND_INIT",
	"MENU_BROWSER",
	"MENU_PANO_LOADING",
	"MENU_PANO_FADEIN",
	"MENU_PANO_REOPEN_FADEIN",
	"MENU_PANO_FULLY_VISIBLE",
	"MENU_PANO_FADEOUT",
	"NUM_MENU_STATES"
};

const char* Oculus360Photos::MenuStateString( const OvrMenuState state )
{
	OVR_ASSERT( state >= 0 && state < NUM_MENU_STATES );
	return menuStateNames[ state ];
}

int Oculus360Photos::ToggleCurrentAsFavorite()
{
	// Save MetaData - 
	TagAction result = MetaData->ToggleTag( const_cast< OvrPhotosMetaDatum * >( ActivePano ), "Favorites" );

	switch ( result )
	{
	case TAG_ADDED:
		Browser->AddToFavorites( ActivePano );
		break;
	case TAG_REMOVED:
		Browser->RemoveFromFavorites( ActivePano );
		break;
	case TAG_ERROR:
	default:
		OVR_ASSERT( false );
		break;
	}

	return result;
}

int Oculus360Photos::GetNumPanosInActiveCategory() const
{
	return Browser->GetNumPanosInActive();
}

bool Oculus360Photos::GetWantSrgbFramebuffer() const
{
	return true;
}

bool Oculus360Photos::AllowPanoInput() const
{
	return Browser->IsClosed() && MenuState == MENU_PANO_FULLY_VISIBLE;
}

}
