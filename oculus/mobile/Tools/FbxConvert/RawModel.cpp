/************************************************************************************

Filename    :   RawModel.cpp
Content     :   Raw model.
Created     :   May, 2014
Authors     :   J.M.P. van Waveren

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#include <algorithm>
#include <stdint.h>

#define FBX_TOOL
#include "../../VRLib/jni/LibOVR/Include/OVR.h"
#include "../../VRLib/jni/LibOVR/Src/Kernel/OVR_Hash.h"
#include "../../VRLib/jni/LibOVR/Src/Kernel/OVR_String.h"
#include "../../VRLib/jni/LibOVR/Src/Kernel/OVR_String_Utils.h"

#include "../../VRLib/jni/3rdParty/stb/stb_image.h"
#include "../../VRLib/jni/3rdParty/stb/stb_image_write.h"

using namespace OVR;

#include "Image_Utils.h"
#include "RawModel.h"

void Warning( const char * format, ... );
void Error( const char * format, ... );

static float MinFloat( float x, float y )
{
	return ( x < y ) ? x : y;
}

static float MaxFloat( float x, float y )
{
	return ( x > y ) ? x : y;
}

static float Log2f( float f )
{
	return logf( f ) * 1.442695041f;
}

static float Sinf( float f )
{
	return sinf( f );
}

bool RawVertex::operator==( const RawVertex & other ) const
{
	return	( position == other.position ) &&
			( normal == other.normal ) &&
			( tangent == other.tangent ) &&
			( binormal == other.binormal ) &&
			( color == other.color ) &&
			( uv0 == other.uv0 ) &&
			( uv1 == other.uv1 ) &&
			( jointIndices == other.jointIndices ) &&
			( jointWeights == other.jointWeights ) &&
			( polarityUv0 == other.polarityUv0 );
}

int RawVertex::Difference( const RawVertex & other ) const
{
	int attributes = 0;
	if ( position != other.position ) { attributes |= RAW_VERTEX_ATTRIBUTE_POSITION; }
	if ( normal != other.normal ) { attributes |= RAW_VERTEX_ATTRIBUTE_NORMAL; }
	if ( tangent != other.tangent ) { attributes |= RAW_VERTEX_ATTRIBUTE_TANGENT; }
	if ( binormal != other.binormal ) { attributes |= RAW_VERTEX_ATTRIBUTE_BINORMAL; }
	if ( color != other.color ) { attributes |= RAW_VERTEX_ATTRIBUTE_COLOR; }
	if ( uv0 != other.uv0 ) { attributes |= RAW_VERTEX_ATTRIBUTE_UV0; }
	if ( uv1 != other.uv1 ) { attributes |= RAW_VERTEX_ATTRIBUTE_UV1; }
	if ( jointIndices != other.jointIndices ) { attributes |= RAW_VERTEX_ATTRIBUTE_JOINT_INDICES; }
	if ( jointWeights != other.jointWeights ) { attributes |= RAW_VERTEX_ATTRIBUTE_JOINT_WEIGHTS; }
	return attributes;
}

void RawModel::AddVertexAttribute( const RawVertexAttribute attrib )
{
	vertexAttributes |= attrib;
}

int RawModel::AddVertex( const RawVertex & vertex )
{
	int index = -1;
	if ( vertexHash.Get( vertex, &index ) )
	{
		return index;
	}
	vertexHash.Add( vertex, vertices.GetSizeI() );
	vertices.PushBack( vertex );
	return vertices.GetSizeI() - 1;
}

int RawModel::AddTexture( const char * name, const RawTextureUsage usage )
{
	if ( name[0] == '\0' )
	{
		return -1;
	}
	for ( int i = 0; i < textures.GetSizeI(); i++ )
	{
		if ( textures[i].name.CompareNoCase( name ) == 0 && textures[i].usage == usage )
		{
			return i;
		}
	}

	const ImageProperties properties = GetImageProperties( name );

	const UPInt index = textures.AllocBack();
	textures[index].name = name;
	textures[index].width = properties.width;
	textures[index].height = properties.height;
	textures[index].mipLevels = (int)ceilf( Log2f( Alg::Max( (float)properties.width, (float)properties.height ) ) );
	textures[index].usage = usage;
	textures[index].occlusion = ( properties.occlusion == IMAGE_OPAQUE ? RAW_TEXTURE_OCCLUSION_OPAQUE :
								( properties.occlusion == IMAGE_PERFORATED ? RAW_TEXTURE_OCCLUSION_PERFORATED :
								( properties.occlusion == IMAGE_TRANSPARENT ? RAW_TEXTURE_OCCLUSION_TRANSPARENT :
								RAW_TEXTURE_OCCLUSION_OPAQUE ) ) );
	return (int)index;
}

int RawModel::AddMaterial( const RawMaterialType materialType, const int textures[RAW_TEXTURE_USAGE_MAX] )
{
	for ( int i = 0; i < materials.GetSizeI(); i++ )
	{
		if ( materials[i].type != materialType )
		{
			continue;
		}
		bool match = true;
		for ( int j = 0; j < RAW_TEXTURE_USAGE_MAX; j++ )
		{
			if ( materials[i].textures[j] != textures[j] )
			{
				match = false;
				break;
			}
		}
		if ( match )
		{
			return i;
		}
	}
	const UPInt index = materials.AllocBack();
	materials[index].type = materialType;
	for ( int i = 0; i < RAW_TEXTURE_USAGE_MAX; i++ )
	{
		materials[index].textures[i] = textures[i];
	}
	return (int)index;
}

int RawModel::AddSurface( const char * name, const Matrix4f & transform )
{
	assert( name[0] != '\0' );

	for ( int i = 0; i < surfaces.GetSizeI(); i++ )
	{
		if ( surfaces[i].name.CompareNoCase( name ) == 0 )
		{
			return i;
		}
	}
	const UPInt index = surfaces.AllocBack();
	surfaces[index].name = name;
	surfaces[index].transform = transform;
	surfaces[index].atlas = -1;
	surfaces[index].discrete = false;
	surfaces[index].skinRigid = false;
	return (int)index;
}

int RawModel::AddJoint( const char * name, const Matrix4f & transform )
{
	assert( name[0] != '\0' );

	for ( int i = 0; i < joints.GetSizeI(); i++ )
	{
		if ( joints[i].name.CompareNoCase( name ) == 0 )
		{
			return i;
		}
	}
	const UPInt index = joints.AllocBack();
	joints[index].name = name;
	joints[index].transform = transform;
	joints[index].animation = RAW_JOINT_ANIMATION_NONE;
	joints[index].parameters = Vector3f( 0.0f );
	joints[index].timeOffset = 0.0f;
	joints[index].timeScale = 1.0f;
	return (int)index;
}

int RawModel::AddTriangle( const int v0, const int v1, const int v2, const int materialIndex, const int surfaceIndex )
{
	const RawTriangle triangle = { { v0, v1, v2 }, materialIndex, surfaceIndex };
	triangles.PushBack( triangle );
	return triangles.GetSizeI() - 1;
}

int RawModel::AddTag( const int surfaceIndex )
{
	// Find the first triangle for this surface.
	int triangleIndex = -1;
	for ( int i = 0; i < triangles.GetSizeI(); i++ )
	{
		if ( triangles[i].surfaceIndex == surfaceIndex )
		{
			triangleIndex = i;
			break;
		}
	}
	if ( triangleIndex == -1 )
	{
		return -1;
	}

	const Vector3f v0 = vertices[triangles[triangleIndex].verts[0]].position;
	const Vector3f v1 = vertices[triangles[triangleIndex].verts[1]].position;
	const Vector3f v2 = vertices[triangles[triangleIndex].verts[2]].position;

	const Vector3f e0 = ( v1 - v0 ).Normalized();
	const Vector3f e1 = ( v2 - v1 ).Normalized();
	const Vector3f e2 = ( v0 - v2 ).Normalized();

	const float ortho0 = fabsf( e0.Dot( e2 ) );
	const float ortho1 = fabsf( e0.Dot( e1 ) );
	const float ortho2 = fabsf( e1.Dot( e2 ) );

	const int vertexOffset = ( ortho0 < ortho1 ) ? ( ortho0 < ortho2 ? 0 : 2 ) : ( ortho1 < ortho2 ? 1 : 2 );

	const RawVertex & p0 = vertices[triangles[triangleIndex].verts[( vertexOffset + 0 ) % 3]];
	const RawVertex & p1 = vertices[triangles[triangleIndex].verts[( vertexOffset + 1 ) % 3]];
	const RawVertex & p2 = vertices[triangles[triangleIndex].verts[( vertexOffset + 2 ) % 3]];

	Matrix4f matrix;
	matrix.SetTranslation( p0.position );
	matrix.SetXBasis( p1.position - p0.position );
	matrix.SetYBasis( p2.position - p0.position );
	matrix.SetZBasis( matrix.GetXBasis().Cross( matrix.GetYBasis() ).Normalized() );

	const UPInt index = tags.AllocBack();
	tags[index].name = surfaces[surfaceIndex].name;
	tags[index].matrix = matrix;
	tags[index].jointIndices = p0.jointIndices;
	tags[index].jointWeights = p0.jointWeights;

	return (int)index;
}

template< typename _attrib_type_, _attrib_type_ RawVertex::*attribute >
static void SetVertexAttribute( Array< RawVertex > & vertices, const _attrib_type_ & defaultValue )
{
	for ( int i = 0; i < vertices.GetSizeI(); i++ )
	{
		vertices[i].*attribute = defaultValue;
	}
}

void RawModel::RemoveVertexAttribute( const RawVertexAttribute removeAttribs )
{
	if ( removeAttribs == 0 )
	{
		return;
	}

	vertexAttributes &= ~removeAttribs;

	const RawVertex defaultVertex;

	if ( ( removeAttribs & RAW_VERTEX_ATTRIBUTE_POSITION ) != 0 )		{ SetVertexAttribute< Vector3f, &RawVertex::position >( vertices, defaultVertex.position ); }
	if ( ( removeAttribs & RAW_VERTEX_ATTRIBUTE_NORMAL ) != 0 )			{ SetVertexAttribute< Vector3f, &RawVertex::normal >( vertices, defaultVertex.normal ); }
	if ( ( removeAttribs & RAW_VERTEX_ATTRIBUTE_TANGENT ) != 0 )		{ SetVertexAttribute< Vector3f, &RawVertex::tangent >( vertices, defaultVertex.tangent ); }
	if ( ( removeAttribs & RAW_VERTEX_ATTRIBUTE_BINORMAL ) != 0 )		{ SetVertexAttribute< Vector3f, &RawVertex::binormal >( vertices, defaultVertex.binormal ); }
	if ( ( removeAttribs & RAW_VERTEX_ATTRIBUTE_COLOR ) != 0 )			{ SetVertexAttribute< Vector4f, &RawVertex::color >( vertices, defaultVertex.color ); }
	if ( ( removeAttribs & RAW_VERTEX_ATTRIBUTE_UV0 ) != 0 )			{ SetVertexAttribute< Vector2f, &RawVertex::uv0 >( vertices, defaultVertex.uv0 ); }
	if ( ( removeAttribs & RAW_VERTEX_ATTRIBUTE_UV1 ) != 0 )			{ SetVertexAttribute< Vector2f, &RawVertex::uv1 >( vertices, defaultVertex.uv0 ); }
	if ( ( removeAttribs & RAW_VERTEX_ATTRIBUTE_JOINT_INDICES ) != 0 )	{ SetVertexAttribute< Vector4i, &RawVertex::jointIndices >( vertices, defaultVertex.jointIndices ); }
	if ( ( removeAttribs & RAW_VERTEX_ATTRIBUTE_JOINT_WEIGHTS ) != 0 )	{ SetVertexAttribute< Vector4f, &RawVertex::jointWeights >( vertices, defaultVertex.jointWeights ); }
}

void RawModel::RemoveTexture( const int textureIndex )
{
	textures.RemoveAtUnordered( textureIndex );
	for ( int i = 0; i < materials.GetSizeI(); i++ )
	{
		for ( int j = 0; j < RAW_TEXTURE_USAGE_MAX; j++ )
		{
			if ( materials[i].textures[j] == textureIndex )
			{
				materials[i].textures[j] = -1;
			}
			else if ( materials[i].textures[j] == textures.GetSizeI() )
			{
				materials[i].textures[j] = textureIndex;
			}
		}
	}
}

void RawModel::RemoveMaterial( const int materialIndex )
{
	materials.RemoveAtUnordered( materialIndex );

	int triangleCount = 0;
	for ( int i = 0; i < triangles.GetSizeI(); i++ )
	{
		if ( triangles[i].materialIndex != materialIndex )
		{
			triangles[triangleCount] = triangles[i];
			if ( triangles[i].materialIndex == materials.GetSizeI() )
			{
				triangles[triangleCount].materialIndex = materialIndex;
			}
			triangleCount++;
		}
	}
	triangles.Resize( triangleCount );
}

void RawModel::RemoveSurface( const int surfaceIndex )
{
	surfaces.RemoveAtUnordered( surfaceIndex );

	int triangleCount = 0;
	for ( int i = 0; i < triangles.GetSizeI(); i++ )
	{
		if ( triangles[i].surfaceIndex != surfaceIndex )
		{
			triangles[triangleCount] = triangles[i];
			if ( triangles[i].surfaceIndex == surfaces.GetSizeI() )
			{
				triangles[triangleCount].surfaceIndex = surfaceIndex;
			}
			triangleCount++;
		}
	}
	triangles.Resize( triangleCount );
}

struct triVerts_t
{
	int v[3];

	bool operator==( const triVerts_t & other ) const
	{
		return (	v[0] == other.v[0] &&
					v[1] == other.v[1] &&
					v[2] == other.v[2] );
	}
};

int RawModel::RemoveDuplicateTriangles()
{
	Array< RawTriangle > oldTriangles = triangles;
	Hash< triVerts_t, int > triangleHash;
	int duplicates = 0;

	triangles.Clear();
	for ( int i = 0; i < oldTriangles.GetSizeI(); i++ )
	{
		const int minVert = ( oldTriangles[i].verts[0] < oldTriangles[i].verts[1] ) ?
								( oldTriangles[i].verts[0] < oldTriangles[i].verts[2] ? 0 : 2 ) :
								( oldTriangles[i].verts[1] < oldTriangles[i].verts[2] ? 1 : 2 );
		triVerts_t verts;
		for ( int j = 0; j < 3; j++ )
		{
			verts.v[j] = oldTriangles[i].verts[( minVert + j ) % 3];
		}

		if ( triangleHash.Get( verts ) != NULL )
		{
			duplicates++;
			continue;
		}

		triangleHash.Add( verts, i );
		triangles.PushBack( oldTriangles[i] );
	}

	return duplicates;
}

void RawModel::Condense()
{
	// Only keep surfaces that are referenced by one or more triangles.
	{
		Array< RawSurface > oldSurfaces = surfaces;

		surfaces.Clear();

		for ( int i = 0; i < triangles.GetSizeI(); i++ )
		{
			const RawSurface & surface = oldSurfaces[triangles[i].surfaceIndex];
			const int surfaceIndex = AddSurface( surface.name, surface.transform );
			surfaces[surfaceIndex] = surface;
			triangles[i].surfaceIndex = surfaceIndex;
		}
	}

	// Only keep materials that are referenced by one or more triangles.
	{
		Array< RawMaterial > oldMaterials = materials;

		materials.Clear();

		for ( int i = 0; i < triangles.GetSizeI(); i++ )
		{
			const RawMaterial & material = oldMaterials[triangles[i].materialIndex];
			const int materialIndex = AddMaterial( material.type, material.textures );
			materials[materialIndex] = material;
			triangles[i].materialIndex = materialIndex;
		}
	}

	// Only keep textures that are referenced by one or more materials.
	{
		Array< RawTexture > oldTextures = textures;

		textures.Clear();

		for ( int i = 0; i < materials.GetSizeI(); i++ )
		{
			for ( int j = 0; j < RAW_TEXTURE_USAGE_MAX; j++ )
			{
				if ( materials[i].textures[j] >= 0 )
				{
					const RawTexture & texture = oldTextures[materials[i].textures[j]];
					const int textureIndex = AddTexture( texture.name, texture.usage );
					textures[textureIndex] = texture;
					materials[i].textures[j] = textureIndex;
				}
			}
		}
	}

	// Only keep vertices that are referenced by one or more triangles.
	{
		Array< RawVertex > oldVertices = vertices;

		vertexHash.Clear();
		vertices.Clear();

		for ( int i = 0; i < triangles.GetSizeI(); i++ )
		{
			for ( int j = 0; j < 3; j++ )
			{
				triangles[i].verts[j] = AddVertex( oldVertices[triangles[i].verts[j]] );
			}
		}
	}
}

Vector3f VectorNormalize( const Vector3f & v )
{
	const float lengthSqr = v.LengthSq();
	const float SmallestNonDenormal	= float( 1.1754943508222875e-038f );	// ( 1U << 23 )
	const float HugeNumber			= float( 1.8446742974197924e+019f );	// ( ( ( 127U * 3 / 2 ) << 23 ) | ( ( 1 << 23 ) - 1 ) )
	const float lengthRcp = ( lengthSqr >= SmallestNonDenormal ) ? 1.0f / sqrt( lengthSqr ) : HugeNumber;
	return Vector3f( v.x * lengthRcp, v.y * lengthRcp, v.z * lengthRcp );
}

void RawModel::CalculateNormals()
{
	for ( int i = 0; i < vertices.GetSizeI(); i++ )
	{
		vertices[i].normal = Vector3f( 0.0f );
	}

	for ( int i = 0; i < triangles.GetSizeI(); i++ )
	{
		const int * verts = triangles[i].verts;

		const float l0 = ( vertices[verts[1]].position - vertices[verts[0]].position ).LengthSq();
		const float l1 = ( vertices[verts[2]].position - vertices[verts[1]].position ).LengthSq();
		const float l2 = ( vertices[verts[0]].position - vertices[verts[2]].position ).LengthSq();

		const int index = ( l0 > l1 ) ? ( l0 > l2 ? 2 : 1 ) : ( l1 > l2 ? 0 : 1 );

		const Vector3f e0 = vertices[verts[( index + 1 ) % 3]].position - vertices[verts[index]].position;
		const Vector3f e1 = vertices[verts[( index + 2 ) % 3]].position - vertices[verts[index]].position;

		const Vector3f normal = VectorNormalize( e0.Cross( e1 ) );

		for ( int j = 0; j < 3; j++ )
		{
			vertices[triangles[i].verts[j]].normal += normal;
		}
	}

	for ( int i = 0; i < vertices.GetSizeI(); i++ )
	{
		vertices[i].normal = VectorNormalize( vertices[i].normal );
	}

	vertexAttributes |= RAW_VERTEX_ATTRIBUTE_NORMAL;
}

void RawModel::CalculateTangents()
{
	for ( int i = 0; i < vertices.GetSizeI(); i++ )
	{
		vertices[i].tangent = Vector3f( 0.0f );
	}

	for ( int i = 0; i < triangles.GetSizeI(); i++ )
	{
		const int * verts = triangles[i].verts;

		const float l0 = ( vertices[verts[1]].position - vertices[verts[0]].position ).LengthSq();
		const float l1 = ( vertices[verts[2]].position - vertices[verts[1]].position ).LengthSq();
		const float l2 = ( vertices[verts[0]].position - vertices[verts[2]].position ).LengthSq();

		const int index = ( l0 > l1 ) ? ( l0 > l2 ? 2 : 1 ) : ( l1 > l2 ? 0 : 1 );

		const Vector3f d0 = vertices[verts[( index + 1 ) % 3]].position - vertices[verts[index]].position;
		const Vector2f s0 = vertices[verts[( index + 1 ) % 3]].uv0 - vertices[verts[index]].uv0;

		const Vector3f d1 = vertices[verts[( index + 2 ) % 3]].position - vertices[verts[index]].position;
		const Vector2f s1 = vertices[verts[( index + 2 ) % 3]].uv0 - vertices[verts[index]].uv0;

		const float sign = ( s0.x * s1.y - s0.y * s1.x ) < 0.0f ? -1.0f : 1.0f;

		const Vector3f tangent = VectorNormalize( d0 * s1.y - d1 * s0.y ) * sign;

		for ( int j = 0; j < 3; j++ )
		{
			vertices[triangles[i].verts[j]].tangent += tangent;
		}
	}

	for ( int i = 0; i < vertices.GetSizeI(); i++ )
	{
		vertices[i].tangent = VectorNormalize( vertices[i].tangent );
	}

	vertexAttributes |= RAW_VERTEX_ATTRIBUTE_TANGENT;
}

void RawModel::CalculateBinormals()
{
	for ( int i = 0; i < vertices.GetSizeI(); i++ )
	{
		vertices[i].binormal = Vector3f( 0.0f );
	}

	for ( int i = 0; i < triangles.GetSizeI(); i++ )
	{
		const int * verts = triangles[i].verts;

		const float l0 = ( vertices[verts[1]].position - vertices[verts[0]].position ).LengthSq();
		const float l1 = ( vertices[verts[2]].position - vertices[verts[1]].position ).LengthSq();
		const float l2 = ( vertices[verts[0]].position - vertices[verts[2]].position ).LengthSq();

		const int index = ( l0 > l1 ) ? ( l0 > l2 ? 2 : 1 ) : ( l1 > l2 ? 0 : 1 );

		const Vector3f d0 = vertices[verts[( index + 1 ) % 3]].position - vertices[verts[index]].position;
		const Vector2f s0 = vertices[verts[( index + 1 ) % 3]].uv0 - vertices[verts[index]].uv0;

		const Vector3f d1 = vertices[verts[( index + 2 ) % 3]].position - vertices[verts[index]].position;
		const Vector2f s1 = vertices[verts[( index + 2 ) % 3]].uv0 - vertices[verts[index]].uv0;

		const float sign = ( s0.x * s1.y - s0.y * s1.x ) < 0.0f ? -1.0f : 1.0f;

		const Vector3f binormal = VectorNormalize( d1 * s0.x - d0 * s1.x ) * sign;

		for ( int j = 0; j < 3; j++ )
		{
			vertices[triangles[i].verts[j]].binormal += binormal;
		}
	}

	for ( int i = 0; i < vertices.GetSizeI(); i++ )
	{
		vertices[i].binormal = VectorNormalize( vertices[i].binormal );
	}

	vertexAttributes |= RAW_VERTEX_ATTRIBUTE_BINORMAL;
}

void RawModel::TransformGeometry( const Matrix4f & transform )
{
	Matrix4f inverseTransposeTransform = transform.Inverted().Transposed();

	inverseTransposeTransform.M[0][3] = 0.0f;
	inverseTransposeTransform.M[1][3] = 0.0f;
	inverseTransposeTransform.M[2][3] = 0.0f;
	inverseTransposeTransform.M[3][0] = 0.0f;
	inverseTransposeTransform.M[3][1] = 0.0f;
	inverseTransposeTransform.M[3][2] = 0.0f;
	inverseTransposeTransform.M[3][3] = 1.0f;

	for ( int i = 0; i < vertices.GetSizeI(); i++ )
	{
		vertices[i].position = transform.Transform( vertices[i].position );
		vertices[i].normal = VectorNormalize( inverseTransposeTransform.Transform( vertices[i].normal ) );
		vertices[i].tangent = VectorNormalize( inverseTransposeTransform.Transform( vertices[i].tangent ) );
		vertices[i].binormal = VectorNormalize( inverseTransposeTransform.Transform( vertices[i].binormal ) );
	}

	for ( int i = 0; i < joints.GetSizeI(); i++ )
	{
		joints[i].transform = transform * joints[i].transform;
	}

	for ( int i = 0; i < surfaces.GetSizeI(); i++ )
	{
		surfaces[i].transform = transform * surfaces[i].transform;
	}

	for ( int i = 0; i < tags.GetSizeI(); i++ )
	{
		tags[i].matrix = transform * tags[i].matrix;
	}

	if ( transform.Determinant() < 0.0f )
	{
		for ( int i = 0; i < triangles.GetSizeI(); i++ )
		{
			Alg::Swap( triangles[i].verts[1], triangles[i].verts[2] );
		}
	}

	HashVertices();
}

void RawModel::TransformTextures( const Matrix3f & transform )
{
	for ( int i = 0; i < vertices.GetSizeI(); i++ )
	{
		vertices[i].uv0 = transform.Transform( vertices[i].uv0 );
		vertices[i].uv1 = transform.Transform( vertices[i].uv1 );
	}

	HashVertices();
}

String StripNumbers( const char * name )
{
	int length = (int)strlen( name );
	for ( ; length > 0 && name[length - 1] == ' '; length-- ) {}
	for ( int i = length - 1; i > 4; i-- )
	{
		if ( name[i] != ')' ) { break; }
		for ( i--; isdigit( name[i] ); i-- ) {}
		if ( name[i] != '(' ) { break; }
		for ( i--; name[i] == ' '; i-- ) {}
		length = i + 1;
	}
	return String( name, length );
}

void RawModel::StripModoNumbers()
{
	for ( int i = 0; i < surfaces.GetSizeI(); i++ )
	{
		const String newName = StripNumbers( surfaces[i].name );

		bool exists = false;
		for ( int j = 0; j < i; j++ )
		{
			if ( surfaces[j].name.CompareNoCase( newName ) == 0 )
			{
				exists = true;
				break;
			}
		}
		if ( exists )
		{
			Warning( "-stripModoNumbers: keeping '%s' because '%s' already exists\n", surfaces[i].name.ToCStr(), newName.ToCStr() );
			continue;
		}

		surfaces[i].name = newName;
	}
}

struct UniqueTexture
{
	UniqueTexture() :
		data( NULL ),
		width( 0 ),
		height( 0 ),
		components( 0 ) {}

	unsigned char *			data;
	int						width;
	int						height;
	int						components;
};

struct UniqueMaterial
{
	UniqueMaterial() :
		materialIndex( -1 ),
		newMaterialIndex( -1 ),
		repeat( 1.0f ),
		repeatSurfaceIndex( -1 ) {}

	int				materialIndex;
	int				newMaterialIndex;
	UniqueTexture	textures[RAW_TEXTURE_USAGE_MAX];
	Vector2f		repeat;
	int				repeatSurfaceIndex;
};

const float uvEpsilon = 0.01f;

void RawModel::GetTriangleMinMaxUv0( const int triangleIndex, Vector2f & minUv0, Vector2f & maxUv0 )
{
	minUv0 = Vector2f( FLT_MAX );
	maxUv0 = Vector2f( -FLT_MAX );

	for ( int j = 0; j < 3; j++ )
	{
		const RawVertex & v = vertices[triangles[triangleIndex].verts[j]];
		minUv0 = Vector2f::Min( minUv0, v.uv0 );
		maxUv0 = Vector2f::Max( maxUv0, v.uv0 );
	}
}

bool RawModel::RepeatTextures( const char * textureFolder )
{
	Array< UniqueMaterial > unique;

	// Get all the unique materials.
	for ( int i = 0; i < triangles.GetSizeI(); i++ )
	{
		if ( triangles[i].materialIndex < 0 )
		{
			continue;
		}

		int uniqueIndex = -1;
		for ( int j = 0; j < unique.GetSizeI(); j++ )
		{
			if ( unique[j].materialIndex == triangles[i].materialIndex )
			{
				uniqueIndex = j;
				break;
			}
		}
		if ( uniqueIndex == -1 )
		{
			uniqueIndex = (int)unique.AllocBack();
			unique[uniqueIndex].materialIndex = triangles[i].materialIndex;
		}

		Vector2f minUv0( FLT_MAX );
		Vector2f maxUv0( -FLT_MAX );
		GetTriangleMinMaxUv0( i, minUv0, maxUv0 );

		// Make the texture coordinates positive.
		const Vector2f uv0Bias( - floorf( minUv0.x ), - floorf( minUv0.y ) );
		minUv0 += uv0Bias;
		maxUv0 += uv0Bias;

		// If the texture usage is not in the [0,1] range.
		if ( !(	minUv0.x >= 0.0f - uvEpsilon &&
				minUv0.y >= 0.0f - uvEpsilon &&
				maxUv0.x <= 1.0f + uvEpsilon &&
				maxUv0.y <= 1.0f + uvEpsilon ) )
		{

			const Vector2f repeat( ceilf( maxUv0.x - uvEpsilon ), ceilf( maxUv0.y - uvEpsilon ) );

			if ( unique[uniqueIndex].repeatSurfaceIndex == -1 && ( repeat.x > 2.0f || repeat.y > 2.0f ) )
			{
				unique[uniqueIndex].repeatSurfaceIndex = triangles[i].surfaceIndex;
			}

			unique[uniqueIndex].repeat.x = Alg::Max( unique[uniqueIndex].repeat.x, repeat.x );
			unique[uniqueIndex].repeat.y = Alg::Max( unique[uniqueIndex].repeat.y, repeat.y );
		}
	}

	for ( int i = 0; i < unique.GetSizeI(); i++ )
	{
		const RawMaterial & mat = materials[unique[i].materialIndex];

		if ( unique[i].repeatSurfaceIndex != -1 )
		{
			Warning( "single triangle from surface '%s' repeats texture '%s': %1.0f, %1.0f\n",
						surfaces[unique[i].repeatSurfaceIndex].name.ToCStr(),
						StringUtils::GetFileNameString( textures[mat.textures[0]].name ).ToCStr(),
						unique[i].repeat.x, unique[i].repeat.y );
		}

		// Clamp the repeat.
		unique[i].repeat.x = Alg::Min( unique[i].repeat.x, 2.0f );
		unique[i].repeat.y = Alg::Min( unique[i].repeat.y, 2.0f );

		const int repeatX = (int) unique[i].repeat.x;
		const int repeatY = (int) unique[i].repeat.y;

		if ( repeatX <= 1 || repeatY <= 1 )
		{
			continue;
		}

		int newMaterialTextures[RAW_TEXTURE_USAGE_MAX] = { 0 };
		newMaterialTextures[RAW_TEXTURE_USAGE_EMISSIVE] = mat.textures[RAW_TEXTURE_USAGE_EMISSIVE];

		for ( int j = 0; j < RAW_TEXTURE_USAGE_MAX; j++ )
		{
			newMaterialTextures[j] = -1;

			if ( j == RAW_TEXTURE_USAGE_EMISSIVE || mat.textures[j] == -1 )
			{
				continue;
			}

			unique[i].textures[j].data = stbi_load( textures[mat.textures[j]].name,
													& unique[i].textures[j].width,
													& unique[i].textures[j].height,
													& unique[i].textures[j].components, 4 );
			if ( unique[i].textures[j].data == NULL )
			{
				Error( "failed to load '%s'\n", (const char*)textures[mat.textures[j]].name );
				return false;
			}

			const int oldWidth = unique[i].textures[j].width;
			const int oldHeight = unique[i].textures[j].height;
			unsigned char * oldData = unique[i].textures[j].data;

			const int newWidth = oldWidth * repeatX;
			const int newHeight = oldHeight * repeatY;
			unsigned char * newData = (unsigned char *) malloc( newWidth * newHeight * 4 );

			for ( int dstsY = 0; dstsY < newHeight; dstsY++ )
			{
				const int srcY = dstsY % oldHeight;
				for ( int dstX = 0; dstX < newWidth; dstX++ )
				{
					const int srcX = dstX % oldWidth;
					for ( int channel = 0; channel < 4; channel++ )
					{
						newData[( dstsY * newWidth + dstX ) * 4 + channel] = oldData[( srcY * oldWidth + srcX ) * 4 + channel];
					}
				}
			}

			printf( "creating %dx%d '%s' (repeat %d,%d)\n", newWidth, newHeight,
					StringUtils::GetFileNameString( textures[mat.textures[j]].name ).ToCStr(), repeatX, repeatY );

			const char * usageStr = "unknown";
			switch ( j )
			{
				case RAW_TEXTURE_USAGE_DIFFUSE:		usageStr = "diffuse"; break;
				case RAW_TEXTURE_USAGE_NORMAL:		usageStr = "normal"; break;
				case RAW_TEXTURE_USAGE_SPECULAR:	usageStr = "specular"; break;
				case RAW_TEXTURE_USAGE_EMISSIVE:	usageStr = "emissive"; break;
				case RAW_TEXTURE_USAGE_REFLECTION:	usageStr = "reflection"; break;
			}

			const String uniqueFileName = (const char *)StringUtils::Va( "%s%s_%dx%d_%s.tga",
							textureFolder, StringUtils::GetFileBaseString( textures[mat.textures[j]].name ).ToCStr(),
							newWidth, newHeight, usageStr );
			if ( stbi_write_tga( uniqueFileName, newWidth, newHeight, 4, newData ) == 0 )
			{
				Error( "failed to write '%s'\n", uniqueFileName.ToCStr() );
				return false;
			}

			// free the txture data
			free( newData );
			free( unique[i].textures[j].data );
			unique[i].textures[j].data = NULL;

			// Add the unique texture.
			newMaterialTextures[j] = AddTexture( uniqueFileName, (RawTextureUsage) j );
		}

		// Add the unique textures as a new material.
		unique[i].newMaterialIndex = AddMaterial( RAW_MATERIAL_TYPE_OPAQUE, newMaterialTextures );
	}

	// Replace the triangle materials.
	for ( int i = 0; i < triangles.GetSizeI(); i++ )
	{
		if ( triangles[i].materialIndex < 0 )
		{
			continue;
		}

		int uniqueIndex = -1;
		for ( int j = 0; j < unique.GetSizeI(); j++ )
		{
			if ( unique[j].materialIndex == triangles[i].materialIndex )
			{
				uniqueIndex = j;
				break;
			}
		}
		if ( uniqueIndex == -1 )
		{
			assert( false );
			continue;
		}

		if ( unique[uniqueIndex].newMaterialIndex == -1 )
		{
			continue;
		}

		triangles[i].materialIndex = unique[uniqueIndex].newMaterialIndex;

		Vector2f minUv0( FLT_MAX );
		Vector2f maxUv0( -FLT_MAX );
		GetTriangleMinMaxUv0( i, minUv0, maxUv0 );

		if ( !(	minUv0.x >= 0.0f - uvEpsilon &&
				minUv0.y >= 0.0f - uvEpsilon &&
				maxUv0.x <= 1.0f + uvEpsilon &&
				maxUv0.y <= 1.0f + uvEpsilon ) )
		{
			const Vector2f uv0Bias( - floorf( minUv0.x ), - floorf( minUv0.y ) );
			const Vector2f uv0Scale( 1.0f / unique[uniqueIndex].repeat.x, 1.0f / unique[uniqueIndex].repeat.y );

			for ( int j = 0; j < 3; j++ )
			{
				RawVertex v = vertices[triangles[i].verts[j]];
				v.uv0 = ( v.uv0 + uv0Bias ) * uv0Scale;
				triangles[i].verts[j] = AddVertex( v );
			}
		}
	}

	return true;
}

struct AtlasMaterial
{
	AtlasMaterial() :
		materialIndex( -1 ),
		minUv0( FLT_MAX ),
		maxUv0( -FLT_MAX ) {}

	int				materialIndex;
	Vector2f		minUv0;
	Vector2f		maxUv0;
	Array< int >	surfaces;
};

struct AtlasTexture
{
	AtlasTexture() :
		textureIndex( -1 ),
		occlusion( RAW_TEXTURE_OCCLUSION_OPAQUE ),
		data( NULL ),
		width( 0 ),
		height( 0 ),
		components( 0 ),
		x( 0 ),
		y( 0 ),
		scale( 1.0f ),
		bias( 0.0f ) {}

	int						textureIndex;
	String					name;
	RawTextureOcclusion		occlusion;
	unsigned char *			data;
	int						width;
	int						height;
	int						components;
	int						x;
	int						y;
	Vector2f				scale;
	Vector2f				bias;
};

struct AtlasTextureSort
{
    static bool Compare( const AtlasTexture & a, const AtlasTexture & b )
    {
		return Alg::Max( a.width, a.height ) > Alg::Max( b.width, b.height );
	}
};

int NextPOT( const int value )
{
	for ( int i = 1; i < 0x7FFFFFFF; i <<= 1 )
	{
		if ( i >= value )
		{
			return i;
		}
	}
	return value;
}

float		Frac( const float v ) { return v - floor( v ); }
Vector2f	Frac( const Vector2f & v ) { return Vector2f( Frac( v.x ), Frac( v.y ) ); }
Vector3f	Frac( const Vector3f & v ) { return Vector3f( Frac( v.x ), Frac( v.y ), Frac( v.z ) ); }
Vector4f	Frac( const Vector4f & v ) { return Vector4f( Frac( v.x ), Frac( v.y ), Frac( v.z ), Frac( v.w ) ); }

bool RawModel::CreateTextureAtlases( const char * textureFolder, const bool repeatTextures )
{
	if ( repeatTextures )
	{
		RepeatTextures( textureFolder );
	}

	Array< AtlasMaterial > atlasMaterials[MAX_ATLASES];
	int numAtlases = 0;

	// Get the materials for each atlas.
	for ( int i = 0; i < triangles.GetSizeI(); i++ )
	{
		if ( triangles[i].materialIndex < 0 )
		{
			continue;
		}

		const int atlas = surfaces[triangles[i].surfaceIndex].atlas;
		if ( atlas < 0 )
		{
			continue;
		}
		if ( atlas >= numAtlases )
		{
			numAtlases = atlas + 1;
		}

		UPInt index = -1;
		for ( int j = 0; j < atlasMaterials[atlas].GetSizeI(); j++ )
		{
			if ( atlasMaterials[atlas][j].materialIndex == triangles[i].materialIndex )
			{
				index = j;
				break;
			}
		}
		if ( index == -1 )
		{
			index = atlasMaterials[atlas].AllocBack();
			atlasMaterials[atlas][index].materialIndex = triangles[i].materialIndex;
			atlasMaterials[atlas][index].surfaces.PushBack( triangles[i].surfaceIndex );
		}
		else
		{
			for ( int j = atlasMaterials[atlas][index].surfaces.GetSizeI(); j >= 0; j-- )
			{
				if ( j == 0 )
				{
					atlasMaterials[atlas][index].surfaces.PushBack( triangles[i].surfaceIndex );
				}
				else if ( atlasMaterials[atlas][index].surfaces[j - 1] == triangles[i].surfaceIndex )
				{
					break;
				}
			}
		}

		for ( int j = 0; j < 3; j++ )
		{
			const RawVertex & v = vertices[triangles[i].verts[j]];
			atlasMaterials[atlas][index].minUv0 = Vector2f::Min( atlasMaterials[atlas][index].minUv0, v.uv0 );
			atlasMaterials[atlas][index].maxUv0 = Vector2f::Max( atlasMaterials[atlas][index].maxUv0, v.uv0 );
		}
	}

	Array< AtlasTexture > atlasTextures[MAX_ATLASES][RAW_TEXTURE_USAGE_MAX];
	int newAtlasMaterials[MAX_ATLASES] = { -1 };

	// Create each atlas.
	for ( int atlas = 0; atlas < numAtlases; atlas++ )
	{
		// Get the textures for each atlas.
		for ( int i = 0; i < atlasMaterials[atlas].GetSizeI(); i++ )
		{
			const AtlasMaterial & am = atlasMaterials[atlas][i];
			const RawMaterial & mat = materials[am.materialIndex];
			for ( int usage = 0; usage < RAW_TEXTURE_USAGE_MAX; usage++ )
			{
				if ( mat.textures[usage] < 0 )
				{
					continue;
				}

				bool found = false;
				for ( int j = 0; j < atlasTextures[atlas][usage].GetSizeI(); j++ )
				{
					if ( mat.textures[usage] == atlasTextures[atlas][usage][j].textureIndex )
					{
						found = true;
						break;
					}
				}
				if ( found )
				{
					continue;
				}

				const UPInt t = atlasTextures[atlas][usage].AllocBack();
				atlasTextures[atlas][usage][t].textureIndex = mat.textures[usage];
				atlasTextures[atlas][usage][t].name = textures[mat.textures[usage]].name;
				atlasTextures[atlas][usage][t].occlusion = textures[mat.textures[usage]].occlusion;

				// Print a warning for surfaces that use tiled textures.
				if ( usage == RAW_TEXTURE_USAGE_DIFFUSE )
				{
					if (	am.minUv0.x < 0.0f - uvEpsilon || am.minUv0.x > 1.0f + uvEpsilon ||
							am.minUv0.y < 0.0f - uvEpsilon || am.minUv0.y > 1.0f + uvEpsilon )
					{
						for ( int n = 0; n < am.surfaces.GetSizeI(); n++ )
						{
							Warning( "surface '%s' tiles '%s' over [%3.1f,%3.1f]-[%3.1f,%3.1f]\n",
										surfaces[am.surfaces[n]].name.ToCStr(),
										StringUtils::GetFileNameString( textures[mat.textures[usage]].name ).ToCStr(),
										am.minUv0.x, am.minUv0.y, am.maxUv0.x, am.maxUv0.y );
						}
					}
				}
			}
		}

		if ( atlasTextures[atlas][RAW_TEXTURE_USAGE_NORMAL].GetSizeI() != 0 &&
				atlasTextures[atlas][RAW_TEXTURE_USAGE_NORMAL].GetSizeI() != atlasTextures[atlas][RAW_TEXTURE_USAGE_DIFFUSE].GetSizeI() )
		{
			Error( "diffuse and normal texture mismatch for atlas %d\n", atlas );
			return false;
		}
		if ( atlasTextures[atlas][RAW_TEXTURE_USAGE_SPECULAR].GetSizeI() != 0 &&
				atlasTextures[atlas][RAW_TEXTURE_USAGE_SPECULAR].GetSizeI() != atlasTextures[atlas][RAW_TEXTURE_USAGE_DIFFUSE].GetSizeI() )
		{
			Error( "diffuse and specular texture mismatch for atlas %d\n", atlas );
			return false;
		}
		if ( atlasTextures[atlas][RAW_TEXTURE_USAGE_REFLECTION].GetSizeI() > 1 )
		{
			Error( "not all materials for atlas %d use the same reflection cube map\n", atlas );
			return false;
		}

		int newAtlasTextures[RAW_TEXTURE_USAGE_MAX];
		RawTextureOcclusion atlasTextureOcclusion = RAW_TEXTURE_OCCLUSION_OPAQUE;

		// Create one atlas texture at a time.
		for ( int usage = 0; usage < RAW_TEXTURE_USAGE_MAX; usage++ )
		{
			if ( atlasTextures[atlas][usage].GetSizeI() <= 0 )
			{
				newAtlasTextures[usage] = -1;
				continue;
			}

			// All reflection cube maps must be the same.
			if ( usage == RAW_TEXTURE_USAGE_REFLECTION )
			{
				newAtlasTextures[usage] = ( atlasTextures[atlas][usage].GetSizeI() > 0 ) ? atlasTextures[atlas][usage][0].textureIndex : -1;
				continue;
			}

			// Load the textures.
			for ( int i = 0; i < atlasTextures[atlas][usage].GetSizeI(); i++ )
			{
				atlasTextures[atlas][usage][i].data = stbi_load( atlasTextures[atlas][usage][i].name,
																& atlasTextures[atlas][usage][i].width,
																& atlasTextures[atlas][usage][i].height,
																& atlasTextures[atlas][usage][i].components, 4 );
				if ( atlasTextures[atlas][usage][i].data == NULL )
				{
					Error( "failed to load '%s'\n", (const char *)atlasTextures[atlas][usage][i].name );
					return false;
				}

				if ( textures[atlasTextures[atlas][usage][i].textureIndex].occlusion > atlasTextureOcclusion )
				{
					atlasTextureOcclusion = textures[atlasTextures[atlas][usage][i].textureIndex].occlusion;
				}
			}

			// Sort the textures based on largest axis.
			Alg::QuickSort( atlasTextures[atlas][usage], AtlasTextureSort::Compare );

			// The first texture is placed at the origin.
			int totalWidth = atlasTextures[atlas][usage][0].width;
			int totalHeight = atlasTextures[atlas][usage][0].height;

			// Place the other textures within the atlas.
			for ( int i = 1; i < atlasTextures[atlas][usage].GetSizeI(); i++ )
			{
				int bestWidth = MAX_ATLAS_SIZE + 1;
				int bestHeight = MAX_ATLAS_SIZE + 1;

				atlasTextures[atlas][usage][i].x = -1;
				atlasTextures[atlas][usage][i].y = -1;

				for ( int j = 0; j < i; j++ )
				{
					// Test every corner of the texture already in the atlas.
					for ( int yj = 0; yj <= atlasTextures[atlas][usage][j].height; yj += atlasTextures[atlas][usage][j].height )
					{
						for ( int xj = 0; xj <= atlasTextures[atlas][usage][j].width; xj += atlasTextures[atlas][usage][j].width )
						{
							// Test every corner of the new texture to be added to the atlas.
							for ( int yi = 0; yi <= atlasTextures[atlas][usage][i].height; yi += atlasTextures[atlas][usage][i].height )
							{
								for ( int xi = 0; xi <= atlasTextures[atlas][usage][i].width; xi += atlasTextures[atlas][usage][i].width )
								{
									// Test if the new texture can be placed at this corner combination.
									const int x = atlasTextures[atlas][usage][j].x + xj - xi;
									const int y = atlasTextures[atlas][usage][j].y + yj - yi;
									if ( x < 0 || y < 0 )
									{
										continue;
									}

									// Test if the new texture at this position overlaps with an existing texture in the atlas.
									bool overlap = false;
									for ( int n = 0; n < i; n++ )
									{
										if (	x < atlasTextures[atlas][usage][n].x + atlasTextures[atlas][usage][n].width &&
												x + atlasTextures[atlas][usage][i].width > atlasTextures[atlas][usage][n].x &&
												y < atlasTextures[atlas][usage][n].y + atlasTextures[atlas][usage][n].height &&
												y + atlasTextures[atlas][usage][i].height > atlasTextures[atlas][usage][n].y )
										{
											overlap = true;
											break;
										}
									}
									if ( overlap )
									{
										continue;
									}

									// If this results in a smaller POT atlas size.
									const int newWidth = Alg::Max( totalWidth, NextPOT( x + atlasTextures[atlas][usage][i].width ) );
									const int newHeight = Alg::Max( totalHeight, NextPOT( y + atlasTextures[atlas][usage][i].height ) );
									if ( newWidth * newHeight < bestWidth * bestHeight )
									{
										bestWidth = newWidth;
										bestHeight = newHeight;
										atlasTextures[atlas][usage][i].x = x;
										atlasTextures[atlas][usage][i].y = y;
									}
								}
							}
						}
					}
				}

				if ( atlasTextures[atlas][usage][i].x == -1 || atlasTextures[atlas][usage][i].y == -1 )
				{
					Error( "texture '%s' (%dx%d) could not be placed in atlas %d (%dx%d)\n",
							atlasTextures[atlas][usage][i].name.ToCStr(),
							atlasTextures[atlas][usage][i].width, atlasTextures[atlas][usage][i].height,
							atlas, totalWidth, totalHeight );
					return false;
				}

				totalWidth = Alg::Max( totalWidth, NextPOT( atlasTextures[atlas][usage][i].x + atlasTextures[atlas][usage][i].width ) );
				totalHeight = Alg::Max( totalHeight, NextPOT( atlasTextures[atlas][usage][i].y + atlasTextures[atlas][usage][i].height ) );
			}

			// Create the atlas texture.
			unsigned char * atlasData = (unsigned char *) malloc( totalWidth * totalHeight * 4 * sizeof( unsigned char ) );
			memset( atlasData, 0, totalWidth * totalHeight * 4 * sizeof( unsigned char ) );
			for ( int i = 0; i < atlasTextures[atlas][usage].GetSizeI(); i++ )
			{
				for ( int y = 0; y < atlasTextures[atlas][usage][i].height; y++ )
				{
					memcpy( atlasData + ( ( atlasTextures[atlas][usage][i].y + y ) * totalWidth + atlasTextures[atlas][usage][i].x ) * 4 * sizeof( unsigned char ),
							atlasTextures[atlas][usage][i].data + y * atlasTextures[atlas][usage][i].width * 4 * sizeof( unsigned char ),
							atlasTextures[atlas][usage][i].width * 4 * sizeof( unsigned char ) );
				}
			}

			// Pack the data to just three components if the whole atlas is opaque.
			int components = 4;
			if ( 0 ) //atlasTextureOcclusion != RAW_TEXTURE_OCCLUSION_OPAQUE )
			{
				for ( int y = 0; y < totalHeight; y++ )
				{
					for ( int x = 0; x < totalWidth; x++ )
					{
						atlasData[( y * totalWidth + x ) * 3 + 0] = atlasData[( y + totalWidth + x ) * 4 + 0];
						atlasData[( y * totalWidth + x ) * 3 + 1] = atlasData[( y + totalWidth + x ) * 4 + 1];
						atlasData[( y * totalWidth + x ) * 3 + 2] = atlasData[( y + totalWidth + x ) * 4 + 2];
					}
				}
				components = 3;
			}

			const char * usageStr = "unknown";
			switch ( usage )
			{
				case RAW_TEXTURE_USAGE_DIFFUSE:		usageStr = "diffuse"; break;
				case RAW_TEXTURE_USAGE_NORMAL:		usageStr = "normal"; break;
				case RAW_TEXTURE_USAGE_SPECULAR:	usageStr = "specular"; break;
				case RAW_TEXTURE_USAGE_EMISSIVE:	usageStr = "emissive"; break;
				case RAW_TEXTURE_USAGE_REFLECTION:	assert( false ); break;
			}

			// Write out the atlas texture.
			const String atlasFileName = (const char *)StringUtils::Va( "%satlas%d_%s.tga", textureFolder, atlas, usageStr );
			printf( "writing %dx%d '%s'\n", totalWidth, totalHeight, atlasFileName.ToCStr() );
			if ( stbi_write_tga( atlasFileName, totalWidth, totalHeight, components, atlasData ) == 0 )
			{
				Error( "failed to write '%s'\n", atlasFileName.ToCStr() );
				return false;
			}

			// Add the atlas as a new texture.
			newAtlasTextures[usage] = AddTexture( atlasFileName, (RawTextureUsage) usage );

			// Free the original textures and calculate a scale and bias to transform the texture coordinates.
			for ( int i = 0; i < atlasTextures[atlas][usage].GetSizeI(); i++ )
			{
				free( atlasTextures[atlas][usage][i].data );
				atlasTextures[atlas][usage][i].data = NULL;

				atlasTextures[atlas][usage][i].scale.x = (float) atlasTextures[atlas][usage][i].width / totalWidth;
				atlasTextures[atlas][usage][i].scale.y = (float) atlasTextures[atlas][usage][i].height / totalHeight;
				atlasTextures[atlas][usage][i].bias.x = (float) atlasTextures[atlas][usage][i].x / totalWidth;
				atlasTextures[atlas][usage][i].bias.y = (float) atlasTextures[atlas][usage][i].y / totalHeight;
			}
		}

		// Create new atlas material.
		newAtlasMaterials[atlas] = AddMaterial( RAW_MATERIAL_TYPE_OPAQUE, newAtlasTextures );
	}

	// Change the triangles to use the new atlas materials.
	for ( int i = 0; i < triangles.GetSizeI(); i++ )
	{
		if ( triangles[i].materialIndex < 0 )
		{
			continue;
		}

		const int atlas = surfaces[triangles[i].surfaceIndex].atlas;
		if ( atlas < 0 )
		{
			continue;
		}

		const RawMaterial & mat = materials[triangles[i].materialIndex];

		Vector2f scale0( 1.0f );
		Vector2f bias0( 0.0f );

		for ( int j = 0; j < atlasTextures[atlas][RAW_TEXTURE_USAGE_DIFFUSE].GetSizeI(); j++ )
		{
			if ( mat.textures[RAW_TEXTURE_USAGE_DIFFUSE] == atlasTextures[atlas][RAW_TEXTURE_USAGE_DIFFUSE][j].textureIndex )
			{
				scale0 = atlasTextures[atlas][RAW_TEXTURE_USAGE_DIFFUSE][j].scale;
				bias0 = atlasTextures[atlas][RAW_TEXTURE_USAGE_DIFFUSE][j].bias;
				break;
			}
		}

		Vector2f scale1( 1.0f );
		Vector2f bias1( 0.0f );

		for ( int j = 0; j < atlasTextures[atlas][RAW_TEXTURE_USAGE_EMISSIVE].GetSizeI(); j++ )
		{
			if ( mat.textures[RAW_TEXTURE_USAGE_EMISSIVE] == atlasTextures[atlas][RAW_TEXTURE_USAGE_EMISSIVE][j].textureIndex )
			{
				scale1 = atlasTextures[atlas][RAW_TEXTURE_USAGE_EMISSIVE][j].scale;
				bias1 = atlasTextures[atlas][RAW_TEXTURE_USAGE_EMISSIVE][j].bias;
				break;
			}
		}

		for ( int j = 0; j < 3; j++ )
		{
			RawVertex v = vertices[triangles[i].verts[j]];
			v.uv0 = v.uv0 * scale0 + bias0;
			v.uv1 = v.uv1 * scale1 + bias1;
			triangles[i].verts[j] = AddVertex( v );
		}

		triangles[i].materialIndex = newAtlasMaterials[atlas];
	}

	return true;
}

void RawModel::SkinRigidSurfaces()
{
	Array< RawVertex > oldVertices = vertices;

	vertexHash.Clear();
	vertices.Clear();

	bool skinned = false;

	for ( int i = 0; i < triangles.GetSizeI(); i++ )
	{
		for ( int j = 0; j < 3; j++ )
		{
			RawVertex vertex = oldVertices[triangles[i].verts[j]];

			if ( surfaces[triangles[i].surfaceIndex].skinRigid )
			{
				vertex.jointIndices[0] = AddJoint( surfaces[triangles[i].surfaceIndex].name, surfaces[triangles[i].surfaceIndex].transform );
				vertex.jointIndices[1] = 0;
				vertex.jointIndices[2] = 0;
				vertex.jointIndices[3] = 0;
				vertex.jointWeights[0] = 1.0f;
				vertex.jointWeights[1] = 0.0f;
				vertex.jointWeights[2] = 0.0f;
				vertex.jointWeights[3] = 0.0f;
				skinned = true;
			}

			triangles[i].verts[j] = AddVertex( vertex );
		}
	}

	if ( skinned )
	{
		AddVertexAttribute( RAW_VERTEX_ATTRIBUTE_JOINT_WEIGHTS );
		AddVertexAttribute( RAW_VERTEX_ATTRIBUTE_JOINT_INDICES );
	}
}

bool RawModel::SetJointAnimation( const char * jointName, const RawJointAnimation animation, const Vector3f & parameters, const float timeOffset, const float timeScale )
{
	for ( int i = 0; i < joints.GetSizeI(); i++ )
	{
		if ( joints[i].name.CompareNoCase( jointName ) == 0 )
		{
			joints[i].animation = animation;
			joints[i].parameters = parameters;
			joints[i].timeOffset = timeOffset;
			joints[i].timeScale = timeScale;
			return true;
		}
	}
	return false;
}

template< int axis >
struct VertexAxisSortPositive
{
    static bool Compare( const RawVertex & a, const RawVertex & b )
    {
		return a.position[axis] < b.position[axis];
	}
};

template< int axis >
struct VertexAxisSortNegative
{
    static bool Compare( const RawVertex & a, const RawVertex & b )
    {
		return a.position[axis] > b.position[axis];
	}
};

struct VertexOriginSortNeg
{
    static bool Compare( const RawVertex & a, const RawVertex & b )
    {
		return a.position.LengthSq() > b.position.LengthSq();
	}
};

struct VertexOriginSortPos
{
    static bool Compare( const RawVertex & a, const RawVertex & b )
    {
		return a.position.LengthSq() < b.position.LengthSq();
	}
};

struct TriangleVertexSort
{
    static bool Compare( const RawTriangle & a, const RawTriangle & b )
    {
		if ( a.verts[0] != b.verts[0] )
		{
			return a.verts[0] < b.verts[0];
		}
		return Alg::Min( a.verts[1], a.verts[2] ) < Alg::Min( b.verts[1], b.verts[2] );
	}
};

void RawModel::SortGeometry( const int axis )
{
	Array< RawVertex > oldVertices = vertices;

	// Sort the vertices.
	switch ( axis )
	{
		case SORT_ORIGIN_NEG:	Alg::QuickSort( vertices, VertexOriginSortNeg::Compare ); break;
		case SORT_AXIS_NEG_Z:	Alg::QuickSort( vertices, VertexAxisSortNegative< 2 >::Compare ); break;
		case SORT_AXIS_NEG_Y:	Alg::QuickSort( vertices, VertexAxisSortNegative< 1 >::Compare ); break;
		case SORT_AXIS_NEG_X:	Alg::QuickSort( vertices, VertexAxisSortNegative< 0 >::Compare ); break;
		case SORT_AXIS_POS_X:	Alg::QuickSort( vertices, VertexAxisSortPositive< 0 >::Compare ); break;
		case SORT_AXIS_POS_Y:	Alg::QuickSort( vertices, VertexAxisSortPositive< 1 >::Compare ); break;
		case SORT_AXIS_POS_Z:	Alg::QuickSort( vertices, VertexAxisSortPositive< 2 >::Compare ); break;
		case SORT_ORIGIN_POS:	Alg::QuickSort( vertices, VertexOriginSortPos::Compare ); break;
		default: return;
	}

	printf( "sorting...\n" );

	// Add the sorted vertices to the hash;
	vertexHash.Clear();
	for ( int i = 0; i < vertices.GetSizeI(); i++ )
	{
		vertexHash.Add( vertices[i], i );
	}

	// Remap the triangle vertices.
	for ( int i = 0; i < triangles.GetSizeI(); i++ )
	{
		for ( int j = 0; j < 3; j++ )
		{
			triangles[i].verts[j] = AddVertex( oldVertices[triangles[i].verts[j]] );
		}
	}

	// Sort the triangle vertices such that the vertex with the smallest index comes first
	for ( int i = 0; i < triangles.GetSizeI(); i++ )
	{
		const int minVert =	( triangles[i].verts[0] < triangles[i].verts[1] ) ?
							( triangles[i].verts[0] < triangles[i].verts[2] ? 0 : 2 ) :
							( triangles[i].verts[1] < triangles[i].verts[2] ? 1 : 2 );

		const int v0 = triangles[i].verts[( minVert + 0 ) % 3];
		const int v1 = triangles[i].verts[( minVert + 1 ) % 3];
		const int v2 = triangles[i].verts[( minVert + 2 ) % 3];

		triangles[i].verts[0] = v0;
		triangles[i].verts[1] = v1;
		triangles[i].verts[2] = v2;
	}


	// Sort the triangles based on the first vertex index of each triangle.
	Alg::QuickSort( triangles, TriangleVertexSort::Compare );

	// Re-add the materials in sorted triangle order.
	Array< RawMaterial > oldMaterials = materials;
	materials.Clear();
	for ( int i = 0; i < triangles.GetSizeI(); i++ )
	{
		const RawMaterial & material = oldMaterials[triangles[i].materialIndex];
		const int materialIndex = AddMaterial( material.type, material.textures );
		materials[materialIndex] = material;
		triangles[i].materialIndex = materialIndex;
	}

	// Re-add the surfaces in sorted triangle order.
	Array< RawSurface > oldSurfaces = surfaces;
	surfaces.Clear();
	for ( int i = 0; i < triangles.GetSizeI(); i++ )
	{
		const RawSurface & surface = oldSurfaces[triangles[i].surfaceIndex];
		const int surfaceIndex = AddSurface( surface.name, surface.transform );
		surfaces[surfaceIndex] = surface;
		triangles[i].surfaceIndex = surfaceIndex;
	}
}

void RawModel::ForceOpaque()
{
	for ( int i = 0; i < textures.GetSizeI(); i++ )
	{
		textures[i].occlusion = RAW_TEXTURE_OCCLUSION_OPAQUE;
	}
	for ( int i = 0; i < materials.GetSizeI(); i++ )
	{
		if ( materials[i].type == RAW_MATERIAL_TYPE_PERFORATED ||
				materials[i].type == RAW_MATERIAL_TYPE_TRANSPARENT )
		{
			materials[i].type = RAW_MATERIAL_TYPE_OPAQUE;
		}
	}
}

/*
	Geometric Tools, LLC
	Copyright (c) 1998-2011
	Code originally written by David Eberly.
	Published in "Geometric Tools for Computer Graphics".
	Distributed under the Boost Software License, Version 1.0.
	http://www.boost.org/LICENSE_1_0.txt
	http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
*/
static float ClosestPointOnTriangle( const Vector3f & v0, const Vector3f & v1, const Vector3f & v2,
										const Vector3f & point, float bary[3] )
{
	const Vector3f diff = v0 - point;
	const Vector3f edge0 = v1 - v0;
	const Vector3f edge1 = v2 - v0;
	const float a00 = edge0.LengthSq();
	const float a01 = edge0.Dot( edge1 );
	const float a11 = edge1.LengthSq();
	const float b0 = diff.Dot( edge0 );
	const float b1 = diff.Dot( edge1 );
	const float c = diff.LengthSq();
	const float det = fabs( a00 * a11 - a01 * a01 );
	float s = a01 * b1 - a11 * b0;
	float t = a01 * b0 - a00 * b1;
	float sqrDistance;

	if ( s + t <= det )
	{
		if ( s < 0.0f )
		{
			if ( t < 0.0f )  // region 4
			{
				if ( b0 < 0.0f )
				{
					t = 0.0f;
					if ( -b0 >= a00 )
					{
						s = 1.0f;
						sqrDistance = a00 + 2.0f * b0 + c;
					}
					else
					{
						s = -b0 / a00;
						sqrDistance = b0 * s + c;
					}
				}
				else
				{
					s = 0.0f;
					if ( b1 >= 0.0f )
					{
						t = 0.0f;
						sqrDistance = c;
					}
					else if (-b1 >= a11)
					{
						t = 1.0f;
						sqrDistance = a11 + 2.0f * b1 + c;
					}
					else
					{
						t = -b1 / a11;
						sqrDistance = b1 * t + c;
					}
				}
			}
			else  // region 3
			{
				s = 0.0f;
				if ( b1 >= 0.0f )
				{
					t = 0.0f;
					sqrDistance = c;
				}
				else if ( -b1 >= a11 )
				{
					t = 1.0f;
					sqrDistance = a11 + 2.0f * b1 + c;
				}
				else
				{
					t = -b1 / a11;
					sqrDistance = b1 * t + c;
				}
			}
		}
		else if ( t < 0.0f )  // region 5
		{
			t = 0.0f;
			if ( b0 >= 0.0f )
			{
				s = 0.0f;
				sqrDistance = c;
			}
			else if ( -b0 >= a00 )
			{
				s = 1.0f;
				sqrDistance = a00 + 2.0f * b0 + c;
			}
			else
			{
				s = -b0 / a00;
				sqrDistance = b0 * s + c;
			}
		}
		else  // region 0
		{
			// minimum at interior point
			float invDet = 1.0f / det;
			s *= invDet;
			t *= invDet;
			sqrDistance = s * ( a00 * s + a01 * t + 2.0f * b0 ) +
				t * ( a01 * s + a11 * t + 2.0f * b1 ) + c;
		}
	}
	else
	{
		if ( s < 0.0f )  // region 2
		{
			const float tmp0 = a01 + b0;
			const float tmp1 = a11 + b1;
			if ( tmp1 > tmp0 )
			{
				const float numer = tmp1 - tmp0;
				const float denom = a00 - 2.0f * a01 + a11;
				if ( numer >= denom )
				{
					s = 1.0f;
					t = 0.0f;
					sqrDistance = a00 + 2.0f * b0 + c;
				}
				else
				{
					s = numer / denom;
					t = 1.0f - s;
					sqrDistance = s * ( a00 * s + a01 * t + 2.0f * b0 ) +
						t * ( a01 * s + a11 * t + 2.0f * b1 ) + c;
				}
			}
			else
			{
				s = 0.0f;
				if ( tmp1 <= 0.0f )
				{
					t = 1.0f;
					sqrDistance = a11 + 2.0f * b1 + c;
				}
				else if ( b1 >= 0.0f )
				{
					t = 0.0f;
					sqrDistance = c;
				}
				else
				{
					t = -b1 / a11;
					sqrDistance = b1 * t + c;
				}
			}
		}
		else if ( t < 0.0f )  // region 6
		{
			const float tmp0 = a01 + b1;
			const float tmp1 = a00 + b0;
			if ( tmp1 > tmp0 )
			{
				const float numer = tmp1 - tmp0;
				const float denom = a00 - 2.0f * a01 + a11;
				if ( numer >= denom )
				{
					t = 1.0f;
					s = 0.0f;
					sqrDistance = a11 + 2.0f * b1 + c;
				}
				else
				{
					t = numer / denom;
					s = 1.0f - t;
					sqrDistance = s * ( a00 * s + a01 * t + 2.0f * b0 ) +
						t * ( a01 * s + a11 * t + 2.0f * b1 ) + c;
				}
			}
			else
			{
				t = 0.0f;
				if ( tmp1 <= 0.0f )
				{
					s = 1.0f;
					sqrDistance = a00 + 2.0f * b0 + c;
				}
				else if ( b0 >= 0.0f )
				{
					s = 0.0f;
					sqrDistance = c;
				}
				else
				{
					s = -b0 / a00;
					sqrDistance = b0 * s + c;
				}
			}
		}
		else  // region 1
		{
			const float numer = a11 + b1 - a01 - b0;
			if ( numer <= 0.0f )
			{
				s = 0.0f;
				t = 1.0f;
				sqrDistance = a11 + 2.0f * b1 + c;
			}
			else
			{
				const float denom = a00 - 2.0f * a01 + a11;
				if ( numer >= denom )
				{
					s = 1.0f;
					t = 0.0f;
					sqrDistance = a00 + 2.0f * b0 + c;
				}
				else
				{
					s = numer / denom;
					t = 1.0f - s;
					sqrDistance = s * ( a00 * s + a01 * t + 2.0f * b0 ) +
						t * ( a01 * s + a11 * t + 2.0f * b1 ) + c;
				}
			}
		}
	}

	// Account for numerical round-off error.
	if ( sqrDistance < 0.0f )
	{
		sqrDistance = 0.0f;
	}

	bary[0] = 1.0f - s - t;
	bary[1] = s;
	bary[2] = t;

	return sqrDistance;
}

