#ifndef __SEARCH_BORDER_H__
#define __SEARCH_BORDER_H__

// This file (C) 2004-2009 Steven Boswell.  All rights reserved.
// Released to the public under the GNU General Public License v2.
// See the file COPYING for more information.

#include "config.h"
#include <assert.h>
#include <new>
#include "mjpeg_types.h"
#include "TemplateLib.hh"
#include "Limits.hh"
#include "DoublyLinkedList.hh"
#include "Allocator.hh"
#include "SetRegion2D.hh"

// HACK: for development error messages.
#include <stdio.h>



// Define this to print region unions/subtractions.
#ifdef DEBUG_REGION2D
//	#define PRINTREGIONMATH
#endif // DEBUG_REGION2D



// Define this to print details of the search-border's progress.
#ifdef DEBUG_REGION2D
//	#define PRINT_SEARCHBORDER
#endif // DEBUG_REGION2D



// The generic search-border class.  It's parameterized by the numeric
// type to use for pixel indices and a numeric type big enough to hold
// the product of the largest expected frame width/height.
// When constructed, it's configured with the size of the frame in which
// it operates, and the width/height of pixel groups to operate on.
//
// The search border keeps track of all regions on the border between
// the searched area and the not-yet-searched area.  When no new
// pixel-group could possibly intersect a region, that region is removed
// from the border and handed back to the client.
//
// The meaning of the regions it keeps track of is abstract.  The
// original purpose was to help the motion-searcher keep track of moved
// regions, i.e. pixels in the new frame that match pixels in the
// reference frame, only moved.  But it could presumably be used for
// other purposes, e.g. if it assembled regions of pixels that had
// exactly one frame reference, it could be used to detect bit noise,
// film lint/scratches, or even LaserDisc rot.
//
// As the search-border proceeds through the frame, it maintains a list
// of all regions that intersect with the current pixel-group.  That
// way, when it comes time to add a new match, all regions that
// intersect the current pixel-group are already known, and duplicate
// matches can be skipped.
template <class PIXELINDEX, class FRAMESIZE>
class SearchBorder
{
public:
	typedef Region2D<PIXELINDEX,FRAMESIZE> BaseRegion_t;
		// The base class for all our region types.

	#ifdef SET_REGION_IMPLEMENTED_WITH_VECTOR
	typedef Vector<typename BaseRegion_t::Extent,
				typename BaseRegion_t::Extent,
				Ident<typename BaseRegion_t::Extent,
					typename BaseRegion_t::Extent>,
				Less<typename BaseRegion_t::Extent> >
			RegionImp_t;
	#else // SET_REGION_IMPLEMENTED_WITH_VECTOR
	typedef SkipList<typename BaseRegion_t::Extent,
				typename BaseRegion_t::Extent,
				Ident<typename BaseRegion_t::Extent,
					typename BaseRegion_t::Extent>,
				Less<typename BaseRegion_t::Extent> >
			RegionImp_t;
	#endif // SET_REGION_IMPLEMENTED_WITH_VECTOR
		// The container class that implements our set-based region.

	typedef SetRegion2D<PIXELINDEX,FRAMESIZE,RegionImp_t> Region_t;
		// How we use Region2D<>.

	class MovedRegion;
		// A moved region of pixels that has been detected.
		// Derived from Region_t; defined below.

	SearchBorder (typename Region_t::Allocator &a_rAlloc
			= Region_t::Extents::Imp::sm_oNodeAllocator);
		// Default constructor.

	virtual ~SearchBorder();
		// Destructor.

	void Init (Status_t &a_reStatus,
			PIXELINDEX a_tnWidth, PIXELINDEX a_tnHeight,
			PIXELINDEX a_tnSearchRadiusX, PIXELINDEX a_tnSearchRadiusY,
			PIXELINDEX a_tnPGW, PIXELINDEX a_tnPGH);
		// Initializer.  Provide the dimensions of the frame, of the search
		// window, and of pixel-groups.

	void StartFrame (Status_t &a_reStatus);
		// Initialize the search-border, i.e. start in the upper-left
		// corner.

	void MoveRight (Status_t &a_reStatus);
		// Move one pixel to the right, adding and removing regions from
		// the potentially-intersecting list.

	void MoveLeft (Status_t &a_reStatus);
		// Move one pixel to the left, adding and removing regions from
		// the potentially-intersecting list.

	void MoveDown (Status_t &a_reStatus);
		// Move down a line.  Find all regions that can no longer
		// be contiguous with new matches, and hand them back to the
		// client.  Then rotate the border structures, making the
		// least-recent current border into the last border, etc.

	inline FRAMESIZE NumberOfActiveRegions (void) const;
		// Return the number of regions that would intersect with the
		// current pixel-group.

	FRAMESIZE GetExistingMatchSize (PIXELINDEX a_tnMotionX,
			PIXELINDEX a_tnMotionY) const;
		// Determine if a region with this motion vector already
		// intersects/borders the current pixel-group (i.e. it doesn't
		// need to be flood-filled and added again).
		// If so, return its size.
		// If not, return zero.

	void AddNewRegion (Status_t &a_reStatus, MovedRegion &a_rRegion);
		// Add the given region, with the given motion-vector.
		// Causes a_rRegion to be emptied.

	void RemoveRegion (MovedRegion *a_pRegion);
		// Removes the given region from the search-border.

	MovedRegion *ChooseBestActiveRegion (Status_t &a_reStatus,
			FRAMESIZE &a_rtnSecondBestActiveRegionSize);
		// Loop through all regions that matched the current pixel-group,
		// find the single best one, and return it.
		// Backpatch the size of the second-best active-region.

	void FinishFrame (Status_t &a_reStatus);
		// Clean up the search border at the end of a frame, e.g. hand
		// all remaining regions back to the client.

	virtual void OnCompletedRegion (Status_t &a_reStatus,
			MovedRegion *a_pRegion) = 0;
		// Hand a completed region to our client.  Subclasses must
		// override this to describe how to do this.

	void DeleteRegion (MovedRegion *a_pRegion);
		// Delete a region that was returned by ChooseBestActiveRegion()
		// or OnCompletedRegion().

	// A moved region of pixels that has been detected.
	// All extents are in the coordinate system of the new frame; that
	// makes it easy to unify/subtract regions without regard to their
	// motion vector.
	class MovedRegion : public Region_t
	{
	private:
		typedef Region_t BaseClass;
			// Keep track of who our base class is.

		void *operator new (size_t);
		void operator delete (void *) {}
		void *operator new[] (size_t);
		void operator delete[] (void *);
			// Disallow allocation from system memory.
			// (This helps enforce the use of DeleteRegion().)

	public:
		MovedRegion (typename BaseClass::Allocator &a_rAlloc
				= BaseClass::Extents::Imp::sm_oNodeAllocator);
			// Default constructor.  Must be followed by Init().

		MovedRegion (Status_t &a_reStatus, typename BaseClass::Allocator
				&a_rAlloc = BaseClass::Extents::Imp::sm_oNodeAllocator);
			// Initializing constructor.  Creates an empty region.

		MovedRegion (Status_t &a_reStatus, const MovedRegion &a_rOther);
			// Copy constructor.

		void Init (Status_t &a_reStatus);
			// Initializer.  Must be called on default-constructed
			// regions.

		void Assign (Status_t &a_reStatus, const MovedRegion &a_rOther);
			// Make the current region a copy of the other region.

		virtual ~MovedRegion();
			// Destructor.

		inline void Move (MovedRegion &a_rOther);
			// Move the contents of the other region into the current
			// region.
			// The current region must be empty.

		inline void SetMotionVector (PIXELINDEX a_tnX,
				PIXELINDEX a_tnY);
			// Set the motion vector.

		inline void GetMotionVector (PIXELINDEX &a_rtnX,
				PIXELINDEX &a_rtnY) const;
			// Get the motion vector.

		// Comparison class, suitable for Set<>.
		class SortBySizeThenMotionVectorLength
		{
		public:
			inline bool operator() (const MovedRegion *a_pLeft,
				const MovedRegion *a_pRight) const;
		};

		inline FRAMESIZE GetSquaredMotionVectorLength (void) const;
			// Get the squared length of the motion vector.
			// Needed by SortBySizeThenMotionVectorLength().

		FRAMESIZE m_tnReferences;
			// The number of beginnings/endings of extents of this
			// region that are on the border.  When this goes to zero,
			// that means no other matches could possibly be added to
			// the region, and m_pRegion will get handed back to the
			// search-border's client.

	private:
		PIXELINDEX m_tnMotionX, m_tnMotionY;
			// The motion vector associated with this region.

		FRAMESIZE m_tnSquaredLength;
			// The squared length of the motion vector.
			// Used for sorting.

#ifndef NDEBUG

	public:
		PIXELINDEX m_tnX, m_tnY;
			// The location of the pixel-group-match that originally
			// created this region.

		// The number of region objects in existence.
	private:
		static uint32_t sm_ulInstances;
	public:
		static uint32_t GetInstances (void)
			{ return sm_ulInstances; }

#endif // NDEBUG
	};

private:
	typename Region_t::Allocator &m_rSetRegionExtentAllocator;
		// Where we get memory to allocate set-region extents.

	PIXELINDEX m_tnWidth, m_tnHeight;
		// The dimension of each reference frame.

	PIXELINDEX m_tnSearchRadiusX, m_tnSearchRadiusY;
		// The search radius, i.e. how far from the current pixel
		// group we look when searching for possible moved instances of
		// the group.

	PIXELINDEX m_tnPGW, m_tnPGH;
		// The dimension of pixel groups.

	PIXELINDEX m_tnX, m_tnY;
		// The index of the current pixel group.  Actually the index
		// of the top-left pixel in the current pixel group.  This
		// gets moved in a zigzag pattern, back and forth across the
		// frame and then down, until the end of the frame is reached.

	PIXELINDEX m_tnStepX;
		// Whether we're zigging or zagging.

	// A structure that keeps track of which active-region (if any) exists
	// for a given motion-vector.
	struct MotionVectorMatch
	{
	public:
		FRAMESIZE m_tnLargestRegionSize;
			// The size of the largest active-region with this
			// motion-vector.

		FRAMESIZE m_tnActiveExtents;
			// The number of active-region extents that intersect/border
			// the current pixel-group.

		MotionVectorMatch() : m_tnLargestRegionSize (0),
				m_tnActiveExtents (0) {}
			// Default constructor.
	};

	MotionVectorMatch *m_pMotionVectorMatches;
		// An array of ((2 * m_tnSearchRadiusX + 1)
		// * (2 * m_tnSearchRadiusY + 1) cells that keep track of which
		// active-region, if any, exists for a given motion-vector.

	MotionVectorMatch &GetMotionVectorMatch (PIXELINDEX a_tnMotionX,
			PIXELINDEX a_tnMotionY) const;
		// Get the motion-vector-match cell for this motion-vector.

	void AddMotionVectorMatch (const MovedRegion *a_pRegion);
		// Add the given region to the motion-vector-match array.

	void RemoveMotionVectorMatch (const MovedRegion *a_pRegion);
		// Remove the given region from the motion-vector-match array.

	// A class that keeps track of region extents on the border between
	// the searched area and the not-yet-searched area, i.e. the only
	// regions that have a chance of growing.
	class BorderExtentBoundary
	{
	public:
		PIXELINDEX m_tnIndex;
			// The index of the endpoint of an extent.
			// (An endpoint can be a beginning or an ending.)

		PIXELINDEX m_tnLine;
			// The vertical line on which this endpoint resides.

		bool m_bIsEnding;
			// false if this is the beginning of an extent, true if
			// it's the end of an extent.

		#ifdef BORDEREXTENTBOUNDARYSET_IMPLEMENTED_WITH_VECTOR
		PIXELINDEX m_tnCounterpartIndex;
		#else // BORDEREXTENTBOUNDARYSET_IMPLEMENTED_WITH_VECTOR
		BorderExtentBoundary *m_pCounterpart;
		#endif // BORDEREXTENTBOUNDARYSET_IMPLEMENTED_WITH_VECTOR
			// The ending to go with this beginning, or the beginning to
			// go with this ending.
			//
			// If our corresponding set class is implemented with skip
			// lists, this can be a pointer, because the address of nodes
			// doesn't change after they're created.  This allows the
			// counterpart to be found without doing a search.
			//
			// If our corresponding set class is implemented with vectors,
			// this has to be a pixel-index, and the counterpart has to be
			// found with a search, because the address of nodes can change
			// after they're created.

		MovedRegion *m_pRegion;
			// The region with the given extent.

		PIXELINDEX m_tnMotionX, m_tnMotionY;
			// The region's motion vector.  Copied here so that our sort
			// order doesn't depend on m_pRegion's contents, i.e. so
			// that we thrash memory less.

		BorderExtentBoundary();
			// Default constructor.

		BorderExtentBoundary (PIXELINDEX a_tnIndex, PIXELINDEX a_tnLine,
				bool a_bIsEnding, MovedRegion *a_pRegion);
			// Initializing constructor.

		~BorderExtentBoundary();
			// Destructor.

		#ifndef NDEBUG
		bool operator == (const BorderExtentBoundary &a_rOther) const;
		#endif // !NDEBUG
			// Equality operator.

		// Comparison class, suitable for Set<>.
		class SortByIndexThenTypeThenMotionVectorThenRegionAddress
		{
		public:
			inline bool operator() (const BorderExtentBoundary &a_rLeft,
				const BorderExtentBoundary &a_rRight) const;
		};

		// Comparison class, suitable for Set<>.
		class SortByMotionVectorThenTypeThenRegionAddress
		{
		public:
			inline bool operator() (const BorderExtentBoundary &a_rLeft,
				const BorderExtentBoundary &a_rRight) const;
		};
	};

