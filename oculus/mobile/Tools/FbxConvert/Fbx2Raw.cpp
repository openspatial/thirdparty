/************************************************************************************

Filename    :   Fbx2Raw.cpp
Content     :   Load and convert an FBX to a raw model.
Created     :   May, 2014
Authors     :   J.M.P. van Waveren

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#include <stdint.h>
#include <stdio.h>
#include <fbxsdk.h>

#define FBX_TOOL
#include "../../VRLib/jni/LibOVR/Include/OVR.h"
#include "../../VRLib/jni/LibOVR/Src/Kernel/OVR_Hash.h"
#include "../../VRLib/jni/LibOVR/Src/Kernel/OVR_String.h"
#include "../../VRLib/jni/LibOVR/Src/Kernel/OVR_String_Utils.h"

#include "File_Utils.h"

using namespace OVR;

#include "RawModel.h"
#include "Fbx2Raw.h"

template< typename _type_ >
class FbxLayerElementAccess
{
public:

	FbxLayerElementAccess( const FbxLayerElementTemplate< _type_ > * layer, int count ) :
		mappingMode( FbxGeometryElement::eNone ),
		elements( NULL ),
		indices( NULL )
	{
		if ( count <= 0 || layer == NULL )
		{
			return;
		}
		const FbxGeometryElement::EMappingMode newMappingMode = layer->GetMappingMode();
		if (	newMappingMode == FbxGeometryElement::eByControlPoint ||
				newMappingMode == FbxGeometryElement::eByPolygonVertex ||
				newMappingMode == FbxGeometryElement::eByPolygon )
		{
			mappingMode = newMappingMode;
			elements = &layer->GetDirectArray();
			indices = ( layer->GetReferenceMode() == FbxGeometryElement::eIndexToDirect ||
						layer->GetReferenceMode() == FbxGeometryElement::eIndex ) ? &layer->GetIndexArray() : NULL;
		}
	}

	bool LayerPresent() const
	{
		return ( mappingMode != FbxGeometryElement::eNone );
	}

	_type_ GetElement( const int polygonIndex, const int polygonVertexIndex, const int controlPointIndex, const _type_ defaultValue ) const
	{
		if ( mappingMode != FbxGeometryElement::eNone )
		{
			int index = ( mappingMode == FbxGeometryElement::eByControlPoint ) ? controlPointIndex :
						( ( mappingMode == FbxGeometryElement::eByPolygonVertex ) ? polygonVertexIndex : polygonIndex );
			index = ( indices != NULL ) ? (*indices)[index] : index;
			_type_ element = elements->GetAt( index );
			return element;
		}
		return defaultValue;
	}

	_type_ GetElement( const int polygonIndex, const int polygonVertexIndex, const int controlPointIndex, const _type_ defaultValue, const FbxMatrix & transform, const bool normalize ) const
	{
		if ( mappingMode != FbxGeometryElement::eNone )
		{
			_type_ element = transform.MultNormalize( GetElement( polygonIndex, polygonVertexIndex, controlPointIndex, defaultValue ) );
			if ( normalize )
			{
				element.Normalize();
			}
			return element;
		}
		return defaultValue;
	}

private:
	FbxGeometryElement::EMappingMode				mappingMode;
	const FbxLayerElementArrayTemplate< _type_ > *	elements;
	const FbxLayerElementArrayTemplate< int > *		indices;
};

class FbxMaterialAccess
{
public:

	FbxMaterialAccess( const FbxMesh * pMesh ) :
		mappingMode( FbxGeometryElement::eNone ),
		mesh( NULL ),
		indices( NULL )
	{
		if ( pMesh->GetElementMaterialCount() <= 0 )
		{
			return;
		}

		const FbxGeometryElement::EMappingMode materialMappingMode = pMesh->GetElementMaterial()->GetMappingMode();
		if ( materialMappingMode != FbxGeometryElement::eByPolygon && materialMappingMode != FbxGeometryElement::eAllSame )
		{
			return;
		}

		const FbxGeometryElement::EReferenceMode materialReferenceMode = pMesh->GetElementMaterial()->GetReferenceMode();
		if ( materialReferenceMode != FbxGeometryElement::eIndexToDirect )
		{
			return;
		}

		mappingMode = materialMappingMode;
		mesh = pMesh;
		indices = &pMesh->GetElementMaterial()->GetIndexArray();
	}

	const FbxSurfaceMaterial * GetMaterial( const int polygonIndex ) const
	{
		if ( mappingMode != FbxGeometryElement::eNone )
		{
			const int materialIndex = ( mappingMode == FbxGeometryElement::eByPolygon ) ? polygonIndex : 0;
			const FbxSurfaceMaterial * pMaterial = mesh->GetNode()->GetSrcObject<FbxSurfaceMaterial>( indices->GetAt( materialIndex ) );
			return pMaterial;
		}
		return NULL;
	}

private:
	FbxGeometryElement::EMappingMode			mappingMode;
	const FbxMesh *								mesh;
	const FbxLayerElementArrayTemplate< int > * indices;
};

class FbxSkinningAccess
{
public:

	static const int MAX_WEIGHTS = 4;

	FbxSkinningAccess( const FbxMesh * pMesh )
	{
		const FbxSkin * skin = ( pMesh->GetDeformerCount( FbxDeformer::eSkin ) > 0 ) ? static_cast<const FbxSkin *>( pMesh->GetDeformer( 0, FbxDeformer::eSkin ) ) : NULL;
		if ( skin != NULL )
		{
			const int controlPointCount = pMesh->GetControlPointsCount();

			vertexJointIndices.Resize( controlPointCount );
			vertexJointWeights.Resize( controlPointCount );

			const int clusterCount = skin->GetClusterCount();
			for ( int clusterIndex = 0; clusterIndex < clusterCount; clusterIndex++ )
			{
				const FbxCluster * cluster = skin->GetCluster( clusterIndex );
				const int indexCount = cluster->GetControlPointIndicesCount();
				const int * clusterIndices = cluster->GetControlPointIndices();
				const double * clusterWeights = cluster->GetControlPointWeights();
				for ( int i = 0; i < indexCount; i++ )
				{
					if ( clusterIndices[i] < 0 || clusterIndices[i] >= controlPointCount )
					{
						continue;
					}
					if ( clusterWeights[i] <= vertexJointWeights[clusterIndices[i]][MAX_WEIGHTS - 1] )
					{
						continue;
					}
					vertexJointIndices[clusterIndices[i]][MAX_WEIGHTS - 1] = clusterIndex;
					vertexJointWeights[clusterIndices[i]][MAX_WEIGHTS - 1] = (float)clusterWeights[i];
					for ( int j = MAX_WEIGHTS - 1; j > 0; j-- )
					{
						if ( vertexJointWeights[clusterIndices[i]][j - 1] >= vertexJointWeights[clusterIndices[i]][j] )
						{
							break;
						}
						Alg::Swap( vertexJointIndices[clusterIndices[i]][j - 1], vertexJointIndices[clusterIndices[i]][j] );
						Alg::Swap( vertexJointWeights[clusterIndices[i]][j - 1], vertexJointWeights[clusterIndices[i]][j] );
					}
				}
				jointNames.PushBack( cluster->GetName() );
			}

			for ( int i = 0; i < controlPointCount; i++ )
			{
				vertexJointWeights[i] = vertexJointWeights[i].Normalized();
			}
		}
	}

	bool IsSkinned() const
	{
		return vertexJointWeights.GetSize() > 0;
	}

	const char * GetJointName( const int jointIndex ) const
	{
		return jointNames[jointIndex].ToCStr();
	}

	const Vector4i GetVertexIndices( const int controlPointIndex ) const
	{
		return ( vertexJointIndices.GetSize() > 0 ) ? vertexJointIndices[controlPointIndex] : Vector4i( 0 );
	}

	const Vector4f GetVertexWeights( const int controlPointIndex ) const
	{
		return ( vertexJointWeights.GetSize() > 0 ) ? vertexJointWeights[controlPointIndex] : Vector4f( 0.0f );
	}

private:
	Array< String >		jointNames;
	Array< Vector4i >	vertexJointIndices;
	Array< Vector4f >	vertexJointWeights;
};

static bool TriangleTexturePolarity( const Vector2f & uv0, const Vector2f & uv1, const Vector2f & uv2 )
{
	const Vector2f d0 = uv1 - uv0;
	const Vector2f d1 = uv2 - uv0;

	return ( d0.x * d1.y - d0.y * d1.x < 0.0f );
}

static const char * GetTextureName( const FbxSurfaceMaterial * pMaterial, const char * textureType, const Hash< const FbxTexture *, FbxString > & textureNames )
{
	if ( pMaterial != NULL )
	{
		const FbxProperty prop = pMaterial->FindProperty( textureType );
		if ( prop.IsValid() )
		{
			const FbxTexture * pTexture = prop.GetSrcObject<FbxTexture>();
			if ( pTexture != NULL )
			{
				const FbxString * name = textureNames.Get( pTexture );
				if ( name != NULL )
				{
					return name->Buffer();
				}
			}
		}
	}
	return "";
}

static RawMaterialType GetMaterialType( const FbxSurfaceMaterial * pMaterial, const RawModel & raw, const int textures[RAW_TEXTURE_USAGE_MAX] )
{
	if ( pMaterial == NULL )
	{
		return RAW_MATERIAL_TYPE_OPAQUE;
	}

	// Look up the shading model, typically "Phong" or "Lambert".
	const String shadingModel = (const char*)pMaterial->ShadingModel.Get();

	// Check the material name for a special extension.
	const String materialName = pMaterial->GetName();
	if ( materialName.Right( 9 ).CompareNoCase( "_additive" ) == 0 )
	{
		return RAW_MATERIAL_TYPE_ADDITIVE;
	}

	// Determine material type based on texture occlusion.
	if ( textures[RAW_TEXTURE_USAGE_DIFFUSE] >= 0 )
	{
		switch ( raw.GetTexture( textures[RAW_TEXTURE_USAGE_DIFFUSE] ).occlusion )
		{
			case RAW_TEXTURE_OCCLUSION_OPAQUE:		return RAW_MATERIAL_TYPE_OPAQUE;
			case RAW_TEXTURE_OCCLUSION_PERFORATED:	return RAW_MATERIAL_TYPE_PERFORATED;
			case RAW_TEXTURE_OCCLUSION_TRANSPARENT:	return RAW_MATERIAL_TYPE_TRANSPARENT;
		}
	}

	// Default to simply opaque.
	return RAW_MATERIAL_TYPE_OPAQUE;
}

static void ReadMesh( FbxManager * pFbxManager, FbxScene * pScene, FbxNode * pNode, const Hash< const FbxTexture *, FbxString > & textureNames, RawModel & raw )
{
	FbxGeometryConverter meshConverter( pFbxManager );
	meshConverter.Triangulate( pNode->GetNodeAttribute(), true );
	const FbxMesh * pMesh = pNode->GetMesh();

	const FbxVector4 meshTranslation = pNode->GetGeometricTranslation( FbxNode::eSourcePivot );
	const FbxVector4 meshRotation = pNode->GetGeometricRotation( FbxNode::eSourcePivot );
	const FbxVector4 meshScaling = pNode->GetGeometricScaling( FbxNode::eSourcePivot );
	const FbxAMatrix meshTransform( meshTranslation, meshRotation, meshScaling );
	const FbxAMatrix & globalTransform = pScene->GetAnimationEvaluator()->GetNodeGlobalTransform( pNode );
	const FbxMatrix transform = globalTransform * meshTransform;
	const FbxMatrix inverseTransposeTransform = transform.Inverse().Transpose();
	const Matrix4f surfaceTransform(
		(float)globalTransform[0][0], (float)globalTransform[1][0], (float)globalTransform[2][0], (float)globalTransform[3][0],
		(float)globalTransform[0][1], (float)globalTransform[1][1], (float)globalTransform[2][1], (float)globalTransform[3][1],
		(float)globalTransform[0][2], (float)globalTransform[1][2], (float)globalTransform[2][2], (float)globalTransform[3][2],
		(float)globalTransform[0][3], (float)globalTransform[1][3], (float)globalTransform[2][3], (float)globalTransform[3][3] );

	const char * meshName = ( pNode->GetName()[0] != '\0' ) ? pNode->GetName() : pMesh->GetName();
	const int rawSurfaceIndex = raw.AddSurface( meshName, surfaceTransform );

	const FbxVector4 * controlPoints = pMesh->GetControlPoints();
	const FbxLayerElementAccess< FbxVector4 > normalLayer( pMesh->GetElementNormal(), pMesh->GetElementNormalCount() );
	const FbxLayerElementAccess< FbxVector4 > binormalLayer( pMesh->GetElementBinormal(), pMesh->GetElementBinormalCount() );
	const FbxLayerElementAccess< FbxVector4 > tangentLayer( pMesh->GetElementTangent(), pMesh->GetElementTangentCount() );
	const FbxLayerElementAccess< FbxColor >   colorLayer( pMesh->GetElementVertexColor(), pMesh->GetElementVertexColorCount() );
	const FbxLayerElementAccess< FbxVector2 > uvLayer0( pMesh->GetElementUV( 0 ), pMesh->GetElementUVCount() );
	const FbxLayerElementAccess< FbxVector2 > uvLayer1( pMesh->GetElementUV( 1 ), pMesh->GetElementUVCount() );
	const FbxSkinningAccess skinning( pMesh );
	const FbxMaterialAccess materials( pMesh );

	raw.AddVertexAttribute( RAW_VERTEX_ATTRIBUTE_POSITION );
	if ( normalLayer.LayerPresent() ) { raw.AddVertexAttribute( RAW_VERTEX_ATTRIBUTE_NORMAL ); }
	if ( tangentLayer.LayerPresent() ) { raw.AddVertexAttribute( RAW_VERTEX_ATTRIBUTE_TANGENT ); }
	if ( binormalLayer.LayerPresent() ) { raw.AddVertexAttribute( RAW_VERTEX_ATTRIBUTE_BINORMAL ); }
	if ( colorLayer.LayerPresent() ) { raw.AddVertexAttribute( RAW_VERTEX_ATTRIBUTE_COLOR ); }
	if ( uvLayer0.LayerPresent() ) { raw.AddVertexAttribute( RAW_VERTEX_ATTRIBUTE_UV0 ); }
	if ( uvLayer1.LayerPresent() ) { raw.AddVertexAttribute( RAW_VERTEX_ATTRIBUTE_UV1 ); }
	if ( skinning.IsSkinned() ) { raw.AddVertexAttribute( RAW_VERTEX_ATTRIBUTE_JOINT_WEIGHTS ); }
	if ( skinning.IsSkinned() ) { raw.AddVertexAttribute( RAW_VERTEX_ATTRIBUTE_JOINT_INDICES ); }

	int polygonVertexIndex = 0;

	for ( int polygonIndex = 0; polygonIndex < pMesh->GetPolygonCount(); polygonIndex++ )
	{
		FBX_ASSERT( pMesh->GetPolygonSize( polygonIndex ) == 3 );

		const FbxSurfaceMaterial * pMaterial = materials.GetMaterial( polygonIndex );

		int textures[RAW_TEXTURE_USAGE_MAX] = { 0 };
		textures[RAW_TEXTURE_USAGE_DIFFUSE]		= raw.AddTexture( GetTextureName( pMaterial, FbxSurfaceMaterial::sDiffuse, textureNames ), RAW_TEXTURE_USAGE_DIFFUSE );
		textures[RAW_TEXTURE_USAGE_NORMAL]		= raw.AddTexture( GetTextureName( pMaterial, FbxSurfaceMaterial::sNormalMap, textureNames ), RAW_TEXTURE_USAGE_NORMAL );
		textures[RAW_TEXTURE_USAGE_SPECULAR]	= raw.AddTexture( GetTextureName( pMaterial, FbxSurfaceMaterial::sSpecular, textureNames ), RAW_TEXTURE_USAGE_SPECULAR );
		textures[RAW_TEXTURE_USAGE_EMISSIVE]	= raw.AddTexture( GetTextureName( pMaterial, FbxSurfaceMaterial::sEmissive, textureNames ), RAW_TEXTURE_USAGE_EMISSIVE );
		textures[RAW_TEXTURE_USAGE_REFLECTION]	= raw.AddTexture( GetTextureName( pMaterial, FbxSurfaceMaterial::sReflection, textureNames ), RAW_TEXTURE_USAGE_REFLECTION );

		const RawMaterialType materialType = GetMaterialType( pMaterial, raw, textures );
		const int rawMaterialIndex = raw.AddMaterial( materialType, textures );

		RawVertex rawVertices[3];
		for ( int vertexIndex = 0; vertexIndex < 3; vertexIndex++, polygonVertexIndex++ )
		{
			const int controlPointIndex = pMesh->GetPolygonVertex( polygonIndex, vertexIndex );

			// Note that the default values here must be the same as the RawVertex default values!
			const FbxVector4 fbxPosition = transform.MultNormalize( controlPoints[controlPointIndex] );
			const FbxVector4 fbxNormal = normalLayer.GetElement( polygonIndex, polygonVertexIndex, controlPointIndex, FbxVector4( 0.0f, 0.0f, 0.0f, 0.0f ), inverseTransposeTransform, true );
			const FbxVector4 fbxTangent = tangentLayer.GetElement( polygonIndex, polygonVertexIndex, controlPointIndex, FbxVector4( 0.0f, 0.0f, 0.0f, 0.0f ), inverseTransposeTransform, true );
			const FbxVector4 fbxBinormal = binormalLayer.GetElement( polygonIndex, polygonVertexIndex, controlPointIndex, FbxVector4( 0.0f, 0.0f, 0.0f, 0.0f ), inverseTransposeTransform, true );
			const FbxColor   fbxColor = colorLayer.GetElement( polygonIndex, polygonVertexIndex, controlPointIndex, FbxColor( 0.0f, 0.0f, 0.0f, 0.0f ) );
			const FbxVector2 fbxUV0 = uvLayer0.GetElement( polygonIndex, polygonVertexIndex, controlPointIndex, FbxVector2( 0.0f, 0.0f ) );
			const FbxVector2 fbxUV1 = uvLayer1.GetElement( polygonIndex, polygonVertexIndex, controlPointIndex, FbxVector2( 0.0f, 0.0f ) );

			RawVertex & vertex = rawVertices[vertexIndex];
			vertex.position.x = (float)fbxPosition[0];
			vertex.position.y = (float)fbxPosition[1];
			vertex.position.z = (float)fbxPosition[2];
			vertex.normal.x = (float)fbxNormal[0];
			vertex.normal.y = (float)fbxNormal[1];
			vertex.normal.z = (float)fbxNormal[2];
			vertex.tangent.x = (float)fbxTangent[0];
			vertex.tangent.y = (float)fbxTangent[1];
			vertex.tangent.z = (float)fbxTangent[2];
			vertex.binormal.x = (float)fbxBinormal[0];
			vertex.binormal.y = (float)fbxBinormal[1];
			vertex.binormal.z = (float)fbxBinormal[2];
			vertex.color.x = (float)fbxColor.mRed;
			vertex.color.y = (float)fbxColor.mGreen;
			vertex.color.z = (float)fbxColor.mBlue;
			vertex.color.w = (float)fbxColor.mAlpha;
			vertex.uv0.x = (float)fbxUV0[0];
			vertex.uv0.y = (float)fbxUV0[1];
			vertex.uv1.x = (float)fbxUV1[0];
			vertex.uv1.y = (float)fbxUV1[1];
			vertex.jointIndices = skinning.GetVertexIndices( controlPointIndex );
			vertex.jointWeights = skinning.GetVertexWeights( controlPointIndex );
			if ( skinning.IsSkinned() )
			{
				// remap the mesh joint indices to raw model joint indices
				for ( int i = 0; i < 4; i++ )
				{
					vertex.jointIndices[i] = raw.AddJoint( skinning.GetJointName( vertex.jointIndices[i] ), surfaceTransform );
				}
			}
			vertex.polarityUv0 = false;
		}

		if ( textures[RAW_TEXTURE_USAGE_NORMAL] != -1 )
		{
			// Distinguish vertices that are used by triangles with a different texture polarity to avoid degenerate tangent space smoothing.
			const bool polarity = TriangleTexturePolarity( rawVertices[0].uv0, rawVertices[1].uv0, rawVertices[2].uv0 );
			rawVertices[0].polarityUv0 = polarity;
			rawVertices[1].polarityUv0 = polarity;
			rawVertices[2].polarityUv0 = polarity;
		}

		int rawVertexIndices[3];
		for ( int vertexIndex = 0; vertexIndex < 3; vertexIndex++ )
		{
			rawVertexIndices[vertexIndex] = raw.AddVertex( rawVertices[vertexIndex] );
		}

		raw.AddTriangle( rawVertexIndices[0], rawVertexIndices[1], rawVertexIndices[2], rawMaterialIndex, rawSurfaceIndex );
	}
}

static void ReadNode( FbxManager * pManager, FbxScene * pScene, FbxNode * pNode, const Hash< const FbxTexture *, FbxString > & textureNames, RawModel & raw )
{
	if ( !pNode->GetVisibility() )
	{
		return;
	}

	FbxNodeAttribute * pNodeAttribute = pNode->GetNodeAttribute();
	if ( pNodeAttribute != NULL )
	{
		const FbxNodeAttribute::EType attributeType = pNodeAttribute->GetAttributeType();
		switch ( attributeType )
		{
			case FbxNodeAttribute::eMesh:
			case FbxNodeAttribute::eNurbs:
			case FbxNodeAttribute::eNurbsSurface:
			case FbxNodeAttribute::ePatch:
			{
				ReadMesh( pManager, pScene, pNode, textureNames, raw );
				break;
			}
			case FbxNodeAttribute::eMarker:
			case FbxNodeAttribute::eSkeleton:
			case FbxNodeAttribute::eCamera:
			case FbxNodeAttribute::eNull:
			case FbxNodeAttribute::eLight:
			{
				break;
			}
		}
	}

	for ( int child = 0; child < pNode->GetChildCount(); child++ )
	{
		ReadNode( pManager, pScene, pNode->GetChild( child ), textureNames, raw );
	}
}

static FbxString GetFileFolder( const char * fileName )
{
	char cleanPath[StringUtils::MAX_PATH_LENGTH];
	StringUtils::GetCleanPath( cleanPath, fileName, '\\' );

	FbxString folder = cleanPath;
	folder = folder.Left( folder.ReverseFind( '\\' ) + 1 );	// strip the file name
	if ( folder.GetLen() < 2 || folder[1] != ':' )
	{
		folder = FileUtils::GetCurrentFolder() + folder.Buffer();
	}
	return folder;
}

static String GetInferredFileName( const char * fbxFileName, const char * directory, const Array< String > & directoryFileList )
{
	// Get the file name with file extension.
	const String fileName = StringUtils::GetFileNameString( StringUtils::GetCleanPathString( fbxFileName ) );

	// Try to find a match with extension.
	for ( int i = 0; i < directoryFileList.GetSizeI(); i++ )
	{
		if ( fileName.CompareNoCase( directoryFileList[i] ) == 0 )
		{
			return String( directory + directoryFileList[i] );
		}
	}

	// Some FBX textures end with "_c.dds" while the source texture is a ".tga".
	const String lastSix = fileName.Substring( fileName.GetLength() - 6, fileName.GetLength() );
	const bool isDDS = ( lastSix.CompareNoCase( "_c.dds" ) == 0 );

	// Get the file name without file extension.
	const String fileBase = isDDS ? fileName.Substring( 0, fileName.GetLength() - 6 ) : StringUtils::GetFileBaseString( fileName );

	// Try to find a match without file extension.
	for ( int i = 0; i < directoryFileList.GetSizeI(); i++ )
	{
		const String listedFileBase = StringUtils::GetFileBaseString( directoryFileList[i] );

		// If the two extension-less base names match.
		if ( fileBase.CompareNoCase( listedFileBase ) == 0 )
		{
			// Return the name with extension of the file in the directory.
			return String( directory + directoryFileList[i] );
		}
	}

	// Return the original file with extension
	return fbxFileName;
}

/*
	The texture file names inside of the FBX often contain some long author-specific
	path with the wrong extensions. For instance, all of the art assets may be PSD
	files in the FBX metadata, but in practice they are delivered as TGA or PNG files.

	This function takes a texture file name stored in the FBX, which may be an absolute
	path on the author's computer such as "C:\MyProject\TextureName.psd", and matches
	it to a list of existing texture files in the same directory as the FBX file.
*/
static void FindFbxTextures( FbxScene * pScene, const char * fbxFileName, const char * extensions, Hash< const FbxTexture *, FbxString > & textureNames )
{
	// Get the folder the FBX file is in.
	const FbxString folder = GetFileFolder( fbxFileName );

	// Check if there is a filename.fbm folder to which embedded textures were extracted.
	const FbxString fbmFolderName = folder + StringUtils::GetFileBaseString( fbxFileName ) + ".fbm/";

	// Search either in the folder with embedded textures or in the same folder as the FBX file.
	const FbxString searchFolder = FileUtils::FolderExists( fbmFolderName ) ? fbmFolderName : folder;

	// Get a list with all the texture files from either the folder with embedded textures or the same folder as the FBX file.
	Array< String > fileList;
	FileUtils::ListFolderFiles( fileList, searchFolder, extensions );

	// Try to match the FBX texture names with the actual files on disk.
	for ( int i = 0; i < pScene->GetTextureCount(); i++ )
	{
		const FbxTexture * pTexture = pScene->GetTexture( i );
		const FbxFileTexture * pFileTexture = FbxCast<FbxFileTexture>( pTexture );
		if ( pFileTexture == NULL )
		{
			continue;
		}
		const FbxString name = GetInferredFileName( pFileTexture->GetFileName(), searchFolder, fileList ).ToCStr();
		textureNames.Add( pTexture, name );
	}
}