/*
	Fast, Minimum Storage Ray/Triangle Intersection
	Tomas Möller, Ben Trumbore
	Journal of Graphics Tools, 1997

	Returns true if the triangle is not back-face culled.
	'bary' will hold the barycentric coordinates of the intersection.
*/
static bool RayTriangle( const Vector3f & rayStart, const Vector3f & rayDir,
						const Vector3f & v0, const Vector3f & v1, const Vector3f & v2,
						float bary[3] )
{
	assert( rayDir.IsNormalized() );

	const Vector3f edge1 = v1 - v0;
	const Vector3f edge2 = v2 - v0;

	const Vector3f tv = rayStart - v0;
	const Vector3f pv = rayDir.Cross( edge2 );
	const Vector3f qv = tv.Cross( edge1 );
	const float det = edge1.Dot( pv );

	// If the determinant is negative then the triangle is backfacing.
	if ( det < Math<float>::SmallestNonDenormal )
	{
		return false;
	}

	const float rcpDet = 1.0f / det;
	const float s = tv.Dot( pv ) * rcpDet;
	const float t = rayDir.Dot( qv ) * rcpDet;

	bary[0] = 1.0f - s - t;
	bary[1] = s;
	bary[2] = t;

	return true;
}

static float CalculateTriangleTextureLOD( const Vector3f & v0, const Vector3f & v1, const Vector3f & v2,
					const Vector2f & t0, const Vector2f & t1, const Vector2f & t2,
					const Vector3f & viewOrigin, const Vector2f & fov, const Vector2i & screenResolution,
					const Vector2i & textureResolution, const float mipLevels, const float maxAniso )
{
	float bary[3];
	ClosestPointOnTriangle( v0, v1, v2, viewOrigin, bary );

	const Vector3f posCenter = v0 * bary[0] + v1 * bary[1] + v2 * bary[2];
	const Vector3f dirCenter = ( posCenter - viewOrigin ).Normalized();
	const Matrix4f axis( Quatf( Vector3f( 0, 0, -1 ), dirCenter ) );
	const Vector3f dirRight = ( dirCenter + axis.GetXBasis() * ( Sinf( DegreeToRad( fov.x ) ) / screenResolution.x ) ).Normalized();
	const Vector3f dirUp = ( dirCenter + axis.GetYBasis() * ( Sinf( DegreeToRad( fov.y ) ) / screenResolution.y ) ).Normalized();

	float bary0[3];
	if ( !RayTriangle( viewOrigin, dirCenter, v0, v1, v2, bary0 ) )
	{
		return mipLevels;
	}
	float bary1[3];
	if ( !RayTriangle( viewOrigin, dirRight, v0, v1, v2, bary1 ) )
	{
		return mipLevels;
	}
	float bary2[3];
	if ( !RayTriangle( viewOrigin, dirUp, v0, v1, v2, bary2 ) )
	{
		return mipLevels;
	}

	const Vector2f texCenter = t0 * bary0[0] + t1 * bary0[1] + t2 * bary0[2];
	const Vector2f texRight = t0 * bary1[0] + t1 * bary1[1] + t2 * bary1[2];
	const Vector2f texUp = t0 * bary2[0] + t1 * bary2[1] + t2 * bary2[2];

	const Vector2f ddx( ( texRight.x - texCenter.x ) * textureResolution.x, ( texRight.y - texCenter.y ) * textureResolution.y );
	const Vector2f ddy( ( texUp.x - texCenter.x ) * textureResolution.x, ( texUp.y - texCenter.y ) * textureResolution.y );

	const float maxAnisoLog2 = Log2f( maxAniso );
	const float px = ddx.Dot( ddx );
	const float py = ddy.Dot( ddy );
	const float maxLod = 0.5f * Log2f( MaxFloat( px, py ) ); // log2(sqrt()) = 0.5f * log2()
	const float minLod = 0.5f * Log2f( MinFloat( px, py ) );
	const float anisoLOD = maxLod - MinFloat( maxLod - minLod, maxAnisoLog2 );

	return anisoLOD;
}