	#ifdef BORDEREXTENTBOUNDARYSET_IMPLEMENTED_WITH_VECTOR
	typedef Set<BorderExtentBoundary, typename BorderExtentBoundary
		::SortByIndexThenTypeThenMotionVectorThenRegionAddress,
		Vector<BorderExtentBoundary,BorderExtentBoundary,
			Ident<BorderExtentBoundary,BorderExtentBoundary>,
			typename BorderExtentBoundary
			::SortByIndexThenTypeThenMotionVectorThenRegionAddress> >
		BorderExtentBoundarySet;
	#else // BORDEREXTENTBOUNDARYSET_IMPLEMENTED_WITH_VECTOR
	typedef Set<BorderExtentBoundary, typename BorderExtentBoundary
		::SortByIndexThenTypeThenMotionVectorThenRegionAddress,
		SkipList<BorderExtentBoundary,BorderExtentBoundary,
			Ident<BorderExtentBoundary,BorderExtentBoundary>,
			typename BorderExtentBoundary
			::SortByIndexThenTypeThenMotionVectorThenRegionAddress> >
		BorderExtentBoundarySet;
	#endif // BORDEREXTENTBOUNDARYSET_IMPLEMENTED_WITH_VECTOR
	typedef typename BorderExtentBoundarySet::Allocator BEBS_Allocator_t;
	BEBS_Allocator_t m_oBorderExtentsAllocator;
	BorderExtentBoundarySet m_setBorderStartpoints,
			m_setBorderEndpoints;
		// The borders, i.e. the startpoint/endpoints for every
		// region under construction, for every line in the current
		// pixel-group's vertical extent.

	typename BorderExtentBoundarySet::ConstIterator
			*m_paitBorderStartpoints,
			*m_paitBorderEndpoints;
		// The next last/current/first-border startpoints/endpoints whose
		// regions will be added or removed, when we move left or right.
		// (1 + m_tnPGH + 1 iterators allocated for each.)

	void FixStartpointEndpointIterators (void);
		// Fix the last/current/first-border startpoints/endpoint
		// iterators.  Necessary after adding or removing a region.

	bool m_bFixStartpointEndpointIterators;
		// True if the startpoint/endpoint-iterators need to be fixed.
		// Used to avoid needless recalculation during match-throttling.

	typedef Set<BorderExtentBoundary, typename BorderExtentBoundary
		::SortByMotionVectorThenTypeThenRegionAddress>
		IntersectingRegionsSet;
	typedef typename IntersectingRegionsSet::Allocator IRS_Allocator_t;
	IRS_Allocator_t m_oBorderRegionsAllocator;
	IntersectingRegionsSet m_setBorderRegions;
		// All regions that could possibly intersect the current pixel
		// group, should a match be found.  Sorted by motion vector,
		// since matches must be added to a region with the exact same
		// motion vector.  There may be more than one such region; that
		// means the current match causes those regions to be contiguous
		// and thus they will get merged together.
		// (Note that this set is also sorted by type, i.e. whether
		// it's a beginning or end.  We only put beginnings into this
		// set.  This extra sort criteria is used to help us find the
		// range of regions that all have the same motion vector, to let
		// us set up an umambiguous upper-bound for the search.)

	typedef Allocator<1> MovedRegionAllocator_t;
	MovedRegionAllocator_t m_oMovedRegionAllocator;
		// Where we get memory to allocate MovedRegion instances.

#ifndef NDEBUG
public:
	static uint32_t GetMovedRegionCount (void)
		{ return MovedRegion::GetInstances(); }
#endif // !NDEBUG
};



// Default constructor.
template <class PIXELINDEX, class FRAMESIZE>
SearchBorder<PIXELINDEX,FRAMESIZE>::SearchBorder
		(typename Region_t::Allocator &a_rAlloc)
	: m_rSetRegionExtentAllocator (a_rAlloc),
	m_oBorderExtentsAllocator (1048576),
	m_setBorderStartpoints (typename BorderExtentBoundary
		::SortByIndexThenTypeThenMotionVectorThenRegionAddress(),
		m_oBorderExtentsAllocator),
	m_setBorderEndpoints (typename BorderExtentBoundary
		::SortByIndexThenTypeThenMotionVectorThenRegionAddress(),
		m_oBorderExtentsAllocator),
	m_oBorderRegionsAllocator (131072),
	m_setBorderRegions (typename BorderExtentBoundary
		::SortByMotionVectorThenTypeThenRegionAddress(),
		m_oBorderRegionsAllocator),
	m_oMovedRegionAllocator (262144)
{
	// No frame dimensions yet.
	m_tnWidth = m_tnHeight = PIXELINDEX (0);

	// No search radius yet.
	m_tnSearchRadiusX = m_tnSearchRadiusY = PIXELINDEX (0);

	// No pixel-group width/height yet.
	m_tnPGW = m_tnPGH = PIXELINDEX (0);

	// No active search yet.
	m_tnX = m_tnY = m_tnStepX = PIXELINDEX (0);

	// No motion-vector matches yet.
	m_pMotionVectorMatches = NULL;

	// No startpoint/endpoint iterators yet.
	m_paitBorderStartpoints = m_paitBorderEndpoints = NULL;

	// No need to recalculate startpoint/endpoint-iterators yet.
	m_bFixStartpointEndpointIterators = false;
}



// Destructor.
template <class PIXELINDEX, class FRAMESIZE>
SearchBorder<PIXELINDEX,FRAMESIZE>::~SearchBorder()
{
	// Make sure our client didn't stop in the middle of a frame.
	assert (m_setBorderStartpoints.Size() == 0);
	assert (m_setBorderEndpoints.Size() == 0);
	assert (m_setBorderRegions.Size() == 0);

	// Free up our motion-vector matches.
	delete[] m_pMotionVectorMatches;

	// Free up our arrays of iterators.
	delete[] m_paitBorderStartpoints;
	delete[] m_paitBorderEndpoints;
}



// Initializer.
template <class PIXELINDEX, class FRAMESIZE>
void
SearchBorder<PIXELINDEX,FRAMESIZE>::Init (Status_t &a_reStatus,
	PIXELINDEX a_tnWidth, PIXELINDEX a_tnHeight,
	PIXELINDEX a_tnSearchRadiusX, PIXELINDEX a_tnSearchRadiusY,
	PIXELINDEX a_tnPGW, PIXELINDEX a_tnPGH)
{
	FRAMESIZE tnMotionVectorMatches;
		// The number of motion-vector-match cells to allocate.

	// Make sure they didn't start us off with an error.
	assert (a_reStatus == g_kNoError);

	// Make sure the width & height are reasonable.
	assert (a_tnWidth > PIXELINDEX (0));
	assert (a_tnHeight > PIXELINDEX (0));

	// Initialize the sets that implement our border-regions.
	m_setBorderStartpoints.Init (a_reStatus, false);
	if (a_reStatus != g_kNoError)
		goto cleanup0;
	m_setBorderEndpoints.Init (a_reStatus, false);
	if (a_reStatus != g_kNoError)
		goto cleanup0;
	{
		typename IntersectingRegionsSet::InitParams oInitSetBorderRegions
			(rand(), true /* allocate internal nodes from allocator */);
		m_setBorderRegions.Init (a_reStatus, false, oInitSetBorderRegions);
		if (a_reStatus != g_kNoError)
			goto cleanup0;
	}

	// Allocate space for our iterators into the startpoint/endpoint
	// sets.  (These move left/right/down with the current pixel-group,
	// and run over regions that get added/removed from the border
	// regions set.)
	m_paitBorderStartpoints = new typename
		BorderExtentBoundarySet::ConstIterator[a_tnPGH + 2];
	if (m_paitBorderStartpoints == NULL)
		goto cleanup0;
	m_paitBorderEndpoints = new typename
		BorderExtentBoundarySet::ConstIterator[a_tnPGH + 2];
	if (m_paitBorderEndpoints == NULL)
		goto cleanup1;

	// Allocate space for motion-vector matches.
	tnMotionVectorMatches
		= (FRAMESIZE (2) * FRAMESIZE (a_tnSearchRadiusX) + FRAMESIZE (1))
		* (FRAMESIZE (2) * FRAMESIZE (a_tnSearchRadiusY) + FRAMESIZE (1));
	m_pMotionVectorMatches = new MotionVectorMatch[tnMotionVectorMatches];
	if (m_pMotionVectorMatches == NULL)
		goto cleanup2;

	// Finally, store our parameters.
	m_tnWidth = a_tnWidth;
	m_tnHeight = a_tnHeight;
	m_tnSearchRadiusX = a_tnSearchRadiusX;
	m_tnSearchRadiusY = a_tnSearchRadiusY;
	m_tnPGW = a_tnPGW;
	m_tnPGH = a_tnPGH;

	// All done.
	return;

	// Clean up after errors.
//cleanup3:
//	delete[] m_pMotionVectorMatches;
cleanup2:
	delete[] m_paitBorderEndpoints;
	m_paitBorderEndpoints = NULL;
cleanup1:
	delete[] m_paitBorderStartpoints;
	m_paitBorderStartpoints = NULL;
cleanup0:
	;
}



// Default constructor.  Must be followed by Init().
template <class PIXELINDEX, class FRAMESIZE>
SearchBorder<PIXELINDEX,FRAMESIZE>::MovedRegion::MovedRegion
		(typename BaseClass::Allocator &a_rAlloc)
	: BaseClass (a_rAlloc)
{
	// One more instance.
	#ifndef NDEBUG
	++sm_ulInstances;
	#endif // !NDEBUG

	// No motion-vector yet.
	m_tnMotionX = m_tnMotionY = PIXELINDEX (0);
	m_tnSquaredLength = FRAMESIZE (0);

	// No pixel-group-location yet.
	#ifndef NDEBUG
	m_tnX = m_tnY = PIXELINDEX (0);
	#endif // !NDEBUG

	// No references yet.
	m_tnReferences = 0;
}



// Initializing constructor.  Creates an empty region.
template <class PIXELINDEX, class FRAMESIZE>
SearchBorder<PIXELINDEX,FRAMESIZE>::MovedRegion::MovedRegion
		(Status_t &a_reStatus, typename BaseClass::Allocator &a_rAlloc)
	: BaseClass (a_rAlloc)
{
	// One more instance.
	#ifndef NDEBUG
	++sm_ulInstances;
	#endif // !NDEBUG

	// Make sure they didn't start us off with an error.
	assert (a_reStatus == g_kNoError);

	// No motion-vector yet.
	m_tnMotionX = m_tnMotionY = PIXELINDEX (0);
	m_tnSquaredLength = FRAMESIZE (0);

	// No pixel-group-location yet.
	#ifndef NDEBUG
	m_tnX = m_tnY = PIXELINDEX (0);
	#endif // !NDEBUG

	// No references yet.
	m_tnReferences = 0;

	// Initialize ourselves.
	Init (a_reStatus);
	if (a_reStatus != g_kNoError)
		return;
}



// Copy constructor.
template <class PIXELINDEX, class FRAMESIZE>
SearchBorder<PIXELINDEX,FRAMESIZE>::MovedRegion::MovedRegion
		(Status_t &a_reStatus, const MovedRegion &a_rOther)
	: BaseClass (a_reStatus, a_rOther)
{
	// One more instance.
	#ifndef NDEBUG
	++sm_ulInstances;
	#endif // !NDEBUG

	// No motion-vector yet.
	m_tnMotionX = m_tnMotionY = PIXELINDEX (0);
	m_tnSquaredLength = FRAMESIZE (0);

	// No pixel-group-location yet.
	#ifndef NDEBUG
	m_tnX = m_tnY = PIXELINDEX (0);
	#endif // !NDEBUG

	// No references yet.
	m_tnReferences = 0;

	// If copying our base class failed, leave.
	if (a_reStatus != g_kNoError)
		return;

	// Copy the motion vector.
	m_tnMotionX = a_rOther.m_tnMotionX;
	m_tnMotionY = a_rOther.m_tnMotionY;
	m_tnSquaredLength = a_rOther.m_tnSquaredLength;

	// Copy the pixel-group-location.
	#ifndef NDEBUG
	m_tnX = a_rOther.m_tnX;
	m_tnY = a_rOther.m_tnY;
	#endif // !NDEBUG
}



// Initializer.  Must be called on default-constructed regions.
template <class PIXELINDEX, class FRAMESIZE>
void
SearchBorder<PIXELINDEX,FRAMESIZE>::MovedRegion::Init
	(Status_t &a_reStatus)
{
	// Make sure they didn't start us off with an error.
	assert (a_reStatus == g_kNoError);

	// Initialize the base-class.
	BaseClass::Init (a_reStatus);
	if (a_reStatus != g_kNoError)
		return;
}



// Make the current region a copy of the other region.
template <class PIXELINDEX, class FRAMESIZE>
void
SearchBorder<PIXELINDEX,FRAMESIZE>::MovedRegion::Assign
	(Status_t &a_reStatus, const MovedRegion &a_rOther)
{
	// Make sure they didn't start us off with an error.
	assert (a_reStatus == g_kNoError);

	// Assign the base class.
	BaseClass::Assign (a_reStatus, a_rOther);
	if (a_reStatus != g_kNoError)
		return;

	// Copy the motion vector.
	m_tnMotionX = a_rOther.m_tnMotionX;
	m_tnMotionY = a_rOther.m_tnMotionY;
	m_tnSquaredLength = a_rOther.m_tnSquaredLength;

	// Copy the pixel-group-location.
	#ifndef NDEBUG
	m_tnX = a_rOther.m_tnX;
	m_tnY = a_rOther.m_tnY;
	#endif // !NDEBUG
}



