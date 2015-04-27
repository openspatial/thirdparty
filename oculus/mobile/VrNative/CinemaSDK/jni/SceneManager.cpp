/************************************************************************************

Filename    :   SceneManager.cpp
Content     :	Handles rendering of current scene and movie.
Created     :   September 3, 2013
Authors     :	Jim Dos�, based on a fork of VrVideo.cpp from VrVideo by John Carmack.

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the Cinema/ directory. An additional grant 
of patent rights can be found in the PATENTS file in the same directory.

*************************************************************************************/

#include "Kernel/OVR_String_Utils.h"
#include "VrApi/VrApi.h"
#include "VrApi/VrApi_Helpers.h"

#include "CinemaApp.h"
#include "Native.h"
#include "SceneManager.h"


namespace OculusCinema
{

SceneManager::SceneManager( CinemaApp &cinema ) :
	Cinema( cinema ),
	StaticLighting(),
	UseOverlay( true ),
	MovieTexture( NULL ),
	MovieTextureTimestamp( 0 ),
	FreeScreenActive( false ),
	FreeScreenScale( 1.0f ),
	FreeScreenDistance( 1.5f ),
	FreeScreenOrientation(),
	FreeScreenAngles(),
	ForceMono( false ),
	CurrentMovieWidth( 0 ),
	CurrentMovieHeight( 480 ),
	MovieTextureWidth( 0 ),
	MovieTextureHeight( 0 ),
	CurrentMovieFormat( VT_2D ),
	MovieRotation( 0 ),
	MovieDuration( 0 ),
	FrameUpdateNeeded( false ),
	ClearGhostsFrames( 0 ),
	UnitSquare(),
	CurrentMipMappedMovieTexture( 0 ),
	MipMappedMovieTextures(),
	MipMappedMovieFBOs(),
	ScreenVignetteTexture( 0 ),
	ScreenVignetteSbsTexture( 0 ),
	SceneProgramIndex( SCENE_PROGRAM_DYNAMIC_ONLY ),
	Scene(),
	SceneScreenSurface( NULL ),
	SceneScreenTag( NULL ),
	SceneSeatPositions(),
	SceneSeatCount( 0 ),
	SeatPosition( 0 ),
	SceneScreenMatrix(),
	SceneScreenBounds(),
	AllowMove( false ),
	VoidedScene( false )

{
	MipMappedMovieTextures[0] = MipMappedMovieTextures[1] = MipMappedMovieTextures[2] = 0;
}

void SceneManager::OneTimeInit( const char * launchIntent )
{
	LOG( "SceneManager::OneTimeInit" );

	const double start = ovr_GetTimeInSeconds();

	UnitSquare = BuildTesselatedQuad( 1, 1 );

	UseOverlay = true;

	ScreenVignetteTexture = BuildScreenVignetteTexture( 1 );
	ScreenVignetteSbsTexture = BuildScreenVignetteTexture( 2 );

	LOG( "SceneManager::OneTimeInit: %3.1f seconds", ovr_GetTimeInSeconds() - start );
}

void SceneManager::OneTimeShutdown()
{
	LOG( "SceneManager::OneTimeShutdown" );

	// Free GL resources

	UnitSquare.Free();

	if ( ScreenVignetteTexture != 0 )
	{
		glDeleteTextures( 1, & ScreenVignetteTexture );
		ScreenVignetteTexture = 0;
	}

	if ( ScreenVignetteSbsTexture != 0 )
	{
		glDeleteTextures( 1, & ScreenVignetteSbsTexture );
		ScreenVignetteSbsTexture = 0;
	}
}

//=========================================================================================

static Vector3f MatrixUp( const Matrix4f & m )
{
	return Vector3f( m.M[1][0], m.M[1][1], m.M[1][2] );
}

static Vector3f MatrixForward( const Matrix4f & m )
{
	return Vector3f( -m.M[2][0], -m.M[2][1], -m.M[2][2] );
}

static Vector3f AnglesForMatrix( const Matrix4f &m )
{
	const Vector3f viewForward = MatrixForward( m );
	const Vector3f viewUp = MatrixUp( m );

	Vector3f angles;

	angles.z = 0.0f;

	if ( viewForward.y > 0.7f )
	{
		angles.y = atan2( viewUp.x, viewUp.z );
	}
	else if ( viewForward.y < -0.7f )
	{
		angles.y = atan2( -viewUp.x, -viewUp.z );
	}
	else if ( viewUp.y < 0.0f )
	{
		angles.y = atan2( viewForward.x, viewForward.z );
	}
	else
	{
		angles.y = atan2( -viewForward.x, -viewForward.z );
	}

	angles.x = atan2( viewForward.y, viewUp.y );

	return angles;
}

//=========================================================================================

// Sets the scene to the given model, sets the material on the
// model to the current sceneProgram for static / dynamic lighting,
// and updates:
// SceneScreenSurface
// SceneScreenBounds
// SeatPosition
void SceneManager::SetSceneModel( const SceneDef &sceneDef )
{
	LOG( "SetSceneModel %s", sceneDef.SceneModel->FileName.ToCStr() );

	VoidedScene = false;
	UseOverlay = true;

	SceneInfo = sceneDef;
	Scene.SetWorldModel( *SceneInfo.SceneModel );
	SceneScreenSurface = const_cast< SurfaceDef * >( Scene.FindNamedSurface( "screen" ) );
	SceneScreenTag = Scene.FindNamedTag( "screen" );

	SceneScreenMatrix.M[0][0] = -1.0f;
	SceneScreenMatrix.M[1][0] = 0.0f;
	SceneScreenMatrix.M[2][0] = 0.0f;

	SceneScreenMatrix.M[0][1] = 0.0f;
	SceneScreenMatrix.M[1][1] = 1.0f;
	SceneScreenMatrix.M[2][1] = 0.0f;

	SceneScreenMatrix.M[0][2] = 0.0f;
	SceneScreenMatrix.M[1][2] = 0.0f;
	SceneScreenMatrix.M[2][2] = -1.0f;

	for ( SceneSeatCount = 0; SceneSeatCount < MAX_SEATS; SceneSeatCount++ )
	{
		const ModelTag * tag = Scene.FindNamedTag( StringUtils::Va( "cameraPos%d", SceneSeatCount + 1 ) );
		if ( tag == NULL )
		{
			break;
		}
		SceneSeatPositions[SceneSeatCount] = tag->matrix.GetTranslation();
		SceneSeatPositions[SceneSeatCount].y -= Cinema.app->GetVrViewParms().EyeHeight;
	}

	if ( !sceneDef.UseSeats )
	{
		SceneSeatPositions[ SceneSeatCount ].z = 0.0f;
		SceneSeatPositions[ SceneSeatCount ].x = 0.0f;
		SceneSeatPositions[ SceneSeatCount ].y = 0.0f;
		SceneSeatCount++;
		SetSeat( 0 );
	}
	else if ( SceneSeatCount > 0 )
	{
		SetSeat( 0 );
	}
	else
	{
		// if no seats, create some at the position of the seats in home_theater
		for( int seatPos = -1; seatPos <= 2; seatPos++ )
		{
			SceneSeatPositions[ SceneSeatCount ].z = 3.6f;
			SceneSeatPositions[ SceneSeatCount ].x = 3.0f + seatPos * 0.9f - 0.45;
			SceneSeatPositions[ SceneSeatCount ].y = 0.0f;
			SceneSeatCount++;
		}

		for( int seatPos = -1; seatPos <= 1; seatPos++ )
		{
			SceneSeatPositions[ SceneSeatCount ].z = 1.8f;
			SceneSeatPositions[ SceneSeatCount ].x = 3.0f + seatPos * 0.9f;
			SceneSeatPositions[ SceneSeatCount ].y = -0.3f;
			SceneSeatCount++;
		}
		SetSeat( 1 );
	}

	Scene.YawOffset = 0.0f;
	Scene.Znear = 0.1f;
	Scene.Zfar = 2000.0f;

	ClearGazeCursorGhosts();

	if ( SceneInfo.UseDynamicProgram )
	{
		SetSceneProgram( SceneProgramIndex, SCENE_PROGRAM_ADDITIVE );

		if ( SceneScreenSurface )
		{
			SceneScreenBounds = SceneScreenSurface->cullingBounds;

			// force to a solid black material that cuts a hole in alpha
			SceneScreenSurface->materialDef.programObject = Cinema.ShaderMgr.ScenePrograms[0].program;
			SceneScreenSurface->materialDef.uniformMvp = Cinema.ShaderMgr.ScenePrograms[0].uMvp;
		}
	}

	FreeScreenActive = false;
	if ( SceneInfo.UseFreeScreen )
	{
		Vector3f angles = AnglesForMatrix( Scene.ViewMatrix );
		angles.x = 0.0f;
		angles.z = 0.0f;
		SetFreeScreenAngles( angles );
		FreeScreenActive = true;
	}
}

void SceneManager::SetSceneProgram( const sceneProgram_t opaqueProgram, const sceneProgram_t additiveProgram )
{
	if ( !Scene.WorldModel.Definition || !SceneInfo.UseDynamicProgram )
	{
		return;
	}

	const GlProgram & dynamicOnlyProg = Cinema.ShaderMgr.ScenePrograms[SCENE_PROGRAM_DYNAMIC_ONLY];
	const GlProgram & opaqueProg = Cinema.ShaderMgr.ScenePrograms[opaqueProgram];
	const GlProgram & additiveProg = Cinema.ShaderMgr.ScenePrograms[additiveProgram];
	const GlProgram & diffuseProg = Cinema.ShaderMgr.ProgSingleTexture;

	LOG( "SetSceneProgram: %d(%d), %d(%d)", opaqueProgram, opaqueProg.program, additiveProgram, additiveProg.program );

	ModelDef & def = *const_cast< ModelDef * >( &Scene.WorldModel.Definition->Def );
	for ( int i = 0; i < def.surfaces.GetSizeI(); i++ )
	{
		if ( &def.surfaces[i] == SceneScreenSurface )
		{
			continue;
		}

		MaterialDef & materialDef = def.surfaces[i].materialDef;

		if ( materialDef.gpuState.blendSrc == GL_ONE && materialDef.gpuState.blendDst == GL_ONE )
		{
			// Non-modulated additive material.
			if ( materialDef.textures[1] != 0 )
			{
				materialDef.textures[0] = materialDef.textures[1];
				materialDef.textures[1] = 0;
			}

			materialDef.programObject = additiveProg.program;
			materialDef.uniformMvp = additiveProg.uMvp;
		}
		else if ( materialDef.textures[1] != 0 )
		{
			// Modulated material.
			if ( materialDef.programObject != opaqueProg.program &&
				( materialDef.programObject == dynamicOnlyProg.program ||
					opaqueProg.program == dynamicOnlyProg.program ) )
			{
				Alg::Swap( materialDef.textures[0], materialDef.textures[1] );
			}

			materialDef.programObject = opaqueProg.program;
			materialDef.uniformMvp = opaqueProg.uMvp;
		}
		else
		{
			// Non-modulated diffuse material.
			materialDef.programObject = diffuseProg.program;
			materialDef.uniformMvp = diffuseProg.uMvp;
		}
	}

	SceneProgramIndex = opaqueProgram;
}

//=========================================================================================

static Vector3f ViewOrigin( const Matrix4f & view )
{
	return Vector3f( view.M[0][3], view.M[1][3], view.M[2][3] );
}

Posef SceneManager::GetScreenPose() const
{
	if ( FreeScreenActive )
	{
		const float applyScale = pow( 2.0f, FreeScreenScale );
		const Matrix4f screenMvp = FreeScreenOrientation *
				Matrix4f::Translation( 0, 0, -FreeScreenDistance*applyScale ) *
				Matrix4f::Scaling( applyScale, applyScale * (3.0f/4.0f), applyScale );

		return Posef( Quatf( FreeScreenOrientation ), ViewOrigin( screenMvp ) );
	}
	else
	{
		Vector3f pos = SceneScreenBounds.GetCenter();
		return Posef( Quatf(), pos );
	}
}

Vector2f SceneManager::GetScreenSize() const
{
	if ( FreeScreenActive )
	{
		Vector3f size = GetFreeScreenScale();
		return Vector2f( size.x * 2.0f, size.y * 2.0f );
	}
	else
	{
		Vector3f size = SceneScreenBounds.GetSize();
		return Vector2f( size.x, size.y );
	}
}

Vector3f SceneManager::GetFreeScreenScale() const
{
	// Scale is stored in a form that feels linear, raise to exponent to
	// get value to apply.
	const float applyScale = powf( 2.0f, FreeScreenScale );

	// adjust size based on aspect ratio
	float scaleX = 1.0f;
	float scaleY = (float)CurrentMovieHeight / CurrentMovieWidth;
	if ( scaleY > 0.6f )
	{
		scaleX *= 0.6f / scaleY;
		scaleY = 0.6f;
	}

	return Vector3f( applyScale * scaleX, applyScale * scaleY, applyScale );
}

Matrix4f SceneManager::FreeScreenMatrix() const
{
	const Vector3f scale = GetFreeScreenScale();
	return FreeScreenOrientation *
			Matrix4f::Translation( 0, 0, -FreeScreenDistance * scale.z ) *
			Matrix4f::Scaling( scale );
}

// Aspect is width / height
Matrix4f SceneManager::BoundsScreenMatrix( const Bounds3f & bounds, const float movieAspect ) const
{
	const Vector3f size = bounds.b[1] - bounds.b[0];
	const Vector3f center = bounds.b[0] + size * 0.5f;
	const float	screenHeight = size.y;
	const float screenWidth = OVR::Alg::Max( size.x, size.z );
	float widthScale;
	float heightScale;
	float aspect = ( movieAspect == 0.0f ) ? 1.0f : movieAspect;
	if ( screenWidth / screenHeight > aspect )
	{	// screen is wider than movie, clamp size to height
		heightScale = screenHeight * 0.5f;
		widthScale = heightScale * aspect;
	}
	else
	{	// screen is taller than movie, clamp size to width
		widthScale = screenWidth * 0.5f;
		heightScale = widthScale / aspect;
	}

	const float rotateAngle = ( size.x > size.z ) ? 0.0f : M_PI * 0.5f;

	return	Matrix4f::Translation( center ) *
			Matrix4f::RotationY( rotateAngle ) *
			Matrix4f::Scaling( widthScale, heightScale, 1.0f );
}

Matrix4f SceneManager::ScreenMatrix() const
{
	if ( FreeScreenActive )
	{
		return FreeScreenMatrix();
	}
	else
	{
		return BoundsScreenMatrix( SceneScreenBounds,
			( CurrentMovieHeight == 0 ) ? 1.0f : ( (float)CurrentMovieWidth / CurrentMovieHeight ) );
	}
}

bool SceneManager::GetUseOverlay() const
{
	// Don't enable the overlay when in throttled state
	//return ( UseOverlay && !ovr_GetPowerLevelStateThrottled() );

	// Quality is degraded too much when disabling the overlay.
	// Default behavior is 30Hz TW + no chromatic aberration
	return UseOverlay;
}

void SceneManager::ClearMovie()
{
	Native::StopMovie( Cinema.app );

	SetSceneProgram( SCENE_PROGRAM_DYNAMIC_ONLY, SCENE_PROGRAM_ADDITIVE );

	MovieTextureTimestamp = 0;
	FrameUpdateNeeded = true;
	CurrentMovieWidth = 0;
	MovieRotation = 0;
	MovieDuration = 0;

	delete MovieTexture;
	MovieTexture = NULL;
}

void SceneManager::SetFreeScreenAngles( const Vector3f &angles )
{
	FreeScreenAngles = angles;

	Matrix4f rollPitchYaw = Matrix4f::RotationY( FreeScreenAngles.y ) * Matrix4f::RotationX( FreeScreenAngles.x );
	const Vector3f ForwardVector( 0.0f, 0.0f, -1.0f );
	const Vector3f UpVector( 0.0f, 1.0f, 0.0f );
	const Vector3f forward = rollPitchYaw.Transform( ForwardVector );
	const Vector3f up = rollPitchYaw.Transform( UpVector );

	Vector3f shiftedEyePos = Scene.CenterEyePos();
	Vector3f headModelOffset = Scene.HeadModelOffset( 0.0f, FreeScreenAngles.x, FreeScreenAngles.y,
			Scene.ViewParms.HeadModelDepth, Scene.ViewParms.HeadModelHeight );
	shiftedEyePos += headModelOffset;
	Matrix4f result = Matrix4f::LookAtRH( shiftedEyePos, shiftedEyePos + forward, up );

	FreeScreenOrientation = result.Inverted();
}

void SceneManager::PutScreenInFront()
{
	FreeScreenOrientation = Scene.ViewMatrix.Inverted();
	Cinema.app->RecenterYaw( false );
}

void SceneManager::ClampScreenToView()
{
	if ( !FreeScreenActive )
	{
		return;
	}

	Vector3f viewAngles = AnglesForMatrix( Scene.ViewMatrix );
	Vector3f deltaAngles = FreeScreenAngles - viewAngles;

	if ( deltaAngles.y > M_PI )
	{	// screen is a bit under PI and view is a bit above -PI
		deltaAngles.y -= 2 * M_PI;
	}
	else if ( deltaAngles.y < -M_PI )
	{	// screen is a bit above -PI and view is a bit below PI
		deltaAngles.y += 2 * M_PI;
	}
	deltaAngles.y = OVR::Alg::Clamp( deltaAngles.y, -( float )M_PI * 0.20f, ( float )M_PI * 0.20f );

	if ( deltaAngles.x > M_PI )
	{	// screen is a bit under PI and view is a bit above -PI
		deltaAngles.x -= 2 * M_PI;
	}
	else if ( deltaAngles.x < -M_PI )
	{	// screen is a bit above -PI and view is a bit below PI
		deltaAngles.x += 2 * M_PI;
	}
	deltaAngles.x = OVR::Alg::Clamp( deltaAngles.x, -( float )M_PI * 0.125f, ( float )M_PI * 0.125f );

	SetFreeScreenAngles( viewAngles + deltaAngles );
}

void SceneManager::ClearGazeCursorGhosts()
{
	// clear gaze cursor to avoid seeing it lerp
	ClearGhostsFrames = 3;
}

void SceneManager::ToggleLights( const float duration )
{
	const double now = ovr_GetTimeInSeconds();
	StaticLighting.Set( now, StaticLighting.Value( now ), now + duration, 1.0 - StaticLighting.endValue );
}

void SceneManager::LightsOn( const float duration )
{
	const double now = ovr_GetTimeInSeconds();
	StaticLighting.Set( now, StaticLighting.Value( now ), now + duration, 1.0 );
}

void SceneManager::LightsOff( const float duration )
{
	const double now = ovr_GetTimeInSeconds();
	StaticLighting.Set( now, StaticLighting.Value( now ), now + duration, 0.0 );
}

//============================================================================================

GLuint SceneManager::BuildScreenVignetteTexture( const int horizontalTile ) const
{
	// make it an even border at 16:9 aspect ratio, let it get a little squished at other aspects
	static const int scale = 6;
	static const int width = 16 * scale * horizontalTile;
	static const int height = 9 * scale;
	unsigned char buffer[width * height];
	memset( buffer, 255, sizeof( buffer ) );
	for ( int i = 0; i < width; i++ )
	{
		buffer[i] = 0;
		buffer[width*height - 1 - i] = 0;
	}
	for ( int i = 0; i < height; i++ )
	{
		buffer[i * width] = 0;
		buffer[i * width + width - 1] = 0;
		if ( horizontalTile == 2 )
		{
			buffer[i * width + width / 2 - 1] = 0;
			buffer[i * width + width / 2] = 0;
		}
	}
    GLuint texId;
	glGenTextures( 1, &texId );
    glBindTexture( GL_TEXTURE_2D, texId );
    glTexImage2D( GL_TEXTURE_2D, 0, GL_LUMINANCE, width, height, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, buffer );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
    glBindTexture( GL_TEXTURE_2D, 0 );

    GL_CheckErrors( "screenVignette" );
    return texId;
}

int SceneManager::BottomMipLevel( const int width, const int height ) const
{
	int bottomMipLevel = 0;
	int dimension = OVR::Alg::Max( width, height );

	while( dimension > 1 )
	{
		bottomMipLevel++;
		dimension >>= 1;
	}

	return bottomMipLevel;
}

void SceneManager::SetSeat( int newSeat )
{
	SeatPosition = newSeat;
	Scene.FootPos = SceneSeatPositions[ SeatPosition ];
}

bool SceneManager::ChangeSeats( const VrFrame & vrFrame )
{
	bool changed = false;
	if ( SceneSeatCount > 0 )
	{
		Vector3f direction( 0.0f );
		if ( vrFrame.Input.buttonPressed & BUTTON_LSTICK_UP )
		{
			changed = true;
			direction[2] += 1.0f;
		}
		if ( vrFrame.Input.buttonPressed & BUTTON_LSTICK_DOWN )
		{
			changed = true;
			direction[2] -= 1.0f;
		}
		if ( vrFrame.Input.buttonPressed & BUTTON_LSTICK_RIGHT )
		{
			changed = true;
			direction[0] += 1.0f;
		}
		if ( vrFrame.Input.buttonPressed & BUTTON_LSTICK_LEFT )
		{
			changed = true;
			direction[0] -= 1.0f;
		}

		if ( changed )
		{
			// Find the closest seat in the desired direction away from the current seat.
			direction.Normalize();
			const float distance = direction.Dot( Scene.FootPos );
			float bestSeatDistance = Math<float>::MaxValue;
			int bestSeat = -1;
			for ( int i = 0; i < SceneSeatCount; i++ )
			{
				const float d = direction.Dot( SceneSeatPositions[i] ) - distance;
				if ( d > 0.01f && d < bestSeatDistance )
				{
					bestSeatDistance = d;
					bestSeat = i;
				}
			}
			if ( bestSeat != -1 )
			{
				SetSeat( bestSeat );
			}
		}
	}

	return changed;
}

/*
 * Command
 *
 * Actions that need to be performed on the render thread.
 */
bool SceneManager::Command( const char * msg )
{
	// Always include the space in MatchesHead to prevent problems
	// with commands with matching prefixes.
	LOG( "SceneManager::Command: %s", msg );

	if ( MatchesHead( "newVideo ", msg ) )
	{
		delete MovieTexture;
		MovieTexture = new SurfaceTexture( Cinema.app->GetVrJni() );
		LOG( "RC_NEW_VIDEO texId %i", MovieTexture->textureId );

		MessageQueue * receiver;
		sscanf( msg, "newVideo %p", &receiver );

		receiver->PostPrintf( "surfaceTexture %p", MovieTexture->javaObject );

		// don't draw the screen until we have the new size
		CurrentMovieWidth = 0;
		return true;
	}

	if ( MatchesHead( "video ", msg ) )
	{
		int width, height;
		sscanf( msg, "video %i %i %i %i", &width, &height, &MovieRotation, &MovieDuration );

		const MovieDef *movie = Cinema.GetCurrentMovie();
		assert( movie );

		// always use 2d form lobby movies
		if ( ( movie == NULL ) || SceneInfo.LobbyScreen )
		{
			CurrentMovieFormat = VT_2D;
		}
		else
		{
			CurrentMovieFormat = movie->Format;

			// if movie format is not set, make some assumptions based on the width and if it's 3D
			if ( movie->Format == VT_UNKNOWN )
			{
				if ( movie->Is3D )
				{
					if ( width > height * 3 )
					{
						CurrentMovieFormat = VT_LEFT_RIGHT_3D_FULL;
					}
					else
					{
						CurrentMovieFormat = VT_LEFT_RIGHT_3D;
					}
				}
				else
				{
					CurrentMovieFormat = VT_2D;
				}
			}
		}

		MovieTextureWidth = width;
		MovieTextureHeight = height;

		// Disable overlay on larger movies to reduce judder
		long numberOfPixels = MovieTextureWidth * MovieTextureHeight;
		LOG( "Movie size: %dx%d = %d pixels", MovieTextureWidth, MovieTextureHeight, numberOfPixels );

		// use the void theater on large movies
		if ( numberOfPixels > 1920 * 1080 )
		{
			LOG( "Oversized movie.  Switching to Void scene to reduce judder" );
			SetSceneModel( *Cinema.ModelMgr.VoidScene );
			UseOverlay = false;
			VoidedScene = true;

			// downsize the screen resolution to fit into a 960x540 buffer
			float aspectRatio = ( float )width / ( float )height;
			if ( aspectRatio < 1.0f )
			{
				MovieTextureWidth = aspectRatio * 540.0f;
				MovieTextureHeight = 540;
			}
			else
			{
				MovieTextureWidth = 960;
				MovieTextureHeight = aspectRatio * 960.0f;
			}
		}

		switch( CurrentMovieFormat )
		{
			case VT_LEFT_RIGHT_3D_FULL:
				CurrentMovieWidth = width / 2;
				CurrentMovieHeight = height;
				break;

			case VT_TOP_BOTTOM_3D_FULL:
				CurrentMovieWidth = width;
				CurrentMovieHeight = height / 2;
				break;

			default:
				CurrentMovieWidth = width;
				CurrentMovieHeight = height;
				break;
		}

		Cinema.MovieLoaded( CurrentMovieWidth, CurrentMovieHeight, MovieDuration );

		// Create the texture that we will mip map from the external image
		for ( int i = 0 ; i < 3 ; i++ )
		{
			if ( MipMappedMovieFBOs[i] )
			{
				glDeleteFramebuffers( 1, &MipMappedMovieFBOs[i] );
			}
			if ( MipMappedMovieTextures[i] )
			{
				glDeleteTextures( 1, &MipMappedMovieTextures[i] );
			}
			glGenTextures( 1, &MipMappedMovieTextures[i] );
			glBindTexture( GL_TEXTURE_2D, MipMappedMovieTextures[i] );

			glTexImage2D( GL_TEXTURE_2D, 0, Cinema.app->GetAppInterface()->GetWantSrgbFramebuffer() ? GL_SRGB8_ALPHA8 :GL_RGBA,
					MovieTextureWidth, MovieTextureHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL );

			glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
			glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
			glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR );
			glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );

			glGenFramebuffers( 1, &MipMappedMovieFBOs[i] );
			glBindFramebuffer( GL_FRAMEBUFFER, MipMappedMovieFBOs[i] );
			glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
					MipMappedMovieTextures[i], 0 );
			glBindFramebuffer( GL_FRAMEBUFFER, 0 );
		}
		glBindTexture( GL_TEXTURE_2D, 0 );

		return true;
	}

	return false;
}