void RawModel::TexLod( const Vector3f & viewOrigin, const Vector2f & fov, const Vector2i & screenResolution, const float maxAniso )
{
	for ( int i = 0; i < vertices.GetSizeI(); i++ )
	{
		vertices[i].color.x = 1.0f;
		vertices[i].color.y = 0.0f;
		vertices[i].color.z = 0.0f;
		vertices[i].color.w = 1.0f;
	}

	for ( int i = 0; i < triangles.GetSizeI(); i++ )
	{
		if ( triangles[i].materialIndex == -1 )
		{
			continue;
		}

		const int diffuseIndex = materials[triangles[i].materialIndex].textures[RAW_TEXTURE_USAGE_DIFFUSE];
		if ( diffuseIndex == -1 )
		{
			continue;
		}
		const Vector2i textureDimensions( textures[diffuseIndex].width, textures[diffuseIndex].height );
		const float mipLevels = (float)textures[diffuseIndex].mipLevels;

		const float lod = CalculateTriangleTextureLOD(
												vertices[triangles[i].verts[0]].position,
												vertices[triangles[i].verts[1]].position,
												vertices[triangles[i].verts[2]].position,
												vertices[triangles[i].verts[0]].uv0,
												vertices[triangles[i].verts[1]].uv0,
												vertices[triangles[i].verts[2]].uv0,
												viewOrigin, fov, screenResolution,
												textureDimensions, mipLevels, maxAniso );

		const float color = Alg::Min( lod * 0.25f, 1.0f );
		for ( int j = 0; j < 3; j++ )
		{
			vertices[triangles[i].verts[j]].color.x = Alg::Min( vertices[triangles[i].verts[j]].color.x, color );
		}
	}

	for ( int i = 0; i < vertices.GetSizeI(); i++ )
	{
		vertices[i].color.y = 1.0f - vertices[i].color.x;
	}

	vertexAttributes |= RAW_VERTEX_ATTRIBUTE_COLOR;

	for ( int i = 0; i < materials.GetSizeI(); i++ )
	{
		for ( int j = 0; j < RAW_TEXTURE_USAGE_MAX; j++ )
		{
			materials[i].textures[j] = -1;
		}
	}

	textures.Clear();
}

