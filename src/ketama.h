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

#ifdef __cplusplus /* If this is a C++ compiler, use C linkage */
extern "C" {
#endif

#include <string.h>

typedef int (*compfn)( const void*, const void* );

typedef struct
{
    unsigned int point;  // point on circle
    char ip[22];
} mcs;

typedef struct
{
    char addr[22];
    unsigned long memory;
} serverinfo;

typedef struct
{
    int numpoints;
    void* array; //array of mcs structs
} continuum;

typedef continuum* ketama_continuum;

/** \brief Frees any allocated memory.
  * \param contptr The continuum that you want to be destroy. */
void ketama_smoke( ketama_continuum contptr );

/** \brief Maps a key onto a server in the continuum.
  * \param key The key that you want to map to a specific server.
  * \param cont Pointer to the continuum in which we will search.
  * \return The mcs struct that the given key maps to. */
mcs* ketama_get_server( char* key, size_t keyLen, ketama_continuum );

/** \brief Print the server list of a continuum to stdout.
  * \param cont The continuum to print. */
void ketama_print_continuum( ketama_continuum c );

/** \brief Compare two server entries in the circle.
  * \param a The first entry.
  * \param b The second entry.
  * \return -1 if b greater a, +1 if a greater b or 0 if both are equal. */
//int ketama_compare( mcs*, mcs* );


#ifdef __cplusplus /* If this is a C++ compiler, end C linkage */
}
#endif

