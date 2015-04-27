/************************************************************************************

Filename    :   Raw2RenderModel.cpp
Content     :   Render model builder.
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

void Warning( const char * format, ... );
void Error( const char * format, ... );

typedef unsigned short TriangleIndex;

template< typename _attrib_type_, _attrib_type_ RawVertex::*attribute >
static void AppendBinaryAttributeArray( const RawModel & raw, Array< uint8_t > & out )
{
	Array< _attrib_type_ > attribs;
	raw.GetAttributeArray< _attrib_type_, attribute >( attribs );
	const UPInt size = out.GetSize();
	out.Resize( size + attribs.GetSize() * sizeof( attribs[0] ) );
	memcpy( &out[size], attribs.GetDataPtr(), attribs.GetSize() * sizeof( attribs[0] ) );
}

static void AppendBinaryIndexArray( const RawModel & raw, Array< uint8_t > & out )
{
	const UPInt size = out.GetSize();
	out.Resize( size + raw.GetTriangleCount() * 3 * sizeof( TriangleIndex ) );
	TriangleIndex * indices = (TriangleIndex *) &out[size];
	for ( int i = 0; i < raw.GetTriangleCount(); i++ )
	{
		indices[i * 3 + 0] = raw.GetTriangle( i ).verts[0];
		indices[i * 3 + 1] = raw.GetTriangle( i ).verts[1];
		indices[i * 3 + 2] = raw.GetTriangle( i ).verts[2];
	}
}

ModelData * Raw2RenderModel( const RawModel & raw, const int keepAttribs, const bool showRenderOrder, const bool fullText )
{
	printf( "building render model...\n" );

	if ( raw.GetVertexCount() > 2 * raw.GetTriangleCount() )
	{
		Warning( "High vertex count. Make sure there are no unnecessary vertex attributes. (see -attrib cmd-line option)\n" );
	}

	Array< RawModel > materialModels;
	raw.CreateMaterialModels( materialModels, ( 1 << ( sizeof( TriangleIndex ) * 8 ) ), keepAttribs );

	if ( showRenderOrder )
	{
		for ( int i = 0; i < materialModels.GetSizeI(); i++ )
		{
			materialModels[i].RenderOrder( i );
		}
	}

	const Bounds3f bounds = raw.GetBounds();

	printf( "%7d vertices\n", raw.GetVertexCount() );
	printf( "%7d triangles\n", raw.GetTriangleCount() );
	printf( "%7d textures\n", raw.GetTextureCount() );
	printf( "%7d joints\n", raw.GetJointCount() );
	printf( "%7d tags\n", raw.GetTagCount() );
	printf( "%7d surfaces\n", materialModels.GetSizeI() );
	printf( "mins = { %4.1f, %4.1f, %4.1f }\n", bounds.b[0].x, bounds.b[0].y, bounds.b[0].z );
	printf( "maxs = { %4.1f, %4.1f, %4.1f }\n", bounds.b[1].x, bounds.b[1].y, bounds.b[1].z );

	ModelData * data = new ModelData;

	JSON * JSON_render_model = JSON::CreateObject();
	{
		// textures

		JSON * JSON_texture_array = JSON::CreateArray();
		for ( int textureIndex = 0; textureIndex < raw.GetTextureCount(); textureIndex++ )
		{
			const RawTexture & texture = raw.GetTexture( textureIndex );
			String textureName = StringUtils::GetFileBaseString( texture.name );
			if ( texture.usage == RAW_TEXTURE_USAGE_REFLECTION )
			{
				textureName = StripCubeMapSidesFileBaseExtensions( textureName );
				textureName += cubeMapFileBaseExtension;
			}

			JSON * JSON_texture = JSON::CreateObject();
			JSON_texture->AddNumberItem( "index", textureIndex );
			JSON_texture->AddStringItem( "name", textureName );
			switch ( texture.usage )
			{
				case RAW_TEXTURE_USAGE_DIFFUSE:			JSON_texture->AddStringItem( "usage", "diffuse" ); break;
				case RAW_TEXTURE_USAGE_NORMAL:			JSON_texture->AddStringItem( "usage", "normal" ); break;
				case RAW_TEXTURE_USAGE_SPECULAR:		JSON_texture->AddStringItem( "usage", "specular" ); break;
				case RAW_TEXTURE_USAGE_EMISSIVE:		JSON_texture->AddStringItem( "usage", "emissive" ); break;
				case RAW_TEXTURE_USAGE_REFLECTION:		JSON_texture->AddStringItem( "usage", "reflection" ); break;
			}
			switch ( texture.occlusion )
			{
				case RAW_TEXTURE_OCCLUSION_OPAQUE:		JSON_texture->AddStringItem( "occlusion", "opaque" ); break;
				case RAW_TEXTURE_OCCLUSION_PERFORATED:	JSON_texture->AddStringItem( "occlusion", "perforated" ); break;
				case RAW_TEXTURE_OCCLUSION_TRANSPARENT:	JSON_texture->AddStringItem( "occlusion", "transparent" ); break;
			}
			JSON_texture_array->AddArrayElement( JSON_texture );
		}
		JSON_render_model->AddItem( "textures", JSON_texture_array );

		// joints

		JSON * JSON_joint_array = JSON::CreateArray();
		for ( int jointIndex = 0; jointIndex < raw.GetJointCount(); jointIndex++ )
		{
			const RawJoint & joint = raw.GetJoint( jointIndex );
			JSON * JSON_joint = JSON::CreateObject();
			JSON_joint->AddStringItem( "name", joint.name );
			JSON_joint->AddStringItem( "transform", StringUtils::ToString( joint.transform ) );
			JSON_joint->AddStringItem( "animation", ( joint.animation == RAW_JOINT_ANIMATION_NONE ? "none" :
													( joint.animation == RAW_JOINT_ANIMATION_ROTATE ? "rotate" :
													( joint.animation == RAW_JOINT_ANIMATION_SWAY ? "sway" :
													( joint.animation == RAW_JOINT_ANIMATION_BOB ? "bob" : "none" ) ) ) ) );
			JSON_joint->AddNumberItem( "parmX", joint.parameters.x );
			JSON_joint->AddNumberItem( "parmY", joint.parameters.y );
			JSON_joint->AddNumberItem( "parmZ", joint.parameters.z );
			JSON_joint->AddNumberItem( "timeOffset", joint.timeOffset );
			JSON_joint->AddNumberItem( "timeScale", joint.timeScale );
			JSON_joint_array->AddArrayElement( JSON_joint );
		}
		JSON_render_model->AddItem( "joints", JSON_joint_array );

		// tags

		JSON * JSON_tag_array = JSON::CreateArray();
		for ( int tagIndex = 0; tagIndex < raw.GetTagCount(); tagIndex++ )
		{
			const RawTag & tag = raw.GetTag( tagIndex );
			JSON * JSON_tag = JSON::CreateObject();
			JSON_tag->AddStringItem( "name", tag.name );
			JSON_tag->AddStringItem( "matrix", StringUtils::ToString( tag.matrix ) );
			JSON_tag->AddStringItem( "jointIndices", StringUtils::ToString( tag.jointIndices ) );
			JSON_tag->AddStringItem( "jointWeights", StringUtils::ToString( tag.jointWeights ) );
			JSON_tag_array->AddArrayElement( JSON_tag );
		}
		JSON_render_model->AddItem( "tags", JSON_tag_array );

		// surfaces

		JSON * JSON_surface_array = JSON::CreateArray();
		for ( int surfaceIndex = 0; surfaceIndex < materialModels.GetSizeI(); surfaceIndex++ )
		{
			const RawModel & surfaceModel = materialModels[surfaceIndex];

			JSON * JSON_surface = JSON::CreateObject();
			{
				// surface source meshes

				JSON * JSON_source_array = JSON::CreateArray();
				for ( int i = 0; i < surfaceModel.GetSurfaceCount(); i++ )
				{
					JSON_source_array->AddArrayString( surfaceModel.GetSurface( i ).name );
				}
				JSON_surface->AddItem( "source", JSON_source_array );

				// surface material

				JSON * JSON_material = JSON::CreateObject();
				{
					const RawMaterial & surfaceMaterial = surfaceModel.GetMaterial( surfaceModel.GetTriangle( 0 ).materialIndex );

					switch ( surfaceMaterial.type )
					{
						case RAW_MATERIAL_TYPE_OPAQUE:		JSON_material->AddStringItem( "type", "opaque" ); break;
						case RAW_MATERIAL_TYPE_PERFORATED:	JSON_material->AddStringItem( "type", "perforated" ); break;
						case RAW_MATERIAL_TYPE_TRANSPARENT:	JSON_material->AddStringItem( "type", "transparent" ); break;
						case RAW_MATERIAL_TYPE_ADDITIVE:	JSON_material->AddStringItem( "type", "additive" ); break;
					}

					JSON_material->AddNumberItem( "diffuse", surfaceMaterial.textures[RAW_TEXTURE_USAGE_DIFFUSE] );
					JSON_material->AddNumberItem( "normal", surfaceMaterial.textures[RAW_TEXTURE_USAGE_NORMAL] );
					JSON_material->AddNumberItem( "specular", surfaceMaterial.textures[RAW_TEXTURE_USAGE_SPECULAR] );
					JSON_material->AddNumberItem( "emissive", surfaceMaterial.textures[RAW_TEXTURE_USAGE_EMISSIVE] );
					JSON_material->AddNumberItem( "reflection", surfaceMaterial.textures[RAW_TEXTURE_USAGE_REFLECTION] );
				}
				JSON_surface->AddItem( "material", JSON_material );

				// surface bounds

				JSON_surface->AddStringItem( "bounds", StringUtils::ToString( surfaceModel.GetBounds() ) );

				// surface vertices

				JSON * JSON_vertices = JSON::CreateObject();
				{
					JSON_vertices->AddNumberItem( "vertexCount", surfaceModel.GetVertexCount() );

					if ( ( surfaceModel.GetVertexAttributes() & RAW_VERTEX_ATTRIBUTE_POSITION ) != 0 )
					{
						JSON_vertices->AddStringItem( "position", fullText ? GetVector3fAttributeString< &RawVertex::position >( surfaceModel ) : "Vector3f" );
						AppendBinaryAttributeArray< Vector3f, &RawVertex::position >( surfaceModel, data->binary );
					}
					if ( ( surfaceModel.GetVertexAttributes() & RAW_VERTEX_ATTRIBUTE_NORMAL ) != 0 )
					{
						JSON_vertices->AddStringItem( "normal", fullText ? GetVector3fAttributeString< &RawVertex::normal >( surfaceModel ) : "Vector3f" );
						AppendBinaryAttributeArray< Vector3f, &RawVertex::normal>( surfaceModel, data->binary );
					}
					if ( ( surfaceModel.GetVertexAttributes() & RAW_VERTEX_ATTRIBUTE_TANGENT ) != 0 )
					{
						JSON_vertices->AddStringItem( "tangent", fullText ? GetVector3fAttributeString< &RawVertex::tangent >( surfaceModel ) : "Vector3f" );
						AppendBinaryAttributeArray< Vector3f, &RawVertex::tangent>( surfaceModel, data->binary );
					}
					if ( ( surfaceModel.GetVertexAttributes() & RAW_VERTEX_ATTRIBUTE_BINORMAL ) != 0 )
					{
						JSON_vertices->AddStringItem( "binormal", fullText ? GetVector3fAttributeString< &RawVertex::binormal >( surfaceModel ) : "Vector3f" );
						AppendBinaryAttributeArray< Vector3f, &RawVertex::binormal>( surfaceModel, data->binary );
					}
					if ( ( surfaceModel.GetVertexAttributes() & RAW_VERTEX_ATTRIBUTE_COLOR ) != 0 )
					{
						JSON_vertices->AddStringItem( "color", fullText ? GetVector4fAttributeString< &RawVertex::color >( surfaceModel ) : "Vector4f" );
						AppendBinaryAttributeArray< Vector4f, &RawVertex::color >( surfaceModel, data->binary );
					}
					if ( ( surfaceModel.GetVertexAttributes() & RAW_VERTEX_ATTRIBUTE_UV0 ) != 0 )
					{
						JSON_vertices->AddStringItem( "uv0", fullText ? GetVector2fAttributeString< &RawVertex::uv0 >( surfaceModel ) : "Vector2f" );
						AppendBinaryAttributeArray< Vector2f, &RawVertex::uv0 >( surfaceModel, data->binary );
					}
					if ( ( surfaceModel.GetVertexAttributes() & RAW_VERTEX_ATTRIBUTE_UV1 ) != 0 )
					{
						JSON_vertices->AddStringItem( "uv1", fullText ? GetVector2fAttributeString< &RawVertex::uv1 >( surfaceModel ) : "Vector2f" );
						AppendBinaryAttributeArray< Vector2f, &RawVertex::uv1 >( surfaceModel, data->binary );
					}
					if ( ( surfaceModel.GetVertexAttributes() & RAW_VERTEX_ATTRIBUTE_JOINT_INDICES ) != 0 )
					{
						JSON_vertices->AddStringItem( "jointIndices", fullText ? GetVector4iAttributeString< &RawVertex::jointIndices >( surfaceModel ) : "Vector4i" );
						AppendBinaryAttributeArray< Vector4i, &RawVertex::jointIndices >( surfaceModel, data->binary );
					}
					if ( ( surfaceModel.GetVertexAttributes() & RAW_VERTEX_ATTRIBUTE_JOINT_WEIGHTS ) != 0 )
					{
						JSON_vertices->AddStringItem( "jointWeights", fullText ? GetVector4fAttributeString< &RawVertex::jointWeights >( surfaceModel ) : "Vector4f" );
						AppendBinaryAttributeArray< Vector4f, &RawVertex::jointWeights >( surfaceModel, data->binary );
					}
				}
				JSON_surface->AddItem( "vertices", JSON_vertices );

				// surface triangles

				JSON * JSON_triangles = JSON::CreateObject();
				{
					JSON_triangles->AddNumberItem( "indexCount", surfaceModel.GetTriangleCount() * 3 );
					JSON_triangles->AddStringItem( "indices", fullText ? GetIndicesString( surfaceModel ) : "UInt16" );
					AppendBinaryIndexArray( surfaceModel, data->binary );
				}
				JSON_surface->AddItem( "triangles", JSON_triangles );
			}
			JSON_surface_array->AddArrayElement( JSON_surface );
		}
		JSON_render_model->AddItem( "surfaces", JSON_surface_array );
	}

	data->json = JSON_render_model;

	return data;
}
