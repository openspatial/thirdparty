/************************************************************************************

Filename    :   Raw2RayTraceModel.cpp
Content     :   Ray trace model builder.
Created     :   May, 2014
Authors     :   J.M.P. van Waveren

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/
#include <stdint.h>

#pragma warning( disable : 4351 )	// new behavior: elements of array will be default initialized

#define FBX_TOOL
#include "../../VRLib/jni/LibOVR/Include/OVR.h"
#include "../../VRLib/jni/LibOVR/Src/Kernel/OVR_Hash.h"
#include "../../VRLib/jni/LibOVR/Src/Kernel/OVR_String_Utils.h"
#include "../../VRLib/jni/LibOVR/Src/Kernel/OVR_JSON.h"

using namespace OVR;

#include "../../VRLib/jni/ModelTrace.h"

#include "RawModel.h"
#include "ModelData.h"

/*
	A ray-trace model is a triangle soup with a
	Surface Area Heuristic (SAH) optimized KD-Tree.

	1. Heuristics for Ray Tracing using Space Subdivision
	J. David MacDonald, Kellogg S. Booth
	Visual Computer, Issue 6, Pages 153-65, 1990

	2. On Improving KD Tree for Ray Shooting
	Vlastimil Havran, Jiri Bittner
	In Proceedings of WSCG, Pages 209-216, 2002.

	3. On Building Fast KD-Trees for Ray Tracing, and on Doing That in O(N log N)
	Ingo Wald, Vlastimil Havran
	Proceedings of the IEEE Symposium on Interactive Ray Tracing, Pages 61-69, 2006
*/

#define RT_KDTREE_EPSILON					0.00001f
#define RT_KDTREE_MIN_SURFACE_AREA			0.0001f
#define RT_KDTREE_SAH_COST_TRAVERSAL		15.0f	// Kt
#define RT_KDTREE_SAH_COST_INTERSECTION		20.0f	// Ki
#define RT_KDTREE_SAH_EMPTY_SPACE_FACTOR	0.8f	// lambda(p) function
#define RT_KDTREE_CLIP_TRIANGLES			0		// clip triangles to node bounds
	
enum splitSide_t
{
	KDTREE_SPLIT_SIDE_LEFT,
	KDTREE_SPLIT_SIDE_RIGHT,
	KDTREE_SPLIT_SIDE_BOTH
};
	
enum eventType_t
{
	KDTREE_EVENT_TYPE_END,
	KDTREE_EVENT_TYPE_PLANAR,
	KDTREE_EVENT_TYPE_START
};

struct triangle_t
{
	uint32_t		v0;
	uint32_t		v1;
	uint32_t		v2;
	splitSide_t		side;
};

struct sah_t
{
					sah_t( const float cost_, const splitSide_t side_ ) :
						cost( cost_ ),
						side( side_ ) {}

	float			cost;
	splitSide_t		side;
};

struct event_t
{		
	event_t() :
		type( KDTREE_EVENT_TYPE_END ),
		plane( 0 ),
		dist( FLT_MAX ),
		triangle( NULL )
	{
	}
	event_t( const eventType_t type_, const uint32_t plane_, const float dist_, triangle_t * triangle_ ) :
		type( type_ ),
		plane( plane_ ),
		dist( dist_ ),
		triangle( triangle_ )
	{
	}
	bool operator<( const event_t & other ) const
	{
		if ( plane != other.plane )
		{
			return ( plane < other.plane );
		}
		if ( dist != other.dist )
		{
			return ( dist < other.dist );
		}
		if ( type != other.type )
		{
			return ( type < other.type );
		}
		return ( triangle < other.triangle );
	}

	eventType_t		type;
	uint32_t		plane;
	float			dist;
	triangle_t *	triangle;
};

struct node_t
{
	node_t() :
		leaf( false ),
		plane( 0 ),
		dist( 0.0f ),
		aabb(),
		children(),
		ropes()
	{
	}

	bool				leaf;
	uint32_t			plane;
	float				dist;
	Bounds3f			aabb;
	Array<triangle_t *>	triangles;
	Array<event_t>		events;
	uint32_t			children[2];
	uint32_t			ropes[6];
};

static kdtree_node_t CreateCompactNode( const uint32_t plane, const float dist, const bool leaf, const uint32_t index )
{
	kdtree_node_t node;
	node.dist = dist;
	node.data = ( index << 3 ) | ( plane << 1 ) | ( (uint32_t)leaf );
	return node;
}