void RawModel::RenderOrder( const int color )
{
	const Vector4f colorTable[6][2] =
	{
		{ Vector4f( 1, 0, 0, 1 ), Vector4f( 0, 1, 0, 1 ) },
		{ Vector4f( 1, 0, 0, 1 ), Vector4f( 0, 0, 1, 1 ) },
		{ Vector4f( 1, 0, 0, 1 ), Vector4f( 0, 1, 1, 1 ) },
		{ Vector4f( 0, 0, 1, 1 ), Vector4f( 0, 1, 0, 1 ) },
		{ Vector4f( 0, 0, 1, 1 ), Vector4f( 1, 0, 0, 1 ) },
		{ Vector4f( 0, 0, 1, 1 ), Vector4f( 1, 1, 0, 1 ) }
	};

	const Vector4f * blendColors = colorTable[color % 6];

	for ( int i = 0; i < vertices.GetSizeI(); i++ )
	{
		const float blend = (float) i / ( vertices.GetSizeI() - 1 );
		vertices[i].color = blendColors[0] * blend + blendColors[1] * ( 1.0f - blend );
	}

	vertexAttributes |= RAW_VERTEX_ATTRIBUTE_COLOR;

	for ( int i = 0; i < materials.GetSizeI(); i++ )
	{
		for ( int j = 0; j < RAW_TEXTURE_USAGE_MAX; j++ )
		{
			materials[i].textures[j] = -1;
		}
	}

	textures.Clear();
}

