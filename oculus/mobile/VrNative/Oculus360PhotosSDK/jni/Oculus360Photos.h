/************************************************************************************

Filename    :   Oculus360Photos.h
Content     :   360 Panorama Viewer
Created     :   August 13, 2014
Authors     :   John Carmack, Warsam Osman

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the Oculus360Photos/ directory. An additional grant 
of patent rights can be found in the PATENTS file in the same directory.

************************************************************************************/

#ifndef OCULUS360PHOTOS_H
#define OCULUS360PHOTOS_H

#include "ModelView.h"
#include "VRMenu/Fader.h"
#include "Kernel/OVR_Lockless.h"

namespace OVR {

class PanoBrowser;
class OvrPanoMenu;
class OvrMetaData;
struct OvrMetaDatum;
class OvrPhotosMetaData;
struct OvrPhotosMetaDatum;

class Oculus360Photos : public VrAppInterface
{
public:
	enum OvrMenuState
	{
		MENU_NONE,
		MENU_BACKGROUND_INIT,
		MENU_BROWSER,
		MENU_PANO_LOADING,
		MENU_PANO_FADEIN,
		MENU_PANO_REOPEN_FADEIN,
		MENU_PANO_FULLY_VISIBLE,
		MENU_PANO_FADEOUT,
		NUM_MENU_STATES
	};

	class DoubleBufferedTextureData
	{
	public:
		DoubleBufferedTextureData();
		~DoubleBufferedTextureData();

		// Returns the current texid to render
		GLuint		GetRenderTexId() const;

		// Returns the free texid for load
		GLuint		GetLoadTexId() const;

		// Set the texid after creating a new texture.
		void		SetLoadTexId( const GLuint texId );

		// Swaps the buffers
		void		Swap();

		// Update the last loaded size
		void		SetSize( const int width, const int height );

		// Return true if passed in size match the load index size
		bool		SameSize( const int width, const int height ) const;

	private:
		GLuint			TexId[ 2 ];
		int				Width[ 2 ];
		int				Height[ 2 ];
		volatile int	CurrentIndex;
	};

	Oculus360Photos();
	~Oculus360Photos();

	virtual void		OneTimeInit( const char * fromPackage, const char * launchIntentJSON, const char * launchIntentURI );
	virtual void		OneTimeShutdown();
	virtual void		ConfigureVrMode( ovrModeParms & modeParms );
	virtual Matrix4f 	DrawEyeView( const int eye, const float fovDegrees );
	virtual Matrix4f 	Frame( VrFrame vrFrame );
	virtual void		Command( const char * msg );
	virtual bool 		OnKeyEvent( const int keyCode, const KeyState::eKeyEventType eventType );
	virtual bool		GetWantSrgbFramebuffer() const;

	void				OnPanoActivated( const OvrMetaDatum * panoData );
	PanoBrowser *		GetBrowser()										{ return Browser; }
	OvrPhotosMetaData *	GetMetaData()										{ return MetaData; }
	const OvrPhotosMetaDatum * GetActivePano() const						{ return ActivePano; }
	void				SetActivePano( const OvrPhotosMetaDatum * data )	{ OVR_ASSERT( data );  ActivePano = data; }
	float				GetFadeLevel() const								{ return CurrentFadeLevel;  }
	int					GetNumPanosInActiveCategory() const;

	void				SetMenuState( const OvrMenuState state );
	OvrMenuState		GetCurrentState() const								{ return  MenuState; }

	int					ToggleCurrentAsFavorite();

	bool				GetUseOverlay() const;
	bool				AllowPanoInput() const;
	MessageQueue &		GetBGMessageQueue() { return BackgroundCommands;  }
	
private:
	// Background textures loaded into GL by background thread using shared context
	static void *		BackgroundGLLoadThread( void * v );
	void				StartBackgroundPanoLoad( const char * filename );
	const char *		MenuStateString( const OvrMenuState state );
	bool 				LoadMetaData( const char * metaFile );
	void				LoadRgbaCubeMap( const int resolution, const unsigned char * const rgba[ 6 ], const bool useSrgbFormat );
	void				LoadRgbaTexture( const unsigned char * data, int width, int height, const bool useSrgbFormat );

	// shared vars
	jclass				mainActivityClass;	// need to look up from main thread
	GlGeometry			Globe;

	OvrSceneView		Scene;
	SineFader			Fader;
	
	// Pano data and menus
	Array< String > 			SearchPaths;
	OvrPhotosMetaData *			MetaData;
	OvrPanoMenu *				PanoMenu;
	PanoBrowser *				Browser;
	const OvrPhotosMetaDatum *	ActivePano;
	String						StartupPano;

	// panorama vars
	DoubleBufferedTextureData	BackgroundPanoTexData;
	DoubleBufferedTextureData	BackgroundCubeTexData;
	bool				CurrentPanoIsCubeMap;

	GlProgram			TexturedMvpProgram;
	GlProgram			CubeMapPanoProgram;
	GlProgram			PanoramaProgram;

	VrFrame				FrameInput;
	OvrMenuState		MenuState;

	const float			FadeOutRate;
	const float			FadeInRate;
	const float			PanoMenuVisibleTime;
	float				CurrentFadeRate;
	float				CurrentFadeLevel;	
	float				PanoMenuTimeLeft;
	float				BrowserOpenTime;

	bool				UseOverlay;				// use the TimeWarp environment overlay
	bool				UseSrgb;
	
	// Background texture commands produced by FileLoader consumed by BackgroundGLLoadThread
	MessageQueue		BackgroundCommands;

	// The background loader loop will exit when this is set true.
	LocklessUpdater<bool>		ShutdownRequest;

	// BackgroundGLLoadThread private GL context used for loading background textures
	EGLint				EglClientVersion;
	EGLDisplay			EglDisplay;
	EGLConfig			EglConfig;
	EGLSurface			EglPbufferSurface;
	EGLContext			EglShareContext;
};

}

#endif // OCULUS360PHOTOS_H