static kdtree_leaf_t CreateCompactLeaf( const size_t firstTriangle, const Array< triangle_t * > & triangles,
									const uint32_t ropes[6], const Bounds3f & aabb,
									Array< int > & tree_overflow )
{
	kdtree_leaf_t leaf;
	memset( &leaf, 0, sizeof( leaf ) );

	const int end_count = triangles.GetSizeI() > RT_KDTREE_MAX_LEAF_TRIANGLES ? ( RT_KDTREE_MAX_LEAF_TRIANGLES - 1 ) : RT_KDTREE_MAX_LEAF_TRIANGLES;
	for ( int i = 0; i < end_count; i++ )
	{
		leaf.triangles[i] = ( i < triangles.GetSizeI() ) ? (int)( ( (size_t)triangles[i] - firstTriangle ) / sizeof( triangle_t ) ) : -1;
	}
			
	// if there are more than RT_KDTREE_MAX_LEAF_TRIANGLES triangles then add them to the overflow container
	if ( triangles.GetSizeI() > RT_KDTREE_MAX_LEAF_TRIANGLES )
	{
		// store overflow index in last triangle
		leaf.triangles[RT_KDTREE_MAX_LEAF_TRIANGLES - 1] = 0x80000000 | ( (int)( tree_overflow.GetSizeI() ) & 0x7FFFFFFF );

		for ( int i = end_count; i < triangles.GetSizeI(); i++ )
		{
			tree_overflow.PushBack( (int)( ( (size_t)triangles[i] - firstTriangle ) / sizeof( triangle_t ) ) );
		}
		tree_overflow.PushBack( -1 );
	}

	for ( int i = 0; i < 6; i++ )
	{
		leaf.ropes[i] = ( ropes[i] != 0 ) ? (int)ropes[i] : -1;
	}

	leaf.bounds = aabb;

	return leaf;
}

static float SurfaceArea( const Bounds3f & aabb )
{
	const Vector3f diagonal( aabb.GetMaxs() - aabb.GetMins() );
	return 2.0f * ( diagonal.x * diagonal.y + diagonal.x * diagonal.z + diagonal.y * diagonal.z );
}

static float SurfaceAreaCost( const float Pleft, const float Pright, const float Nleft, const float Nright )
{
	return ( ( ( Nleft == 0 || Nright == 0 ) ? RT_KDTREE_SAH_EMPTY_SPACE_FACTOR : 1.0f ) *
			( RT_KDTREE_SAH_COST_TRAVERSAL + RT_KDTREE_SAH_COST_INTERSECTION * ( Pleft * Nleft + Pright * Nright ) ) );
}

static sah_t SurfaceAreaHeuristic( const Bounds3f & parentAabb, const Bounds3f & leftAabb, const Bounds3f & rightAabb,
					const uint32_t Nleft, const uint32_t Nright, const uint32_t Nplanar )
{
	const float surfaceAreaParent = SurfaceArea( parentAabb );
	const float surfaceAreaLeft = SurfaceArea( leftAabb );
	const float surfaceAreaRight = SurfaceArea( rightAabb );

	const float Pleft = surfaceAreaLeft / surfaceAreaParent;
	const float Pright = surfaceAreaRight / surfaceAreaParent;

	const float costLeft = SurfaceAreaCost( Pleft, Pright, (float)Nleft + Nplanar, (float)Nright );
	const float costRight = SurfaceAreaCost( Pleft, Pright, (float)Nleft, (float)Nright + Nplanar );

	if ( costLeft < costRight )
	{
		return sah_t( costLeft, KDTREE_SPLIT_SIDE_LEFT );
	}
	else
	{
		return sah_t( costRight, KDTREE_SPLIT_SIDE_RIGHT );
	}
}

