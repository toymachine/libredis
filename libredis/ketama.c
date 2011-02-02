/*
* Copyright (c) 2007, Last.fm, All rights reserved.
* Richard Jones <rj@last.fm>
* Christian Muehlhaeuser <chris@last.fm>
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*     * Redistributions of source code must retain the above copyright
*       notice, this list of conditions and the following disclaimer.
*     * Redistributions in binary form must reproduce the above copyright
*       notice, this list of conditions and the following disclaimer in the
*       documentation and/or other materials provided with the distribution.
*     * Neither the name of the Last.fm Limited nor the
*       names of its contributors may be used to endorse or promote products
*       derived from this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY Last.fm ``AS IS'' AND ANY
* EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
* DISCLAIMED. IN NO EVENT SHALL Last.fm BE LIABLE FOR ANY
* DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
* (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
* LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
* ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
* SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "ketama.h"
#include "md5.h"

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>           /* floor & floorf                       */
#include <string.h>

#include "common.h"
#include "alloc.h"
#include "list.h"

#define INIT_MAX_SERVERS 1

typedef struct
{
    unsigned int point;  // point on circle
    int ordinal; // entry of server info
} mcs;

typedef struct
{
    char addr[ADDR_SIZE];
    unsigned long memory;
} serverinfo;

struct _Ketama
{
    int numpoints;
    unsigned int numservers;
    unsigned int maxservers;
    unsigned long memory;
    mcs* continuum; //array of mcs structs
    serverinfo *servers; //array of numservers serverinfo structs
};

typedef int (*compfn)( const void*, const void* );
int Ketama_compare( mcs *a, mcs *b );


Ketama *Ketama_new()
{
	Ketama *ketama = Alloc_alloc_T(Ketama);
	ketama->numpoints = 0;
	ketama->numservers = 0;
	ketama->maxservers = 0;
	ketama->memory = 0;
	ketama->continuum = NULL;
	ketama->servers = NULL;
	return ketama;
}

void Ketama_free(Ketama *ketama)
{
	if(ketama->continuum != NULL) {
		Alloc_free(ketama->continuum, ketama->numservers * sizeof(mcs) * 160);
		ketama->continuum = NULL;
	}
	if(ketama->servers != NULL) {
		Alloc_free(ketama->servers, ketama->maxservers * sizeof(serverinfo));
		ketama->servers = NULL;
	}
	Alloc_free_T(ketama, Ketama);
}

int Ketama_add_server(Ketama *ketama, const char *addr, int port, unsigned long weight)
{
	assert(ketama->numservers <= ketama->maxservers);
	if(ketama->servers == NULL) {
		DEBUG(("ketama init server list\n"));
		ketama->maxservers = INIT_MAX_SERVERS;
		ketama->servers = Alloc_alloc(sizeof(serverinfo) * ketama->maxservers);
	}
	if(ketama->numservers >= ketama->maxservers) {
		int oldmaxservers = ketama->maxservers;
		ketama->maxservers *= 2;
		DEBUG(("ketama expand server list from %d to %d entries\n", oldmaxservers, ketama->maxservers));
		ketama->servers = Alloc_realloc(ketama->servers, sizeof(serverinfo) * ketama->maxservers, sizeof(serverinfo) * oldmaxservers);
	}
	serverinfo *info = &(ketama->servers[ketama->numservers]);
	snprintf(info->addr, sizeof(info->addr), "%s:%d", addr, port); //TODO check error (e.g. address too long)
	info->memory = weight;
	ketama->numservers += 1;
	ketama->memory += weight;

	return 0;
}

/** \brief Hashing function, converting a string to an unsigned int by using MD5.
  * \param inString The string that you want to hash.
  * \return The resulting hash. */
//unsigned int ketama_hashi( char* inString, size_t inLen );

/** \brief Hashinf function to 16 bytes char array using MD%.
 * \param inString The string that you want to hash.
 * \param md5pword The resulting hash. */
//void ketama_md5_digest( char* inString, size_t inLen, unsigned char md5pword[16] );

/* ketama.h does not expose this function */
void Ketama_md5_digest( const char* inString, size_t inLen, unsigned char md5pword[16] )
{
    md5_state_t md5state;

    md5_init( &md5state );
    md5_append( &md5state, (unsigned char *)inString, inLen);
    md5_finish( &md5state, md5pword );
}

unsigned int Ketama_hashi( const char* inString, size_t inLen )
{
    unsigned char digest[16];

    Ketama_md5_digest( inString, inLen, digest );
    return (unsigned int)(( digest[3] << 24 )
                        | ( digest[2] << 16 )
                        | ( digest[1] <<  8 )
                        |   digest[0] );
}