// Move the contents of the other region into the current region.
// The current region must be empty.
template <class PIXELINDEX, class FRAMESIZE>
void
SearchBorder<PIXELINDEX,FRAMESIZE>::MovedRegion::Move
	(MovedRegion &a_rOther)
{
	// Make sure neither region is already in the search-border, i.e.
	// has any references.
	assert (m_tnReferences == 0);
	assert (a_rOther.m_tnReferences == 0);

	// Move the base class.
	BaseClass::Move (a_rOther);

	// Copy the motion vector.
	m_tnMotionX = a_rOther.m_tnMotionX;
	m_tnMotionY = a_rOther.m_tnMotionY;
	m_tnSquaredLength = a_rOther.m_tnSquaredLength;

	// Copy the pixel-group-location.
	#ifndef NDEBUG
	m_tnX = a_rOther.m_tnX;
	m_tnY = a_rOther.m_tnY;
	#endif // !NDEBUG
}



// Destructor.
template <class PIXELINDEX, class FRAMESIZE>
SearchBorder<PIXELINDEX,FRAMESIZE>::MovedRegion::~MovedRegion()
{
	// Make sure there are no references left.
	assert (m_tnReferences == 0);

	// One less instance.
	#ifndef NDEBUG
	--sm_ulInstances;
	#endif // !NDEBUG

	// Nothing additional to do.
}



// Set the motion vector.
template <class PIXELINDEX, class FRAMESIZE>
inline void
SearchBorder<PIXELINDEX,FRAMESIZE>::MovedRegion::SetMotionVector
	(PIXELINDEX a_tnX, PIXELINDEX a_tnY)
{
	// Set the motion vector.
	m_tnMotionX = a_tnX;
	m_tnMotionY = a_tnY;

	// Calculate the square of the vector's length.  (It's used for
	// sorting, and we don't want to recalculate it on every
	// comparison.)
	m_tnSquaredLength = FRAMESIZE (m_tnMotionX) * FRAMESIZE (m_tnMotionX)
		+ FRAMESIZE (m_tnMotionY) * FRAMESIZE (m_tnMotionY);
}



// Get the motion vector.
template <class PIXELINDEX, class FRAMESIZE>
inline void
SearchBorder<PIXELINDEX,FRAMESIZE>::MovedRegion::GetMotionVector
	(PIXELINDEX &a_rtnX, PIXELINDEX &a_rtnY) const
{
	// Easy enough.
	a_rtnX = m_tnMotionX;
	a_rtnY = m_tnMotionY;
}



// Comparison operator.
template <class PIXELINDEX, class FRAMESIZE>
inline bool
SearchBorder<PIXELINDEX,FRAMESIZE>::MovedRegion
	::SortBySizeThenMotionVectorLength::operator()
	(const MovedRegion *a_pLeft, const MovedRegion *a_pRight) const
{
	FRAMESIZE nLeftPoints, nRightPoints;
		// The number of points in each region.
	FRAMESIZE tnLeftLen, tnRightLen;
		// The (squared) length of each motion vector.

	// Make sure they gave us some regions to compare.
	assert (a_pLeft != NULL);
	assert (a_pRight != NULL);

	// First, compare by the number of points in each region.
	// Sort bigger regions first.
	nLeftPoints = a_pLeft->NumberOfPoints();
	nRightPoints = a_pRight->NumberOfPoints();
	if (nLeftPoints > nRightPoints)
		return true;
	if (nLeftPoints < nRightPoints)
		return false;

	// Then compare on motion vector length.
	// Sort smaller vectors first.
	tnLeftLen = a_pLeft->GetSquaredMotionVectorLength();
	tnRightLen = a_pRight->GetSquaredMotionVectorLength();
	if (tnLeftLen < tnRightLen)
		return true;
	// if (tnLeftLen >= tnRightLen)
		return false;
}



// Get the squared length of the motion vector.
template <class PIXELINDEX, class FRAMESIZE>
inline FRAMESIZE
SearchBorder<PIXELINDEX,FRAMESIZE>::MovedRegion
	::GetSquaredMotionVectorLength (void) const
{
	// Easy enough.
	return m_tnSquaredLength;
}



#ifndef NDEBUG

// The number of region objects in existence.
template <class PIXELINDEX, class FRAMESIZE>
uint32_t
SearchBorder<PIXELINDEX,FRAMESIZE>::MovedRegion::sm_ulInstances;

#endif // !NDEBUG



// Default constructor.
template <class PIXELINDEX, class FRAMESIZE>
SearchBorder<PIXELINDEX,FRAMESIZE>::BorderExtentBoundary
	::BorderExtentBoundary()
{
	// Fill in the blanks.
	m_tnIndex = m_tnLine = PIXELINDEX (0);
	m_bIsEnding = false;
	#ifdef BORDEREXTENTBOUNDARYSET_IMPLEMENTED_WITH_VECTOR
	m_tnCounterpartIndex = PIXELINDEX (0);
	#else // BORDEREXTENTBOUNDARYSET_IMPLEMENTED_WITH_VECTOR
	m_pCounterpart = NULL;
	#endif // BORDEREXTENTBOUNDARYSET_IMPLEMENTED_WITH_VECTOR
	m_pRegion = NULL;
	m_tnMotionX = m_tnMotionY = PIXELINDEX (0);
}



// Initializing constructor.
template <class PIXELINDEX, class FRAMESIZE>
SearchBorder<PIXELINDEX,FRAMESIZE>::BorderExtentBoundary
	::BorderExtentBoundary (PIXELINDEX a_tnIndex,
	PIXELINDEX a_tnLine, bool a_bIsEnding,
	MovedRegion *a_pRegion)
{
	// Make sure they gave us a region.
	assert (a_pRegion != NULL);
	assert (a_pRegion->m_pRegion != NULL);

	// Fill in the blanks.
	m_tnIndex = a_tnIndex;
	m_tnLine = a_tnLine;
	m_bIsEnding = a_bIsEnding;
	#ifdef BORDEREXTENTBOUNDARYSET_IMPLEMENTED_WITH_VECTOR
	m_tnCounterpartIndex = PIXELINDEX (0);
	#else // BORDEREXTENTBOUNDARYSET_IMPLEMENTED_WITH_VECTOR
	m_pCounterpart = NULL;
	#endif // BORDEREXTENTBOUNDARYSET_IMPLEMENTED_WITH_VECTOR
	m_pRegion = a_pRegion;
	a_pRegion->m_pRegion->GetMotionVector (m_tnMotionX, m_tnMotionY);
}



// Destructor.
template <class PIXELINDEX, class FRAMESIZE>
SearchBorder<PIXELINDEX,FRAMESIZE>::BorderExtentBoundary
	::~BorderExtentBoundary()
{
	// Nothing to do.
}



#ifndef NDEBUG

// Equality operator.
template <class PIXELINDEX, class FRAMESIZE>
bool
SearchBorder<PIXELINDEX,FRAMESIZE>::BorderExtentBoundary
	::operator == (const BorderExtentBoundary &a_rOther) const
{
	// Compare ourselves, field by field.
	return (m_tnIndex == a_rOther.m_tnIndex
		&& m_tnLine == a_rOther.m_tnLine
		&& m_bIsEnding == a_rOther.m_bIsEnding
		#ifdef BORDEREXTENTBOUNDARYSET_IMPLEMENTED_WITH_VECTOR
		&& m_tnCounterpartIndex == a_rOther.m_tnCounterpartIndex
		#else // BORDEREXTENTBOUNDARYSET_IMPLEMENTED_WITH_VECTOR
		&& m_pCounterpart == a_rOther.m_pCounterpart
		#endif // BORDEREXTENTBOUNDARYSET_IMPLEMENTED_WITH_VECTOR
		&& m_pRegion == a_rOther.m_pRegion
		&& m_tnMotionX == a_rOther.m_tnMotionX
		&& m_tnMotionY == a_rOther.m_tnMotionY);
}

#endif // !NDEBUG



// Comparison operator.
template <class PIXELINDEX, class FRAMESIZE>
inline bool
SearchBorder<PIXELINDEX,FRAMESIZE> ::BorderExtentBoundary
	::SortByIndexThenTypeThenMotionVectorThenRegionAddress
	::operator() (const BorderExtentBoundary &a_rLeft,
	const BorderExtentBoundary &a_rRight) const
{
	// First, sort by the boundary's pixel line.
	if (a_rLeft.m_tnLine < a_rRight.m_tnLine)
		return true;
	if (a_rLeft.m_tnLine > a_rRight.m_tnLine)
		return false;

	// Then sort by the boundary's pixel index.
	if (a_rLeft.m_tnIndex < a_rRight.m_tnIndex)
		return true;
	if (a_rLeft.m_tnIndex > a_rRight.m_tnIndex)
		return false;

	// (Not needed: startpoints & endpoints are in separate sets now.)
	// Then sort beginnings before endings.
	#if 0
	if (!a_rLeft.m_bIsEnding && a_rRight.m_bIsEnding)
		return true;
	if (a_rLeft.m_bIsEnding && !a_rRight.m_bIsEnding)
		return false;
	#endif

	// Sort next by motion vector.  (It doesn't matter how the sort
	// order is defined from the motion vectors, just that one is
	// defined.)
	if (a_rLeft.m_tnMotionX < a_rRight.m_tnMotionX)
		return true;
	if (a_rLeft.m_tnMotionX > a_rRight.m_tnMotionX)
		return false;
	if (a_rLeft.m_tnMotionY < a_rRight.m_tnMotionY)
		return true;
	if (a_rLeft.m_tnMotionY > a_rRight.m_tnMotionY)
		return false;

	// Finally, disambiguate by region address.
	if (a_rLeft.m_pRegion < a_rRight.m_pRegion)
		return true;
	// if (a_rLeft.m_pRegion >= a_rRight.m_pRegion)
		return false;
}



// Comparison operator.
template <class PIXELINDEX, class FRAMESIZE>
inline bool
SearchBorder<PIXELINDEX,FRAMESIZE>::BorderExtentBoundary
	::SortByMotionVectorThenTypeThenRegionAddress::operator()
	(const BorderExtentBoundary &a_rLeft,
	const BorderExtentBoundary &a_rRight) const
{
	// Sort by motion vector.  (It doesn't matter how the sort
	// order is defined from the motion vectors, just that one is
	// defined.)
	if (a_rLeft.m_tnMotionX < a_rRight.m_tnMotionX)
		return true;
	if (a_rLeft.m_tnMotionX > a_rRight.m_tnMotionX)
		return false;
	if (a_rLeft.m_tnMotionY < a_rRight.m_tnMotionY)
		return true;
	if (a_rLeft.m_tnMotionY > a_rRight.m_tnMotionY)
		return false;

	// Next, sort beginnings before endings.
	if (!a_rLeft.m_bIsEnding && a_rRight.m_bIsEnding)
		return true;
	if (a_rLeft.m_bIsEnding && !a_rRight.m_bIsEnding)
		return false;

	// Next, sort by index.  (Regions may have more than one extent
	// on a border.)
	if (a_rLeft.m_tnIndex < a_rRight.m_tnIndex)
		return true;
	if (a_rLeft.m_tnIndex > a_rRight.m_tnIndex)
		return false;

	// Next, sort by lines.  (The same region may have extents on
	// multiple lines, and this also matches the order in the
	// startpoints/endpoints sets, so that searches take maximum
	// advantage of the search finger.)
	if (a_rLeft.m_tnLine < a_rRight.m_tnLine)
		return true;
	if (a_rLeft.m_tnLine > a_rRight.m_tnLine)
		return false;

	// Finally, disambiguate by region address.
	if (a_rLeft.m_pRegion < a_rRight.m_pRegion)
		return true;
	//if (a_rLeft.m_pRegion >= a_rRight.m_pRegion)
		return false;
}



// Initialize the search-border, i.e. start in the upper-left corner.
template <class PIXELINDEX, class FRAMESIZE>
void
SearchBorder<PIXELINDEX,FRAMESIZE>::StartFrame (Status_t &a_reStatus)
{
	// Make sure they didn't start us off with an error.
	assert (a_reStatus == g_kNoError);

	// Make sure the borders are empty.
	assert (m_setBorderStartpoints.Size() == 0);
	assert (m_setBorderEndpoints.Size() == 0);
	assert (m_setBorderRegions.Size() == 0);

	// Make sure there are no leftover moved-regions from last time.
	assert (m_oMovedRegionAllocator.GetNumAllocated() == 0);
	assert (m_rSetRegionExtentAllocator.GetNumAllocated() == 0);

	// Set up our iterators into the borders.
	for (int i = 0; i <= m_tnPGH + 1; ++i)
	{
		m_paitBorderStartpoints[i] = m_setBorderStartpoints.End();
		m_paitBorderEndpoints[i] = m_setBorderEndpoints.End();
	}

	// No need to recalculate startpoint/endpoint-iterators yet.
	m_bFixStartpointEndpointIterators = false;

	// Start in the upper-left corner, and prepare to move to the right.
	m_tnX = m_tnY = PIXELINDEX (0);
	m_tnStepX = PIXELINDEX (1);
}



// Get the motion-vector-match cell for this motion-vector.
template <class PIXELINDEX, class FRAMESIZE>
typename SearchBorder<PIXELINDEX,FRAMESIZE>::MotionVectorMatch &
SearchBorder<PIXELINDEX,FRAMESIZE>::GetMotionVectorMatch
	(PIXELINDEX a_tnMotionX, PIXELINDEX a_tnMotionY) const
{
	// Make sure the motion-vector is in range.
	assert (a_tnMotionX >= -m_tnSearchRadiusX
		&& a_tnMotionX <= m_tnSearchRadiusX);
	assert (a_tnMotionY >= -m_tnSearchRadiusY
		&& a_tnMotionY <= m_tnSearchRadiusY);

	// Find the corresponding motion-vector-match cell.
	FRAMESIZE tnMajorAxis = FRAMESIZE (2) * FRAMESIZE (m_tnSearchRadiusX)
		+ FRAMESIZE (1);
	FRAMESIZE tnXOffset = FRAMESIZE (a_tnMotionX)
		+ FRAMESIZE (m_tnSearchRadiusX);
	FRAMESIZE tnYOffset = FRAMESIZE (a_tnMotionY)
		+ FRAMESIZE (m_tnSearchRadiusY);
	FRAMESIZE tnIndex = tnXOffset + tnMajorAxis * tnYOffset;

	// Return it to our caller.
	return m_pMotionVectorMatches[tnIndex];
}