static sah_t FindPlane( node_t & node )
{
	sah_t bestSah( FLT_MAX, KDTREE_SPLIT_SIDE_BOTH );

	// No split if the node surface area is too small.
	if ( SurfaceArea( node.aabb ) < RT_KDTREE_MIN_SURFACE_AREA )
	{
		return bestSah;
	}

	// For each plane start with all triangles at the right side.
	uint32_t Nleft[3] = { 0, 0, 0 };
	uint32_t Nplanar[3] = { 0, 0, 0 };
	uint32_t Nright[3] = { (uint32_t)node.triangles.GetSize(), (uint32_t)node.triangles.GetSize(), (uint32_t)node.triangles.GetSize() };

	for ( int i = 0; i < node.events.GetSizeI(); )
	{
		const uint32_t plane = node.events[i].plane;
		const float dist = node.events[i].dist;

		uint32_t typeCount[3] = { 0 };
		do
		{
			typeCount[node.events[i++].type]++;
		} while( i < node.events.GetSizeI() && node.events[i].plane == plane && node.events[i].dist == dist );

		Nplanar[plane] = typeCount[KDTREE_EVENT_TYPE_PLANAR];
		Nright[plane] -= typeCount[KDTREE_EVENT_TYPE_PLANAR];
		Nright[plane] -= typeCount[KDTREE_EVENT_TYPE_END];

		assert( Nleft[plane] + Nplanar[plane] + Nright[plane] >= node.triangles.GetSize() );

		// Prevent splitting off empty flat cells.
		if ( ( Nleft[plane] != 0 || dist > ( node.aabb.GetMins()[plane] + RT_KDTREE_EPSILON ) ) &&
				( Nright[plane] != 0 || dist < ( node.aabb.GetMaxs()[plane] - RT_KDTREE_EPSILON ) ) )
		{
			Bounds3f leftAabb( node.aabb );
			Bounds3f rightAabb( node.aabb );
			leftAabb.GetMaxs()[plane] = dist;
			rightAabb.GetMins()[plane] = dist;

			const sah_t sah = SurfaceAreaHeuristic( node.aabb, leftAabb, rightAabb, Nleft[plane], Nright[plane], Nplanar[plane] );
			if ( sah.cost < bestSah.cost )
			{
				node.plane = plane;
				node.dist = dist;
				bestSah = sah;
			}
		}

		Nleft[plane] += typeCount[KDTREE_EVENT_TYPE_START];
		Nleft[plane] += typeCount[KDTREE_EVENT_TYPE_PLANAR];
		Nplanar[plane] = 0;
	}

	// All triangles should now be on the left side.
	assert( Nleft[0] == node.triangles.GetSize() && Nleft[1] == node.triangles.GetSize() && Nleft[2] == node.triangles.GetSize() );
	assert( Nplanar[0] == 0 && Nplanar[1] == 0 && Nplanar[2] == 0 );
	assert( Nright[0] == 0 && Nright[1] == 0 && Nright[2] == 0 );

	return bestSah;
}

static int ClipPolygonToPlane( Vector3f * dstPoints, const Vector3f * srcPoints, int srcPointCount, int planeAxis, float planeDist, float planeDir ) 
{
	int sides[16];

	for ( int p0 = 0; p0 < srcPointCount; p0++ )
	{
		int p1 = ( p0 + 1 ) & ( ( p0 + 1 - srcPointCount ) >> 31 );
		sides[p0] = planeDir * srcPoints[p0][planeAxis] < planeDist;
		dstPoints[p0 * 2 + 0] = srcPoints[p0];
		const float d0 = planeDir * planeDist - srcPoints[p0][planeAxis];
		const float d1 = planeDir * planeDist - srcPoints[p1][planeAxis];
		const float delta = d0 - d1;
		const float SmallestNonDenormal	= float( 1.1754943508222875e-038f );	// ( 1U << 23 )
		const float fraction = fabsf( delta ) > SmallestNonDenormal ? ( d0 / delta ) : 1.0f;
		if ( fraction <= 0.0f )
		{
			dstPoints[p0 * 2 + 1] = srcPoints[p0];
		}
		else if ( fraction >= 1.0f )
		{
			dstPoints[p0 * 2 + 1] = srcPoints[p1];
		}
		else
		{
			dstPoints[p0 * 2 + 1] = srcPoints[p0] + ( srcPoints[p1] - srcPoints[p0] ) * fraction;
		}
		dstPoints[p0 * 2 + 1][planeAxis] = planeDir * planeDist;
	}

	sides[srcPointCount] = sides[0];

	int dstPointCount = 0;
	for ( int p = 0; p < srcPointCount; p++ )
	{
		if ( sides[p + 0] != 0 )
		{
			dstPoints[dstPointCount++] = dstPoints[p * 2 + 0];
		}
		if ( sides[p + 0] != sides[p + 1] )
		{
			dstPoints[dstPointCount++] = dstPoints[p * 2 + 1];
		}
	}

	assert( dstPointCount <= 16 );
	return dstPointCount;
}

