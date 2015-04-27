/************************************************************************************

Filename    :   PanoMenu.h
Content     :   VRMenu shown when within panorama
Created     :   September 15, 2014
Authors     :   Warsam Osman

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the Oculus360Photos/ directory. An additional grant 
of patent rights can be found in the PATENTS file in the same directory.

************************************************************************************/

#if !defined( OVR_PanoMenu_h )
#define OVR_PanoMenu_h

#include "VRMenu/VRMenu.h"
#include "VRMenu/Fader.h"

namespace OVR {

class Oculus360Photos;
class SearchPaths;
class OvrPanoMenuRootComponent;
class OvrAttributionInfo;
class OvrMetaData;
struct OvrMetaDatum;

//==============================================================
// OvrPanoMenu
class OvrPanoMenu : public VRMenu
{
public:
	static char const *	MENU_NAME;
	static const VRMenuId_t ID_CENTER_ROOT;
	static const VRMenuId_t	ID_BROWSER_BUTTON;
	static const VRMenuId_t	ID_FAVORITES_BUTTON;

	// only one of these every needs to be created
	static  OvrPanoMenu *		Create(
		App * app,
		Oculus360Photos * photos,
		OvrVRMenuMgr & menuMgr,
		BitmapFont const & font,
		OvrMetaData & metaData,
		float fadeOutTime,
		float radius );
	
	void					UpdateButtonsState( const OvrMetaDatum * const ActivePano, bool showSwipeOverride = false );
	void					StartFadeOut();

	Oculus360Photos *		GetPhotos() 					{ return Photos; }
	OvrMetaData & 			GetMetaData() 					{ return MetaData; }
	menuHandle_t			GetLoadingIconHandle() const	{ return LoadingIconHandle; }

	float					GetFadeAlpha() const;
	bool					Interacting() const				{ return GetFocusedHandle().IsValid(); }

private:
	OvrPanoMenu( App * app, Oculus360Photos * photos, OvrVRMenuMgr & menuMgr, BitmapFont const & font, 
		OvrMetaData & metaData, float fadeOutTime, float radius );

	virtual ~OvrPanoMenu();

	virtual void			OnItemEvent_Impl( App * app, VRMenuId_t const itemId, VRMenuEvent const & event );
	virtual void			Frame_Impl( App * app, VrFrame const & vrFrame, OvrVRMenuMgr & menuMgr, BitmapFont const & font,
									BitmapFontSurface & fontSurface, gazeCursorUserId_t const gazeUserId );

	// Globals
	App *					AppPtr;
	OvrVRMenuMgr &			MenuMgr;
	const BitmapFont &		Font;
	Oculus360Photos *		Photos;
	OvrMetaData & 			MetaData;

	menuHandle_t			LoadingIconHandle;
	menuHandle_t			AttributionHandle;
	menuHandle_t			BrowserButtonHandle;
	menuHandle_t			FavoritesButtonHandle;
	menuHandle_t			SwipeLeftIndicatorHandle;
	menuHandle_t			SwipeRightIndicatorHandle;

	SineFader				Fader;
	const float				FadeOutTime;
	float					currentFadeRate;

	const float				Radius;

	float					ButtonCoolDown;
};

}

#endif
