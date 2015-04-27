/************************************************************************************

Filename    :   Image_Utils.h
Content     :   Image utility functions.
Created     :   May, 2014
Authors     :   Jonathan Wright

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/
#ifndef __IMAGE_UTILS_H__
#define __IMAGE_UTILS_H__

enum ImageOcclusion
{
	IMAGE_OPAQUE,
	IMAGE_PERFORATED,
	IMAGE_TRANSPARENT
};

struct ImageProperties
{
	int width;
	int height;
	ImageOcclusion occlusion;
};

ImageProperties GetImageProperties( char const * filePath );

#endif // !__IMAGE_UTILS_H__