// Add the given region to the motion-vector-match array.
template <class PIXELINDEX, class FRAMESIZE>
void
SearchBorder<PIXELINDEX,FRAMESIZE>::AddMotionVectorMatch
	(const MovedRegion *a_pRegion)
{
	// Make sure they gave us a region.
	assert (a_pRegion != NULL);

	// Get the region's motion vector.
	PIXELINDEX tnMotionX, tnMotionY;
	a_pRegion->GetMotionVector (tnMotionX, tnMotionY);

	// Find the corresponding motion-vector-match cell.
	MotionVectorMatch &rMatch
		= GetMotionVectorMatch (tnMotionX, tnMotionY);

	// That's one more extent.
	++rMatch.m_tnActiveExtents;

	// Remember the largest such region with this motion vector.
	//
	// GetExistingMatchSize() does all our work for us if there are
	// existing extents, because the new extent has already been added
	// to the border-regions set.
	if (rMatch.m_tnActiveExtents == 1)
		rMatch.m_tnLargestRegionSize = a_pRegion->NumberOfPoints();
	else
		rMatch.m_tnLargestRegionSize = GetExistingMatchSize (tnMotionX,
			tnMotionY);
}



// Remove the given region from the motion-vector-match array.
template <class PIXELINDEX, class FRAMESIZE>
void
SearchBorder<PIXELINDEX,FRAMESIZE>::RemoveMotionVectorMatch
	(const MovedRegion *a_pRegion)
{
	// Make sure they gave us a region.
	assert (a_pRegion != NULL);

	// Get the region's motion vector.
	PIXELINDEX tnMotionX, tnMotionY;
	a_pRegion->GetMotionVector (tnMotionX, tnMotionY);

	// Find the corresponding motion-vector-match cell.
	MotionVectorMatch &rMatch
		= GetMotionVectorMatch (tnMotionX, tnMotionY);

	// That's one less extent.
	assert (rMatch.m_tnActiveExtents > 0);
	--rMatch.m_tnActiveExtents;

	// Remember that the largest size must be recalculated.
	rMatch.m_tnLargestRegionSize = 0;
}



// Move one pixel to the right, adding and removing regions from
// the potentially-intersecting list.
template <class PIXELINDEX, class FRAMESIZE>
void
SearchBorder<PIXELINDEX,FRAMESIZE>::MoveRight (Status_t &a_reStatus)
{
	PIXELINDEX tnI;
		// Used to loop through iterators.
	typename IntersectingRegionsSet::Iterator itRemove;
		// An item being removed from the possibly-intersecting set.

	// Make sure they didn't start us off with an error.
	assert (a_reStatus == g_kNoError);

	// Make sure we knew we were moving right.
	assert (m_tnStepX == 1);

	#ifdef PRINT_SEARCHBORDER
	if (frame == 61 && m_setBorderRegions.Size() > 0)
	{
		fprintf (stderr, "Here's what SearchBorder::MoveRight() "
			"starts with (x %d, y %d):\n", int (m_tnX), int (m_tnY));
		for (itRemove = m_setBorderRegions.Begin();
			 itRemove != m_setBorderRegions.End();
			 ++itRemove)
		{
			fprintf (stderr, "\t(%d,%d), motion vector (%d,%d)\n",
				(*itRemove).m_tnIndex, (*itRemove).m_tnLine,
				(*itRemove).m_tnMotionX, (*itRemove).m_tnMotionY);
		}
	}
	#endif // PRINT_SEARCHBORDER

	// If we need to recalculate startpoint/endpoint-iterators, do so.
	if (m_bFixStartpointEndpointIterators)
	{
		FixStartpointEndpointIterators();
		m_bFixStartpointEndpointIterators = false;
	}

	// All active regions with a current-border endpoint at old X, and
	// all active regions with a first/last-border endpoint at old X + 1,
	// are no longer active.
	for (tnI = ((m_tnY > 0) ? 0 : 1); tnI <= m_tnPGH + 1; ++tnI)
	{
		// Determine if we're on the first/last-border.
		// We'll need to adjust a few things by 1 if so.
		PIXELINDEX tnIfFirstLast
			= (tnI == 0 || tnI == m_tnPGH + PIXELINDEX (1)) ? 1 : 0;

		// Get the endpoint to search along.
		typename BorderExtentBoundarySet::ConstIterator
			&ritEndpoint = m_paitBorderEndpoints[tnI];

		// Make sure it's right where we expect it to be.
		assert (ritEndpoint == m_setBorderEndpoints.End()
			|| (*ritEndpoint).m_tnLine > m_tnY + tnI - PIXELINDEX (1)
			|| ((*ritEndpoint).m_tnLine == m_tnY + tnI - PIXELINDEX (1)
				&& (*ritEndpoint).m_tnIndex
					>= (m_tnX + tnIfFirstLast)));

		// Remove all active regions that have endpoints at this index.
		while (ritEndpoint != m_setBorderEndpoints.End()
		&& (*ritEndpoint).m_tnLine == m_tnY + tnI - PIXELINDEX (1)
		&& (*ritEndpoint).m_tnIndex == (m_tnX + tnIfFirstLast))
		{
			// Find the endpoint's corresponding startpoint.
			//
			// If our corresponding set class is implemented with skip
			// lists, this is merely a pointer dereference, because the
			// address of nodes doesn't change after they're created.
			//
			// If our corresponding set class is implemented with vectors,
			// the counterpart has to be found with a search, because the
			// address of nodes can change after they're created.
			#ifdef BORDEREXTENTBOUNDARYSET_IMPLEMENTED_WITH_VECTOR
			BorderExtentBoundary oStartpoint (*ritEndpoint);
			oStartpoint.m_tnIndex = oStartpoint.m_tnCounterpartIndex;
			typename BorderExtentBoundarySet::ConstIterator itStartpoint
				= m_setBorderStartpoints.Find (oStartpoint);
			// (Make sure the corresponding startpoint was found.)
			assert (itStartpoint != m_setBorderStartpoints.End());
			const BorderExtentBoundary &rHere = *itStartpoint;
			// (Make sure the startpoint thinks it's matched with this
			// endpoint.)
			assert (rHere.m_tnCounterpartIndex
				== (*ritEndpoint).m_tnIndex);
			#else // BORDEREXTENTBOUNDARYSET_IMPLEMENTED_WITH_VECTOR
			const BorderExtentBoundary &rHere
				= *((*ritEndpoint).m_pCounterpart);
			#endif // BORDEREXTENTBOUNDARYSET_IMPLEMENTED_WITH_VECTOR

			#ifdef PRINT_SEARCHBORDER
			if (frame == 61)
			fprintf (stderr, "Found current-border endpoint, remove "
				"active-border region (%d,%d), "
					"motion vector (%d,%d)\n",
				rHere.m_tnIndex, rHere.m_tnLine,
				rHere.m_tnMotionX, rHere.m_tnMotionY);
			#endif // PRINT_SEARCHBORDER

			// Now find it & remove it.
			itRemove = m_setBorderRegions.Find (rHere);
			assert (itRemove != m_setBorderRegions.End());
			RemoveMotionVectorMatch ((*itRemove).m_pRegion);
			m_setBorderRegions.Erase (itRemove);

			// Move to the next endpoint.
			++ritEndpoint;
		}
	}

	// All active regions with a current-border startpoint at
	// new X + m_tnPGW, and all active regions with a last-border
	// startpoint at new X + m_tnPGW - 1, are now active.
	for (tnI = ((m_tnY > 0) ? 0 : 1); tnI <= m_tnPGH + 1; ++tnI)
	{
		// Determine if we're on the first/last-border.
		// We'll need to adjust a few things by 1 if not.
		PIXELINDEX tnIfFirstLast
			= (tnI == 0 || tnI == m_tnPGH + PIXELINDEX (1)) ? 0 : 1;

		// Get the startpoint to search along.
		typename BorderExtentBoundarySet::ConstIterator
			&ritStartpoint = m_paitBorderStartpoints[tnI];

		// Make sure it's right where we expect it to be.
		assert (ritStartpoint
			== m_setBorderStartpoints.End()
		|| (*ritStartpoint).m_tnLine > m_tnY + tnI - PIXELINDEX (1)
		|| ((*ritStartpoint).m_tnLine == m_tnY + tnI - PIXELINDEX (1)
			&& (*ritStartpoint).m_tnIndex
				>= (m_tnX + m_tnPGW + tnIfFirstLast)));

		// Add all active regions that have startpoints at this index.
		while (ritStartpoint != m_setBorderStartpoints.End()
		&& (*ritStartpoint).m_tnLine == m_tnY + tnI - PIXELINDEX (1)
		&& (*ritStartpoint).m_tnIndex
			== (m_tnX + m_tnPGW + tnIfFirstLast))
		{
			const BorderExtentBoundary &rHere = *ritStartpoint;

			#ifdef PRINT_SEARCHBORDER
			if (frame == 61)
			{
			fprintf (stderr, "Add active-border region (%d,%d), "
					"motion vector (%d,%d)\n",
				rHere.m_tnIndex, rHere.m_tnLine,
				rHere.m_tnMotionX, rHere.m_tnMotionY);
			}
			#endif // PRINT_SEARCHBORDER

			#ifndef NDEBUG
			typename IntersectingRegionsSet::InsertResult oInsertResult=
			#endif // !NDEBUG
				m_setBorderRegions.Insert (a_reStatus, rHere);
			if (a_reStatus != g_kNoError)
				return;
			assert (oInsertResult.m_bInserted);
			AddMotionVectorMatch (rHere.m_pRegion);

			// Move to the next startpoint.
			++ritStartpoint;
		}
	}

	// Finally, move one step to the right.
	++m_tnX;
}



