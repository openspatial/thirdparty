/************************************************************************************

PublicHeader:   None
Filename    :   path_utils.cpp
Content     :   Utility functions for manipulating file paths
Created     :   May 8, 2014
Author      :   Jonathan E. Wright
Notes       :

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

************************************************************************************/

#include <stdlib.h>
#include <iostream>
#include <Include\OVR.h>
#include "path_utils.h"

namespace OVR {

void PathUtils::GetFilenameWithExtension( char const * path, std::string & filename )
{
	char drive[MAX_PATH];
	char dir[MAX_PATH];
	char fname[MAX_PATH];
	char fext[MAX_PATH];
#if defined( WIN32 )
	_splitpath_s( path, drive, MAX_PATH, dir, MAX_PATH, fname, MAX_PATH, fext, MAX_PATH );
#else
	_splitpath( path, drive, dir, fname, fext );
#endif
	filename = fname;
	filename += fext;
}

void PathUtils::GetFilenameAndExtension( char const * path, std::string & filename, std::string & extension )
{
	char drive[MAX_PATH];
	char dir[MAX_PATH];
	char fname[MAX_PATH];
	char fext[MAX_PATH];
#if defined( WIN32 )
	_splitpath_s( path, drive, MAX_PATH, dir, MAX_PATH, fname, MAX_PATH, fext, MAX_PATH );
#else
	_splitpath( path, drive, dir, fname, fext );
#endif
	filename = fname;
	extension += fext;
}

void PathUtils::GetFilename( char const * path, std::string & filename )
{
	char drive[MAX_PATH];
	char dir[MAX_PATH];
	char fname[MAX_PATH];
	char fext[MAX_PATH];
#if defined( WIN32 )
	_splitpath_s( path, drive, MAX_PATH, dir, MAX_PATH, fname, MAX_PATH, fext, MAX_PATH );
#else
	_splitpath( path, drive, dir, fname, fext );
#endif
	filename = fname;
}

void PathUtils::StripExtension( char const * path, std::string & pathWithoutExt )
{
	char drive[MAX_PATH];
	char dir[MAX_PATH];
	char fname[MAX_PATH];
	char fext[MAX_PATH];
#if defined( WIN32 )
	_splitpath_s( path, drive, MAX_PATH, dir, MAX_PATH, fname, MAX_PATH, fext, MAX_PATH );
#else
	_splitpath( path, drive, dir, fname, fext );
#endif
	pathWithoutExt = drive;
	pathWithoutExt += dir;
	pathWithoutExt += fname;
}

void PathUtils::SplitPath( char const * path, std::string & outPath, std::string & filename, std::string & extension )
{
	char drive[MAX_PATH];
	char dir[MAX_PATH];
	char fname[MAX_PATH];
	char fext[MAX_PATH];
#if defined( WIN32 )
	_splitpath_s( path, drive, MAX_PATH, dir, MAX_PATH, fname, MAX_PATH, fext, MAX_PATH );
#else
	_splitpath( path, drive, dir, fname, fext );
#endif
	outPath = drive;
	outPath += dir;
	filename = fname;
	extension = fext;
}

#if defined( WIN32 )
static char const PATH_SEPARATORS[] = { '\\', '/', '\0' };
#else
static char const PATH_SEPARATORS[] = { '/', '\0' };
#endif

bool PathUtils::IsPathSeparator( char const ch )
{

	for ( int i = 0; PATH_SEPARATORS[i] != '\0'; ++i )
	{
		if ( ch == PATH_SEPARATORS[i] )
		{
			return true;
		}
	}
	return false;
}


void PathUtils::AppendPath( std::string & path, std::string const & append )
{
	size_t len = path.length();
	if ( len == 0 )
	{
		path = append;
		return;
	}

	if ( !IsPathSeparator( path[len-1] ) )
	{
		path += PATH_SEPARATORS[0];
	}
	path += append;
}

void PathUtils::SetExtension( std::string & path, char const * ext )
{
	size_t len = path.length();
	if ( len == 0 )
	{
		path = ext;
		return;
	}
	char const * pExt = ext;
	if ( ext[0] == '.' )
	{
		pExt++;	// skip the period
	}

	size_t dotIdx = len + 1;
	for ( size_t i = 0; i < len; ++i )
	{
		size_t idx = len - i - 1;
		if ( IsPathSeparator( path[idx] ) )
		{
			break;	// a file extension cannot come before a path separator
		}
		if ( path[idx] == '.' )
		{
			// make sure this is not a .. in the path
			if ( ( idx == 0 || path[idx - 1] != '.' ) && ( idx >= len - 1 || path[idx + 1] != '.' ) )
			{
				dotIdx = idx;
				break;
			}
		}
	}
	if ( dotIdx < len + 1 )
	{
		path.resize( dotIdx + 1 );
	} 
	else 
	{
		path += '.';
	}
	path += pExt;
}

} // namespace OVR