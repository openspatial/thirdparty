/************************************************************************************

Filename    :   RawModel.h
Content     :   Raw model.
Created     :   May, 2014
Authors     :   J.M.P. van Waveren

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/
#ifndef __RAWMODEL_H__
#define __RAWMODEL_H__

const int MAX_ATLASES		= 128;
const int MAX_ATLAS_SIZE	= 16384;

enum RawVertexAttribute
{
	RAW_VERTEX_ATTRIBUTE_POSITION		= 1 << 0,
	RAW_VERTEX_ATTRIBUTE_NORMAL			= 1 << 1,
	RAW_VERTEX_ATTRIBUTE_TANGENT		= 1 << 2,
	RAW_VERTEX_ATTRIBUTE_BINORMAL		= 1 << 3,
	RAW_VERTEX_ATTRIBUTE_COLOR			= 1 << 4,
	RAW_VERTEX_ATTRIBUTE_UV0			= 1 << 5,
	RAW_VERTEX_ATTRIBUTE_UV1			= 1 << 6,
	RAW_VERTEX_ATTRIBUTE_JOINT_INDICES	= 1 << 7,
	RAW_VERTEX_ATTRIBUTE_JOINT_WEIGHTS	= 1 << 8,

	RAW_VERTEX_ATTRIBUTE_AUTO			= 1 << 31
};

struct RawVertex
{
	RawVertex() :
		polarityUv0( false ),
		pad1( false ),
		pad2( false ),
		pad3( false )
	{
	}

	Vector3f				position;
	Vector3f				normal;
	Vector3f				tangent;
	Vector3f				binormal;
	Vector4f				color;
	Vector2f				uv0;
	Vector2f				uv1;
	Vector4i				jointIndices;
	Vector4f				jointWeights;

	bool					polarityUv0;
	bool					pad1;
	bool					pad2;
	bool					pad3;

	bool					operator==( const RawVertex & other ) const;
	int						Difference( const RawVertex & other ) const;
};

enum RawTextureUsage
{
	RAW_TEXTURE_USAGE_DIFFUSE,
	RAW_TEXTURE_USAGE_NORMAL,
	RAW_TEXTURE_USAGE_SPECULAR,
	RAW_TEXTURE_USAGE_EMISSIVE,
	RAW_TEXTURE_USAGE_REFLECTION,
	RAW_TEXTURE_USAGE_MAX
};

enum RawTextureOcclusion
{
	RAW_TEXTURE_OCCLUSION_OPAQUE,
	RAW_TEXTURE_OCCLUSION_PERFORATED,
	RAW_TEXTURE_OCCLUSION_TRANSPARENT
};

struct RawTexture
{
	String					name;
	int						width;
	int						height;
	int						mipLevels;
	RawTextureUsage			usage;
	RawTextureOcclusion		occlusion;
};

enum RawMaterialType
{
	RAW_MATERIAL_TYPE_OPAQUE,
	RAW_MATERIAL_TYPE_PERFORATED,
	RAW_MATERIAL_TYPE_TRANSPARENT,
	RAW_MATERIAL_TYPE_ADDITIVE
};

struct RawMaterial
{
	RawMaterialType			type;
	int						textures[RAW_TEXTURE_USAGE_MAX];
};

struct RawSurface
{
	String					name;
	Matrix4f				transform;
	int						atlas;
	bool					discrete;
	bool					skinRigid;
};

enum RawJointAnimation
{
	RAW_JOINT_ANIMATION_NONE,
	RAW_JOINT_ANIMATION_ROTATE,
	RAW_JOINT_ANIMATION_SWAY,
	RAW_JOINT_ANIMATION_BOB
};

struct RawJoint
{
	String					name;
	Matrix4f				transform;
	RawJointAnimation		animation;
	Vector3f				parameters;
	float					timeOffset;
	float					timeScale;
};

struct RawTriangle
{
	int						verts[3];
	int						materialIndex;
	int						surfaceIndex;
};

struct RawTag
{
	String					name;
	Matrix4f				matrix;
	Vector4i				jointIndices;
	Vector4f				jointWeights;
};

enum
{
	SORT_ORIGIN_NEG		= -4,
	SORT_AXIS_NEG_Z		= -3,
	SORT_AXIS_NEG_Y		= -2,
	SORT_AXIS_NEG_X		= -1,
	SORT_AXIS_NONE		= 0,
	SORT_AXIS_POS_X		= 1,
	SORT_AXIS_POS_Y		= 2,
	SORT_AXIS_POS_Z		= 3,
	SORT_ORIGIN_POS		= 4
};

class RawModel
{
public:
						RawModel() : vertexAttributes( 0 ) {}

	// Add geometry.
	void				AddVertexAttribute( const RawVertexAttribute attrib );
	int					AddVertex( const RawVertex & vertex );
	int					AddTexture( const char * name, const RawTextureUsage usage );
	int					AddMaterial( const RawMaterialType materialType, const int textures[RAW_TEXTURE_USAGE_MAX] );
	int					AddSurface( const char * name, const Matrix4f & transform );
	int					AddJoint( const char * name, const Matrix4f & transform );
	int					AddTriangle( const int v0, const int v1, const int v2, const int materialIndex, const int surfaceIndex );
	int					AddTag( const int surfaceIndex );

	// Remove geometry.
	void				RemoveVertexAttribute( const RawVertexAttribute removeAttribs );
	void				RemoveTexture( const int textureIndex );	// remove texture and any references to this texture
	void				RemoveMaterial( const int materialIndex );	// remove material and any triangle using the material
	void				RemoveSurface( const int surfaceIndex );	// remove surface and any triangle part of the surface
	int					RemoveDuplicateTriangles();

	// Remove unused vertices, textures or materials after removing vertex attributes, textures, materials or surfaces.
	void				Condense();

	// Calculate normal, tangent, binormal vectors
	void				CalculateNormals();
	void				CalculateTangents();
	void				CalculateBinormals();

	// Transform the geometry or the texture space.
	void				TransformGeometry( const Matrix4f & transform );
	void				TransformTextures( const Matrix3f & transform );

	// Set surface flags.
	void				SetSurfaceAtlas( const int surfaceIndex, const int atlas ) { surfaces[surfaceIndex].atlas = atlas; }
	void				SetSurfaceDiscrete( const int surfaceIndex, const bool discrete ) { surfaces[surfaceIndex].discrete = discrete; }
	void				SetSurfaceSkinned( const int surfaceIndex, const bool rigid ) { surfaces[surfaceIndex].skinRigid = rigid; }

	// Strip the numbers of the form " (?)" Modo adds to names if the Modo source data has duplicates.
	void				StripModoNumbers();

	// Create texture atlases.
	bool				CreateTextureAtlases( const char * textureFolder, const bool repeatTextures );

	// Skin each surface that is set rigid with SetSurfaceSkinned() such that it is rigidly attached to a joint.
	void				SkinRigidSurfaces();

	// Set a simple joint animation.
	bool				SetJointAnimation( const char * jointName, const RawJointAnimation animation, const Vector3f & parameters, const float timeOffset, const float timeScale );

	// Sort the geometry along one of the coordinate axes or from the origin.
	void				SortGeometry( const int axis );

	// Force all textures to opaque.
	void				ForceOpaque();

	// Set vertex colors to represent finest visible texture LOD (green = finest, red = coarsest).
	void				TexLod( const Vector3f & viewOrigin, const Vector2f & fov, const Vector2i & screenResolution, const float maxAniso );

	// Set vertex colors based on render order.
	void				RenderOrder( const int color );

	// Get an axis aligned bounding box containing all geometry.
	Bounds3f			GetBounds() const;

	// Get the attributes stored per vertex.
	int					GetVertexAttributes() const { return vertexAttributes; }

	// Iterate over the vertices.
	int					GetVertexCount() const { return vertices.GetSizeI(); }
	const RawVertex &	GetVertex( const int index ) const { return vertices[index]; }

	// Iterate over the textures.
	int					GetTextureCount() const { return textures.GetSizeI(); }
	const RawTexture &	GetTexture( const int index ) const { return textures[index]; }

	// Iterate over the materials.
	int					GetMaterialCount() const { return materials.GetSizeI(); }
	const RawMaterial &	GetMaterial( const int index ) const { return materials[index]; }

	// Iterate over the surfaces.
	int					GetSurfaceCount() const { return surfaces.GetSizeI(); }
	const RawSurface &	GetSurface( const int index ) const { return surfaces[index]; }

	// Iterate over the surfaces.
	int					GetJointCount() const { return joints.GetSizeI(); }
	const RawJoint &	GetJoint( const int index ) const { return joints[index]; }

	// Iterate over the triangles.
	int					GetTriangleCount() const { return triangles.GetSizeI(); }
	const RawTriangle &	GetTriangle( const int index ) const { return triangles[index]; }

	// Iterate over the tags.
	int					GetTagCount() const { return tags.GetSizeI(); }
	const RawTag &		GetTag( const int index ) const { return tags[index]; }

	// Create individual attribute arrays.
	// Returns true if the vertices store the particular attribute.
	template< typename _attrib_type_, _attrib_type_ RawVertex::*attribute >
	void				GetAttributeArray( Array< _attrib_type_ > & out ) const;

	// Create a triangle index that references the unique vertices with all their attributes.
	// If 'materialIndex' != -1 then only those triangles are included that use the given material.
	// If 'surfaceIndex' != -1 then only those triangles are included that are part of the given surface.
	void				GetIndexArray( Array< uint32_t > & outIndices, const int materialIndex = -1, const int surfaceIndex = -1 ) const;

	// This creates a new triangle index that references unique vertex positions with uvs.
	void				GetIndexedVertices( Array< Vector3f > & outVertices, Array< Vector2f > & outUvs, Array< int > & outIndices ) const;

	// Create an array with a raw model for each material.
	// Multiple surfaces with the same material will turn into a single model.
	// However, surfaces that are marked as 'discrete' will turn into separate models.
	void				CreateMaterialModels( Array< RawModel > & materialModels, const int maxModelVertices, const int keepAttribs ) const;

private:
	int						vertexAttributes;
	Hash< RawVertex, int >	vertexHash;
	Array< RawVertex >		vertices;
	Array< RawTexture >		textures;
	Array< RawMaterial >	materials;
	Array< RawSurface >		surfaces;
	Array< RawJoint >		joints;
	Array< RawTriangle >	triangles;
	Array< RawTag >			tags;

	void HashVertices();
	void GetTriangleMinMaxUv0( const int triangleIndex, Vector2f & minUv0, Vector2f &maxUv0 );
	bool RepeatTextures( const char * textureFolder );
};

template< typename _attrib_type_, _attrib_type_ RawVertex::*attribute >
inline void RawModel::GetAttributeArray( Array< _attrib_type_ > & out ) const
{
	out.Resize( vertices.GetSizeI() );
	for ( int i = 0; i < vertices.GetSizeI(); i++ )
	{
		out[i] = vertices[i].*attribute;
	}
}

#endif // !__RAWMODEL_H__