// Move one pixel to the left, adding and removing regions from
// the potentially-intersecting list.
template <class PIXELINDEX, class FRAMESIZE>
void
SearchBorder<PIXELINDEX,FRAMESIZE>::MoveLeft (Status_t &a_reStatus)
{
	int tnI;
		// Used to loop through iterators.
	typename IntersectingRegionsSet::Iterator itRemove;
		// An item being removed from the possibly-intersecting set.

	// Make sure they didn't start us off with an error.
	assert (a_reStatus == g_kNoError);

	// Make sure we knew we were moving left.
	assert (m_tnStepX == -1);

	#ifdef PRINT_SEARCHBORDER
	if (frame == 61 && m_setBorderRegions.Size() > 0)
	{
		fprintf (stderr, "Here's what SearchBorder::MoveLeft() "
			"starts with (x %d, y %d):\n", int (m_tnX), int (m_tnY));
		for (itRemove = m_setBorderRegions.Begin();
			 itRemove != m_setBorderRegions.End();
			 ++itRemove)
		{
			fprintf (stderr, "\t(%d,%d), motion vector (%d,%d)\n",
				(*itRemove).m_tnIndex, (*itRemove).m_tnLine,
				(*itRemove).m_tnMotionX, (*itRemove).m_tnMotionY);
		}
	}
	#endif // PRINT_SEARCHBORDER

	// If we need to recalculate startpoint/endpoint-iterators, do so.
	if (m_bFixStartpointEndpointIterators)
	{
		FixStartpointEndpointIterators();
		m_bFixStartpointEndpointIterators = false;
	}

	// All active regions with a current-border startpoint at
	// old X + m_tnPGW, and all active regions with a last-border
	// startpoint at old X + m_tnPGW - 1, are no longer active.
	for (tnI = ((m_tnY > 0) ? 0 : 1); tnI <= m_tnPGH + 1; ++tnI)
	{
		// Determine if we're on the first/last-border.
		// We'll need to adjust a few things by 1 if so.
		PIXELINDEX tnIfFirstLast
			= (tnI == 0 || tnI == m_tnPGH + PIXELINDEX (1)) ? 1 : 0;

		// Get the current startpoint.
		typename BorderExtentBoundarySet::ConstIterator &ritBorder
			= m_paitBorderStartpoints[tnI];

		// Make sure it's right where we want to be.
		#ifndef NDEBUG
		{
			typename BorderExtentBoundarySet::ConstIterator itPrev
				= ritBorder;
			--itPrev;
			assert (ritBorder == m_setBorderStartpoints.End()
			|| (*ritBorder).m_tnLine > m_tnY + tnI - PIXELINDEX (1)
			|| ((*ritBorder).m_tnLine == m_tnY + tnI - PIXELINDEX (1)
				&& ((*ritBorder).m_tnIndex
						> (m_tnX + m_tnPGW - tnIfFirstLast)
					&& (itPrev == m_setBorderStartpoints.End()
						|| (*itPrev).m_tnLine
							< m_tnY + tnI - PIXELINDEX (1)
						|| ((*itPrev).m_tnLine
							== m_tnY + tnI - PIXELINDEX (1)
							&& (*itPrev).m_tnIndex
								<= (m_tnX + m_tnPGW - tnIfFirstLast))))));
		}
		#endif // !NDEBUG

		// Remove all active regions that have startpoints at this
		// index.
		while ((--ritBorder) != m_setBorderStartpoints.End()
		&& (*ritBorder).m_tnLine == m_tnY + tnI - PIXELINDEX (1)
		&& (*ritBorder).m_tnIndex
			== (m_tnX + m_tnPGW - tnIfFirstLast))
		{
			const BorderExtentBoundary &rHere = *ritBorder;

			#ifdef PRINT_SEARCHBORDER
			if (frame == 61)
			{
			fprintf (stderr, "Found current-border startpoint, remove "
					"active-border region (%d,%d), "
					"motion vector (%d,%d)\n",
				rHere.m_tnIndex, rHere.m_tnLine,
				rHere.m_tnMotionX, rHere.m_tnMotionY);
			}
			#endif // PRINT_SEARCHBORDER

			itRemove = m_setBorderRegions.Find (rHere);

			#ifdef PRINT_SEARCHBORDER
			if (frame == 61 && itRemove == m_setBorderRegions.End())
			{
				fprintf (stderr, "NOT FOUND!\n"
					"Here's what was found:\n");
				for (itRemove = m_setBorderRegions.Begin();
					 itRemove != m_setBorderRegions.End();
					 ++itRemove)
				{
					fprintf (stderr, "\t(%d,%d), "
							"motion vector (%d,%d)\n",
						(*itRemove).m_tnIndex, (*itRemove).m_tnLine,
						(*itRemove).m_tnMotionX,
						(*itRemove).m_tnMotionY);
				}
			}
			#endif // PRINT_SEARCHBORDER

			assert (itRemove != m_setBorderRegions.End());
			RemoveMotionVectorMatch ((*itRemove).m_pRegion);
			m_setBorderRegions.Erase (itRemove);
		}
		++ritBorder;
	}

	// All active regions with a current-border endpoint at new X, and
	// all active regions with a last-border endpoint at new X + 1, are
	// now active.
	for (tnI = ((m_tnY > 0) ? 0 : 1); tnI <= m_tnPGH + 1; ++tnI)
	{
		// Determine if we're on the first/last-border.
		// We'll need to adjust a few things by 1 if not.
		PIXELINDEX tnIfFirstLast
			= (tnI == 0 || tnI == m_tnPGH + PIXELINDEX (1)) ? 0 : 1;

		// Get the current startpoint.
		typename BorderExtentBoundarySet::ConstIterator &ritBorder
			= m_paitBorderEndpoints[tnI];

		// Make sure it's right where we want to be.
		#ifndef NDEBUG
		{
			typename BorderExtentBoundarySet::ConstIterator itPrev
				= ritBorder;
			--itPrev;
			assert (ritBorder == m_setBorderEndpoints.End()
			|| (*ritBorder).m_tnLine > m_tnY + tnI - PIXELINDEX (1)
			|| ((*ritBorder).m_tnLine == m_tnY + tnI - PIXELINDEX (1)
				&& ((*ritBorder).m_tnIndex > (m_tnX - tnIfFirstLast)
				&& (itPrev == m_setBorderEndpoints.End()
					|| (*itPrev).m_tnLine
						< m_tnY + tnI - PIXELINDEX (1)
					|| ((*itPrev).m_tnLine
						== m_tnY + tnI - PIXELINDEX (1)
						&& (*itPrev).m_tnIndex
							<= (m_tnX - tnIfFirstLast))))));
		}
		#endif // !NDEBUG

		// Add all active regions that have endpoints at this index.
		while ((--ritBorder) != m_setBorderEndpoints.End()
		&& (*ritBorder).m_tnLine == m_tnY + tnI - PIXELINDEX (1)
		&& (*ritBorder).m_tnIndex == (m_tnX - tnIfFirstLast))
		{
			// Find the endpoint's corresponding startpoint.
			//
			// If our corresponding set class is implemented with skip
			// lists, this is merely a pointer dereference, because the
			// address of nodes doesn't change after they're created.
			//
			// If our corresponding set class is implemented with vectors,
			// the counterpart has to be found with a search, because the
			// address of nodes can change after they're created.
			assert ((*ritBorder).m_bIsEnding);
			#ifdef BORDEREXTENTBOUNDARYSET_IMPLEMENTED_WITH_VECTOR
			BorderExtentBoundary oStartpoint (*ritBorder);
			oStartpoint.m_tnIndex = oStartpoint.m_tnCounterpartIndex;
			typename BorderExtentBoundarySet::ConstIterator itStartpoint
				= m_setBorderStartpoints.Find (oStartpoint);
			// (Make sure the corresponding startpoint was found.)
			assert (itStartpoint != m_setBorderStartpoints.End());
			const BorderExtentBoundary &rHere = *itStartpoint;
			// (Make sure the startpoint thinks it's matched with this
			// endpoint.)
			assert (rHere.m_tnCounterpartIndex
				== (*ritBorder).m_tnIndex);
			#else // BORDEREXTENTBOUNDARYSET_IMPLEMENTED_WITH_VECTOR
			const BorderExtentBoundary &rHere
				= *((*ritBorder).m_pCounterpart);
			#endif // BORDEREXTENTBOUNDARYSET_IMPLEMENTED_WITH_VECTOR

			#ifdef PRINT_SEARCHBORDER
			if (frame == 61)
			fprintf (stderr, "Add active-border region (%d,%d), "
					"motion vector (%d,%d)\n",
				rHere.m_tnIndex, rHere.m_tnLine,
				rHere.m_tnMotionX, rHere.m_tnMotionY);
			#endif // PRINT_SEARCHBORDER

			// Now insert it.
			#ifndef NDEBUG
			typename IntersectingRegionsSet::InsertResult oInsertResult=
			#endif // !NDEBUG
				m_setBorderRegions.Insert (a_reStatus, rHere);
			if (a_reStatus != g_kNoError)
				return;

			#ifdef PRINT_SEARCHBORDER
			if (frame == 61 && !oInsertResult.m_bInserted)
			{
				fprintf (stderr, "FOUND!\nHere's what was found:\n");
				fprintf (stderr, "\t(%d,%d), motion vector (%d,%d)\n",
					(*oInsertResult.m_itPosition).m_tnIndex,
					(*oInsertResult.m_itPosition).m_tnLine,
					(*oInsertResult.m_itPosition).m_tnMotionX,
					(*oInsertResult.m_itPosition).m_tnMotionY);
				fprintf (stderr, "Here are the active regions:\n");
				for (itRemove = m_setBorderRegions.Begin();
					 itRemove != m_setBorderRegions.End();
					 ++itRemove)
				{
					fprintf (stderr, "\t(%d,%d), motion vector (%d,%d)\n",
						(*itRemove).m_tnIndex, (*itRemove).m_tnLine,
						(*itRemove).m_tnMotionX, (*itRemove).m_tnMotionY);
				}
			}
			#endif // PRINT_SEARCHBORDER

			assert (oInsertResult.m_bInserted);
			AddMotionVectorMatch (rHere.m_pRegion);
		}
		++ritBorder;
	}

	// Finally, move one step to the left.
	--m_tnX;
}