Bounds3f RawModel::GetBounds() const
{
	Bounds3f bounds;
	bounds.Clear();
	for ( int i = 0; i < vertices.GetSizeI(); i++ )
	{
		bounds.AddPoint( vertices[i].position );
	}
	return bounds;
}

void RawModel::GetIndexArray( Array< uint32_t > & outIndices, const int materialIndex, const int surfaceIndex ) const
{
	int triangleCount = 0;
	for ( int i = 0; i < triangles.GetSizeI(); i++ )
	{
		if (	( materialIndex == -1 || triangles[i].materialIndex == materialIndex ) &&
				( surfaceIndex == -1 || triangles[i].surfaceIndex == surfaceIndex ) )
		{
			triangleCount++;
		}
	}
	outIndices.Resize( triangleCount * 3 );
	triangleCount = 0;
	for ( int i = 0; i < triangles.GetSizeI(); i++ )
	{
		if (	( materialIndex == -1 || triangles[i].materialIndex == materialIndex ) &&
				( surfaceIndex == -1 || triangles[i].surfaceIndex == surfaceIndex ) )
		{
			for ( int j = 0; j < 3; j++ )
			{
				outIndices[triangleCount * 3 + j] = triangles[i].verts[j];
			}
			triangleCount++;
		}
	}
}

