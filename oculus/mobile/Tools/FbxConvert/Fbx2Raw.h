/************************************************************************************

Filename    :   Fbx2Raw.h
Content     :   Load and convert an FBX to a raw model.
Created     :   May, 2014
Authors     :   J.M.P. van Waveren

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/
#ifndef __FBX2RAW_H__
#define __FBX2RAW_H__

bool LoadFBXFile( const char * fbxFileName, const char * textureExtensions, RawModel & raw );

#endif // !__FBX2RAW_H__
