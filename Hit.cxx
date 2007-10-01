///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// TreeSearch::Hit                                                           //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "Hit.h"
#include "TSeqCollection.h"

#include <iostream>

using std::cout;
using std::endl;
using std::make_pair;

ClassImp(TreeSearch::Hit)
ClassImp(TreeSearch::MCHit)
ClassImp(TreeSearch::HitPairIter)

namespace TreeSearch {

//_____________________________________________________________________________
void Hit::Print( Option_t* opt ) const
{
  // Print hit info

  cout << "Hit: wnum=" << GetWireNum()
       << " wpos="     << GetWirePos()
       << " z="        << GetZ()
       << " res="      << GetResolution()
       << " time="     << GetDriftTime()
       << " drift="    << GetDriftDist()
       << " trk="      << GetTrackDist();
  if( *opt != 'C' )
    cout << endl;
}

//_____________________________________________________________________________
void MCHit::Print( Option_t* opt ) const
{
  // Print hit info

  Hit::Print("C");
  cout << " MCpos=" << GetMCPos()
       << endl;
}

//_____________________________________________________________________________

}

namespace TreeSearch {

//_____________________________________________________________________________
HitPairIter::HitPairIter( const TSeqCollection* collA,
			  const TSeqCollection* collB,
			  Double_t maxdist ) 
  : fCollA(collA), fCollB(collB), fIterA(0), fIterB(0),
    fSaveIter(0), fSaveHit(0), fMaxDist(maxdist), fStarted(kFALSE),
    fScanning(kFALSE)
{
  // Constructor

  if( !fIterA && fCollA )
    fIterA = fCollA->MakeIterator();
  if( !fIterB && fCollB ) {
    fIterB = fCollB->MakeIterator();
    if( !fSaveIter )
      fSaveIter = fCollB->MakeIterator();
  }
  // Initialize our state so we point to the first item
  Next();
}

//_____________________________________________________________________________
HitPairIter::HitPairIter( const HitPairIter& rhs )
  : fCollA(rhs.fCollA), fCollB(rhs.fCollB), fIterA(0), fIterB(0),
    fSaveIter(0), fSaveHit(rhs.fSaveHit),
    fMaxDist(fMaxDist), fStarted(rhs.fStarted), fScanning(rhs.fScanning),
    fCurrent(rhs.fCurrent), fNext(rhs.fNext)
{
  // Copy ctor

  if( fCollA ) {
    fIterA = fCollA->MakeIterator();
    *fIterA = *rhs.fIterA;
  }
  if( fCollB ) {
    fIterB = fCollB->MakeIterator();
    *fIterB = *rhs.fIterB;
    fSaveIter = fCollB->MakeIterator();
    *fSaveIter = *rhs.fSaveIter;
  }
}

//_____________________________________________________________________________
HitPairIter& HitPairIter::operator=( const HitPairIter& rhs )
{
  // Assignment operator

  if( this != &rhs ) {
    fCollA     = rhs.fCollA;
    fCollB     = rhs.fCollB;
    fMaxDist   = rhs.fMaxDist;
    fStarted   = rhs.fStarted;
    fScanning  = rhs.fScanning;
    fCurrent   = rhs.fCurrent;
    fNext      = rhs.fNext;
    delete fIterA;
    delete fIterB;
    delete fSaveIter;
    if( fCollA ) {
      fIterA = fCollA->MakeIterator();
      *fIterA = *rhs.fIterA;
    }
    if( fCollB ) {
      fIterB = fCollB->MakeIterator();
      *fIterB = *rhs.fIterB;
      fSaveIter = fCollB->MakeIterator();
      *fSaveIter = *rhs.fSaveIter;
    }
  }
  return *this;
}

//_____________________________________________________________________________
HitPairIter::~HitPairIter()
{
  // Destructor

  delete fIterA;
  delete fIterB;
  delete fSaveIter;
}


//_____________________________________________________________________________
void HitPairIter::Reset()
{
  // Reset the iterator to the start

  fStarted = fScanning = kFALSE;
  if( fIterA )
    fIterA->Reset();
  if( fIterB )
    fIterB->Reset();
  // Our initial state is to point to the first object
  Next();
}


//_____________________________________________________________________________
HitPairIter& HitPairIter::Next()
{
  // Return next pair of hits along the wire plane. If a hit in either
  // plane is unpaired (no matching hit on the other plane within maxdist)
  // then only that hit is set in the returned pair object. If both
  // hits returned are zero, then there are no more hits in either plane.

  if( !fStarted ) {
    fNext = make_pair( fIterA ? fIterA->Next() : 0, 
		       fIterB ? fIterB->Next() : 0 );
    fStarted = kTRUE;
  }

  fCurrent = fNext;
  Hit* hitA = static_cast<Hit*>( fCurrent.first );
  Hit* hitB = static_cast<Hit*>( fCurrent.second );

  if( hitA && hitB ) {
    switch( hitA->Compare(hitB,fMaxDist) ) {
    case -1: // A<B 
      fNext.first  = fIterA->Next();
      fCurrent.second = 0;
      break;
    case  1: // A>B
      fNext.second = fIterB->Next();
      fCurrent.first = 0;
      break;
    default: // A==B
      {
	// Found a pair
	Hit* nextB = static_cast<Hit*>( fIterB->Next() );
	if( !nextB || hitA->Compare(nextB,fMaxDist) < 0 ) {
	  if( fScanning ) {
	    // End of a scan of plane B with fixed hitA
	    fScanning = kFALSE;
	    // Return B to the point where we started the scan
	    *fIterB = *fSaveIter;
	    hitB = fSaveHit;
	    // Advance to the next hit in A
	    hitA = static_cast<Hit*>( fIterA->Next() );
	    // Advance B until either B >= A or B == nextB (the hit in B that
	    // ended the prior scan), whichever comes first.
	    // The Bs for which saveB <= B < nextB have been paired with the 
	    // prior A in the prior scan and so can't be considered unpaired,
	    // but they might pair with the new A, still.
	    if( hitA ) {
	      while( hitB != nextB && hitB->Compare(hitA,fMaxDist) < 0 )
		hitB = static_cast<Hit*>( fIterB->Next() );
	    } else {
	      // Of course, if there are no more hits in A, we only have to 
	      // scan the rest of the Bs.
	      hitB = nextB;
	    }
	    fNext = make_pair( hitA, hitB );

	  } else {
	    // This is the normal case: nextB > hitA (usually true for 
	    // small maxdist). So hitA/hitB are a pair, and
	    // the next hits to consider are the next ones in each plane.
	    fNext = make_pair( fIterA->Next(), nextB );
	  }
	} else {
	  // A==B and A==nextB, so more than one B matches this A.
	  // Start scanning mode where we keep hitA fixed and walk along 
	  // B as long as B==A.
	  if( !fScanning ) {
	    fScanning = kTRUE;
	    // Save the starting point of the scan. We have to save both
	    // the iterator and the object because ROOT's TIterator
	    // is rather dumb (no Current() method).
	    *fSaveIter = *fIterB;
	    fSaveHit = hitB;
	  }
	  // nextB != 0 and hitA == nextB guaranteed here, so the
	  // next iteration will give hitA==hitB again.
	  fNext.second = nextB;
	}
	break;
      }
    }
  } else if( hitA ) {
    fNext.first  = fIterA->Next();
  } else if( hitB ) {
    fNext.second = fIterB->Next();
  }

  return *this;
}

} // end namespace TreeSearch

///////////////////////////////////////////////////////////////////////////////