char *Ketama_get_server_address(Ketama *ketama, int ordinal)
{
	if (ordinal < 0 || ordinal >= ketama->numservers || !ketama->servers) {
		DEBUG(("Incorrect call to Ketama_get_server_address"));
		return "";
	}

	return ketama->servers[ordinal].addr;
}


int Ketama_get_server_ordinal(Ketama *ketama, const char* key, size_t key_len)
{
	if (!ketama->continuum) {
		return -1;
	}

    unsigned int h = Ketama_hashi( key, key_len );
    int highp = ketama->numpoints;
    mcs *mcsarr = ketama->continuum;
    int lowp = 0, midp;
    unsigned int midval, midval1;

    // divide and conquer array search to find server with next biggest
    // point after what this key hashes to
    while ( 1 )
    {
        midp = (int)( ( lowp+highp ) / 2 );

        if ( midp == ketama->numpoints ) {
            return mcsarr[0].ordinal; // if at the end, roll back to zeroth
        }

        midval = mcsarr[midp].point;
        midval1 = midp == 0 ? 0 : mcsarr[midp-1].point;

        if ( h <= midval && h > midval1 ) {
            return mcsarr[midp].ordinal;
        }

        if ( midval < h ) {
            lowp = midp + 1;
        }
        else {
            highp = midp - 1;
        }

        if ( lowp > highp ) {
            return mcsarr[0].ordinal;
        }
    }
}


/** \brief Generates the continuum of servers (each server as many points on a circle).
  * \param key Shared memory key for storing the newly created continuum.
  * \param filename Server definition file, which will be parsed to create this continuum.
  * \return 0 on failure, 1 on success. */
void Ketama_create_continuum(Ketama *ketama)
{
	if (ketama->numservers == 0 || ketama->continuum) {
		DEBUG(("Ketama_create_continuum called in incorrect state"));
		return;
	}

	DEBUG(("Server definitions read: %u servers, total memory: %lu.\n", ketama->numservers, ketama->memory ));

    /* Continuum will hold one mcs for each point on the circle: */
    ketama->continuum = Alloc_alloc(ketama->numservers * sizeof(mcs) * 160);
    unsigned k, cont = 0;

    for(int i = 0; i < ketama->numservers; i++)
    {
        serverinfo *sinfo = &(ketama->servers[i]);
    	float pct = (float)sinfo->memory / (float)ketama->memory;
        unsigned int ks = floorf( pct * 40.0 * (float)ketama->numservers );
#ifndef NDEBUG
        int hpct = floorf( pct * 100.0 );
        DEBUG(("Server no. %d: %s (mem: %lu = %u%% or %d of %d)\n", i, sinfo->addr, sinfo->memory, hpct, ks, ketama->numservers * 40 ));
#endif

        for( k = 0; k < ks; k++ )
        {
            /* 40 hashes, 4 numbers per hash = 160 points per server */
            char ss[ADDR_SIZE + 11];
            unsigned char digest[16];

            int len = snprintf( ss, sizeof(ss), "%s-%d", sinfo->addr, k );
            if (len > sizeof(ss) - 1)
            	len = sizeof(ss) - 1;
            Ketama_md5_digest( ss, len, digest );

            /* Use successive 4-bytes from hash as numbers 
             * for the points on the circle: */
            int h;
            for( h = 0; h < 4; h++ )
            {
                ketama->continuum[cont].point = ( digest[3+h*4] << 24 )
                                      | ( digest[2+h*4] << 16 )
                                      | ( digest[1+h*4] <<  8 )
                                      |   digest[h*4];

				ketama->continuum[cont].ordinal = i;
                cont++;
            }
        }
    }

    DEBUG(("cont: %d\n", cont));
    ketama->numpoints = cont;
    //free( slist );

    /* Sorts in ascending order of "point" */
    qsort( (void*) ketama->continuum, cont, sizeof( mcs ), (compfn)Ketama_compare );

}

void Ketama_print_continuum( Ketama *ketama )
{
    int a;
    printf( "Numpoints in continuum: %d\n", ketama->numpoints );

    if ( ketama->continuum == NULL )
    {
        printf( "Continuum empty\n" );
    }
    else
    {
        for( a = 0; a < ketama->numpoints; a++ )
        {
            printf( "%d (%u)\n", ketama->continuum[a].ordinal, ketama->continuum[a].point );
        }
    }
}

int Ketama_compare( mcs *a, mcs *b )
{
    return ( a->point < b->point ) ?  -1 : ( ( a->point > b->point ) ? 1 : 0 );
}