struct PositionUv
{
	bool operator==( const PositionUv & other ) const
	{
		return ( position == other.position ) && ( uv == other.uv );
	}

	Vector3f	position;
	Vector2f	uv;
};

void RawModel::GetIndexedVertices( Array< Vector3f > & outPositions, Array< Vector2f > & outUvs, Array< int > & outIndices ) const
{
	Hash< PositionUv, int > positionHash;

	outIndices.Resize( triangles.GetSize() * 3 );
	for ( int i = 0; i < triangles.GetSizeI(); i++ )
	{
		for ( int j = 0; j < 3; j++ )
		{
			const PositionUv pu = { vertices[triangles[i].verts[j]].position, vertices[triangles[i].verts[j]].uv0 };

			int index = -1;
			if ( positionHash.Get( pu, & index ) )
			{
				outIndices[i * 3 + j] = index;
			}
			else
			{
				outIndices[i * 3 + j] = outPositions.GetSizeI();
				positionHash.Add( pu, outPositions.GetSizeI() );
				outPositions.PushBack( pu.position );
				outUvs.PushBack( pu.uv );
			}
		}
	}
}

struct TriangleModelSort
{
    static bool Compare( const RawTriangle & a, const RawTriangle & b )
    {
		if ( a.materialIndex != b.materialIndex )
		{
			return a.materialIndex < b.materialIndex;
		}
		if ( a.surfaceIndex != b.surfaceIndex )
		{
			return a.surfaceIndex < b.surfaceIndex;
		}
        return a.verts[0] < b.verts[0];
    }
};