// Move down a line, finding all regions that can no longer be
// contiguous with new matches, and handing them back to the client.
template <class PIXELINDEX, class FRAMESIZE>
void
SearchBorder<PIXELINDEX,FRAMESIZE>::MoveDown (Status_t &a_reStatus)
{
	typename BorderExtentBoundarySet::Iterator itBorder, itNextBorder;
		// Used to run through the last-border.
	typename IntersectingRegionsSet::Iterator itActive, itNextActive;
		// Used to run through the active-borders set.
	int i;
		// Used to loop through things.

	// Make sure they didn't start us off with an error.
	assert (a_reStatus == g_kNoError);

	// If we need to recalculate startpoint/endpoint-iterators, do so.
	if (m_bFixStartpointEndpointIterators)
	{
		FixStartpointEndpointIterators();
		m_bFixStartpointEndpointIterators = false;
	}

	// Run through the last border, disconnect the regions from all
	// endpoints.  If that leaves a region with no references and no
	// siblings, then the region is fully constructed, and it gets
	// handed back to the client.
	for (i = 0; i < 2; ++i)
	{
		BorderExtentBoundarySet &rBorder
			= ((i == 0) ? m_setBorderStartpoints: m_setBorderEndpoints);

		// Actually, we run through all the startpoints/endpoints on the
		// last-border or above it.  A flood-fill could find some extents
		// above the last-border if they're smaller than a pixel-group.
		for (itBorder = rBorder.Begin();
			 itBorder != rBorder.End()
			 	&& (*itBorder).m_tnLine <= m_tnY - PIXELINDEX (1);
			 itBorder = itNextBorder)
		{
			// Find the next border to examine, since we'll be removing
			// the current border.
			itNextBorder = itBorder;
			++itNextBorder;

			// Get the endpoint here, and its under-construction region.
			BorderExtentBoundary &rEndpoint = *itBorder;
			MovedRegion *pRegion = rEndpoint.m_pRegion;

			// That's one less reference to this region.  (Note that, by
			// deliberate coincidence, setting the endpoint's region to
			// NULL won't affect the sort order, so this is safe.)
			rEndpoint.m_pRegion = NULL;
			assert (pRegion->m_tnReferences > 0);
			--pRegion->m_tnReferences;

			// Are there any references left to this region?
			if (pRegion->m_tnReferences == 0)
			{
				// No.  The region is fully constructed.  Move it to
				// the list of regions that'll get applied to the
				// new frame's reference-image representation.
				OnCompletedRegion (a_reStatus, pRegion);
				if (a_reStatus != g_kNoError)
				{
					DeleteRegion (pRegion);
					return;
				}
			}

			// Finally, remove this startpoint/endpoint.
			rBorder.Erase (itBorder);
		}
	}

	// Run through the active-borders set, remove all references that
	// came from the now-cleared last-border.
	//
	// It seems slow to loop through all the active-borders, but the
	// alternative is to search for every relevant startpoint/endpoint,
	// which is probably slower.
	if (m_tnY > 0)
	{
		for (itActive = m_setBorderRegions.Begin();
			 itActive != m_setBorderRegions.End();
			 itActive = itNextActive)
		{
			// Get the next item, since we may remove the current item.
			itNextActive = itActive;
			++itNextActive;

			// If this region reference came from the last-border (or above
			// it), get rid of it.
			if ((*itActive).m_tnLine <= m_tnY - 1)
			{
				RemoveMotionVectorMatch ((*itActive).m_pRegion);
				m_setBorderRegions.Erase (itActive);
			}
		}
	}

	// The old last-border is gone, and we have a new current-border.
	// So move all our startpoint/endpoint iterators back one.
	for (i = 1; i <= m_tnPGH + 1; ++i)
	{
		m_paitBorderStartpoints[i-1] = m_paitBorderStartpoints[i];
		m_paitBorderEndpoints[i-1] = m_paitBorderEndpoints[i];
	}

	// Create iterators for the new first-border.
	{
		BorderExtentBoundary oStartpoint, oEndpoint;
			// New startpoints/endpoints as we create them.

		// Figure out where the startpoint/endpoint iterators need to
		// point to now.
		oStartpoint.m_tnIndex = m_tnX + m_tnPGW;
		oEndpoint.m_tnIndex = m_tnX + PIXELINDEX (1);
		oStartpoint.m_tnLine = oEndpoint.m_tnLine = m_tnY + m_tnPGH
			+ PIXELINDEX (1);
		oStartpoint.m_bIsEnding = false;
		oEndpoint.m_bIsEnding = true;
		oStartpoint.m_pRegion = oEndpoint.m_pRegion = NULL;
		oStartpoint.m_tnMotionX = oStartpoint.m_tnMotionY
			= oEndpoint.m_tnMotionX = oEndpoint.m_tnMotionY
			= Limits<PIXELINDEX>::Min;

		// Now create the new first-border iterators.
		m_paitBorderStartpoints[m_tnPGH + 1]
			= m_setBorderStartpoints.LowerBound (oStartpoint);
		m_paitBorderEndpoints[m_tnPGH + 1]
			= m_setBorderEndpoints.LowerBound (oEndpoint);
	}

	// Any region with (if m_tnX == 0) a last-border startpoint at
	// m_tnPGW or (if m_tnX == m_tnWidth - m_tnPGW) a last-border
	// endpoint of m_tnX will no longer be contiguous with the current
	// pixel-group, but they will be the next time the search border
	// moves left/right.
	//
	// Similarly, any region with (if m_tnX == 0) a bottommost-current-
	// border startpoint at m_tnPGW or (if m_tnX == m_tnWidth - m_tnPGW) a
	// bottommost-current-border endpoint of m_tnX will now be contiguous
	// with the current pixel-group.
	//
	// But not if we're past the bottom line.  This happens when we're
	// called by FinishFrame().
	if (m_tnY < m_tnHeight - m_tnPGH)
	{
		typename IntersectingRegionsSet::Iterator itRemove;
			// An item being removed from the possibly-intersecting set.

		if (m_tnX == 0)
		{
			// Fix the new last-border.
			{
				// Get the iterator we'll be using.
				typename BorderExtentBoundarySet::ConstIterator &ritBorder
					= m_paitBorderStartpoints[0];

				// Make sure it's right where we want to be.
				#ifndef NDEBUG
				{
					typename BorderExtentBoundarySet::ConstIterator itPrev
						= ritBorder;
					--itPrev;
					assert (ritBorder == m_setBorderStartpoints.End()
					|| (*ritBorder).m_tnLine > m_tnY
					|| ((*ritBorder).m_tnLine == m_tnY
						&& ((*ritBorder).m_tnIndex > m_tnPGW
							&& (itPrev == m_setBorderStartpoints.End()
								|| (*itPrev).m_tnLine < m_tnY
								|| ((*itPrev).m_tnLine == m_tnY
									&& (*itPrev).m_tnIndex <= m_tnPGW)))));
				}
				#endif // !NDEBUG

				// Remove all active regions that have startpoints at this
				// index.
				while ((--ritBorder) != m_setBorderStartpoints.End()
				&& (*ritBorder).m_tnLine == m_tnY
				&& (*ritBorder).m_tnIndex == m_tnPGW)
				{
					#ifdef PRINT_SEARCHBORDER
					if (frame == 61)
					fprintf (stderr, "Moving down a line, remove "
							"active-border region (%d,%d), "
							"motion vector (%d,%d)\n",
						(*ritBorder).m_tnIndex, (*ritBorder).m_tnLine,
						(*ritBorder).m_tnMotionX,
						(*ritBorder).m_tnMotionY);
					#endif // PRINT_SEARCHBORDER

					itRemove = m_setBorderRegions.Find (*ritBorder);

					#ifdef PRINT_SEARCHBORDER
					if (frame == 61
						&& itRemove == m_setBorderRegions.End())
					{
						fprintf (stderr, "NOT FOUND!\n"
							"Here's what was found:\n");
						for (itRemove = m_setBorderRegions.Begin();
							 itRemove != m_setBorderRegions.End();
							 ++itRemove)
						{
							fprintf (stderr, "\t(%d,%d), "
									"motion vector (%d,%d)\n",
								(*itRemove).m_tnIndex,
								(*itRemove).m_tnLine,
								(*itRemove).m_tnMotionX,
								(*itRemove).m_tnMotionY);
						}
					}
					#endif // PRINT_SEARCHBORDER

					assert (itRemove != m_setBorderRegions.End());
					RemoveMotionVectorMatch ((*itRemove).m_pRegion);
					m_setBorderRegions.Erase (itRemove);
				}
				++ritBorder;
			}

			// Fix the new bottommost-current-border.
			{
				// Get the iterator we'll be using.
				typename BorderExtentBoundarySet::ConstIterator &ritBorder
					= m_paitBorderStartpoints[m_tnPGH];

				// Make sure it's right where we want to be.
				assert (ritBorder == m_setBorderStartpoints.End()
				|| (*ritBorder).m_tnLine > m_tnY + m_tnPGH
				|| ((*ritBorder).m_tnLine == m_tnY + m_tnPGH
					&& ((*ritBorder).m_tnIndex >= m_tnPGW)));

				// Add all active regions that have startpoints at this
				// index.
				while (ritBorder != m_setBorderStartpoints.End()
				&& (*ritBorder).m_tnLine == m_tnY + m_tnPGH
				&& (*ritBorder).m_tnIndex == m_tnPGW)
				{
					#ifdef PRINT_SEARCHBORDER
					if (frame == 61)
					fprintf (stderr, "Moving down a line, add "
							"active-border region (%d,%d), "
							"motion vector (%d,%d)\n",
						(*ritBorder).m_tnIndex, (*ritBorder).m_tnLine,
						(*ritBorder).m_tnMotionX,
						(*ritBorder).m_tnMotionY);
					#endif // PRINT_SEARCHBORDER

					// This region is now part of the border.
					typename IntersectingRegionsSet::InsertResult
						oInsertResult = m_setBorderRegions.Insert
							(a_reStatus, *ritBorder);
					if (a_reStatus != g_kNoError)
						goto cleanup;

					#ifdef PRINT_SEARCHBORDER
					if (frame == 61 && !oInsertResult.m_bInserted)
					{
						fprintf (stderr, "FOUND!\n"
							"Here's what was found:\n");
						fprintf (stderr, "\t(%d,%d), "
								"motion vector (%d,%d)\n",
							(*oInsertResult.m_itPosition).m_tnIndex,
							(*oInsertResult.m_itPosition).m_tnLine,
							(*oInsertResult.m_itPosition).m_tnMotionX,
							(*oInsertResult.m_itPosition).m_tnMotionY);
						fprintf (stderr, "Here are the active regions:\n");
						for (itRemove = m_setBorderRegions.Begin();
							 itRemove != m_setBorderRegions.End();
							 ++itRemove)
						{
							fprintf (stderr, "\t(%d,%d), "
									"motion vector (%d,%d)\n",
								(*itRemove).m_tnIndex,
								(*itRemove).m_tnLine,
								(*itRemove).m_tnMotionX,
								(*itRemove).m_tnMotionY);
						}
					}
					#endif // PRINT_SEARCHBORDER

					assert (oInsertResult.m_bInserted);
					AddMotionVectorMatch ((*ritBorder).m_pRegion);

					++ritBorder;
				}
			}

			// Find all the extents of regions on the first-border
			// that will intersect/border the new current-pixel-group,
			// and put them into the border-regions set.
			{
				BorderExtentBoundary oStartpoint;
					// Used to search for startpoints.
				typename BorderExtentBoundarySet::ConstIterator itHere;
					// Used to iterate through startpoints.

				// All extents on the same line as the first-border,
				// whose startpoint X is less than the pixel-group width,
				// are extents that intersect/border the new
				// current-pixel-group.  So find the first startpoint on
				// the line.
				oStartpoint.m_tnIndex = 0;
				oStartpoint.m_tnLine = m_tnY + m_tnPGH + PIXELINDEX (1);
				oStartpoint.m_bIsEnding = false;
				oStartpoint.m_tnMotionX = oStartpoint.m_tnMotionY
					= Limits<PIXELINDEX>::Min;
				itHere = m_setBorderStartpoints.LowerBound (oStartpoint);

				// Loop through all the relevant startpoints, add them to
				// the border-regions set.
				while (itHere != m_setBorderStartpoints.End()
					&& (*itHere).m_tnLine == oStartpoint.m_tnLine
					&& (*itHere).m_tnIndex < m_tnPGW)
				{
					// This region is now part of the border.
					typename IntersectingRegionsSet::InsertResult
						oInsertResult = m_setBorderRegions.Insert
							(a_reStatus, *itHere);
					if (a_reStatus != g_kNoError)
						goto cleanup;
					assert (oInsertResult.m_bInserted);
					AddMotionVectorMatch ((*itHere).m_pRegion);

					// Move to the next startpoint.
					++itHere;
				}
			}
		}
		else
		{
			assert (m_tnX == m_tnWidth - m_tnPGW);

			// Fix the new last-border.
			{
				// Get the iterator we'll be using.
				typename BorderExtentBoundarySet::ConstIterator &ritBorder
					= m_paitBorderEndpoints[0];

				// Make sure it's right where we expect it to be.
				#ifndef NDEBUG
				{
					typename BorderExtentBoundarySet::ConstIterator itPrev
						= ritBorder;
					--itPrev;
					assert (ritBorder == m_setBorderEndpoints.End()
						|| (*ritBorder).m_tnLine > m_tnY
						|| ((*ritBorder).m_tnLine == m_tnY
							&& (*ritBorder).m_tnIndex >= m_tnX
							&& (itPrev == m_setBorderEndpoints.End()
								|| (*itPrev).m_tnLine < m_tnY
								|| ((*itPrev).m_tnLine == m_tnY
									&& (*itPrev).m_tnIndex < m_tnX))));
				}
				#endif // !NDEBUG

				// Remove all active regions that have endpoints at this
				// index.
				while (ritBorder != m_setBorderEndpoints.End()
				&& (*ritBorder).m_tnLine == m_tnY
				&& (*ritBorder).m_tnIndex == m_tnX)
				{
					// Find the endpoint's corresponding startpoint.
					//
					// If our corresponding set class is implemented with
					// skip lists, this is merely a pointer dereference,
					// because the address of nodes doesn't change after
					// they're created.
					//
					// If our corresponding set class is implemented with
					// vectors, the counterpart has to be found with a
					// search, because the address of nodes can change
					// after they're created.
					#ifdef BORDEREXTENTBOUNDARYSET_IMPLEMENTED_WITH_VECTOR
					BorderExtentBoundary oStartpoint (*ritBorder);
					oStartpoint.m_tnIndex
						= oStartpoint.m_tnCounterpartIndex;
					typename BorderExtentBoundarySet::ConstIterator
						itStartpoint
							= m_setBorderStartpoints.Find (oStartpoint);
					// (Make sure the corresponding startpoint was found.)
					assert (itStartpoint != m_setBorderStartpoints.End());
					const BorderExtentBoundary &rHere = *itStartpoint;
					// (Make sure the startpoint thinks it's matched with
					// this endpoint.)
					assert (rHere.m_tnCounterpartIndex
						== (*ritBorder).m_tnIndex);
					#else // BORDEREXTENTBOUNDARYSET_IMPLEMENTED_WITH_VECTOR
					const BorderExtentBoundary &rHere
						= *((*ritBorder).m_pCounterpart);
					#endif // BORDEREXTENTBOUNDARYSET_IMPLEMENTED_WITH_VECTOR

					#ifdef PRINT_SEARCHBORDER
					if (frame == 61)
					fprintf (stderr, "Moving down a line, remove "
							"active-border region (%d,%d), "
							"motion vector (%d,%d)\n",
						rHere.m_tnIndex, rHere.m_tnLine,
						rHere.m_tnMotionX, rHere.m_tnMotionY);
					#endif // PRINT_SEARCHBORDER

					itRemove = m_setBorderRegions.Find (rHere);

					#ifdef PRINT_SEARCHBORDER
					if (frame == 61
						&& itRemove == m_setBorderRegions.End())
					{
						fprintf (stderr, "NOT FOUND!\n"
							"Here's what was found:\n");
						for (itRemove = m_setBorderRegions.Begin();
							 itRemove != m_setBorderRegions.End();
							 ++itRemove)
						{
							fprintf (stderr, "\t(%d,%d), "
									"motion vector (%d,%d)\n",
								(*itRemove).m_tnIndex,
								(*itRemove).m_tnLine,
								(*itRemove).m_tnMotionX,
								(*itRemove).m_tnMotionY);
						}
					}
					#endif // PRINT_SEARCHBORDER

					assert (itRemove != m_setBorderRegions.End());
					RemoveMotionVectorMatch ((*itRemove).m_pRegion);
					m_setBorderRegions.Erase (itRemove);

					++ritBorder;
				}
			}

			// Fix the new bottommost-current-border.
			{
				// Get the iterator we'll be using.
				typename BorderExtentBoundarySet::ConstIterator &ritBorder
					= m_paitBorderEndpoints[m_tnPGH];

				// Make sure it's right where we expect it to be.
				assert (ritBorder == m_setBorderEndpoints.End()
					|| (*ritBorder).m_tnLine > m_tnY + m_tnPGH
					|| ((*ritBorder).m_tnLine == m_tnY + m_tnPGH
						&& (*ritBorder).m_tnIndex > m_tnX));

				// Remove all active regions that have endpoints at this
				// index.
				while ((--ritBorder) != m_setBorderEndpoints.End()
				&& (*ritBorder).m_tnLine == m_tnY + m_tnPGH
				&& (*ritBorder).m_tnIndex == m_tnX)
				{
					// Find the endpoint's corresponding startpoint.
					//
					// If our corresponding set class is implemented with
					// skip lists, this is merely a pointer dereference,
					// because the address of nodes doesn't change after
					// they're created.
					//
					// If our corresponding set class is implemented with
					// vectors, the counterpart has to be found with a
					// search, because the address of nodes can change
					// after they're created.
					#ifdef BORDEREXTENTBOUNDARYSET_IMPLEMENTED_WITH_VECTOR
					BorderExtentBoundary oStartpoint (*ritBorder);
					oStartpoint.m_tnIndex
						= oStartpoint.m_tnCounterpartIndex;
					typename BorderExtentBoundarySet::ConstIterator
						itStartpoint
							= m_setBorderStartpoints.Find (oStartpoint);
					// (Make sure the corresponding startpoint was found.)
					assert (itStartpoint != m_setBorderStartpoints.End());
					const BorderExtentBoundary &rHere = *itStartpoint;
					// (Make sure the startpoint thinks it's matched with
					// this endpoint.)
					assert (rHere.m_tnCounterpartIndex
						== (*ritBorder).m_tnIndex);
					#else // BORDEREXTENTBOUNDARYSET_IMPLEMENTED_WITH_VECTOR
					const BorderExtentBoundary &rHere
						= *((*ritBorder).m_pCounterpart);
					#endif // BORDEREXTENTBOUNDARYSET_IMPLEMENTED_WITH_VECTOR

					#ifdef PRINT_SEARCHBORDER
					if (frame == 61)
					fprintf (stderr, "Moving down a line, remove "
							"active-border region (%d,%d), "
							"motion vector (%d,%d)\n",
						rHere.m_tnIndex, rHere.m_tnLine,
						rHere.m_tnMotionX, rHere.m_tnMotionY);
					#endif // PRINT_SEARCHBORDER

					// This region is now part of the border.
					typename IntersectingRegionsSet::InsertResult
						oInsertResult = m_setBorderRegions.Insert
							(a_reStatus, rHere);
					if (a_reStatus != g_kNoError)
						goto cleanup;

					#ifdef PRINT_SEARCHBORDER
					if (frame == 61 && !oInsertResult.m_bInserted)
					{
						fprintf (stderr, "FOUND!\n"
							"Here's what was found:\n");
						fprintf (stderr, "\t(%d,%d), "
								"motion vector (%d,%d)\n",
							(*oInsertResult.m_itPosition).m_tnIndex,
							(*oInsertResult.m_itPosition).m_tnLine,
							(*oInsertResult.m_itPosition).m_tnMotionX,
							(*oInsertResult.m_itPosition).m_tnMotionY);
						fprintf (stderr, "Here are the active regions:\n");
						for (itRemove = m_setBorderRegions.Begin();
							 itRemove != m_setBorderRegions.End();
							 ++itRemove)
						{
							fprintf (stderr, "\t(%d,%d), "
									"motion vector (%d,%d)\n",
								(*itRemove).m_tnIndex,
								(*itRemove).m_tnLine,
								(*itRemove).m_tnMotionX,
								(*itRemove).m_tnMotionY);
						}
					}
					#endif // PRINT_SEARCHBORDER

					assert (oInsertResult.m_bInserted);
					AddMotionVectorMatch (rHere.m_pRegion);
				}

				++ritBorder;
			}

			// Find all the extents of regions on the first-border
			// that will intersect/border the new current-pixel-group,
			// and put them into the border-regions set.
			{
				BorderExtentBoundary oEndpoint;
					// Used to search for endpoints.
				typename BorderExtentBoundarySet::ConstIterator itHere;
					// Used to iterate through endpoints.

				// All extents on the same line as the first-border,
				// whose endpoint X is greater than (framewidth - PGW),
				// are extents that intersect/border the new
				// current-pixel-group.  So find the first endpoint at
				// that location on the line.
				oEndpoint.m_tnIndex = m_tnX + PIXELINDEX (1);
				oEndpoint.m_tnLine = m_tnY + m_tnPGH + PIXELINDEX (1);
				oEndpoint.m_bIsEnding = true;
				oEndpoint.m_tnMotionX = oEndpoint.m_tnMotionY
					= Limits<PIXELINDEX>::Min;
				itHere = m_setBorderEndpoints.LowerBound (oEndpoint);

				// Loop through all the relevant endpoints, add their
				// corresponding startpoints to the border-regions set.
				while (itHere != m_setBorderEndpoints.End()
					&& (*itHere).m_tnLine == oEndpoint.m_tnLine)
				{
					// Find the endpoint's corresponding startpoint.
					//
					// If our corresponding set class is implemented with
					// skip lists, this is merely a pointer dereference,
					// because the address of nodes doesn't change after
					// they're created.
					//
					// If our corresponding set class is implemented with
					// vectors, the counterpart has to be found with a
					// search, because the address of nodes can change
					// after they're created.
					#ifdef BORDEREXTENTBOUNDARYSET_IMPLEMENTED_WITH_VECTOR
					BorderExtentBoundary oStartpoint (*itHere);
					oStartpoint.m_tnIndex
						= oStartpoint.m_tnCounterpartIndex;
					typename BorderExtentBoundarySet::ConstIterator
						itStartpoint
							= m_setBorderStartpoints.Find (oStartpoint);
					// (Make sure the corresponding startpoint was found.)
					assert (itStartpoint != m_setBorderStartpoints.End());
					const BorderExtentBoundary &rHere = *itStartpoint;
					// (Make sure the startpoint thinks it's matched with
					// this endpoint.)
					assert (rHere.m_tnCounterpartIndex
						== (*itHere).m_tnIndex);
					#else // BORDEREXTENTBOUNDARYSET_IMPLEMENTED_WITH_VECTOR
					const BorderExtentBoundary &rHere
						= *((*itHere).m_pCounterpart);
					#endif // BORDEREXTENTBOUNDARYSET_IMPLEMENTED_WITH_VECTOR

					// This region is now part of the border.
					typename IntersectingRegionsSet::InsertResult
						oInsertResult = m_setBorderRegions.Insert
							(a_reStatus, rHere);
					if (a_reStatus != g_kNoError)
						goto cleanup;
					assert (oInsertResult.m_bInserted);
					AddMotionVectorMatch (rHere.m_pRegion);

					// Move to the next endpoint.
					++itHere;
				}
			}
		}

		// Finally, move one step down, and prepare to move the other
		// direction across the window.  (But not if we're past the
		// bottom line, i.e. being called by FinishFrame().)
		++m_tnY;
		m_tnStepX = -m_tnStepX;
	}

	// All done.
	return;

	// Clean up after errors.
cleanup:
	return;
}



