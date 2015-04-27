/************************************************************************************

Filename    :   TheaterSelectionView.cpp
Content     :
Created     :	6/19/2014
Authors     :   Jim Dos�

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the Cinema/ directory. An additional grant 
of patent rights can be found in the PATENTS file in the same directory.

*************************************************************************************/

#include "GazeCursor.h"
#include "BitmapFont.h"
#include "VRMenu/VRMenuMgr.h"
#include "CarouselBrowserComponent.h"
#include "TheaterSelectionComponent.h"
#include "TheaterSelectionView.h"
#include "CinemaApp.h"
#include "SwipeHintComponent.h"
#include "PackageFiles.h"
#include "CinemaStrings.h"
#include "Native.h"


namespace OculusCinema {

VRMenuId_t TheaterSelectionView::ID_CENTER_ROOT( 1000 );
VRMenuId_t TheaterSelectionView::ID_ICONS( 1001 );
VRMenuId_t TheaterSelectionView::ID_TITLE_ROOT( 2000 );
VRMenuId_t TheaterSelectionView::ID_SWIPE_ICON_LEFT( 3000 );
VRMenuId_t TheaterSelectionView::ID_SWIPE_ICON_RIGHT( 3001 );

static const int NumSwipeTrails = 3;

TheaterSelectionView::TheaterSelectionView( CinemaApp &cinema ) :
	View( "TheaterSelectionView" ),
	Cinema( cinema ),
	Menu( NULL ),
	CenterRoot( NULL ),
	SelectionObject( NULL ),
	TheaterBrowser( NULL ),
	SelectedTheater( 0 ),
	IgnoreSelectTime( 0 )

{
// This is called at library load time, so the system is not initialized
// properly yet.
}

TheaterSelectionView::~TheaterSelectionView()
{
}

void TheaterSelectionView::OneTimeInit( const char * launchIntent )
{
	LOG( "TheaterSelectionView::OneTimeInit" );

	const double start = ovr_GetTimeInSeconds();

	// Start with "Home theater" selected
	SelectedTheater = 0;

	LOG( "TheaterSelectionView::OneTimeInit: %3.1f seconds", ovr_GetTimeInSeconds() - start );
}

void TheaterSelectionView::OneTimeShutdown()
{
	LOG( "TheaterSelectionView::OneTimeShutdown" );
}

void TheaterSelectionView::SelectTheater(int theater)
{
	SelectedTheater = theater;

	Cinema.SceneMgr.SetSceneModel(Cinema.ModelMgr.GetTheater(SelectedTheater));
	SetPosition(Cinema.app->GetVRMenuMgr(), Cinema.SceneMgr.Scene.FootPos);
}

void TheaterSelectionView::OnOpen()
{
	LOG( "OnOpen" );

	if ( Menu == NULL )
	{
		CreateMenu( Cinema.app, Cinema.app->GetVRMenuMgr(), Cinema.app->GetDefaultFont() );
	}

	SelectTheater( SelectedTheater );
	TheaterBrowser->SetSelectionIndex( SelectedTheater );

	Cinema.SceneMgr.LightsOn( 0.5f );

	Cinema.app->GetSwapParms().WarpProgram = WP_CHROMATIC;

	Cinema.SceneMgr.ClearGazeCursorGhosts();
	Cinema.app->GetGuiSys().OpenMenu( Cinema.app, Cinema.app->GetGazeCursor(), "TheaterSelectionBrowser" );

	// ignore clicks for 0.5 seconds to avoid accidentally clicking through
	IgnoreSelectTime = ovr_GetTimeInSeconds() + 0.5;

	CurViewState = VIEWSTATE_OPEN;
}

void TheaterSelectionView::OnClose()
{
	LOG( "OnClose" );

	Cinema.app->GetGuiSys().CloseMenu( Cinema.app, Menu, false );

	CurViewState = VIEWSTATE_CLOSED;
}

bool TheaterSelectionView::Command( const char * msg )
{
	return false;
}

bool TheaterSelectionView::OnKeyEvent( const int keyCode, const KeyState::eKeyEventType eventType )
{
	return false;
}

Matrix4f TheaterSelectionView::DrawEyeView( const int eye, const float fovDegrees )
{
	return Cinema.SceneMgr.DrawEyeView( eye, fovDegrees );
}

void TheaterSelectionView::SetPosition( OvrVRMenuMgr & menuMgr, const Vector3f &pos )
{
    Posef pose = CenterRoot->GetLocalPose();
    pose.Position = pos;
    CenterRoot->SetLocalPose( pose );

    menuHandle_t titleRootHandle = Menu->HandleForId( menuMgr, ID_TITLE_ROOT );
    VRMenuObject * titleRoot = menuMgr.ToObject( titleRootHandle );
    OVR_ASSERT( titleRoot != NULL );

    pose = titleRoot->GetLocalPose();
    pose.Position = pos;
    titleRoot->SetLocalPose( pose );
}

void TheaterSelectionView::CreateMenu( App * app, OvrVRMenuMgr & menuMgr, BitmapFont const & font )
{
	Menu = VRMenu::Create( "TheaterSelectionBrowser" );

    Vector3f fwd( 0.0f, 0.0f, 1.0f );
	Vector3f up( 0.0f, 1.0f, 0.0f );
	Vector3f defaultScale( 1.0f );

    Array< VRMenuObjectParms const * > parms;

	VRMenuFontParms fontParms( true, true, false, false, false, 1.0f );

	Quatf orientation( Vector3f( 0.0f, 1.0f, 0.0f ), 0.0f );
	Vector3f centerPos( 0.0f, 0.0f, 0.0f );

	VRMenuObjectParms centerRootParms( VRMENU_CONTAINER, Array< VRMenuComponent* >(), VRMenuSurfaceParms(), "CenterRoot",
			Posef( orientation, centerPos ), Vector3f( 1.0f, 1.0f, 1.0f ), fontParms,
			ID_CENTER_ROOT, VRMenuObjectFlags_t(), VRMenuObjectInitFlags_t( VRMENUOBJECT_INIT_FORCE_POSITION ) );
    parms.PushBack( &centerRootParms );

	// title
	const Posef titlePose( Quatf( Vector3f( 0.0f, 1.0f, 0.0f ), 0.0f ), Vector3f( 0.0f, 0.0f, 0.0f ) );

	VRMenuObjectParms titleRootParms( VRMENU_CONTAINER, Array< VRMenuComponent* >(), VRMenuSurfaceParms(),
			"TitleRoot", titlePose, defaultScale, fontParms, ID_TITLE_ROOT,
			VRMenuObjectFlags_t(), VRMenuObjectInitFlags_t( VRMENUOBJECT_INIT_FORCE_POSITION ) );

	parms.PushBack( &titleRootParms );

	Menu->InitWithItems( menuMgr, font, 0.0f, VRMenuFlags_t(), parms );
    parms.Clear();

	int count = Cinema.ModelMgr.GetTheaterCount();
	for ( int i = 0 ; i < count ; i++ )
	{
		const SceneDef & theater = Cinema.ModelMgr.GetTheater( i );

		CarouselItem *item = new CarouselItem();
		item->texture = theater.IconTexture;

		Theaters.PushBack( item );
	}

	Array<PanelPose> panelPoses;

	panelPoses.PushBack( PanelPose( Quatf( up, 0.0f ), Vector3f( -2.85f, 1.8f, -5.8f ), Vector4f( 0.0f, 0.0f, 0.0f, 0.0f ) ) );
	panelPoses.PushBack( PanelPose( Quatf( up, 0.0f ), Vector3f( -1.90f, 1.8f, -5.0f ), Vector4f( 0.25f, 0.25f, 0.25f, 1.0f ) ) );
	panelPoses.PushBack( PanelPose( Quatf( up, 0.0f ), Vector3f( -0.95f, 1.8f, -4.2f ), Vector4f( 0.45f, 0.45f, 0.45f, 1.0f ) ) );
	panelPoses.PushBack( PanelPose( Quatf( up, 0.0f ), Vector3f(  0.00f, 1.8f, -3.4f ), Vector4f( 1.0f, 1.0f, 1.0f, 1.0f ) ) );
	panelPoses.PushBack( PanelPose( Quatf( up, 0.0f ), Vector3f(  0.95f, 1.8f, -4.2f ), Vector4f( 0.45f, 0.45f, 0.45f, 1.0f ) ) );
	panelPoses.PushBack( PanelPose( Quatf( up, 0.0f ), Vector3f(  1.90f, 1.8f, -5.0f ), Vector4f( 0.25f, 0.25f, 0.25f, 1.0f ) ) );
	panelPoses.PushBack( PanelPose( Quatf( up, 0.0f ), Vector3f(  2.85f, 1.8f, -5.8f ), Vector4f( 0.0f, 0.0f, 0.0f, 0.0f ) ) );

    // the centerroot item will get touch relative and touch absolute events and use them to rotate the centerRoot
    menuHandle_t centerRootHandle = Menu->HandleForId( menuMgr, ID_CENTER_ROOT );
    CenterRoot = menuMgr.ToObject( centerRootHandle );
    OVR_ASSERT( CenterRoot != NULL );

    TheaterBrowser = new CarouselBrowserComponent( Theaters, panelPoses );
    CenterRoot->AddComponent( TheaterBrowser );

    int selectionWidth = 0;
    int selectionHeight = 0;
	GLuint selectionTexture = LoadTextureFromApplicationPackage( "assets/VoidTheater.png",
			TextureFlags_t( TEXTUREFLAG_NO_DEFAULT ), selectionWidth, selectionHeight );

	int centerIndex = panelPoses.GetSizeI() / 2;
	for ( int i = 0; i < panelPoses.GetSizeI(); ++i )
	{
		VRMenuSurfaceParms panelSurfParms( "",
				selectionTexture, selectionWidth, selectionHeight, SURFACE_TEXTURE_DIFFUSE,
				0, 0, 0, SURFACE_TEXTURE_MAX,
				0, 0, 0, SURFACE_TEXTURE_MAX );

	    VRMenuId_t panelId = VRMenuId_t( ID_ICONS.Get() + i );
		Quatf rot( up, 0.0f );
		Posef panelPose( rot, fwd );
		VRMenuObjectParms * p = new VRMenuObjectParms( VRMENU_BUTTON, Array< VRMenuComponent* >(),
				panelSurfParms, NULL, panelPose, defaultScale, fontParms, panelId,
				( i == centerIndex ) ? VRMenuObjectFlags_t( VRMENUOBJECT_FLAG_NO_DEPTH ) : VRMenuObjectFlags_t( VRMENUOBJECT_FLAG_NO_FOCUS_GAINED ) | VRMENUOBJECT_FLAG_NO_DEPTH,
				VRMenuObjectInitFlags_t( VRMENUOBJECT_INIT_FORCE_POSITION ) );
		parms.PushBack( p );
	}

    Menu->AddItems( menuMgr, font, parms, centerRootHandle, false );
    DeletePointerArray( parms );
    parms.Clear();

	menuHandle_t selectionHandle = CenterRoot->ChildHandleForId( menuMgr, VRMenuId_t( ID_ICONS.Get() + centerIndex ) );
	SelectionObject = menuMgr.ToObject( selectionHandle );

    Vector3f selectionBoundsExpandMin = Vector3f( 0.0f, -0.25f, 0.0f );
    Vector3f selectionBoundsExpandMax = Vector3f( 0.0f, 0.25f, 0.0f );
	SelectionObject->SetLocalBoundsExpand( selectionBoundsExpandMin, selectionBoundsExpandMax );
	SelectionObject->AddFlags( VRMENUOBJECT_HIT_ONLY_BOUNDS );

	Array<VRMenuObject *> menuObjs;
	Array<CarouselItemComponent *> menuComps;
	for ( int i = 0; i < panelPoses.GetSizeI(); ++i )
	{
		menuHandle_t posterImageHandle = CenterRoot->ChildHandleForId( menuMgr, VRMenuId_t( ID_ICONS.Get() + i ) );
		VRMenuObject *posterImage = menuMgr.ToObject( posterImageHandle );
		menuObjs.PushBack( posterImage );

		CarouselItemComponent *panelComp = new TheaterSelectionComponent( i == centerIndex ? this : NULL );
		posterImage->AddComponent( panelComp );
		menuComps.PushBack( panelComp );
	}
	TheaterBrowser->SetMenuObjects( menuObjs, menuComps );

    // ==============================================================================
    //
    // title
    //
    {
        Posef panelPose( Quatf( up, 0.0f ), Vector3f( 0.0f, 2.5f, -3.4f ) );

		VRMenuFontParms titleFontParms( true, true, false, false, false, 1.3f );

		VRMenuObjectParms p( VRMENU_STATIC, Array< VRMenuComponent* >(),
				VRMenuSurfaceParms(), CinemaStrings::TheaterSelection_Title.ToCStr(), panelPose, defaultScale, titleFontParms, VRMenuId_t( ID_TITLE_ROOT.Get() + 1 ),
				VRMenuObjectFlags_t(), VRMenuObjectInitFlags_t( VRMENUOBJECT_INIT_FORCE_POSITION ) );

		parms.PushBack( &p );

        menuHandle_t titleRootHandle = Menu->HandleForId( menuMgr, ID_TITLE_ROOT );
		Menu->AddItems( menuMgr, font, parms, titleRootHandle, false );
		parms.Clear();
    }

	// ==============================================================================
    //
    // swipe icons
    //
    {
    	int 	swipeIconLeftWidth = 0;
    	int		swipeIconLeftHeight = 0;
    	int 	swipeIconRightWidth = 0;
    	int		swipeIconRightHeight = 0;

    	GLuint swipeIconLeftTexture = LoadTextureFromApplicationPackage( "assets/SwipeSuggestionArrowLeft.png",
				TextureFlags_t( TEXTUREFLAG_NO_DEFAULT ), swipeIconLeftWidth, swipeIconLeftHeight );

		GLuint swipeIconRightTexture = LoadTextureFromApplicationPackage( "assets/SwipeSuggestionArrowRight.png",
				TextureFlags_t( TEXTUREFLAG_NO_DEFAULT ), swipeIconRightWidth, swipeIconRightHeight );

		VRMenuSurfaceParms swipeIconLeftSurfParms( "",
				swipeIconLeftTexture, swipeIconLeftWidth, swipeIconLeftHeight, SURFACE_TEXTURE_DIFFUSE,
				0, 0, 0, SURFACE_TEXTURE_MAX,
				0, 0, 0, SURFACE_TEXTURE_MAX );

		VRMenuSurfaceParms swipeIconRightSurfParms( "",
				swipeIconRightTexture, swipeIconRightWidth, swipeIconRightHeight, SURFACE_TEXTURE_DIFFUSE,
				0, 0, 0, SURFACE_TEXTURE_MAX,
				0, 0, 0, SURFACE_TEXTURE_MAX );

		float yPos = 1.8f - ( selectionHeight - swipeIconLeftHeight ) * 0.5f * VRMenuObject::DEFAULT_TEXEL_SCALE;
		for( int i = 0; i < NumSwipeTrails; i++ )
		{
			Posef swipePose = Posef( Quatf( up, 0.0f ), Vector3f( 0.0f, yPos, -3.4f ) );
			swipePose.Position.x = ( ( selectionWidth + swipeIconLeftWidth * ( i + 2 ) ) * -0.5f ) * VRMenuObject::DEFAULT_TEXEL_SCALE;
			swipePose.Position.z += 0.01f * ( float )i;

			Array< VRMenuComponent* > leftComps;
			leftComps.PushBack( new SwipeHintComponent( TheaterBrowser, false, 1.3333f, 0.4f + ( float )i * 0.13333f, 5.0f ) );

			VRMenuObjectParms * swipeIconLeftParms = new VRMenuObjectParms( VRMENU_BUTTON, leftComps,
				swipeIconLeftSurfParms, "", swipePose, defaultScale, fontParms, VRMenuId_t( ID_SWIPE_ICON_LEFT.Get() + i ),
				VRMenuObjectFlags_t( VRMENUOBJECT_FLAG_NO_DEPTH ) | VRMenuObjectFlags_t( VRMENUOBJECT_DONT_HIT_ALL ),
				VRMenuObjectInitFlags_t( VRMENUOBJECT_INIT_FORCE_POSITION ) );

			parms.PushBack( swipeIconLeftParms );

			swipePose.Position.x = ( ( selectionWidth + swipeIconRightWidth * ( i + 2 ) ) * 0.5f ) * VRMenuObject::DEFAULT_TEXEL_SCALE;

			Array< VRMenuComponent* > rightComps;
			rightComps.PushBack( new SwipeHintComponent( TheaterBrowser, true, 1.3333f, 0.4f + ( float )i * 0.13333f, 5.0f ) );

			VRMenuObjectParms * swipeIconRightParms = new VRMenuObjectParms( VRMENU_STATIC, rightComps,
				swipeIconRightSurfParms, "", swipePose, defaultScale, fontParms, VRMenuId_t( ID_SWIPE_ICON_RIGHT.Get() + i ),
				VRMenuObjectFlags_t( VRMENUOBJECT_FLAG_NO_DEPTH ) | VRMenuObjectFlags_t( VRMENUOBJECT_DONT_HIT_ALL ),
				VRMenuObjectInitFlags_t( VRMENUOBJECT_INIT_FORCE_POSITION ) );

			parms.PushBack( swipeIconRightParms );
		}

		Menu->AddItems( menuMgr, font, parms, centerRootHandle, false );
		DeletePointerArray( parms );
		parms.Clear();
    }

    Cinema.app->GetGuiSys().AddMenu( Menu );
}

void TheaterSelectionView::SelectPressed( void )
{
	const double now = ovr_GetTimeInSeconds();
	if ( now < IgnoreSelectTime )
	{
		// ignore selection for first 0.5 seconds to reduce chances of accidentally clicking through
		return;
	}

	Cinema.SceneMgr.SetSceneModel( Cinema.ModelMgr.GetTheater( SelectedTheater ) );
	if ( Cinema.InLobby )
	{
		Cinema.ResumeOrRestartMovie();
	}
	else
	{
		Cinema.ResumeMovieFromSavedLocation();
	}
}

Matrix4f TheaterSelectionView::Frame( const VrFrame & vrFrame )
{
	// We want 4x MSAA in the selection screen
	EyeParms eyeParms = Cinema.app->GetEyeParms();
	eyeParms.multisamples = 4;
	Cinema.app->SetEyeParms( eyeParms );

	if ( SelectionObject->IsHilighted() )
	{
		TheaterBrowser->CheckGamepad( Cinema.app, vrFrame, Cinema.app->GetVRMenuMgr(), CenterRoot );
	}

	int selectedItem = TheaterBrowser->GetSelection();
	if ( SelectedTheater != selectedItem )
	{
		if ( ( selectedItem >= 0 ) && ( selectedItem < Theaters.GetSizeI() ) )
		{
			LOG( "Select: %d, %d, %d, %d", selectedItem, SelectedTheater, Theaters.GetSizeI(), Cinema.ModelMgr.GetTheaterCount() );
			SelectedTheater = selectedItem;
			SelectTheater( SelectedTheater );
		}
	}

	if ( Menu->IsClosedOrClosing() && !Menu->IsOpenOrOpening() )
	{
		Cinema.MovieSelection( true );
	}

	if ( vrFrame.Input.buttonPressed & BUTTON_B )
	{
		Cinema.app->PlaySound( "touch_up" );
		Cinema.MovieSelection( true );
	}

	return Cinema.SceneMgr.Frame( vrFrame );
}

} // namespace OculusCinema
