/************************************************************************************

PublicHeader:   None
Filename    :   path_utils.h
Content     :   Utility functions for manipulating file paths
Created     :   May 8, 2014
Author      :   Jonathan E. Wright
Notes       :

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

************************************************************************************/

#if !defined( OVR_Path_Utils_h )
#define OVR_Path_Utils_h

namespace OVR {

class PathUtils
{
public:
	static void GetFilenameWithExtension( char const * path, std::string & filename );
	static void GetFilenameAndExtension( char const * path, std::string & filename, std::string & extension );
	static void GetFilename( char const * path, std::string & filename );
	static void StripExtension( char const * path, std::string & pathWithoutExt );
	static void SplitPath( char const * path, std::string & outPath, std::string & filename, std::string & extension );
	static bool IsPathSeparator( char const ch );
	static void AppendPath( std::string & path, std::string const & append );
	static void SetExtension( std::string & path, char const * ext );
};

} // namespace OVR

#endif	// OVR_Path_Utils_h