// Return the number of regions that would intersect with the
// current pixel-group.
template <class PIXELINDEX, class FRAMESIZE>
inline FRAMESIZE
SearchBorder<PIXELINDEX,FRAMESIZE>::NumberOfActiveRegions (void) const
{
	// Easy enough.
	return m_setBorderRegions.Size();
}



// Determine if a region with this motion vector already
// intersects/borders the current pixel-group (i.e. it doesn't
// need to be flood-filled and added again).
// If so, return its size.
// If not, return zero.
template <class PIXELINDEX, class FRAMESIZE>
FRAMESIZE
SearchBorder<PIXELINDEX,FRAMESIZE>::GetExistingMatchSize
	(PIXELINDEX a_tnMotionX, PIXELINDEX a_tnMotionY) const
{
	// Make sure the motion-vector is in range.
	assert (a_tnMotionX >= -m_tnSearchRadiusX
		&& a_tnMotionX <= m_tnSearchRadiusX);
	assert (a_tnMotionY >= -m_tnSearchRadiusY
		&& a_tnMotionY <= m_tnSearchRadiusY);

	// If the existing match size is not known, recalculate it.
	MotionVectorMatch &rMatch = GetMotionVectorMatch (a_tnMotionX,
		a_tnMotionY);
	if (rMatch.m_tnLargestRegionSize == 0 && rMatch.m_tnActiveExtents > 0)
	{
		BorderExtentBoundary oSearchStartpoint;
			// Used to search the set of border-regions.
		typename IntersectingRegionsSet::ConstIterator itMatch;
			// Any match we find.
		#ifndef NDEBUG
		FRAMESIZE tnActiveExtents = 0;
		#endif // !NDEBUG
			// The number of active-region extents with this motion-vector.

		// Look for regions with the given motion vector.
		oSearchStartpoint.m_tnMotionX = a_tnMotionX;
		oSearchStartpoint.m_tnMotionY = a_tnMotionY;
		itMatch = m_setBorderRegions.LowerBound (oSearchStartpoint);

		// Determine the size of the largest such region.
		while (itMatch != m_setBorderRegions.End()
			&& (*itMatch).m_tnMotionX == a_tnMotionX
			&& (*itMatch).m_tnMotionY == a_tnMotionY)
		{
			// Get the next region with this motion-vector.
			const MovedRegion *pRegion = (*itMatch).m_pRegion;

			// Get the size of the region.
			FRAMESIZE tnRegionSize = pRegion->NumberOfPoints();

			// Remember the largest such region with this motion vector.
			rMatch.m_tnLargestRegionSize
				= Max (rMatch.m_tnLargestRegionSize, tnRegionSize);

			// That's one more active-region with this motion-vector.
			#ifndef NDEBUG
			++tnActiveExtents;
			#endif // !NDEBUG

			// Look for another region with this motion-vector.
			++itMatch;
		}

		// Make sure the stored number of active-region extents is
		// accurate.
		assert (rMatch.m_tnActiveExtents == tnActiveExtents);
	}

	// Let our caller know the size of the largest region with this
	// motion vector.
	return rMatch.m_tnLargestRegionSize;
}



// Add the given region, with the given motion-vector.
// Causes a_rRegion to be emptied.
template <class PIXELINDEX, class FRAMESIZE>
void
SearchBorder<PIXELINDEX,FRAMESIZE>::AddNewRegion (Status_t &a_reStatus,
	MovedRegion &a_rRegion)
{
	MovedRegion *pRegion;
		// Our copy of the newly added region.
	typename MovedRegion::ConstIterator itHere;
		// Used to loop through the newly added region.
	BorderExtentBoundary oStartpoint, oEndpoint;
		// New startpoints/endpoints as we create them.
	typename BorderExtentBoundarySet::Iterator itStart, itEnd;
		// Any startpoint/endpoint inserted into the set, as they're
		// inserted.

	// Make sure they didn't start us off with an error.
	assert (a_reStatus == g_kNoError);

	// Make space for a new region.
	pRegion = (MovedRegion *) m_oMovedRegionAllocator.Allocate (0,
		sizeof (MovedRegion));
	if (pRegion == NULL)
	{
		a_reStatus = g_kOutOfMemory;
		goto cleanup0;
	}

	// Create a new region.
	::new ((void *) pRegion) MovedRegion (a_reStatus,
		m_rSetRegionExtentAllocator);
	if (a_reStatus != g_kNoError)
		goto cleanup1;

	// Move the contents of the given region into the new region.
	// This also copies the motion vector and empties the given region.
	pRegion->Move (a_rRegion);

	// Set up the unchanging parts of the startpoint and endpoint.
	oStartpoint.m_bIsEnding = false;
	oEndpoint.m_bIsEnding = true;
	oStartpoint.m_pRegion = oEndpoint.m_pRegion = pRegion;
	pRegion->GetMotionVector (oStartpoint.m_tnMotionX,
		oStartpoint.m_tnMotionY);
	pRegion->GetMotionVector (oEndpoint.m_tnMotionX,
		oEndpoint.m_tnMotionY);

	// Loop through every extent of the new region, create a startpoint
	// and endpoint for each one.  If the extent intersects or borders
	// the current pixel-group, make an entry for it in the active-regions
	// list.
	// Extents before the first-border can be ignored -- their time
	// has already passed.
	for (itHere = (m_tnY == PIXELINDEX (0)) ? pRegion->Begin()
			: pRegion->LowerBound (m_tnY - PIXELINDEX (1), PIXELINDEX (0));
		 itHere != pRegion->End();
		 ++itHere)
	{
		// Get the current extent.
		const typename MovedRegion::Extent &a_rExtent = *itHere;
		PIXELINDEX tnY = a_rExtent.m_tnY;
		PIXELINDEX tnXStart = a_rExtent.m_tnXStart;
		PIXELINDEX tnXEnd = a_rExtent.m_tnXEnd;

		// Make sure it's not before the first-border.  (Sanity check.)
		assert (tnY + PIXELINDEX (1) >= m_tnY);

		// Create the startpoint and endpoint that represent this extent.
		oStartpoint.m_tnIndex = tnXStart;
		oEndpoint.m_tnIndex = tnXEnd;
		oStartpoint.m_tnLine = oEndpoint.m_tnLine = tnY;
		#ifdef BORDEREXTENTBOUNDARYSET_IMPLEMENTED_WITH_VECTOR
		oStartpoint.m_tnCounterpartIndex = tnXEnd;
		oEndpoint.m_tnCounterpartIndex = tnXStart;
		#endif // BORDEREXTENTBOUNDARYSET_IMPLEMENTED_WITH_VECTOR

		// Make a record of this startpoint.
		typename BorderExtentBoundarySet::InsertResult oStartInsertResult
			= m_setBorderStartpoints.Insert (a_reStatus, oStartpoint);
		if (a_reStatus != g_kNoError)
			goto cleanup2;
		assert (oStartInsertResult.m_bInserted);

		// Remember where this startpoint was inserted.
		itStart = oStartInsertResult.m_itPosition;

		// The new startpoint contains another reference to the region.
		++pRegion->m_tnReferences;

		// Make a record of this endpoint.
		typename BorderExtentBoundarySet::InsertResult oEndInsertResult
			= m_setBorderEndpoints.Insert (a_reStatus, oEndpoint);
		if (a_reStatus != g_kNoError)
			goto cleanup3;
		assert (oEndInsertResult.m_bInserted);

		// Remember where this endpoint was inserted.
		itEnd = oEndInsertResult.m_itPosition;

		// The new endpoint contains another reference to the region.
		++pRegion->m_tnReferences;

		// Now that startpoint & endpoint are inserted, we can set up
		// their counterpart links.
		#ifndef BORDEREXTENTBOUNDARYSET_IMPLEMENTED_WITH_VECTOR
		(*itStart).m_pCounterpart = &(*itEnd);
		(*itEnd).m_pCounterpart = &(*itStart);
		#endif // !BORDEREXTENTBOUNDARYSET_IMPLEMENTED_WITH_VECTOR

		// If this extent intersects/borders the current pixel-group,
		// then a reference to it must be added to the border-regions set.
		PIXELINDEX tnIfFirstLast
			= (tnY + PIXELINDEX (1) == m_tnY
				|| tnY == m_tnY + m_tnPGH) ? 1 : 0;
		if (tnY + PIXELINDEX (1) >= m_tnY
			&& tnY <= m_tnY + m_tnPGH
			&& tnXEnd >= m_tnX + tnIfFirstLast
			&& tnXStart < m_tnX + m_tnPGW + PIXELINDEX (1)
				- tnIfFirstLast)
		{
			// Add a reference to this region.
			typename IntersectingRegionsSet::InsertResult oInsertResult
				= m_setBorderRegions.Insert (a_reStatus, *itStart);
			if (a_reStatus != g_kNoError)
				goto cleanup4;
			assert (oInsertResult.m_bInserted);
			AddMotionVectorMatch
				((*(oInsertResult.m_itPosition)).m_pRegion);
		}
	}

	// Does the search-border have any references to this region?
	if (pRegion->m_tnReferences != 0)
	{
		// Yes.  Any number of new startpoints/endpoints could have been
		// inserted, so remember to fix the iterators that keep track of
		// which startpoints/endpoints intersect the current pixel-group.
		m_bFixStartpointEndpointIterators = true;
	}
	else
	{
		// No.  The region is fully constructed.  Move it to
		// the list of regions that'll get applied to the
		// new frame's reference-image representation.
		OnCompletedRegion (a_reStatus, pRegion);
		if (a_reStatus != g_kNoError)
		{
			DeleteRegion (pRegion);
			return;
		}
	}

	// All done.
	return;

	// Clean up after errors.
cleanup4:
	--pRegion->m_tnReferences;
	m_setBorderEndpoints.Erase (itEnd);
cleanup3:
	--pRegion->m_tnReferences;
	m_setBorderStartpoints.Erase (itStart);
cleanup2:
	RemoveRegion (pRegion);
	pRegion->~MovedRegion();
cleanup1:
	m_oMovedRegionAllocator.Deallocate (0, sizeof (MovedRegion), pRegion);
cleanup0:
	;
}