void RawModel::CreateMaterialModels( Array< RawModel > & materialModels, const int maxModelVertices, const int keepAttribs ) const
{
	// Sort all triangles based on material first, then surface, then first vertex index.
	Array< RawTriangle > sortedTriangles = triangles;
	Alg::QuickSort( sortedTriangles, TriangleModelSort::Compare );

	// Overestimate the number of models that will be created to avoid massive reallocation.
	int discreteCount = 0;
	for ( int i = 0; i < surfaces.GetSizeI(); i++ )
	{
		discreteCount += ( surfaces[i].discrete != false );
	}

	materialModels.Clear();
	materialModels.Reserve( materials.GetSizeI() + discreteCount );

	const RawVertex defaultVertex;

	// Create a separate model for each material.
	RawModel * model;
	for ( int i = 0; i < sortedTriangles.GetSizeI(); i++ )
	{
		if ( sortedTriangles[i].materialIndex < 0 || sortedTriangles[i].surfaceIndex < 0 )
		{
			continue;
		}

		if ( i == 0 ||
				model->GetVertexCount() > maxModelVertices - 3 ||
				sortedTriangles[i].materialIndex != sortedTriangles[i - 1].materialIndex ||
				( sortedTriangles[i].surfaceIndex != sortedTriangles[i - 1].surfaceIndex &&
					( surfaces[sortedTriangles[i].surfaceIndex].discrete ||
					surfaces[sortedTriangles[i - 1].surfaceIndex].discrete ) ) )
		{
			materialModels.Resize( materialModels.GetSize() + 1 );
			model = &materialModels[materialModels.GetSize() - 1];
		}

		const int materialIndex = model->AddMaterial( materials[sortedTriangles[i].materialIndex].type, materials[sortedTriangles[i].materialIndex].textures );
		const int surfaceIndex = model->AddSurface( surfaces[sortedTriangles[i].surfaceIndex].name, surfaces[sortedTriangles[i].surfaceIndex].transform );

		int verts[3];
		for ( int j = 0; j < 3; j++ )
		{
			RawVertex vertex = vertices[sortedTriangles[i].verts[j]];
			if ( keepAttribs != -1 )
			{
				int keep = keepAttribs;
				if ( ( keepAttribs & RAW_VERTEX_ATTRIBUTE_POSITION ) != 0 )
				{
					keep |= RAW_VERTEX_ATTRIBUTE_JOINT_INDICES | RAW_VERTEX_ATTRIBUTE_JOINT_WEIGHTS;
				}
				if ( ( keepAttribs & RAW_VERTEX_ATTRIBUTE_AUTO ) != 0 )
				{
					keep |= RAW_VERTEX_ATTRIBUTE_POSITION;

					const RawMaterial & mat = model->GetMaterial( materialIndex );
					if ( mat.textures[RAW_TEXTURE_USAGE_DIFFUSE] != -1 )
					{
						keep |= RAW_VERTEX_ATTRIBUTE_UV0;
					}
					if ( mat.textures[RAW_TEXTURE_USAGE_NORMAL] != -1 )
					{
						keep |= RAW_VERTEX_ATTRIBUTE_NORMAL |
								RAW_VERTEX_ATTRIBUTE_TANGENT |
								RAW_VERTEX_ATTRIBUTE_BINORMAL |
								RAW_VERTEX_ATTRIBUTE_UV0;
					}
					if ( mat.textures[RAW_TEXTURE_USAGE_SPECULAR] != -1 )
					{
						keep |= RAW_VERTEX_ATTRIBUTE_NORMAL |
								RAW_VERTEX_ATTRIBUTE_UV0;
					}
					if ( mat.textures[RAW_TEXTURE_USAGE_EMISSIVE] != -1 )
					{
						keep |= RAW_VERTEX_ATTRIBUTE_UV1;
					}
				}
				if ( ( keep & RAW_VERTEX_ATTRIBUTE_POSITION			) == 0 ) { vertex.position = defaultVertex.position; }
				if ( ( keep & RAW_VERTEX_ATTRIBUTE_NORMAL			) == 0 ) { vertex.normal = defaultVertex.normal; }
				if ( ( keep & RAW_VERTEX_ATTRIBUTE_TANGENT			) == 0 ) { vertex.tangent = defaultVertex.tangent; }
				if ( ( keep & RAW_VERTEX_ATTRIBUTE_BINORMAL			) == 0 ) { vertex.binormal = defaultVertex.binormal; }
				if ( ( keep & RAW_VERTEX_ATTRIBUTE_COLOR			) == 0 ) { vertex.color = defaultVertex.color; }
				if ( ( keep & RAW_VERTEX_ATTRIBUTE_UV0				) == 0 ) { vertex.uv0 = defaultVertex.uv0; }
				if ( ( keep & RAW_VERTEX_ATTRIBUTE_UV1				) == 0 ) { vertex.uv1 = defaultVertex.uv1; }
				if ( ( keep & RAW_VERTEX_ATTRIBUTE_JOINT_INDICES	) == 0 ) { vertex.jointIndices = defaultVertex.jointIndices; }
				if ( ( keep & RAW_VERTEX_ATTRIBUTE_JOINT_WEIGHTS	) == 0 ) { vertex.jointWeights = defaultVertex.jointWeights; }
			}
			verts[j] = model->AddVertex( vertex );
			model->vertexAttributes |= vertex.Difference( defaultVertex );
		}

		model->AddTriangle( verts[0], verts[1], verts[2], materialIndex, surfaceIndex );
	}

	// Normal maps are in local (tangent) space so calculate the
	// normal, binormal and tangent vectors if they are not present.
	for ( int i = 0; i < materialModels.GetSizeI(); i++ )
	{
		if ( materialModels[i].GetMaterial( 0 ).textures[RAW_TEXTURE_USAGE_NORMAL] == -1 )
		{
			continue;
		}
		if ( ( materialModels[i].vertexAttributes & RAW_VERTEX_ATTRIBUTE_NORMAL ) == 0 )
		{
			materialModels[i].CalculateNormals();
		}
		if ( ( materialModels[i].vertexAttributes & RAW_VERTEX_ATTRIBUTE_TANGENT ) == 0 )
		{
			materialModels[i].CalculateTangents();
		}
		if ( ( materialModels[i].vertexAttributes & RAW_VERTEX_ATTRIBUTE_BINORMAL ) == 0 )
		{
			materialModels[i].CalculateBinormals();
		}
	}
}

void RawModel::HashVertices()
{
	vertexHash.Clear();
	for ( int i = 0; i < vertices.GetSizeI(); i++ )
	{
		vertexHash.Add( vertices[i], i );
	}
}