/*
 * DrawEyeView
 */
Matrix4f SceneManager::DrawEyeView( const int eye, const float fovDegrees )
{
	// allow stereo movies to also be played in mono
	const int stereoEye = ForceMono ? 0 : eye;

	if ( SceneInfo.UseDynamicProgram )
	{
		// lights fading in and out, always on if no movie loaded
		const float cinemaLights = ( ( MovieTextureWidth > 0 ) && !SceneInfo.UseFreeScreen ) ?
				(float)StaticLighting.Value( ovr_GetTimeInSeconds() ) : 1.0f;

		if ( cinemaLights <= 0.0f )
		{
			if ( SceneProgramIndex != SCENE_PROGRAM_DYNAMIC_ONLY )
			{
				// switch to dynamic-only to save GPU
				SetSceneProgram( SCENE_PROGRAM_DYNAMIC_ONLY, SCENE_PROGRAM_ADDITIVE );
			}
		}
		else if ( cinemaLights >= 1.0f )
		{
			if ( SceneProgramIndex != SCENE_PROGRAM_STATIC_ONLY )
			{
				// switch to static-only to save GPU
				SetSceneProgram( SCENE_PROGRAM_STATIC_ONLY, SCENE_PROGRAM_ADDITIVE );
			}
		}
		else
		{
			if ( SceneProgramIndex != SCENE_PROGRAM_STATIC_DYNAMIC )
			{
				// switch to static+dynamic lighting
				SetSceneProgram( SCENE_PROGRAM_STATIC_DYNAMIC, SCENE_PROGRAM_ADDITIVE );
			}
		}

		glUseProgram( Cinema.ShaderMgr.ScenePrograms[SceneProgramIndex].program );
		glUniform4f( Cinema.ShaderMgr.ScenePrograms[SceneProgramIndex].uColor, 1.0f, 1.0f, 1.0f, cinemaLights );
		glUseProgram( Cinema.ShaderMgr.ScenePrograms[SCENE_PROGRAM_ADDITIVE].program );
		glUniform4f( Cinema.ShaderMgr.ScenePrograms[SCENE_PROGRAM_ADDITIVE].uColor, 1.0f, 1.0f, 1.0f, cinemaLights );

		// Bind the mip mapped movie texture to Texture2 so it can be sampled from the vertex program for scene lighting.
		glActiveTexture( GL_TEXTURE2 );
		glBindTexture( GL_TEXTURE_2D, MipMappedMovieTextures[CurrentMipMappedMovieTexture] );
	}

	const bool drawScreen = ( SceneScreenSurface || SceneInfo.UseFreeScreen || SceneInfo.LobbyScreen ) && MovieTexture && ( CurrentMovieWidth > 0 );

	// otherwise cracks would show overlay texture
	glClearColor( 0.0f, 0.0f, 0.0f, 1.0f );
	glClear( GL_COLOR_BUFFER_BIT );

	if ( !SceneInfo.UseFreeScreen )
	{
		Scene.DrawEyeView( eye, fovDegrees );
	}

	const Matrix4f mvp = Scene.MvpForEye( eye, fovDegrees );

	// draw the screen on top
	if ( !drawScreen )
	{
		return mvp;
	}

	if ( !SceneInfo.LobbyScreen )
	{
		glDisable( GL_DEPTH_TEST );
	}

	// If we are using the free screen, we still need to draw the surface with black
	if ( FreeScreenActive && !SceneInfo.UseFreeScreen && SceneScreenSurface )
	{
		const GlProgram * prog = &Cinema.ShaderMgr.ScenePrograms[0];
		glUseProgram( prog->program );
		glUniformMatrix4fv( prog->uMvp, 1, GL_FALSE, mvp.Transposed().M[0] );
		SceneScreenSurface->geo.Draw();
	}

	const GlProgram * prog = &Cinema.ShaderMgr.MovieExternalUiProgram;
	glUseProgram( prog->program );
	glUniform4f( prog->uColor, 1, 1, 1, 0.0f );

	glVertexAttrib4f( 2, 1.0f, 1.0f, 1.0f, 1.0f );	// no color attributes on the surface verts, so force to 1.0

	const Matrix4f stretchTop(
			1, 0, 0, 0,
			0, 0.5f, 0, 0,
			0, 0, 1, 0,
			0, 0, 0, 1 );
	const Matrix4f stretchBottom(
			1, 0, 0, 0,
			0, 0.5, 0, 0.5f,
			0, 0, 1, 0,
			0, 0, 0, 1 );
	const Matrix4f stretchRight(
			0.5f, 0, 0, 0.5f,
			0, 1, 0, 0,
			0, 0, 1, 0,
			0, 0, 0, 1 );
	const Matrix4f stretchLeft(
			0.5f, 0, 0, 0,
			0, 1, 0, 0,
			0, 0, 1, 0,
			0, 0, 0, 1 );

	const Matrix4f rotate90(
			0, 1, 0, 0,
			-1, 0, 0, 1,
			0, 0, 1, 0,
			0, 0, 0, 1 );

	const Matrix4f rotate180(
			-1, 0, 0, 1,
			0, -1, 0, 1,
			0, 0, 1, 0,
			0, 0, 0, 1 );

	const Matrix4f rotate270(
			0, -1, 0, 1,
			1, 0, 0, 0,
			0, 0, 1, 0,
			0, 0, 0, 1 );

	Matrix4f texMatrix;

	switch ( CurrentMovieFormat )
	{
		case VT_LEFT_RIGHT_3D:
		case VT_LEFT_RIGHT_3D_FULL:
			texMatrix = ( stereoEye ? stretchRight : stretchLeft );
			break;
		case VT_TOP_BOTTOM_3D:
		case VT_TOP_BOTTOM_3D_FULL:
			texMatrix = ( stereoEye ? stretchBottom : stretchTop );
			break;
		default:
			switch( MovieRotation )
			{
				case 0 :
					texMatrix = Matrix4f::Identity();
					break;
				case 90 :
					texMatrix = rotate90;
					break;
				case 180 :
					texMatrix = rotate180;
					break;
				case 270 :
					texMatrix = rotate270;
					break;
			}
			break;
	}

	//
	// draw the movie texture
	//
	if ( !GetUseOverlay() || SceneInfo.LobbyScreen || ( SceneInfo.UseScreenGeometry && ( SceneScreenSurface != NULL ) ) )
	{
		// no overlay
		Cinema.app->GetSwapParms().WarpProgram = WP_CHROMATIC;
		Cinema.app->GetSwapParms().Images[eye][1].TexId = 0;

		glActiveTexture( GL_TEXTURE0 );
		glBindTexture( GL_TEXTURE_EXTERNAL_OES, MovieTexture->textureId );

		glActiveTexture( GL_TEXTURE1 );
		glBindTexture( GL_TEXTURE_2D, ScreenVignetteTexture );

		glUniformMatrix4fv( prog->uTexm, 1, GL_FALSE, /* not transposed */
				texMatrix.Transposed().M[0] );
		// The UI is always identity for now, but we may scale it later
		glUniformMatrix4fv( prog->uTexm2, 1, GL_FALSE, /* not transposed */
				Matrix4f::Identity().Transposed().M[0] );

		if ( !SceneInfo.LobbyScreen && SceneInfo.UseScreenGeometry && ( SceneScreenSurface != NULL ) )
		{
			glUniformMatrix4fv( prog->uMvp, 1, GL_FALSE, mvp.Transposed().M[0] );
			SceneScreenSurface->geo.Draw();
		}
		else
		{
			const Matrix4f screenModel = ScreenMatrix();
			const Matrix4f screenMvp = mvp * screenModel;
			glUniformMatrix4fv( prog->uMvp, 1, GL_FALSE, screenMvp.Transposed().M[0] );
			UnitSquare.Draw();
		}

		glBindTexture( GL_TEXTURE_EXTERNAL_OES, 0 );	// don't leave it bound
	}
	else
	{
		// use overlay
		const Matrix4f screenModel = ScreenMatrix();
		const ovrMatrix4f mv = Scene.ViewMatrixForEye( eye ) * screenModel;

		Cinema.app->GetSwapParms().WarpProgram = WP_CHROMATIC_MASKED_PLANE;
		Cinema.app->GetSwapParms().Images[eye][1].TexId = MipMappedMovieTextures[CurrentMipMappedMovieTexture];
		Cinema.app->GetSwapParms().Images[eye][1].Pose = Cinema.app->GetSensorForNextWarp().Predicted;
		Cinema.app->GetSwapParms().Images[eye][1].TexCoordsFromTanAngles = texMatrix * TanAngleMatrixFromUnitSquare( &mv );

		// explicitly clear a hole in alpha
		const ovrMatrix4f screenMvp = mvp * screenModel;
		Cinema.app->DrawScreenMask( screenMvp, 0.0f, 0.0f );
	}

	// The framework will automatically draw the floating elements on top of us now.
	return mvp;
}

