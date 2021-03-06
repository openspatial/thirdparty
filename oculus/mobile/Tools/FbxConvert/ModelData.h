/************************************************************************************

Filename    :   ModelData.h
Content     :   JSON plus binary model data.
Created     :   May, 2014
Authors     :   J.M.P. van Waveren

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/
#ifndef __MODELDATA_H__
#define __MODELDATA_H__

/*

JSON plus binary data generated by one of the model converters.

*/

struct ModelData
{
	JSON *				json;
	Array< uint8_t >	binary;
};


/*

The file base extension for the packed cube map and the
source texture file base extensions for the cube map sides.

*/

static const char * cubeMapFileBaseExtension = "_cubemap";

static const char * cubeMapSidesFileBaseExtensions[6] =
{
	"_right",		// +X
	"_left",		// -X
	"_up",			// +Y
	"_down",		// -Y
	"_backward",	// +Z
	"_forward"		// -Z
};

inline String StripCubeMapSidesFileBaseExtensions( const char * fileBase )
{
	String base( fileBase );
	for ( int i = 0; i < 6; i++ )
	{
		base.StripTrailing( cubeMapSidesFileBaseExtensions[i] );
	}
	return base;
}


/*

Fast routines to turn an array of vectors or indices into a string.

These routines use sprintf() to write into an Array< char > to take
advantage of geometric growth.

These routines do not use the StringUtils::ToString() functions because
turning each vector or index into an OVR::String, and appending it to
a final OVR::String, results in multiple memory allocations including
re-allocation of the final OVR::String for *every* single append.

Beyond a certain string length, the OVR::String class should probably
be changed to use geometric growth instead of always re-allocating to
the exact size when appending another string.

*/

template< Vector2f RawVertex::*attribute >
inline String GetVector2fAttributeString( const RawModel & raw )
{
	Array< char > string;
	string.Resize( 1 );
	string[0] = '{';
	for ( int i = 0; i < raw.GetVertexCount(); i++ )
	{
		const UPInt size = string.GetSize();
		string.Resize( size + 1024 );
		string.Resize( size + sprintf( &string[size], "{ %f %f }",
						( raw.GetVertex( i ).*attribute ).x,
						( raw.GetVertex( i ).*attribute ).y ) );
	}
	const UPInt size = string.GetSize();
	string.Resize( size + 2 );
	string[size + 0] = '}';
	string[size + 1] = '\0';
	return String( string.GetDataPtr(), string.GetSize() );
}

template< Vector3f RawVertex::*attribute >
inline String GetVector3fAttributeString( const RawModel & raw )
{
	Array< char > string;
	string.Resize( 1 );
	string[0] = '{';
	for ( int i = 0; i < raw.GetVertexCount(); i++ )
	{
		const UPInt size = string.GetSize();
		string.Resize( size + 1024 );
		string.Resize( size + sprintf( &string[size], "{ %f %f %f }",
						( raw.GetVertex( i ).*attribute ).x,
						( raw.GetVertex( i ).*attribute ).y,
						( raw.GetVertex( i ).*attribute ).z ) );
	}
	const UPInt size = string.GetSize();
	string.Resize( size + 2 );
	string[size + 0] = '}';
	string[size + 1] = '\0';
	return String( string.GetDataPtr(), string.GetSize() );
}

template< Vector4f RawVertex::*attribute >
inline String GetVector4fAttributeString( const RawModel & raw )
{
	Array< char > string;
	string.Resize( 1 );
	string[0] = '{';
	for ( int i = 0; i < raw.GetVertexCount(); i++ )
	{
		const UPInt size = string.GetSize();
		string.Resize( size + 1024 );
		string.Resize( size + sprintf( &string[size], "{ %f %f %f %f }",
						( raw.GetVertex( i ).*attribute ).x,
						( raw.GetVertex( i ).*attribute ).y,
						( raw.GetVertex( i ).*attribute ).z,
						( raw.GetVertex( i ).*attribute ).w ) );
	}
	const UPInt size = string.GetSize();
	string.Resize( size + 2 );
	string[size + 0] = '}';
	string[size + 1] = '\0';
	return String( string.GetDataPtr(), string.GetSize() );
}