static Bounds3f ClippedTriangleAabb( const Bounds3f & aabb, const triangle_t * triangle, const Vector3f * vertices )
{
	Vector3f points1[2 * 16] = { vertices[triangle->v0], vertices[triangle->v1], vertices[triangle->v2] };
	Vector3f points2[2 * 16];

	int pointCount = 3;
	pointCount = ClipPolygonToPlane( points2, points1, pointCount, 0, -aabb.GetMins()[0], -1.0f );
	pointCount = ClipPolygonToPlane( points1, points2, pointCount, 0, +aabb.GetMaxs()[0], +1.0f );
	pointCount = ClipPolygonToPlane( points2, points1, pointCount, 1, -aabb.GetMins()[1], -1.0f );
	pointCount = ClipPolygonToPlane( points1, points2, pointCount, 1, +aabb.GetMaxs()[1], +1.0f );
	pointCount = ClipPolygonToPlane( points2, points1, pointCount, 2, -aabb.GetMins()[2], -1.0f );
	pointCount = ClipPolygonToPlane( points1, points2, pointCount, 2, +aabb.GetMaxs()[2], +1.0f );

	Bounds3f clippedAabb( points1[0], points1[0] );
	for ( int p = 1; p < pointCount; p++ )
	{
		clippedAabb.AddPoint( points1[p] );
	}

#if 0
	Bounds3f triangleAabb;
	triangleAabb.GetMins().x = Alg::Min( vertices[triangle->v0].x, Alg::Min( vertices[triangle->v1].x, vertices[triangle->v2].x ) );
	triangleAabb.GetMins().y = Alg::Min( vertices[triangle->v0].y, Alg::Min( vertices[triangle->v1].y, vertices[triangle->v2].y ) );
	triangleAabb.GetMins().z = Alg::Min( vertices[triangle->v0].z, Alg::Min( vertices[triangle->v1].z, vertices[triangle->v2].z ) );
	triangleAabb.GetMaxs().x = Alg::Max( vertices[triangle->v0].x, Alg::Max( vertices[triangle->v1].x, vertices[triangle->v2].x ) );
	triangleAabb.GetMaxs().y = Alg::Max( vertices[triangle->v0].y, Alg::Max( vertices[triangle->v1].y, vertices[triangle->v2].y ) );
	triangleAabb.GetMaxs().z = Alg::Max( vertices[triangle->v0].z, Alg::Max( vertices[triangle->v1].z, vertices[triangle->v2].z ) );

	for ( int i = 0; i < 3; i++ )
	{
		debug_assert( clippedAabb.GetMins()[i] >= triangleAabb.GetMins()[i] );
		debug_assert( clippedAabb.GetMaxs()[i] <= triangleAabb.GetMaxs()[i] );
		debug_assert( clippedAabb.GetMins()[i] >= aabb.GetMins()[i] );
		debug_assert( clippedAabb.GetMaxs()[i] <= aabb.GetMaxs()[i] );
	}
#endif

	return clippedAabb;
}

static void AddTriangleEvents( Array<event_t> & events, triangle_t * triangle, const uint32_t plane,
								const Bounds3f & aabb, const Vector3f * vertices )
{
	const Vector3f & v0 = vertices[triangle->v0];
	const Vector3f & v1 = vertices[triangle->v1];
	const Vector3f & v2 = vertices[triangle->v2];

#if RT_KDTREE_CLIP_TRIANGLES
	// Clipping the actual triangle to the node bounds can provide a 9% average, up to 35% ray tracing
	// speedup (Havran, 2002) and results in a smaller KD-Tree at the cost of build time.
	const Bounds3f clippedAabb = ClippedTriangleAabb( aabb, triangle, vertices );
	const float clippedMin = clippedAabb.GetMins()[plane];
	const float clippedMax = clippedAabb.GetMaxs()[plane];
#else
	// Clip the triangle bounds to the node bounds.
	const float triangleMin = Alg::Min( v0[plane], Alg::Min( v1[plane], v2[plane] ) );
	const float triangleMax = Alg::Max( v0[plane], Alg::Max( v1[plane], v2[plane] ) );
	const float clippedMin = Alg::Max( triangleMin, aabb.GetMins()[plane] );
	const float clippedMax = Alg::Min( triangleMax, aabb.GetMaxs()[plane] );
#endif

	// If the bounds are empty.
	if ( clippedMin == clippedMax )
	{
		events.PushBack( event_t( KDTREE_EVENT_TYPE_PLANAR, plane, clippedMin, triangle ) );
	}
	else
	{
		assert( clippedMin < clippedMax );	// reversed events would cause problems in FindPlane
		events.PushBack( event_t( KDTREE_EVENT_TYPE_START, plane, clippedMin, triangle ) );
		events.PushBack( event_t( KDTREE_EVENT_TYPE_END, plane, clippedMax, triangle ) );
	}
}

