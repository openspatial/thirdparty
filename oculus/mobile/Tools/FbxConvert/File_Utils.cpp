/************************************************************************************

Filename    :   File_Utils.cpp
Content     :   File utility functions.
Created     :   May, 2014
Authors     :   J.M.P. van Waveren

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/
#include <stdint.h>
#include <stdio.h>
#if defined( __APPLE__ ) || defined( __linux__ )
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#define _getcwd getcwd
#define _mkdir(a) mkdir(a, 777)
#else
#include <direct.h>
#include <process.h>
#endif
#include <sys/stat.h>

#define FBX_TOOL
#include "../../VRLib/jni/LibOVR/Include/OVR.h"
#include "../../VRLib/jni/LibOVR/Src/Kernel/OVR_String.h"
#include "../../VRLib/jni/LibOVR/Src/Kernel/OVR_String_Utils.h"

namespace FileUtils
{

OVR::String GetCurrentFolder()
{
	char cwd[OVR::StringUtils::MAX_PATH_LENGTH];
	if ( !_getcwd( cwd, sizeof( cwd ) ) )
	{
		return OVR::String();
	}
	cwd[sizeof( cwd ) - 1] = '\0';
	OVR::StringUtils::GetCleanPath( cwd, cwd, '\\' );
	const size_t length = strlen( cwd );
	if ( cwd[length - 1] != '\\' && length < OVR::StringUtils::MAX_PATH_LENGTH - 1 )
	{
		cwd[length + 0] = '\\';
		cwd[length + 1] = '\0';
	}
	return OVR::String( cwd );
}

bool FileExists( const char * filePath )
{
	FILE * fp = fopen( filePath, "r" );
	if ( fp != NULL )
	{
		fclose( fp );
		return true;
	}
	return false;
}

bool FolderExists( const char * folderPath )
{
#if defined( __APPLE__ ) || defined( __linux__ )
	DIR * dir = opendir( folderPath );
	if ( dir )
	{
		closedir( dir );
		return true;
	}
	return false;
#else
	const DWORD ftyp = GetFileAttributesA( folderPath );
	if ( ftyp == INVALID_FILE_ATTRIBUTES )
	{
		return false;  // bad path
	}
	return ( ftyp & FILE_ATTRIBUTE_DIRECTORY ) != 0;
#endif
}

uint64_t FileSize( const char * filePath )
{
	FILE * fp = fopen( filePath, "r" );
	if ( fp != NULL )
	{
#if defined( __APPLE__ ) || defined( __linux__ )
		fseek( fp, 0, SEEK_END );
		size_t r = ftell( fp );
		fclose( fp );
		return r;
#else
		struct _stat64 stats;
		__int64 r = _fstati64( fp->_file, &stats );
		fclose( fp );
		if ( r >= 0 )
		{
			return stats.st_size;
		}
#endif
	}
	return 0;
}

uint64_t FileTime( const char * filePath )
{
#if defined( __APPLE__ ) || defined( __linux__ )
	// TODO: I think this may be cross platform
	struct stat attrib;
	if ( stat( filePath, &attrib ) )
	{
		return 0;
	}
	return attrib.st_mtime;
#else
	wchar_t szName[OVR::StringUtils::MAX_PATH_LENGTH];
	mbstowcs( szName, filePath, OVR::StringUtils::MAX_PATH_LENGTH );

	HANDLE hFile = hFile = CreateFile( szName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL );
	if ( hFile == INVALID_HANDLE_VALUE )
    {
		return 0;
	}

	FILETIME ftCreate, ftAccess, ftWrite;
    if ( !GetFileTime( hFile, &ftCreate, &ftAccess, &ftWrite ) )
	{
		CloseHandle( hFile );
        return 0;
	}

	CloseHandle( hFile );

	return ( ( (uint64_t)ftWrite.dwHighDateTime << 32 ) | (uint64_t)ftWrite.dwLowDateTime );
#endif
}

bool MatchExtension( const char * fileExtension, const char * matchExtensions )
{
	if ( matchExtensions[0] == '\0' )
	{
		return true;
	}
	if ( fileExtension[0] == '.' )
	{
		fileExtension++;
	}
	for ( const char * end = matchExtensions; end[0] != '\0'; )
	{
		for ( ; end[0] == ';'; end++ ) {}
		const char * ext = end;
		for ( ; end[0] != ';' && end[0] != '\0'; end++ ) {}
#if defined( __APPLE__ ) || defined( __linux__ )
		if ( strncasecmp( fileExtension, ext, end - ext ) == 0 )
#else
		if ( _strnicmp( fileExtension, ext, end - ext ) == 0 )
#endif
		{
			return true;
		}
	}
	return false;
}

void ListFolderFiles( OVR::Array< OVR::String > & fileList, const char * folder, const char * matchExtensions )
{
#if defined( __APPLE__ ) || defined( __linux__ )
	DIR * dir = opendir( folder );
	if ( dir == NULL )
	{
		return;
	}
	for ( ; ; )
	{
		struct dirent * dp = readdir( dir );
		if ( dp == NULL )
		{
			break;
		}

		if ( dp->d_type == DT_DIR )
		{
			continue;
		}

		const char * fileName = dp->d_name;
		const char * fileExt = strrchr( fileName, '.' );

		if ( !MatchExtension( fileExt, matchExtensions ) )
		{
			continue;
		}

		fileList.PushBack( OVR::String( fileName ) );
	}

	closedir(dir);
#else
	OVR::String pathStr = folder;
	pathStr += "*";

	WCHAR wcwd[OVR::StringUtils::MAX_PATH_LENGTH];
	mbstowcs( wcwd, pathStr, OVR::StringUtils::MAX_PATH_LENGTH );

	WIN32_FIND_DATA FindFileData;
	HANDLE hFind = FindFirstFile( wcwd, &FindFileData );
	if ( hFind != INVALID_HANDLE_VALUE )
	{
		do
		{
			if ( ( FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) == 0 )
			{
				char fileName[OVR::StringUtils::MAX_PATH_LENGTH];
				size_t length = wcstombs( fileName, FindFileData.cFileName, sizeof( fileName ) );
				fileName[sizeof( fileName ) - 1] = '\0';

				const char * fileExt = strrchr( fileName, '.' );

				if ( !MatchExtension( fileExt, matchExtensions ) )
				{
					continue;
				}

				fileList.PushBack( OVR::String( fileName ) );
			}
		} while ( FindNextFile( hFind, &FindFileData ) );

		FindClose( hFind );
	}
#endif
}

bool CreatePath( const char * path )
{
	char folder[OVR::StringUtils::MAX_PATH_LENGTH];
	OVR::StringUtils::GetFolder( folder, path );

	char clean[OVR::StringUtils::MAX_PATH_LENGTH];
	OVR::StringUtils::GetCleanPath( clean, folder, '\\' );

	char build[OVR::StringUtils::MAX_PATH_LENGTH];
	for ( int i = 0; clean[i] != '\0'; i++ )
	{
		if ( clean[i] == '\\' && i > 0 )
		{
			build[i] = '\0';
			if ( i > 1 || build[1] != ':' )
			{
				if ( _mkdir( build ) != 0 && errno != EEXIST )
				{
					return false;
				}
			}
		}
		build[i] = clean[i];
	}
	return true;
}

bool Execute( const char * path )
{
#if defined( __APPLE__ ) || defined( __linux__ )
	return system( path ) != -1;
#else
	return ( _spawnl( _P_WAIT, path, path, NULL ) != -1 );
#endif
}

}