template< Vector4i RawVertex::*attribute >
inline String GetVector4iAttributeString( const RawModel & raw )
{
	Array< char > string;
	string.Resize( 1 );
	string[0] = '{';
	for ( int i = 0; i < raw.GetVertexCount(); i++ )
	{
		const UPInt size = string.GetSize();
		string.Resize( size + 1024 );
		string.Resize( size + sprintf( &string[size], "{ %d %d %d %d }",
						( raw.GetVertex( i ).*attribute ).x,
						( raw.GetVertex( i ).*attribute ).y,
						( raw.GetVertex( i ).*attribute ).z,
						( raw.GetVertex( i ).*attribute ).w ) );
	}
	const UPInt size = string.GetSize();
	string.Resize( size + 2 );
	string[size + 0] = '}';
	string[size + 1] = '\0';
	return String( string.GetDataPtr(), string.GetSize() );
}

inline String GetIndicesString( const RawModel & raw )
{
	Array< char > string;
	string.Resize( 1 );
	string[0] = '{';
	for ( int i = 0; i < raw.GetTriangleCount(); i++ )
	{
		const UPInt size = string.GetSize();
		string.Resize( size + 1024 );
		string.Resize( size + sprintf( &string[size], " %d %d %d",
						raw.GetTriangle( i ).verts[0],
						raw.GetTriangle( i ).verts[1],
						raw.GetTriangle( i ).verts[2] ) );
	}
	const UPInt size = string.GetSize();
	string.Resize( size + 2 );
	string[size + 0] = '}';
	string[size + 1] = '\0';
	return String( string.GetDataPtr(), string.GetSize() );
}

inline String GetVector2fArrayString( const Array< Vector2f > & vertices )
{
	Array< char > string;
	string.Resize( 1 );
	string[0] = '{';
	for ( int i = 0; i < vertices.GetSizeI(); i++ )
	{
		const UPInt size = string.GetSize();
		string.Resize( size + 1024 );
		string.Resize( size + sprintf( &string[size], "{ %f %f }",
						vertices[i].x,
						vertices[i].y ) );
	}
	const UPInt size = string.GetSize();
	string.Resize( size + 2 );
	string[size + 0] = '}';
	string[size + 1] = '\0';
	return String( string.GetDataPtr(), string.GetSize() );
}

inline String GetVector3fArrayString( const Array< Vector3f > & vertices )
{
	Array< char > string;
	string.Resize( 1 );
	string[0] = '{';
	for ( int i = 0; i < vertices.GetSizeI(); i++ )
	{
		const UPInt size = string.GetSize();
		string.Resize( size + 1024 );
		string.Resize( size + sprintf( &string[size], "{ %f %f %f }",
						vertices[i].x,
						vertices[i].y,
						vertices[i].z ) );
	}
	const UPInt size = string.GetSize();
	string.Resize( size + 2 );
	string[size + 0] = '}';
	string[size + 1] = '\0';
	return String( string.GetDataPtr(), string.GetSize() );
}

inline String GetInt32ArrayString( const Array< int > & indices )
{
	Array< char > string;
	string.Resize( 1 );
	string[0] = '{';
	for ( int i = 0; i < indices.GetSizeI(); i++ )
	{
		const UPInt size = string.GetSize();
		string.Resize( size + 1024 );
		string.Resize( size + sprintf( &string[size], " %d", indices[i] ) );
	}
	const UPInt size = string.GetSize();
	string.Resize( size + 2 );
	string[size + 0] = '}';
	string[size + 1] = '\0';
	return String( string.GetDataPtr(), string.GetSize() );
}

#endif // !__MODELDATA_H__