static void SplitNode( node_t & leftNode, node_t & rightNode, const node_t & node, const splitSide_t side,
						const Vector3f * vertices )
{
	leftNode.aabb = node.aabb;
	rightNode.aabb = node.aabb;
	leftNode.aabb.GetMaxs()[node.plane] = node.dist;
	rightNode.aabb.GetMins()[node.plane] = node.dist;

	// Classify the triangles.
	int leftEventCount = 0;
	int rightEventCount = 0;
	for ( int i = 0; i < node.triangles.GetSizeI(); i++ )
	{
		node.triangles[i]->side = KDTREE_SPLIT_SIDE_BOTH;
	}
	for ( int i = 0; i < node.events.GetSizeI(); i++ )
	{
		if ( node.events[i].plane == node.plane )
		{
			if ( node.events[i].type == KDTREE_EVENT_TYPE_END && node.events[i].dist <= node.dist )
			{
				node.events[i].triangle->side = KDTREE_SPLIT_SIDE_LEFT;
				leftEventCount++;
			}
			else if ( node.events[i].type == KDTREE_EVENT_TYPE_START && node.events[i].dist >= node.dist )
			{
				node.events[i].triangle->side = KDTREE_SPLIT_SIDE_RIGHT;
				rightEventCount++;
			}
			else if ( node.events[i].type == KDTREE_EVENT_TYPE_PLANAR )
			{
				if ( node.events[i].dist < node.dist || ( node.events[i].dist == node.dist && side == KDTREE_SPLIT_SIDE_LEFT ) )
				{
					node.events[i].triangle->side = KDTREE_SPLIT_SIDE_LEFT;
					leftEventCount++;
				}
				if ( node.events[i].dist > node.dist || (node.events[i].dist == node.dist && side == KDTREE_SPLIT_SIDE_RIGHT ) )
				{
					node.events[i].triangle->side = KDTREE_SPLIT_SIDE_RIGHT;
					rightEventCount++;
				}
			}
		}
	}
	int bothEventCount = node.triangles.GetSizeI() - leftEventCount - rightEventCount;

	// Splice events and generate new events for triangles overlapping the split plane.
	leftNode.events.Reserve( leftEventCount + bothEventCount );
	rightNode.events.Reserve( rightEventCount + bothEventCount );

	Array<event_t> leftNewEvents;
	Array<event_t> rightNewEvents;
	leftNewEvents.Reserve( bothEventCount );
	rightNewEvents.Reserve( bothEventCount );

	for ( int i = 0; i < node.events.GetSizeI(); i++ )
	{
		if ( node.events[i].triangle->side == KDTREE_SPLIT_SIDE_LEFT )
		{
			leftNode.events.PushBack( node.events[i] );
		}
		else if ( node.events[i].triangle->side == KDTREE_SPLIT_SIDE_RIGHT )
		{
			rightNode.events.PushBack( node.events[i] );
		}
		else if ( node.events[i].triangle->side == KDTREE_SPLIT_SIDE_BOTH )
		{
			if ( node.events[i].type != KDTREE_EVENT_TYPE_END )
			{
				AddTriangleEvents( leftNewEvents, node.events[i].triangle, node.events[i].plane, leftNode.aabb, vertices );
				AddTriangleEvents( rightNewEvents, node.events[i].triangle, node.events[i].plane, rightNode.aabb, vertices );
			}
		}
	}

	// Sort and merge events to maintain sort order.
	Alg::QuickSort( leftNewEvents );
	Alg::MergeArray( leftNode.events, leftNewEvents );
	
	Alg::QuickSort( rightNewEvents );
	Alg::MergeArray( rightNode.events, rightNewEvents );

	// Split the triangles between the left and right node.
	for ( int i = 0; i < node.triangles.GetSizeI(); i++ )
	{
		if ( node.triangles[i]->side == KDTREE_SPLIT_SIDE_LEFT )
		{
			leftNode.triangles.PushBack( node.triangles[i] );
		}
		else if ( node.triangles[i]->side == KDTREE_SPLIT_SIDE_RIGHT )
		{
			rightNode.triangles.PushBack( node.triangles[i] );
		}
		else
		{
			leftNode.triangles.PushBack( node.triangles[i] );
			rightNode.triangles.PushBack( node.triangles[i] );
		}
	}

	// Should never be splitting off flat empty nodes.
	assert( ( leftNode.triangles.GetSizeI() != 0 || node.dist != node.aabb.GetMins()[node.plane] ) &&
			( rightNode.triangles.GetSizeI() != 0 || node.dist != node.aabb.GetMaxs()[node.plane] ) );
}

