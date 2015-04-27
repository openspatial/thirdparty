/************************************************************************************

Filename    :   TestJSON.cpp
Content     :   JSON test code.
Created     :   May, 2014
Authors     :   J.M.P. van Waveren

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#include <stdint.h>

#define FBX_TOOL
#include "../../VRLib/jni/LibOVR/Include/OVR.h"
#include "../../VRLib/jni/LibOVR/Src/Kernel/OVR_Hash.h"
#include "../../VRLib/jni/LibOVR/Src/Kernel/OVR_String_Utils.h"
#include "../../VRLib/jni/LibOVR/Src/Kernel/OVR_SysFile.h"
#include "../../VRLib/jni/LibOVR/Src/Kernel/OVR_JSON.h"
#include "../../VRLib/jni/LibOVR/Src/Kernel/OVR_BinaryFile.h"

#include "File_Utils.h"

using namespace OVR;

#include "../../VRLib/jni/ModelTrace.h"

template< typename _type_ >
void ReadModelArray( Array< _type_ > & out, const char * string, const BinaryReader & bin, const int numElements )
{
	if ( string != NULL && string[0] != '\0' && numElements > 0 )
	{
		if ( !bin.ReadArray( out, numElements ) )
		{
			StringUtils::StringTo( out, string );
		}
	}
}

void ParseModelsJsonTest( const char * jsonFileName, const char * binFileName )
{
	const BinaryReader bin( binFileName, NULL );
	if ( bin.ReadUInt32() != 0x6272766F )
	{
		return;
	}

	JSON * json = JSON::Load( jsonFileName );
	if ( json == NULL )
	{
		return;
	}
	const JsonReader models( json );
	if ( models.IsObject() )
	{
		//
		// Render Model
		//

		const JsonReader render_model( models.GetChildByName( "render_model" ) );
		if ( render_model.IsObject() )
		{
			const JsonReader texture_array( render_model.GetChildByName( "textures" ) );
			if ( texture_array.IsArray() )
			{
				while ( !texture_array.IsEndOfArray() )
				{
					const JsonReader texture( texture_array.GetNextArrayElement() );
					if ( texture.IsObject() )
					{
						const String name = texture.GetChildStringByName( "name" );
						const String usage = texture.GetChildStringByName( "usage" );
						const String occlusion = texture.GetChildStringByName( "occlusion" );
					}
				}
			}

			const JsonReader joint_array( render_model.GetChildByName( "joints" ) );
			if ( joint_array.IsArray() )
			{
				while ( !joint_array.IsEndOfArray() )
				{
					const JsonReader joint( joint_array.GetNextArrayElement() );
					if ( joint.IsObject() )
					{
						struct Joint
						{
							String		name;
							Matrix4f	transform;
							String		animation;
							Vector3f	parameters;
							float		timeOffset;
							float		timeScale;
						} newJoint;
						newJoint.name = joint.GetChildStringByName( "name" );
						newJoint.animation = joint.GetChildStringByName( "animation" );
						StringUtils::StringTo( newJoint.transform, joint.GetChildStringByName( "transform" ) );
						newJoint.parameters.x = joint.GetChildFloatByName( "parmX" );
						newJoint.parameters.y = joint.GetChildFloatByName( "parmY" );
						newJoint.parameters.z = joint.GetChildFloatByName( "parmZ" );
						newJoint.timeScale = joint.GetChildFloatByName( "timeScale" );
						newJoint.timeOffset = joint.GetChildFloatByName( "timeOffset" );
					}
				}
			}

			const JsonReader tag_array( render_model.GetChildByName( "tags" ) );
			if ( tag_array.IsArray() )
			{
				while ( !tag_array.IsEndOfArray() )
				{
					const JsonReader tag( tag_array.GetNextArrayElement() );
					if ( tag.IsObject() )
					{
						struct Tag
						{
							String		name;
							Matrix4f	matrix;
							Vector4i	jointIndices;
							Vector4f	jointWeights;
						} newTag;

						const String name = tag.GetChildStringByName( "name" );
						StringUtils::StringTo( newTag.matrix, tag.GetChildStringByName( "matrix" ) );
						StringUtils::StringTo( newTag.jointIndices, tag.GetChildStringByName( "jointIndices" ) );
						StringUtils::StringTo( newTag.jointWeights, tag.GetChildStringByName( "jointWeights" ) );
					}
				}
			}

			const JsonReader surface_array( render_model.GetChildByName( "surfaces" ) );
			if ( surface_array.IsArray() )
			{
				while ( !surface_array.IsEndOfArray() )
				{
					struct Surface
					{
						String				name;
						String				materialType;
						int					diffuseTexture;
						int					normalTexture;
						int					specularTexture;
						int					emissiveTexture;
						Bounds3f			bounds;
						Array< Vector3f >	position;
						Array< Vector3f >	normal;
						Array< Vector3f >	tangent;
						Array< Vector3f >	binormal;
						Array< Vector4f >	color;
						Array< Vector2f >	uv0;
						Array< Vector2f >	uv1;
						Array< Vector4f >	jointWeights;
						Array< Vector4i >	jointIndices;
						Array< uint16_t >	indices;
					} newSurface;

					const JsonReader surface( surface_array.GetNextArrayElement() );
					if ( surface.IsObject() )
					{
						const JsonReader source( surface.GetChildByName( "source" ) );
						if ( source.IsArray() )
						{
							while ( !source.IsEndOfArray() )
							{
								if ( newSurface.name.GetLength() )
								{
									newSurface.name += ";";
								}
								newSurface.name += source.GetNextArrayString();
							}
						}

						const JsonReader material( surface.GetChildByName( "material" ) );
						if ( material.IsObject() )
						{
							newSurface.materialType		= material.GetChildStringByName( "type" );
							newSurface.diffuseTexture	= material.GetChildInt32ByName( "diffuse", -1 );
							newSurface.normalTexture	= material.GetChildInt32ByName( "normal", -1 );
							newSurface.specularTexture	= material.GetChildInt32ByName( "specular", -1 );
							newSurface.emissiveTexture	= material.GetChildInt32ByName( "emissive", -1 );
						}

						StringUtils::StringTo( newSurface.bounds, surface.GetChildStringByName( "bounds" ) );

						//
						// Vertices
						//

						const JsonReader vertices( surface.GetChildByName( "vertices" ) );
						if ( vertices.IsObject() )
						{
							const int vertexCount = vertices.GetChildInt32ByName( "vertexCount" );
							ReadModelArray( newSurface.position,     vertices.GetChildStringByName( "position" ),		bin, vertexCount );
							ReadModelArray( newSurface.normal,       vertices.GetChildStringByName( "normal" ),			bin, vertexCount );
							ReadModelArray( newSurface.tangent,      vertices.GetChildStringByName( "tangent" ),		bin, vertexCount );
							ReadModelArray( newSurface.binormal,     vertices.GetChildStringByName( "binormal" ),		bin, vertexCount );
							ReadModelArray( newSurface.color,        vertices.GetChildStringByName( "color" ),			bin, vertexCount );
							ReadModelArray( newSurface.uv0,          vertices.GetChildStringByName( "uv0" ),			bin, vertexCount );
							ReadModelArray( newSurface.uv1,          vertices.GetChildStringByName( "uv1" ),			bin, vertexCount );
							ReadModelArray( newSurface.jointIndices, vertices.GetChildStringByName( "jointIndices" ),	bin, vertexCount );
							ReadModelArray( newSurface.jointWeights, vertices.GetChildStringByName( "jointWeights" ),	bin, vertexCount );
						}

						//
						// Triangles
						//

						const JsonReader triangles( surface.GetChildByName( "triangles" ) );
						if ( triangles.IsObject() )
						{
							const int indexCount = triangles.GetChildInt32ByName( "indexCount" );
							ReadModelArray( newSurface.indices, triangles.GetChildStringByName( "indices" ), bin, indexCount );
						}
					}
				}
			}
		}

		//
		// Collision Model
		//

		const JsonReader collision_model( models.GetChildByName( "collision_model" ) );
		if ( collision_model.IsArray() )
		{
			while ( !collision_model.IsEndOfArray() )
			{
				Array< Planef > planes;

				const JsonReader polytope( collision_model.GetNextArrayElement() );
				if ( polytope.IsObject() )
				{
					const String name = polytope.GetChildStringByName( "name" );
					StringUtils::StringTo( planes, polytope.GetChildStringByName( "planes" ) );
				}
			}
		}

		//
		// Ground Collision Model
		//

		const JsonReader ground_collision_model( models.GetChildByName( "ground_collision_model" ) );
		if ( ground_collision_model.IsArray() )
		{
			while ( !ground_collision_model.IsEndOfArray() )
			{
				Array< Planef > planes;

				const JsonReader polytope( ground_collision_model.GetNextArrayElement() );
				if ( polytope.IsObject() )
				{
					const String name = polytope.GetChildStringByName( "name" );
					StringUtils::StringTo( planes, polytope.GetChildStringByName( "planes" ) );
				}
			}
		}

		//
		// Ray-Trace Model
		//

		const JsonReader raytrace_model( models.GetChildByName( "raytrace_model" ) );
		if ( raytrace_model.IsObject() )
		{
			ModelTrace traceModel;

			traceModel.header.numVertices	= raytrace_model.GetChildInt32ByName( "numVertices" );
			traceModel.header.numUvs		= raytrace_model.GetChildInt32ByName( "numUvs" );
			traceModel.header.numIndices	= raytrace_model.GetChildInt32ByName( "numIndices" );
			traceModel.header.numNodes		= raytrace_model.GetChildInt32ByName( "numNodes" );
			traceModel.header.numLeafs		= raytrace_model.GetChildInt32ByName( "numLeafs" );
			traceModel.header.numOverflow	= raytrace_model.GetChildInt32ByName( "numOverflow" );

			StringUtils::StringTo( traceModel.header.bounds, raytrace_model.GetChildStringByName( "bounds" ) );

			ReadModelArray( traceModel.vertices, raytrace_model.GetChildStringByName( "vertices" ), bin, traceModel.header.numVertices );
			ReadModelArray( traceModel.uvs, raytrace_model.GetChildStringByName( "uvs" ), bin, traceModel.header.numUvs );
			ReadModelArray( traceModel.indices, raytrace_model.GetChildStringByName( "indices" ), bin, traceModel.header.numIndices );

			if ( !bin.ReadArray( traceModel.nodes, traceModel.header.numNodes ) )
			{
				const JsonReader nodes_array( raytrace_model.GetChildByName( "nodes" ) );
				if ( nodes_array.IsArray() )
				{
					while ( !nodes_array.IsEndOfArray() )
					{
						const UPInt index = traceModel.nodes.AllocBack();

						const JsonReader node( nodes_array.GetNextArrayElement() );
						if ( node.IsObject() )
						{
							traceModel.nodes[index].data = (UInt32) node.GetChildInt64ByName( "data" );
							traceModel.nodes[index].dist = node.GetChildFloatByName( "dist" );
						}
					}
				}
			}

			if ( !bin.ReadArray( traceModel.leafs, traceModel.header.numLeafs ) )
			{
				const JsonReader leafs_array( raytrace_model.GetChildByName( "leafs" ) );
				if ( leafs_array.IsArray() )
				{
					while ( !leafs_array.IsEndOfArray() )
					{
						const UPInt index = traceModel.leafs.AllocBack();

						const JsonReader leaf( leafs_array.GetNextArrayElement() );
						if ( leaf.IsObject() )
						{
							StringUtils::StringTo( traceModel.leafs[index].triangles, RT_KDTREE_MAX_LEAF_TRIANGLES, leaf.GetChildStringByName( "triangles" ) );
							StringUtils::StringTo( traceModel.leafs[index].ropes, 6, leaf.GetChildStringByName( "ropes" ) );
							StringUtils::StringTo( traceModel.leafs[index].bounds, leaf.GetChildStringByName( "bounds" ) );
						}
					}
				}
			}

			ReadModelArray( traceModel.overflow, raytrace_model.GetChildStringByName( "overflow" ), bin, traceModel.header.numOverflow );
		}
	}
	json->Release();

	assert( bin.IsAtEnd() );
}
