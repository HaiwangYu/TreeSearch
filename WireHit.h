//*-- Author:  Ole Hansen<mailto:ole@jlab.org>; Jefferson Lab; 11-Feb-2013
//
#ifndef ROOT_TreeSearch_WireHit
#define ROOT_TreeSearch_WireHit

///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// TreeSearch::WireHit                                                       //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "Hit.h"
#include "WirePlane.h"

namespace TreeSearch {

  class WireHit : public Hit {

  public:
    WireHit() {}
    WireHit( Int_t wnum, Double_t pos, Int_t tdc, Double_t time, Double_t res,
	     WirePlane* wp ) :
      Hit(pos, res, static_cast<Plane*>(wp)), fWireNum(wnum), fRawTDC(tdc),
      fTime(time), fPosL(pos), fPosR(pos), fCl(0), fMulti(0), fTdiff(0.0) {}
    virtual ~WireHit() {}

    virtual Int_t Compare( const TObject* obj ) const;
    virtual Int_t Compare( const Hit* rhs, Double_t maxdist ) const;
    virtual void  Print( Option_t* opt="" ) const;
 
    Double_t ConvertTimeToDist( Double_t slope );

    Int_t    GetWireNum()    const { return fWireNum; }
    Double_t GetWirePos()    const { return GetPos(); }
    Double_t GetRawTDC()     const { return fRawTDC; }
    Double_t GetDriftTime()  const { return fTime; }
    Double_t GetDriftDist()  const { return fPosR-fPos; }
    Double_t GetPosL()       const { return fPosL; }
    Double_t GetPosR()       const { return fPosR; }

    // Functor for ordering hits in sets
    struct WireNumLess : public std::binary_function< WireHit*, WireHit*, bool >
    {
      bool operator() ( const WireHit* a, const WireHit* b ) const
      { 
	assert( a && b );
	if( a->GetPlane()->GetType() != b->GetPlane()->GetType() )
	  return (a->GetPlane()->GetType() < b->GetPlane()->GetType());
	if( a->GetPlaneNum() < b->GetPlaneNum() ) return true;
	if( a->GetPlaneNum() > b->GetPlaneNum() ) return false;
	if( a->GetWireNum()  < b->GetWireNum()  ) return true;
	if( a->GetWireNum()  > b->GetWireNum()  ) return false;
	return ( a->GetDriftTime() < b->GetDriftTime() );
      }
    };

    // Functor for comparing hits in HitSet::IsSimilarTo().
    // Identical to WireNumLess if fMaxDist = 0. If fMaxDist > 0, all hits
    // that are at most fMaxDist apart are equivalent.
    struct WireDistLess : public std::binary_function< WireHit*, WireHit*, bool >
    {
      WireDistLess( Int_t maxdist ) : fMaxDist(maxdist) { assert(maxdist>=0); }
      bool operator() ( const WireHit* a, const WireHit* b ) const
      { 
	assert( a && b );
	if( a->GetPlane()->GetType() != b->GetPlane()->GetType() )
	  return (a->GetPlane()->GetType() < b->GetPlane()->GetType());
	if( a->GetPlaneNum() < b->GetPlaneNum() ) return true;
	if( a->GetPlaneNum() > b->GetPlaneNum() ) return false;
	if( a->GetWireNum() + fMaxDist < b->GetWireNum()  ) return true;
	if( fMaxDist > 0 ) return false;
	if( a->GetWireNum()  > b->GetWireNum()  ) return false;
	return ( a->GetDriftTime() < b->GetDriftTime() );
      }
      Int_t GetMaxDist() const { return fMaxDist; }
    private:
      Int_t fMaxDist;      // Max allowed distance between hits in a cluster
    };

  protected:
    Int_t    fWireNum;     // Wire number
    Int_t    fRawTDC;      // Raw TDC value (channels)
    Double_t fTime;        // Hit time corrected for TDC offset (s)
    Double_t fPosL;        // fPos - raw drift distance (m)
    Double_t fPosR;        // fPos + raw drift distance (m)

    // Only needed for TESTCODE, but kept for binary compatibility
    Int_t    fCl;          // Neighboring wire also fired
    Int_t    fMulti;       // Additional hits present on same wire
    Double_t fTdiff;       // Time difference to previous multihit
#ifdef TESTCODE
    friend void WirePlane::CheckCrosstalk();
#endif

    ClassDef(WireHit,1)    // Horizontal drift chamber hit
  };


  //___________________________________________________________________________
  // Monte Carlo hit class. Same as a hit plus the MC truth info.
  //TODO: Skeleton version - to be fleshed out

  class MCTrack;
  class MCWireHit : public WireHit {

  public:
    MCWireHit() : fMCTrack(0) {}
    MCWireHit( Int_t wnum, Double_t pos, Int_t tdc, Double_t time, Double_t res,
	       WirePlane* wp, MCTrack* mctrk, Double_t mcpos )
      : WireHit(wnum, pos, tdc, time, res, wp), fMCTrack(mctrk), fMCPos(mcpos) {}
    virtual ~MCWireHit() {}

    virtual void Print( Option_t* opt="" ) const;

    MCTrack* GetMCTrack() const { return fMCTrack; }
    Double_t GetMCPos()   const { return fMCPos; }

  protected:
    MCTrack* fMCTrack;     // MC track generating this hit (0=noise hit)
    Double_t fMCPos;       // Exact MC track crossing position (m)

    ClassDef(MCWireHit,1)  // Monte Carlo hit in horizontal drift chamber
  };

  //___________________________________________________________________________
  inline
  Int_t WireHit::Compare( const TObject* obj ) const 
  {
    // Used for sorting hits in a TSeqCollection (e.g. TList, TClonesArray).
    // A hit is "less than" another hit if its position is smaller.
    // Returns -1 if this is smaller than rhs, 0 if equal, +1 if greater.
    // Also, for hits on the same wire, the first hit on the wire (the one with
    // the smallest time) is "less than" one with a higher time.  If the hits
    // are sorted according to this scheme, they will be in order of increasing
    // wire number and, for each wire, will be in the order in which they hit
    // the wire

#ifdef NDEBUG
    const WireHit* rhs = static_cast<const WireHit*>(obj);
#else
    const WireHit* rhs = dynamic_cast<const WireHit*>(obj);
    assert( rhs );
#endif
    Int_t ret = Hit::Compare(obj);
    if( ret != 0 ) return ret;

    if( fTime < rhs->fTime ) return -1;
    if( fTime > rhs->fTime ) return  1;
    return 0;
  }

  //___________________________________________________________________________
  inline
  Int_t WireHit::Compare( const Hit* hit, Double_t maxdist ) const
  {
    // Determine if two hits are within maxdist of each other.
    // Returns -1 if this<rhs, 0 if overlap, +1 if this>rhs.
#ifdef NDEBUG
    const WireHit* rhs = static_cast<const WireHit*>(hit);
#else
    const WireHit* rhs = dynamic_cast<const WireHit*>(hit);
    assert( rhs );
#endif
    if( fPosR+maxdist < rhs->fPosL )
      // this hit is "smaller than" (to the left of) rhs
      return -1;
    if( rhs->fPosR+maxdist < fPosL )
      // this hit is "larger than" (to the right of) rhs
      return +1;
    // The hits overlap within the maxdist tolerance
    return 0;
  }

///////////////////////////////////////////////////////////////////////////////

} // end namespace TreeSearch


#endif