static void CreateRopes( Array< node_t > & nodes, const uint32_t nodeIndex, uint32_t ropes[6], const Bounds3f & aabb )
{
	if ( nodes[nodeIndex].leaf )
	{
		nodes[nodeIndex].ropes[0] = ropes[0];
		nodes[nodeIndex].ropes[1] = ropes[1];
		nodes[nodeIndex].ropes[2] = ropes[2];
		nodes[nodeIndex].ropes[3] = ropes[3];
		nodes[nodeIndex].ropes[4] = ropes[4];
		nodes[nodeIndex].ropes[5] = ropes[5];
		nodes[nodeIndex].aabb = aabb;
		return;
	}

	// left, right, front, back, bottom, top
	for ( uint32_t side = 0; side < 6; side++ )
	{
		if ( ropes[side] != NULL )
		{
			while ( !nodes[ropes[side]].leaf )
			{
				if ( ( side & 1 ) == 0 ) // left, front, bottom
				{
					if ( nodes[ropes[side]].plane == ( side >> 1 ) ||
							nodes[ropes[side]].dist <= aabb.GetMins()[nodes[ropes[side]].plane] )
					{
						ropes[side] = nodes[ropes[side]].children[1]; // right child
					}
					else
					{
						break;
					}
				}
				else	// right, back, top
				{
					if ( nodes[ropes[side]].plane == ( side >> 1 ) ||
							nodes[ropes[side]].dist >= aabb.GetMaxs()[nodes[ropes[side]].plane] )
					{
						ropes[side] = nodes[ropes[side]].children[0]; // left child
					}
					else
					{
						break;
					}
				}
			}
		}
	}

	const uint32_t leftSide = ( nodes[nodeIndex].plane << 1 ) + 0;
	const uint32_t rightSide = ( nodes[nodeIndex].plane << 1 ) + 1;

	uint32_t leftRopes[6] = { ropes[0], ropes[1], ropes[2], ropes[3], ropes[4], ropes[5] };
	leftRopes[rightSide] = nodes[nodeIndex].children[1];

	Bounds3f leftAabb( aabb );
	leftAabb.GetMaxs()[nodes[nodeIndex].plane] = nodes[nodeIndex].dist;

	CreateRopes( nodes, nodes[nodeIndex].children[0], leftRopes, leftAabb );
		
	uint32_t rightRopes[6] = { ropes[0], ropes[1], ropes[2], ropes[3], ropes[4], ropes[5] };
	rightRopes[leftSide] = nodes[nodeIndex].children[0];

	Bounds3f rightAabb( aabb );
	rightAabb.GetMins()[nodes[nodeIndex].plane] = nodes[nodeIndex].dist;

	CreateRopes( nodes, nodes[nodeIndex].children[1], rightRopes, rightAabb );
}

template< typename _type_ >
static void AppendBinaryArray( const Array< _type_ > & array, Array< uint8_t > & out )
{
	const UPInt size = out.GetSize();
	out.Resize( size + array.GetSize() * sizeof( array[0] ) );
	memcpy( &out[size], array.GetDataPtr(), array.GetSize() * sizeof( array[0] ) );
}

