/************************************************************************************

Filename    :   SceneManager.h
Content     :	Handles rendering of current scene and movie.
Created     :   September 3, 2013
Authors     :	Jim Dos�, based on a fork of VrVideo.cpp from VrVideo by John Carmack.

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the Cinema/ directory. An additional grant 
of patent rights can be found in the PATENTS file in the same directory.

*************************************************************************************/

#if !defined( SceneManager_h )
#define SceneManager_h

#include "MovieManager.h"
#include "ModelView.h"
#include "Lerp.h"

using namespace OVR;

namespace OculusCinema {

class CinemaApp;

class SceneManager
{
public:
						SceneManager( CinemaApp &app_ );

	void 				OneTimeInit( const char * launchIntent );
	void				OneTimeShutdown();

	Matrix4f 			DrawEyeView( const int eye, const float fovDegrees );

	bool 				Command( const char * msg );

	Matrix4f 			Frame( const VrFrame & vrFrame );

	void 				SetSeat( int newSeat );
	bool 				ChangeSeats( const VrFrame & vrFrame );

	void 				ClearMovie();
	void 				PutScreenInFront();

	void				ClearGazeCursorGhosts();  	// clear gaze cursor to avoid seeing it lerp
	
	void 				ToggleLights( const float duration );
	void 				LightsOn( const float duration );
	void 				LightsOff( const float duration );

	void				SetSceneModel( const SceneDef &sceneDef );
	void				SetSceneProgram( const sceneProgram_t opaqueProgram, const sceneProgram_t additiveProgram );

	Posef				GetScreenPose() const;
	Vector2f			GetScreenSize() const;

	void 				SetFreeScreenAngles( const Vector3f &angles );
	Vector3f			GetFreeScreenScale() const;

	Matrix4f			FreeScreenMatrix() const;
	Matrix4f 			BoundsScreenMatrix( const Bounds3f & bounds, const float movieAspect ) const;
	Matrix4f 			ScreenMatrix() const;

	void				AllowMovement( bool allow ) { AllowMove = allow; }
	bool				MovementAllowed() const { return AllowMove; }

	bool				GetUseOverlay() const;

public:
	CinemaApp &			Cinema;

	// Allow static lighting to be faded up or down
	Lerp				StaticLighting;

	bool				UseOverlay;

	SurfaceTexture	* 	MovieTexture;
	long long			MovieTextureTimestamp;

	// FreeScreen mode allows the screen to be oriented arbitrarily, rather
	// than on a particular surface in the scene.
	bool				FreeScreenActive;
	float				FreeScreenScale;
	float				FreeScreenDistance;
	Matrix4f			FreeScreenOrientation;
	Vector3f			FreeScreenAngles;

	// don't make these bool, or sscanf %i will trash adjacent memory!
	int					ForceMono;			// only show the left eye of 3D movies

	// Set when MediaPlayer knows what the stream size is.
	// current is the aspect size, texture may be twice as wide or high for 3D content.
	int					CurrentMovieWidth;	// set to 0 when a new movie is started, don't render until non-0
	int					CurrentMovieHeight;
	int					MovieTextureWidth;
	int					MovieTextureHeight;
	MovieFormat			CurrentMovieFormat;
	int					MovieRotation;
	int					MovieDuration;

	bool				FrameUpdateNeeded;
	int					ClearGhostsFrames;

	GlGeometry			UnitSquare;		// -1 to 1

	// We can't directly create a mip map on the OES_external_texture, so
	// it needs to be copied to a conventional texture.
	// It must be triple buffered for use as a TimeWarp overlay plane.
	int					CurrentMipMappedMovieTexture;	// 0 - 2
	GLuint				MipMappedMovieTextures[3];
	GLuint				MipMappedMovieFBOs[3];

	GLuint				ScreenVignetteTexture;
	GLuint				ScreenVignetteSbsTexture;	// for side by side 3D

	sceneProgram_t		SceneProgramIndex;

	OvrSceneView		Scene;
	SceneDef			SceneInfo;
	SurfaceDef *		SceneScreenSurface;		// override this to the movie texture
	const ModelTag *	SceneScreenTag;			//

	static const int	MAX_SEATS = 8;
	Vector3f			SceneSeatPositions[MAX_SEATS];
	int					SceneSeatCount;
	int					SeatPosition;

	Matrix4f			SceneScreenMatrix;
	Bounds3f			SceneScreenBounds;

	bool 				AllowMove;

	bool				VoidedScene;

private:
	GLuint 				BuildScreenVignetteTexture( const int horizontalTile ) const;
	int 				BottomMipLevel( const int width, const int height ) const;
	void 				ClampScreenToView();
};

} // namespace OculusCinema

#endif // SceneManager_h
