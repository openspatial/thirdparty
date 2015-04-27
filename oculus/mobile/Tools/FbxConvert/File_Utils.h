/************************************************************************************

Filename    :   File_Utils.h
Content     :   File utility functions.
Created     :   May, 2014
Authors     :   J.M.P. van Waveren

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/
#ifndef __FILE_UTILS_H__
#define __FILE_UTILS_H__

namespace FileUtils
{
	OVR::String	GetCurrentFolder();

	bool		FileExists( const char * filePath );
	bool		FolderExists( const char * folderPath );

	uint64_t	FileSize( const char * filePath );
	uint64_t	FileTime( const char * filePath );

	bool		MatchExtension( const char * fileExtension, const char * matchExtensions );
	void		ListFolderFiles( OVR::Array< OVR::String > & fileList, const char * folder, const char * matchExtensions );

	bool		CreatePath( const char * path );
	bool		Execute( const char * path );
}

#endif // !__FILE_UTILS_H__