/*
 * Frame()
 *
 * App override
 */
Matrix4f SceneManager::Frame( const VrFrame & vrFrame )
{
	// disallow player movement
	VrFrame vrFrameWithoutMove = vrFrame;
	if ( !AllowMove )
	{
		vrFrameWithoutMove.Input.sticks[0][0] = 0.0f;
		vrFrameWithoutMove.Input.sticks[0][1] = 0.0f;
	}
	Scene.Frame( Cinema.app->GetVrViewParms(), vrFrameWithoutMove, Cinema.app->GetSwapParms().ExternalVelocity );

	if ( ClearGhostsFrames > 0 )
	{
		Cinema.app->GetGazeCursor().ClearGhosts();
		ClearGhostsFrames--;
	}

	// Check for new movie frames
	// latch the latest movie frame to the texture.
	if ( MovieTexture && CurrentMovieWidth )
	{
		glActiveTexture( GL_TEXTURE0 );
		MovieTexture->Update();
		glBindTexture( GL_TEXTURE_EXTERNAL_OES, 0 );
		if ( MovieTexture->nanoTimeStamp != MovieTextureTimestamp )
		{
			MovieTextureTimestamp = MovieTexture->nanoTimeStamp;
			FrameUpdateNeeded = true;
		}
	}

	// build the mip maps
	if ( FrameUpdateNeeded )
	{
		FrameUpdateNeeded = false;
		CurrentMipMappedMovieTexture = (CurrentMipMappedMovieTexture+1)%3;
		glActiveTexture( GL_TEXTURE1 );
		if ( CurrentMovieFormat == VT_LEFT_RIGHT_3D || CurrentMovieFormat == VT_LEFT_RIGHT_3D_FULL )
		{
			glBindTexture( GL_TEXTURE_2D, ScreenVignetteSbsTexture );
		}
		else
		{
			glBindTexture( GL_TEXTURE_2D, ScreenVignetteTexture );
		}
		glActiveTexture( GL_TEXTURE0 );
		glBindFramebuffer( GL_FRAMEBUFFER, MipMappedMovieFBOs[CurrentMipMappedMovieTexture] );
		glDisable( GL_DEPTH_TEST );
		glDisable( GL_SCISSOR_TEST );
		GL_InvalidateFramebuffer( INV_FBO, true, false );
		glViewport( 0, 0, MovieTextureWidth, MovieTextureHeight );
		if ( Cinema.app->GetAppInterface()->GetWantSrgbFramebuffer() )
		{	// we need this copied without sRGB conversion on the top level
	    	glDisable( GL_FRAMEBUFFER_SRGB_EXT );
		}
		if ( CurrentMovieWidth > 0 )
		{
			glBindTexture( GL_TEXTURE_EXTERNAL_OES, MovieTexture->textureId );
			glUseProgram( Cinema.ShaderMgr.CopyMovieProgram.program );
			UnitSquare.Draw();
			glBindTexture( GL_TEXTURE_EXTERNAL_OES, 0 );
			if ( Cinema.app->GetAppInterface()->GetWantSrgbFramebuffer() )
			{	// we need this copied without sRGB conversion on the top level
		    	glEnable( GL_FRAMEBUFFER_SRGB_EXT );
			}
		}
		else
		{
			// If the screen is going to be black because of a movie change, don't
			// leave the last dynamic color visible.
			glClearColor( 0.2f, 0.2f, 0.2f, 0.2f );
			glClear( GL_COLOR_BUFFER_BIT );
		}
		glBindFramebuffer( GL_FRAMEBUFFER, 0 );

		// texture 2 will hold the mip mapped screen
		glActiveTexture( GL_TEXTURE2 );
		glBindTexture( GL_TEXTURE_2D, MipMappedMovieTextures[CurrentMipMappedMovieTexture] );
		glGenerateMipmap( GL_TEXTURE_2D );
		glBindTexture( GL_TEXTURE_2D, 0 );

		GL_Flush();
	}

	// Generate callbacks into DrawEyeView
	Cinema.app->DrawEyeViewsPostDistorted( Scene.CenterViewMatrix() );

	return Scene.CenterViewMatrix();
}

} // namespace OculusCinema
