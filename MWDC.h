#ifndef ROOT_TreeSearch_MWDC
#define ROOT_TreeSearch_MWDC

///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// TreeSearch::MWDC                                                          //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "Tracker.h"

namespace TreeSearch {

  class MWDC : public Tracker {
  public:
    MWDC( const char* name, const char* description = "",
	  THaApparatus* app = 0 );
    virtual ~MWDC();

    virtual Int_t   Decode( const THaEvData& );
    virtual EStatus Init( const TDatime& date );

    Double_t        GetRefTime( UInt_t i ) const
    { return (i<(UInt_t)fRefMap->GetSize()) ? fRefTime[i] : kBig; }

    // Analysis control flags. Set via database.
    enum {
      kDoTimeCut  = BIT(14), // Use TDC time cut in decoder (defined in planes)
      kPairsOnly  = BIT(15), // Accept only pairs of hits from plane pairs
      kNoPartner  = BIT(17), // Never partner wire planes
    };

  protected:

    THaDetMap*     fRefMap;      // Map of reference channels for VME readout
    vector<float>  fRefTime;     // [fRefMap->GetSize()] ref channel data

    virtual UInt_t GetCrateMapDBcols() const;

    virtual Plane* MakePlane( const char* name, const char* description = "",
			      THaDetectorBase* parent = 0 ) const;
    virtual Projection* MakeProjection( EProjType type, const char* name,
					Double_t angle,
					THaDetectorBase* parent ) const;
    virtual UInt_t MatchRoadsImpl( vector<Rvec_t>& roads, UInt_t ncombos,
				   std::list<std::pair<Double_t,Rvec_t> >& combos_found,
				   Rset_t& unique_found );

    virtual THaAnalysisObject::EStatus PartnerPlanes();

    // Podd interface
    virtual Int_t  ReadDatabase( const TDatime& date );

#ifdef MCDATA
    // Helper class to use with FindHitForMCPoint & FindTrackForMCPoint
    class LRPointUpdater : public MCPointUpdater {
    public:
      virtual void UpdateHit( Podd::MCTrackPoint* pt, Hit* hit,
			      Double_t x ) const;
    };
#endif

    ClassDef(MWDC,0)   // TreeSearch reconstruction of a horizontal drift chamber system
  };

}

///////////////////////////////////////////////////////////////////////////////

#endif
