/*
	vis.h

	PVS/PHS generation tool

	Copyright (C) 1996-1997  Id Software, Inc.
	Copyright (C) 2002 Colin Thompson

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License
	as published by the Free Software Foundation; either version 2
	of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

	See the GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to:

		Free Software Foundation, Inc.
		59 Temple Place - Suite 330
		Boston, MA  02111-1307, USA

*/

#ifndef __vis_h
#define __vis_h

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#if defined (HAVE_PTHREAD_H) && defined (HAVE_PTHREAD)
#define USE_PTHREADS
#endif

#ifdef USE_PTHREADS
#include <pthread.h>
extern pthread_rwlock_t *global_lock;
extern pthread_rwlock_t *portal_locks;

#define	WRLOCK(l) \
	do { \
		if (options.threads > 1) \
			pthread_rwlock_wrlock (l); \
	} while (0)

#define	RDLOCK(l) \
	do { \
		if (options.threads > 1) \
			pthread_rwlock_rdlock (l); \
	} while (0)

#define	UNLOCK(l)	\
	do { \
		if (options.threads > 1)  \
			pthread_rwlock_unlock (l); \
	} while (0)

#define WRLOCK_PORTAL(p) WRLOCK (&portal_locks[p - portals])
#define RDLOCK_PORTAL(p) RDLOCK (&portal_locks[p - portals])
#define UNLOCK_PORTAL(p) UNLOCK (&portal_locks[p - portals])

#else
#define	LOCK
#define	UNLOCK
#endif

#include "QF/set.h"

#define	MAX_PORTALS				32768
#define	PORTALFILE				"PRT1"
#define	PORTALFILE_AM			"PRT1-AM"
#define	PORTALFILE2				"PRT2"
#define	ON_EPSILON				0.1
#define MAX_POINTS_ON_WINDING	64
#define	MAX_PORTALS_ON_CLUSTER	128

typedef struct {
	qboolean    original;	// don't free, it's part of the portal
	int	        numpoints;
	vec3_t      points[8];	// variable sized
} winding_t;

typedef enum {
	stat_none,
	stat_selected,
	stat_working,
	stat_done
} vstatus_t;

typedef struct {
	plane_t     plane;		// normal pointing into neighbor
	int         cluster;	// neighbor
	sphere_t    sphere;		// bounding sphere
	winding_t  *winding;
	vstatus_t   status;
	set_t      *visbits;
	set_t      *mightsee;
	int         nummightsee;
	int         numcansee;
} portal_t;

typedef struct seperating_plane_s {
	struct seperating_plane_s *next;
	plane_t     plane;		// from portal is on positive side
} sep_t;

typedef struct passage_s {
	struct passage_s *next;
	int         from, to;	// cluster numbers
	sep_t      *planes;
} passage_t;

typedef struct cluster_s {
	int         numportals;
	passage_t  *passages;
	portal_t   *portals[MAX_PORTALS_ON_CLUSTER];
	int         visofs;
} cluster_t;

typedef struct pstack_s {
	struct pstack_s *next;		///< linked list of active stack objects
	cluster_t  *cluster;		///< the cluster being sub-vised
	winding_t  *source_winding;	///< clipped source portal winding
	portal_t   *pass_portal;	///< the portal exiting from the cluster
	winding_t  *pass_winding;	///< clipped pass portal winding
	plane_t     pass_plane;		///< plane of the pass portal
	set_t      *mightsee;
	sep_t      *separators[2];
} pstack_t;

typedef struct {
	int         portaltest;		///< number of portals tested via separators
	int         portalpass;		///< number of portals through which vis passes
	int         portalcheck;	///< number of portal checks
	int         targettested;	///< number of times target portal tested
	int         targettrimmed;	///< number of times target portal trimmed
	int         targetclipped;	///< number of times target portal clipped away
	int         sourcetested;	///< number of times source portal tested
	int         sourcetrimmed;	///< number of times source portal trimmed
	int         sourceclipped;	///< number of times source portal clipped away
	int         chains;			///< number of visits to clusters
	int         mighttest;		///< amount mightsee is used for masked tests
	int         vistest;		///< amount visbits is used for masked tests
	int         mightseeupdate;	///< amount of updates to waiting portals
} visstat_t;

typedef struct threaddata_s {
	visstat_t   stats;			///< per-thread statistics merged on completion
	set_t      *clustervis;		///< clusters base portal can see
	portal_t   *base;			///< portal for which this thread is being run
	pstack_t    pstack_head;
	sep_t      *sep_freelist;	///< per-thread list of free separators
	set_pool_t  set_pool;
} threaddata_t;

typedef struct {
	set_t      *portalsee;
	int         clustersee;
} basethread_t;

extern int numportals;
extern int portalclusters;
extern int numrealleafs;
extern int bitbytes;
extern int bitbytes_l;
extern int bitlongs;
extern struct bsp_s *bsp;

extern portal_t *portals;
extern cluster_t *clusters;
extern int *leafcluster;
extern byte *uncompressed;

void FreeWinding (winding_t *w);
winding_t *NewWinding (int points);
winding_t *ClipWinding (winding_t *in, const plane_t *split, qboolean keepon);
winding_t *CopyWinding (const winding_t *w);

void ClusterFlow (int clusternum);
void PortalBase (basethread_t *thread, portal_t *portal);
void PortalFlow (threaddata_t *data, portal_t *portal);
void CalcAmbientSounds (void);

#endif// __vis_h
