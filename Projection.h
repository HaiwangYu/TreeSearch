#ifndef ROOT_TreeSearch_Projection
#define ROOT_TreeSearch_Projection

///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// TreeSearch::Projection                                                    //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "THaAnalysisObject.h"
#include "TreeWalk.h"
#include "Hit.h"
#include "TMath.h"
#include <vector>
#include <map>
#include <list>
#include <cassert>

class THaDetectorBase;
class TBits;

namespace TreeSearch {

  class Hitpattern;
  class PatternTree;
  class WirePlane;
  class Road;

  class Projection : public THaAnalysisObject {
  public:

    Projection( Int_t type, const char* name, Double_t angle,
		THaDetectorBase* parent = 0 );
    Projection( const Projection& orig );
    const Projection& operator=( const Projection& rhs );
    virtual ~Projection();

    void            AddPlane( WirePlane* wp, WirePlane* partner = 0 );
    virtual void    Clear( Option_t* opt="" );
    virtual Int_t   Decode( const THaEvData& );
    EStatus         InitLevel2( const TDatime& date );
    virtual void    Print( Option_t* opt="" ) const;
    void            Reset();

    Int_t           FillHitpattern();
    Int_t           Track();
    Int_t           MakeRoads();


    Double_t        GetAngle() const;
    UInt_t          GetClusterMaxDist() const { return fClusterMaxDist; }
    Double_t        GetCosAngle() const { return fCosAngle; }
    Hitpattern*     GetHitpattern() const { return fHitpattern; }
    TBits*          GetLayerCombos() const { return fLayerCombos; }
    WirePlane*      GetLayer( UInt_t layer )  const { return fLayers[layer]; }
    Double_t        GetLayerZ( UInt_t layer ) const;
    Double_t        GetMaxSlope() const { return fMaxSlope; }
    UInt_t          GetNlevels()  const { return fNlevels; }
    UInt_t          GetNlayers()  const { return (UInt_t)fLayers.size(); }
    UInt_t          GetNplanes()  const { return (UInt_t)fPlanes.size(); }
    UInt_t          GetPatternMaxDist() const { return fPatternMaxDist; }
    TBits*          GetPlaneCombos() const { return fPlaneCombos; }
    WirePlane*      GetPlane( UInt_t plane )  const { return fPlanes[plane]; }
    Double_t        GetPlaneZ( UInt_t plane ) const;
    Double_t        GetSinAngle() const { return fSinAngle; }
    Int_t           GetType()  const { return fType; }
    Double_t        GetWidth() const { return fWidth; }
    Double_t        GetZsize() const;

    void            SetMaxSlope( Double_t m ) { fMaxSlope = m; }
    void            SetPatternTree( PatternTree* pt ) { fPatternTree = pt; }
    void            SetWidth( Double_t width ) { fWidth = width; }
    
    //FIXME: for testing
//  std::vector<TreeSearch::WirePlane*>& GetListOfPlanes() { return fPlanes; }
//  std::vector<TreeSearch::WirePlane*>& GetListOfLayers() { return fLayers; }

  protected:
    Int_t           fType;        // Type of plane (u,v,x,y...)
    std::vector<WirePlane*> fPlanes; // Wire planes in this projection
    std::vector<WirePlane*> fLayers; // Effective detector planes (wp pairs)
    UInt_t          fNlevels;     // Number of levels of search tree
    Double_t        fMaxSlope;    // Maximum physical track slope (0=perp)
    Double_t        fWidth;       // Width of tracking region (m)
    Double_t        fSinAngle;    // Sine of wire angle
    Double_t        fCosAngle;    // Cosine of wire angle
    UInt_t          fClusterMaxDist; // Max allowed distance between hits for
                                  // clustering patterns into roads
    UInt_t          fPatternMaxDist; // Search distance for MakeRoads

    Hitpattern*     fHitpattern;  // Hitpattern of current event
    PatternTree*    fPatternTree; // Precomputed template database

    THaDetectorBase* fDetector;    //! Parent detector

    // Patterns found by TreeSearch
    std::map<const NodeDescriptor,HitSet> fPatternsFound;
    std::list<Road*> fRoads;    // Roads found by MakeRoads

    TBits*           fPlaneCombos; // Allowed plane combos with missing hits
    TBits*           fLayerCombos; // Allowed layer combos with missing hits

#ifdef TESTCODE
    UInt_t n_hits, n_bins, n_binhits, maxhits_bin;
    UInt_t n_test, n_pat, n_roads, n_badroads;
    Double_t t_treesearch, t_roads, t_fit, t_track;
#endif

    void  SetAngle( Double_t a );

    // Podd interface
    virtual Int_t ReadDatabase( const TDatime& date );
    virtual Int_t DefineVariables( EMode mode = kDefine );
    virtual const char* GetDBFileName() const;
    virtual void MakePrefix();

    // NodeVisitor class for comparing patterns in the tree with the
    // hitpattern. Matches represent candidates for track roads and are
    // added to the list of roads for further analysis
    class ComparePattern : public NodeVisitor {
    public:
      ComparePattern( const Hitpattern* hitpat, const TBits* combos,
		      std::map<const NodeDescriptor,HitSet>* matches )
	: fHitpattern(hitpat), fLayerCombos(combos), fMatches(matches)
#ifdef TESTCODE
	, fNtest(0)
#endif
      { assert(fHitpattern && fLayerCombos && fMatches); }
      virtual ETreeOp operator() ( const NodeDescriptor& nd );
#ifdef TESTCODE
      UInt_t GetNtest() const { return fNtest; }
#endif
    private:
      const Hitpattern* fHitpattern;   // Hitpattern to compare to
      const TBits*      fLayerCombos;  // Allowed combos of missing layers
      // Set of matching patterns
      std::map<const NodeDescriptor,HitSet>* fMatches;
#ifdef TESTCODE
      UInt_t fNtest;  // Number of pattern comparisons
#endif
    };

    ClassDef(Projection,0)  // A track projection plane
  };

  //___________________________________________________________________________
  inline
  Double_t Projection::GetAngle() const {
    // Return wire angle in rad, normalized to [-pi,pi]
    Double_t a = TMath::ASin(fSinAngle);
    if( fCosAngle < 0.0 )
      return (fSinAngle > 0.0) ? TMath::TwoPi() - a : -TMath::TwoPi() - a;
  
    return a;
  }

///////////////////////////////////////////////////////////////////////////////

} // end namespace TreeSearch

#endif