bool LoadFBXFile( const char * fbxFileName, const char * textureExtensions, RawModel & raw )
{
    FbxManager * pManager = FbxManager::Create();
	FbxIOSettings * pIoSettings = FbxIOSettings::Create( pManager, IOSROOT );
	pManager->SetIOSettings( pIoSettings );
	FbxImporter * pImporter = FbxImporter::Create( pManager, "" );

	if ( !pImporter->Initialize( fbxFileName, -1, pManager->GetIOSettings() ) )
	{
		printf( "%s\n", pImporter->GetStatus().GetErrorString() );
		pImporter->Destroy();
		pManager->Destroy();
		return false;
	}

	FbxScene * pScene = FbxScene::Create( pManager, "fbxScene" );
	pImporter->Import( pScene );
	pImporter->Destroy();

	if ( pScene == NULL )
	{
		pImporter->Destroy();
		pManager->Destroy();
		return false;
	}

	Hash< const FbxTexture *, FbxString > textureNames;
	FindFbxTextures( pScene, fbxFileName, textureExtensions, textureNames );

	// every model has a root joint
	raw.AddJoint( "root", Matrix4f() );

	ReadNode( pManager, pScene, pScene->GetRootNode(), textureNames, raw );

	pScene->Destroy();
	pManager->Destroy();

	return true;
}