// Removes the given region from the search-border.
template <class PIXELINDEX, class FRAMESIZE>
void
SearchBorder<PIXELINDEX,FRAMESIZE>::RemoveRegion (MovedRegion *a_pRegion)
{
	// Make sure they gave us a region to remove.
	assert (a_pRegion != NULL);

	// Remove all references to this region from the border-regions-set.
	{
		typename IntersectingRegionsSet::Iterator itHere, itNext;
		for (itHere = m_setBorderRegions.Begin();
			 itHere != m_setBorderRegions.End();
			 itHere = itNext)
		{
			// Get the next intersecting-region to examine, since the
			// current intersection-region may get removed.
			itNext = itHere;
			++itNext;

			// If this is a reference to the region being removed, then get
			// rid of the reference.
			if ((*itHere).m_pRegion == a_pRegion)
			{
				RemoveMotionVectorMatch (a_pRegion);
				m_setBorderRegions.Erase (itHere);
			}
		}
	}

	// Remove all the startpoints/endpoints that refer to this region.
	// They can be easily found by looping through the region's extents.
	{
		typename MovedRegion::ConstIterator itHere;
			// Used to loop through moved-region extents.
		BorderExtentBoundary oExtent;
			// Used to search for startpoints/endpoints.

		// The extents will all have the same region and motion-vector.
		oExtent.m_pRegion = a_pRegion;
		a_pRegion->GetMotionVector (oExtent.m_tnMotionX,
			oExtent.m_tnMotionY);

		// Loop through the region's extents, find the corresponding
		// startpoint and endpoint, and remove them.
		for (itHere = a_pRegion->Begin();
			 itHere != a_pRegion->End();
			 ++itHere)
		{
			typename BorderExtentBoundarySet::Iterator itExtent;
				// A startpoint/endpoint that we find.
			#ifndef NDEBUG
			#ifdef BORDEREXTENTBOUNDARYSET_IMPLEMENTED_WITH_VECTOR
			PIXELINDEX tnEndpoint;
			#else // BORDEREXTENTBOUNDARYSET_IMPLEMENTED_WITH_VECTOR
			BorderExtentBoundary *pEndpoint;
			#endif // BORDEREXTENTBOUNDARYSET_IMPLEMENTED_WITH_VECTOR
			#endif // !NDEBUG
				// The endpoint that corresponds to a found startpoint.

			// Get the details of this extent.
			const typename MovedRegion::Extent &rExtent = *itHere;
			PIXELINDEX tnY = rExtent.m_tnY;
			PIXELINDEX tnXStart = rExtent.m_tnXStart;
			PIXELINDEX tnXEnd = rExtent.m_tnXEnd;

			// Look for the corresponding startpoint.
			// There might not be one if the extent is before the
			// last-border.  (A region added during the current line
			// but flood-filled to earlier than the last-border would
			// leave behind a startpoint that must be cleaned up here.)
			oExtent.m_tnIndex = tnXStart;
			oExtent.m_tnLine = tnY;
			oExtent.m_bIsEnding = false;
			itExtent = m_setBorderStartpoints.Find (oExtent);

			// Make sure it was found.
			// Startpoints earlier than the last-border might not be
			// found, and that's OK.
			assert ((m_tnY > 0 && tnY < m_tnY - PIXELINDEX (1))
				|| itExtent != m_setBorderStartpoints.End());

			if (itExtent != m_setBorderStartpoints.End())
			{
				// Remember its endpoint.
				#ifndef NDEBUG
				#ifdef BORDEREXTENTBOUNDARYSET_IMPLEMENTED_WITH_VECTOR
				tnEndpoint = (*itExtent).m_tnCounterpartIndex;
				#else // BORDEREXTENTBOUNDARYSET_IMPLEMENTED_WITH_VECTOR
				pEndpoint = (*itExtent).m_pCounterpart;
				#endif // BORDEREXTENTBOUNDARYSET_IMPLEMENTED_WITH_VECTOR
				#endif // !NDEBUG

				// Remove the startpoint that corresponds to this extent.
				m_setBorderStartpoints.Erase (itExtent);

				// That's one less reference to this region.
				assert (a_pRegion->m_tnReferences > 0);
				--a_pRegion->m_tnReferences;
			}
			#ifndef NDEBUG
			else
			{
				// There will be no corresponding endpoint.
				#ifdef BORDEREXTENTBOUNDARYSET_IMPLEMENTED_WITH_VECTOR
				tnEndpoint = (PIXELINDEX)-1;
				#else // BORDEREXTENTBOUNDARYSET_IMPLEMENTED_WITH_VECTOR
				pEndpoint = NULL;
				#endif // BORDEREXTENTBOUNDARYSET_IMPLEMENTED_WITH_VECTOR
			}
			#endif // !NDEBUG

			// Look for the corresponding endpoint.
			// There might not be one if the extent is before the
			// last-border.  (A region added during the current line
			// but flood-filled to earlier than the last-border would
			// leave behind a endpoint that must be cleaned up here.)
			oExtent.m_tnIndex = tnXEnd;
			oExtent.m_bIsEnding = true;
			itExtent = m_setBorderEndpoints.Find (oExtent);

			// Make sure it was found.
			// Endpoints earlier than the last-border might not be
			// found, and that's OK.
			assert ((m_tnY > 0 && tnY < m_tnY - PIXELINDEX (1))
				|| itExtent != m_setBorderEndpoints.End());

			if (itExtent != m_setBorderEndpoints.End())
			{
				// Make sure it's the endpoint that corresponded to this
				// startpoint.
				#ifdef BORDEREXTENTBOUNDARYSET_IMPLEMENTED_WITH_VECTOR
				assert ((*itExtent).m_tnIndex == tnEndpoint);
				#else // BORDEREXTENTBOUNDARYSET_IMPLEMENTED_WITH_VECTOR
				assert (&(*itExtent) == pEndpoint);
				#endif // BORDEREXTENTBOUNDARYSET_IMPLEMENTED_WITH_VECTOR

				// Remove the endpoint that corresponds to this extent.
				m_setBorderEndpoints.Erase (itExtent);

				// That's one less reference to this region.
				assert (a_pRegion->m_tnReferences > 0);
				--a_pRegion->m_tnReferences;
			}
		}
	}

	// Make sure this region has no references left.
	assert (a_pRegion->m_tnReferences == 0);

	// Any number of startpoints/endpoints could have been removed,
	// so remember to fix the iterators that keep track of which
	// startpoints/endpoints intersect the current pixel-group.
	m_bFixStartpointEndpointIterators = true;
}



// Fix the last/current/first-border startpoints/endpoint
// iterators.  Necessary after adding or removing a region.
template <class PIXELINDEX, class FRAMESIZE>
void
SearchBorder<PIXELINDEX,FRAMESIZE>::FixStartpointEndpointIterators (void)
{
	BorderExtentBoundary oStartpoint, oEndpoint;
		// New startpoints/endpoints as we create them.
	PIXELINDEX tnI;
		// Used to loop through the last/current/first-border iterators.

	// Set up the startpoint and endpoint so that it can be used to
	// search for the new values for the startpoint/endpoint set iterators.
	oStartpoint.m_bIsEnding = false;
	oEndpoint.m_bIsEnding = true;
	oStartpoint.m_pRegion = oEndpoint.m_pRegion = NULL;
	oStartpoint.m_tnMotionX = oStartpoint.m_tnMotionY
		= oEndpoint.m_tnMotionX = oEndpoint.m_tnMotionY
		= Limits<PIXELINDEX>::Min;

	// Loop through all the last/current/first-border lines, and find
	// the new value for each iterator.
	for (tnI = (m_tnY > 0) ? 0 : 1; tnI <= m_tnPGH + PIXELINDEX (1); ++tnI)
	{
		// Figure out where the startpoint/endpoint iterators need to
		// point to now.
		PIXELINDEX tnIfFirstLast
			= (tnI == 0 || tnI == m_tnPGH + PIXELINDEX (1)) ? 1 : 0;
		oStartpoint.m_tnIndex = m_tnX + m_tnPGW + PIXELINDEX (1)
			- tnIfFirstLast;
		oEndpoint.m_tnIndex = m_tnX + tnIfFirstLast;
		oStartpoint.m_tnLine = oEndpoint.m_tnLine
			= m_tnY + tnI - PIXELINDEX (1);

		// Find the new startpoint.
		m_paitBorderStartpoints[tnI]
			= m_setBorderStartpoints.LowerBound (oStartpoint);

		// Make sure it's right where we expect it to be.
		#ifndef NDEBUG
		const typename BorderExtentBoundarySet::ConstIterator
			&ritStartpoint = m_paitBorderStartpoints[tnI];
		#endif // !NDEBUG
		assert (ritStartpoint == m_setBorderStartpoints.End()
		|| (*ritStartpoint).m_tnLine > m_tnY + tnI - PIXELINDEX (1)
		|| ((*ritStartpoint).m_tnLine == m_tnY + tnI - PIXELINDEX (1)
			&& (*ritStartpoint).m_tnIndex
				>= (m_tnX + m_tnPGW + PIXELINDEX (1) - tnIfFirstLast)));

		// Find the new endpoint.
		m_paitBorderEndpoints[tnI]
			= m_setBorderEndpoints.LowerBound (oEndpoint);

		// Make sure it's right where we expect it to be.
		#ifndef NDEBUG
		const typename BorderExtentBoundarySet::ConstIterator &ritEndpoint
			= m_paitBorderEndpoints[tnI];
		#endif // !NDEBUG
		assert (ritEndpoint == m_setBorderEndpoints.End()
			|| (*ritEndpoint).m_tnLine > m_tnY + tnI - PIXELINDEX (1)
			|| ((*ritEndpoint).m_tnLine == m_tnY + tnI - PIXELINDEX (1)
				&& (*ritEndpoint).m_tnIndex
					>= (m_tnX + tnIfFirstLast)));

	}

	// If the search-border is still on the first line, fix the
	// last-border startpoint/endpoint iterators, since otherwise
	// they won't get set up.
	if (m_tnY == 0)
	{
		m_paitBorderStartpoints[0] = m_setBorderStartpoints.Begin();
		m_paitBorderEndpoints[0] = m_setBorderEndpoints.Begin();
	}
}



// Loop through all regions that matched the current pixel-group,
// find the single best one, and return it.
// Backpatch the size of the second-best active-region.
template <class PIXELINDEX, class FRAMESIZE>
typename SearchBorder<PIXELINDEX,FRAMESIZE>::MovedRegion *
SearchBorder<PIXELINDEX,FRAMESIZE>::ChooseBestActiveRegion
	(Status_t &a_reStatus, FRAMESIZE &a_rtnSecondBestActiveRegionSize)
{
	MovedRegion *pSurvivor;
		// The only region to survive pruning -- the biggest one, with
		// the shortest motion vector.
	typename IntersectingRegionsSet::Iterator itHere;
		// The range of regions that are contiguous with the newly-added
		// pixel-group.

	// Make sure they didn't start us off with an error.
	assert (a_reStatus == g_kNoError);

	// There's no second-best active-region yet.
	FRAMESIZE tnSecondBestActiveRegionSize = 0;

	// m_setBorderRegions contains all the regions that intersected
	// with the current pixel-group.  Find the largest region, and,
	// for equal-sized regions, the one with the shortest motion
	// vector.
	pSurvivor = NULL;
	for (itHere = m_setBorderRegions.Begin();
		 itHere != m_setBorderRegions.End();
		 ++itHere)
	{
		if (pSurvivor == NULL)
			pSurvivor = (*itHere).m_pRegion;
		else if ((*itHere).m_pRegion == pSurvivor)
			/* Duplicate */;
		else if (pSurvivor->NumberOfPoints()
				> (*itHere).m_pRegion->NumberOfPoints())
			/* Current survivor has more points */;
		else if (pSurvivor->NumberOfPoints()
				< (*itHere).m_pRegion->NumberOfPoints())
		{
			// This region is larger than the current survivor.
			tnSecondBestActiveRegionSize = pSurvivor->NumberOfPoints();
			pSurvivor = (*itHere).m_pRegion;
		}
		else if (pSurvivor->GetSquaredMotionVectorLength()
				> (*itHere).m_pRegion->GetSquaredMotionVectorLength())
		{
			// This region is the same size as the current survivor, but
			// represents less motion, so it should be preferred.  (This
			// will tend to properly resolve pixel-groups that have lots of
			// matches because of a repeating pattern in the area.)
			tnSecondBestActiveRegionSize = pSurvivor->NumberOfPoints();
			pSurvivor = (*itHere).m_pRegion;
		}
	}

	// Make sure there's at least one survivor.  (This method should not
	// be called unless a match-throttle has been exceeded, and that can't
	// happen unless there's at least one active-region.)
	assert (pSurvivor != NULL);

	// Remove this region from our consideration.
	RemoveRegion (pSurvivor);

	// Return the region.
	a_rtnSecondBestActiveRegionSize = tnSecondBestActiveRegionSize;
	return pSurvivor;
}



// Clean up the search border at the end of a frame, e.g. hand all
// remaining regions back to the client.
template <class PIXELINDEX, class FRAMESIZE>
void
SearchBorder<PIXELINDEX,FRAMESIZE>::FinishFrame (Status_t &a_reStatus)
{
	// Make sure they didn't start us off with an error.
	assert (a_reStatus == g_kNoError);

	// It turns out that we can accomplish this by pretending to move
	// down m_tnPGH+1 lines -- that'll clear out the last-border and the
	// current-border.
	for (int i = 0; i <= m_tnPGH; ++i)
	{
		MoveDown (a_reStatus);
		if (a_reStatus != g_kNoError)
			return;
		++m_tnY;
	}

	// Make sure that emptied the border.
	assert (m_setBorderStartpoints.Size() == 0);
	assert (m_setBorderEndpoints.Size() == 0);
	assert (m_setBorderRegions.Size() == 0);

	// Make sure our temporary memory allocations have been purged.
	assert (m_oBorderExtentsAllocator.GetNumAllocated() == 0);
}



// Delete a region that was returned by ChooseBestActiveRegion()
// or OnCompletedRegion().
template <class PIXELINDEX, class FRAMESIZE>
void
SearchBorder<PIXELINDEX,FRAMESIZE>::DeleteRegion (MovedRegion *a_pRegion)
{
	// Make sure they gave us a region to delete.
	assert (a_pRegion != NULL);

	// Easy enough.
	a_pRegion->~MovedRegion();
	m_oMovedRegionAllocator.Deallocate (0, sizeof (MovedRegion),
		a_pRegion);
}



#endif // __SEARCH_BORDER_H__
