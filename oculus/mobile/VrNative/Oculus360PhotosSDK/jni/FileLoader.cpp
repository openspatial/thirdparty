/************************************************************************************

Filename    :   FileLoader.cpp
Content     :   
Created     :   August 13, 2014
Authors     :   John Carmack

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the Oculus360Photos/ directory. An additional grant 
of patent rights can be found in the PATENTS file in the same directory.

************************************************************************************/

#include "LibOVR/Src/Kernel/OVR_Threads.h"
#include "PackageFiles.h"
#include "FileLoader.h"
#include "Oculus360Photos.h"
#include "turbojpeg.h"
#include <OVR_TurboJpeg.h>

namespace OVR {

MessageQueue		Queue1( 4000 );	// big enough for all the thumbnails that might be needed
MessageQueue		Queue3( 1 );

pthread_mutex_t QueueMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t	QueueWake = PTHREAD_COND_INITIALIZER;
bool			QueueHasCleared = true;

void * Queue1Thread( void * v )
{
	int result = pthread_setname_np( pthread_self(), "FileQueue1" );
	if ( result != 0 )
	{
		LOG( "InitFileQueue: pthread_setname_np failed %s", strerror( result ) );
	}

	// Process incoming messages until queue is empty
	for ( ; ; )
	{
		Queue1.SleepUntilMessage();

		// If Queue3 hasn't finished our last output, sleep until it has.
		pthread_mutex_lock( &QueueMutex );
		while ( !QueueHasCleared )
		{
			// Atomically unlock the mutex and block until we get a message.
			pthread_cond_wait( &QueueWake, &QueueMutex );
		}
		pthread_mutex_unlock( &QueueMutex );

		const char * msg = Queue1.GetNextMessage();

		char commandName[1024] = {};
		sscanf( msg, "%s", commandName );
		const char * filename = msg + strlen( commandName ) + 1;

		MessageQueue * queue = &Queue3;
		char const * suffix = strstr( filename, "_nz.jpg" ); 
		if ( suffix != NULL )	
		{
			// cube map
			const char * const cubeSuffix[6] = { "_px.jpg", "_nx.jpg", "_py.jpg", "_ny.jpg", "_pz.jpg", "_nz.jpg" };
			
			MemBufferFile mbfs[6] = 
			{ 
				MemBufferFile( MemBufferFile::NoInit ), 
				MemBufferFile( MemBufferFile::NoInit ),
				MemBufferFile( MemBufferFile::NoInit ),
				MemBufferFile( MemBufferFile::NoInit ),
				MemBufferFile( MemBufferFile::NoInit ),
				MemBufferFile( MemBufferFile::NoInit ) 
			};

			char filenameWithoutSuffix[1024];
			int suffixStart = suffix - filename;
			strcpy( filenameWithoutSuffix, filename );
			filenameWithoutSuffix[suffixStart] = '\0';
			
			int side = 0;
			for ( ; side < 6; side++ )
			{
				char sideFilename[1024];
				strcpy( sideFilename, filenameWithoutSuffix );
				strcat( sideFilename, cubeSuffix[side] );
				if ( !mbfs[side].LoadFile( sideFilename ) )
				{
					if ( !ovr_ReadFileFromApplicationPackage( sideFilename, mbfs[ side ] ) )
					{
						break;
					}
				}
				LOG( "Queue1 loaded '%s'", sideFilename );
			}
			if ( side >= 6 )
			{
				// if no error occured, post to next thread
				LOG( "%s.PostPrintf( \"%s %p %i %p %i %p %i %p %i %p %i %p %i\" )", "Queue3", commandName, 
						mbfs[0].Buffer, mbfs[0].Length,
						mbfs[1].Buffer, mbfs[1].Length,
						mbfs[2].Buffer, mbfs[2].Length,
						mbfs[3].Buffer, mbfs[3].Length,
						mbfs[4].Buffer, mbfs[4].Length,
						mbfs[5].Buffer, mbfs[5].Length );
				queue->PostPrintf( "%s %p %i %p %i %p %i %p %i %p %i %p %i", commandName,
						mbfs[0].Buffer, mbfs[0].Length,
						mbfs[1].Buffer, mbfs[1].Length,
						mbfs[2].Buffer, mbfs[2].Length,
						mbfs[3].Buffer, mbfs[3].Length,
						mbfs[4].Buffer, mbfs[4].Length,
						mbfs[5].Buffer, mbfs[5].Length );
				for ( int i = 0; i < 6; ++i )
				{
					// make sure we do not free the actual buffers because they're used in the next thread
					mbfs[i].Buffer = NULL;
					mbfs[i].Length = 0;
				}
			}
			else
			{
				// otherwise free the buffers we did manage to allocate
				for ( int i = 0; i < side; ++i )
				{
					mbfs[i].FreeData();
				}
			}
		}
		else
		{
			// non-cube map
			MemBufferFile mbf( filename );
			if ( mbf.Length <= 0 || mbf.Buffer == NULL )
			{
				if ( !ovr_ReadFileFromApplicationPackage( filename, mbf ) )
				{
					continue;
				}
			}
			LOG( "%s.PostPrintf( \"%s %p %i\" )", "Queue3", commandName, mbf.Buffer, mbf.Length );
			queue->PostPrintf( "%s %p %i", commandName, mbf.Buffer, mbf.Length );
			mbf.Buffer = NULL;
			mbf.Length = 0;
		}

		free( (void *)msg );
	}
	return NULL;
}

void * Queue3Thread( void * v )
{
	int result = pthread_setname_np( pthread_self(), "FileQueue3" );
	if ( result != 0 )
	{
		LOG( "InitFileQueue: pthread_setname_np failed %s", strerror( result ) );
	}

	// Process incoming messages until queue is empty
	for ( ; ; )
	{
		Queue3.SleepUntilMessage();
		const char * msg = Queue3.GetNextMessage();

		LOG( "Queue3 msg = '%s'", msg );

		// Note that Queue3 has cleared the message
		pthread_mutex_lock( &QueueMutex );
		QueueHasCleared = true;
		pthread_cond_signal( &QueueWake );
		pthread_mutex_unlock( &QueueMutex );

		char commandName[1024] = {};
		sscanf( msg, "%s", commandName );
		int numBuffers = strcmp( commandName, "cube" ) == 0 ? 6 : 1;
		unsigned * b[6] = {};
		int blen[6];
		if ( numBuffers == 1 )
		{
			sscanf( msg, "%s %p %i", commandName, &b[0], &blen[0] );
		}
		else
		{
			OVR_ASSERT( numBuffers == 6 );		
			sscanf( msg, "%s %p %i %p %i %p %i %p %i %p %i %p %i ", commandName,
					&b[0], &blen[0],
					&b[1], &blen[1],
					&b[2], &blen[2],
					&b[3], &blen[3],
					&b[4], &blen[4],
					&b[5], &blen[5] );
		}

#define USE_TURBO_JPEG
#if !defined( USE_TURBO_JPEG )
		stbi_uc * data[6];
#else
		unsigned char * data[6];
#endif

		int resolutionX = 0;
		int resolutionY = 0;
		int buffCount = 0;
		for ( ; buffCount < numBuffers; buffCount++ )
		{
			int	x, y;
			unsigned * b1 = b[buffCount];
			int b1len = blen[buffCount];

#if !defined( USE_TURBO_JPEG )
			int comp;
			data[buffCount] = stbi_load_from_memory( (const stbi_uc*)b1, b1len, &x, &y, &comp, 4 );
#else
			data[buffCount] = TurboJpegLoadFromMemory( (unsigned char*)b1, b1len, &x, &y );
#endif
			if ( buffCount == 0 )
			{
				resolutionX = x;
				resolutionY = y;
			}

			// done with the loading buffer now
			free( b1 );

			if ( data[buffCount] == NULL )
			{
				LOG( "LoadingThread: failed to load from buffer" );
				break;
			}
		}

		if ( buffCount != numBuffers )	// an image load failed, free everything and abort this load
		{
			for ( int i = 0; i < numBuffers; ++i )
			{
				free( data[i] );
				data[i] = NULL;
			}
		}
		else
		{
			if ( numBuffers == 1 )
			{
				OVR_ASSERT( data[0] != NULL );
				LOG( "Queue3.PostPrintf( \"%s %p %i %i\" )", commandName, data[0], resolutionX, resolutionY );
				( ( Oculus360Photos * )v )->GetBGMessageQueue( ).PostPrintf( "%s %p %i %i", commandName, data[ 0 ], resolutionX, resolutionY );
			}
			else 
			{
				OVR_ASSERT( numBuffers == 6 );
				LOG( "Queue3.PostPrintf( \"%s %i %p %p %p %p %p %p\" )", commandName, resolutionX, 
						data[0], data[1], data[2], data[3], data[4], data[5] );
				( ( Oculus360Photos * )v )->GetBGMessageQueue( ).PostPrintf( "%s %i %p %p %p %p %p %p", commandName, resolutionX,
						data[0], data[1], data[2], data[3], data[4], data[5] );
			}
		}

		free( (void *)msg );
	}
	return NULL;
}

static const int NUM_QUEUE_THREADS = 2;

void InitFileQueue( App * app, Oculus360Photos * photos )
{
    // spawn the queue threads
	void * (*funcs[NUM_QUEUE_THREADS])( void *) = { Queue1Thread, Queue3Thread };

	for ( int i = 0; i < NUM_QUEUE_THREADS; i++ )
	{
		pthread_attr_t loadingThreadAttr;
		pthread_attr_init( &loadingThreadAttr );
		sched_param sparam;
		sparam.sched_priority = Thread::GetOSPriority( Thread::NormalPriority );
		pthread_attr_setschedparam( &loadingThreadAttr, &sparam );
		pthread_t	loadingThread;
		int createLoadingThreadErr = -1;
		if ( i == 1 )
		{
			createLoadingThreadErr = pthread_create( &loadingThread, &loadingThreadAttr, funcs[ i ], ( void * )photos );
		}
		else
		{
			createLoadingThreadErr = pthread_create( &loadingThread, &loadingThreadAttr, funcs[ i ], ( void * )app );
		}
		if ( createLoadingThreadErr != 0 )
		{
			LOG( "loadingThread: pthread_create returned %i", createLoadingThreadErr );
		}
	}
}

}	// namespace OVR
