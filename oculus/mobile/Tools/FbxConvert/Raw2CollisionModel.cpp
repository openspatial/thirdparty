/************************************************************************************

Filename    :   Raw2CollisionModel.cpp
Content     :   Collision model builder.
Created     :   May, 2014
Authors     :   J.M.P. van Waveren

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/
#include <stdint.h>

#define FBX_TOOL
#include "../../VRLib/jni/LibOVR/Include/OVR.h"
#include "../../VRLib/jni/LibOVR/Src/Kernel/OVR_Hash.h"
#include "../../VRLib/jni/LibOVR/Src/Kernel/OVR_String_Utils.h"
#include "../../VRLib/jni/LibOVR/Src/Kernel/OVR_JSON.h"

using namespace OVR;

#include "RawModel.h"
#include "ModelData.h"

/*
	A collision model is an array of polytopes.
	Each polytope is defined by a set of planes.
	The polytopes are not necessarily bounded or convex.
*/
ModelData * Raw2CollisionModel( const RawModel & raw, const float expand )
{
	printf( "building collision model...\n" );

	printf( "%7d polytopes\n", raw.GetSurfaceCount() );

	JSON * JSON_polytope_array = JSON::CreateArray();

	for ( int surfaceIndex = 0; surfaceIndex < raw.GetSurfaceCount(); surfaceIndex++ )
	{
		Array< Planef > planes;

		for ( int triangleIndex = 0; triangleIndex < raw.GetTriangleCount(); triangleIndex++ )
		{
			const RawTriangle & triangle = raw.GetTriangle( triangleIndex );
			if ( triangle.surfaceIndex != surfaceIndex )
			{
				continue;
			}

			const Vector3f & v0 = raw.GetVertex( triangle.verts[0] ).position;
			const Vector3f & v1 = raw.GetVertex( triangle.verts[1] ).position;
			const Vector3f & v2 = raw.GetVertex( triangle.verts[2] ).position;
			const Vector3f normal = ( v1 - v0 ).Cross( v2 - v0 ).Normalized();
			const float dist = - normal.Dot( v0 ) - expand;

			// Only add the plane if there is not already a plane with the same normal.
			bool found = false;
			for ( int i = 0; i < planes.GetSizeI(); i++ )
			{
				if (	fabsf( planes[i].N.x - normal.x ) < 0.001f &&
						fabsf( planes[i].N.y - normal.y ) < 0.001f &&
						fabsf( planes[i].N.z - normal.z ) < 0.001f )
				{
					found = true;
					break;
				}
			}
			if ( found )
			{
				continue;
			}

			// Sort the the planes based on largest Y.
			int insertIndex = 0;
			for ( int i = 0; i < planes.GetSizeI(); i++ )
			{
				if ( normal.y > planes[i].N.y )
				{
					insertIndex = i;
					break;
				}
			}
			planes.InsertAt( insertIndex, Planef( normal, dist ) );
		}

		JSON * JSON_polytope = JSON::CreateObject();
		{
			JSON_polytope->AddStringItem( "name", raw.GetSurface( surfaceIndex ).name );
			JSON_polytope->AddStringItem( "planes", StringUtils::ToString( planes ) );
		}
		JSON_polytope_array->AddArrayElement( JSON_polytope );
	}

	ModelData * data = new ModelData;
	data->json = JSON_polytope_array;

	return data;
}