ModelData * Raw2RayTraceModel( const RawModel & raw, const bool fullText )
{
	printf( "building KD-Tree...\n" );

	Array< Vector3f > vertices;
	Array< Vector2f > uvs;
	Array< int > indices;
	raw.GetIndexedVertices( vertices, uvs, indices );

	Array< triangle_t > triangles;
	triangles.Resize( indices.GetSizeI() / 3 );
	for ( int i = 0; i < indices.GetSizeI() / 3; i++ )
	{
		triangles[i].v0 = indices[i * 3 + 0];
		triangles[i].v1 = indices[i * 3 + 1];
		triangles[i].v2 = indices[i * 3 + 2];
		triangles[i].side = KDTREE_SPLIT_SIDE_BOTH;
	}

	// Create root node.
	Array<node_t> nodeQueue;
	nodeQueue.Resize( 1 );

	// Add all triangles to the root node and calculate root node bounds.
	nodeQueue[0].triangles.Reserve( triangles.GetSize() );
	for ( int i = 0; i < triangles.GetSizeI(); i++ )
	{
		nodeQueue[0].aabb.AddPoint( vertices[triangles[i].v0] );
		nodeQueue[0].aabb.AddPoint( vertices[triangles[i].v1] );
		nodeQueue[0].aabb.AddPoint( vertices[triangles[i].v2] );
		nodeQueue[0].triangles.PushBack( &triangles[i] );
	}

	// Create root node events.
	for ( uint32_t plane = 0; plane < 3; plane++ )
	{
		for ( int i = 0; i < triangles.GetSizeI(); i++ )
		{
			AddTriangleEvents( nodeQueue[0].events, &triangles[i], plane, nodeQueue[0].aabb, vertices.GetDataPtr() );
		}
	}

	// Sort root node events.
	Alg::QuickSort( nodeQueue[0].events );

	size_t leaf_count = 0;
	size_t over_count = 0;

	for ( int i = 0; i < nodeQueue.GetSizeI(); i++ )
	{
		// Find best plane in O(N).
		const sah_t sah = FindPlane( nodeQueue[i] );

		// If no split was found, or if the split is more costly than testing the remaining triangles.
		if ( sah.cost == FLT_MAX || sah.cost > RT_KDTREE_SAH_COST_INTERSECTION * nodeQueue[i].triangles.GetSizeI() )
		{
			nodeQueue[i].leaf = true;
			nodeQueue[i].plane = 0;
			nodeQueue[i].dist = 0.0f;
			nodeQueue[i].events.Clear();
			leaf_count++;
			if ( nodeQueue[i].triangles.GetSize() > RT_KDTREE_MAX_LEAF_TRIANGLES )
			{
				over_count += nodeQueue[i].triangles.GetSize() - RT_KDTREE_MAX_LEAF_TRIANGLES;
			}
			continue;
		}

		nodeQueue[i].children[0] = nodeQueue.GetSizeI() + 0;
		nodeQueue[i].children[1] = nodeQueue.GetSizeI() + 1;
		nodeQueue.Resize( nodeQueue.GetSize() + 2 );

		SplitNode( nodeQueue[nodeQueue[i].children[0]], nodeQueue[nodeQueue[i].children[1]], nodeQueue[i], sah.side, vertices.GetDataPtr() );

		nodeQueue[i].leaf = false;
		nodeQueue[i].triangles.Clear();
		nodeQueue[i].events.Clear();
	}

	CreateRopes( nodeQueue, 0, nodeQueue[0].ropes, nodeQueue[0].aabb );

	Array<kdtree_node_t> tree_nodes;
	Array<kdtree_leaf_t> tree_leafs;
	Array<int> tree_overflow;

	// Reserve space for compact KD-Tree.
	tree_nodes.Reserve( nodeQueue.GetSize() );
	tree_leafs.Reserve( leaf_count );
	tree_overflow.Reserve( over_count );

	// Convert to compact KD-Tree.
	const size_t firstTriangle = (size_t)&triangles[0];
	for ( int i = 0; i < nodeQueue.GetSizeI(); i++ )
	{
		if ( !nodeQueue[i].leaf )
		{
			assert( nodeQueue[i].children[1] == nodeQueue[i].children[0] + 1 );
			tree_nodes.PushBack( CreateCompactNode( nodeQueue[i].plane, nodeQueue[i].dist, false, nodeQueue[i].children[0] ) );
		}
		else
		{
			tree_nodes.PushBack( CreateCompactNode( nodeQueue[i].plane, nodeQueue[i].dist, true, tree_leafs.GetSizeI() ) );
			tree_leafs.PushBack( CreateCompactLeaf( firstTriangle, nodeQueue[i].triangles, nodeQueue[i].ropes, nodeQueue[i].aabb, tree_overflow ) );
		}
	}

	printf( "%7d vertices\n", vertices.GetSizeI() );
	printf( "%7d triangles\n", triangles.GetSizeI() );
	printf( "%7d nodes\n", tree_nodes.GetSize() );
	printf( "%7d leafs\n", tree_leafs.GetSize() );
	printf( "%7d overflow\n", tree_overflow.GetSizeI() );

	ModelData * data = new ModelData;

	JSON * JSON_raytrace_model = JSON::CreateObject();
	{
		JSON_raytrace_model->AddNumberItem( "numVertices", vertices.GetSizeI() );
		JSON_raytrace_model->AddNumberItem( "numUvs", uvs.GetSizeI() );
		JSON_raytrace_model->AddNumberItem( "numIndices", indices.GetSizeI() );
		JSON_raytrace_model->AddNumberItem( "numNodes", tree_nodes.GetSizeI() );
		JSON_raytrace_model->AddNumberItem( "numLeafs", tree_leafs.GetSizeI() );
		JSON_raytrace_model->AddNumberItem( "numOverflow", tree_overflow.GetSizeI() );
		JSON_raytrace_model->AddStringItem( "bounds", StringUtils::ToString( nodeQueue[0].aabb ) );

		AppendBinaryArray( vertices, data->binary );
		AppendBinaryArray( uvs, data->binary );
		AppendBinaryArray( indices, data->binary );
		AppendBinaryArray( tree_nodes, data->binary );
		AppendBinaryArray( tree_leafs, data->binary );
		AppendBinaryArray( tree_overflow, data->binary );

		JSON_raytrace_model->AddStringItem( "vertices", fullText ? GetVector3fArrayString( vertices ) : "Vector3f" );
		JSON_raytrace_model->AddStringItem( "uvs", fullText ? GetVector2fArrayString( uvs ) : "Vector2f" );
		JSON_raytrace_model->AddStringItem( "indices", fullText ? GetInt32ArrayString( indices ) : "Int32" );

		if ( fullText )
		{
			JSON * JSON_node_array = JSON::CreateArray();
			for ( int i = 0; i < tree_nodes.GetSizeI(); i++ )
			{
				JSON * JSON_node = JSON::CreateObject();
				JSON_node->AddNumberItem( "data", tree_nodes[i].data );
				JSON_node->AddNumberItem( "dist", tree_nodes[i].dist );
				JSON_node_array->AddArrayElement( JSON_node );
			}
			JSON_raytrace_model->AddItem( "nodes", JSON_node_array );
		}
		else
		{
			JSON * JSON_node_array = JSON::CreateArray();
			JSON * JSON_node = JSON::CreateObject();
			JSON_node->AddStringItem( "data", "UInt32" );
			JSON_node->AddStringItem( "dist", "float" );
			JSON_node_array->AddArrayElement( JSON_node );
			JSON_raytrace_model->AddItem( "nodes", JSON_node_array );
		}

		if ( fullText )
		{
			JSON * JSON_leaf_array = JSON::CreateArray();
			for ( int i = 0; i < tree_leafs.GetSizeI(); i++ )
			{
				JSON * JSON_leaf = JSON::CreateObject();
				JSON_leaf->AddStringItem( "triangles", StringUtils::ToString( tree_leafs[i].triangles, RT_KDTREE_MAX_LEAF_TRIANGLES ) );
				JSON_leaf->AddStringItem( "ropes", StringUtils::ToString( tree_leafs[i].ropes, 6 ) );
				JSON_leaf->AddStringItem( "bounds", StringUtils::ToString( tree_leafs[i].bounds ) );
				JSON_leaf_array->AddArrayElement( JSON_leaf );
			}
			JSON_raytrace_model->AddItem( "leafs", JSON_leaf_array );
		}
		else
		{
			JSON * JSON_leaf_array = JSON::CreateArray();
			JSON * JSON_leaf = JSON::CreateObject();
			JSON_leaf->AddStringItem( "triangles", StringUtils::Va( "Int32[%d]", RT_KDTREE_MAX_LEAF_TRIANGLES ) );
			JSON_leaf->AddStringItem( "ropes", StringUtils::Va( "Int32[%d]", 6 ) );
			JSON_leaf->AddStringItem( "bounds", "Bounds3f" );
			JSON_leaf_array->AddArrayElement( JSON_leaf );
			JSON_raytrace_model->AddItem( "leafs", JSON_leaf_array );
		}

		JSON_raytrace_model->AddStringItem( "overflow", fullText ? GetInt32ArrayString( tree_overflow ) : "Int32" );
	}
	data->json = JSON_raytrace_model;

	return data;
}
