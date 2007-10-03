///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// TreeSearch::PatternGenerator                                              //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "PatternGenerator.h"
#include "TMath.h"
#include "TString.h"
#include "TError.h"
#include <iostream>
#include <fstream>
#include <iomanip>
#include <map>
#include <ctime>
#if ROOT_VERSION_CODE < ROOT_VERSION(5,8,0)
#include <cstdlib>   // for atof()
#endif

//TODO: Interface to PatternTree
//TODO: Read() tree from binary file

using namespace std;

ClassImp(TreeSearch::PatternGenerator)

// Private definitions, used internally
namespace {

using namespace TreeSearch;

// Iterator over child patterns of a given parent patten
class ChildIter {
private:
  const Pattern fParent;  // copy of parent pattern
  Pattern   fChild;       // current child pattern
  Int_t     fCount;       // trial iterations left to do
  Int_t     fType;        // current pattern type (normal/shifted/mirrored)
public:
  ChildIter( const Pattern& parent ) 
    : fParent(parent), fChild(parent), fType(0) { reset(); }
  ChildIter&      operator++();
  const ChildIter operator++(int) { 
    ChildIter clone(*this);
    ++(*this); 
    return clone;
  }
  Pattern& operator*()            { return fChild; }
           operator bool()  const { return (fCount >= 0); }
  Int_t    type()           const { return fType; }
  void     reset() { 
    fCount = 1<<fParent.GetNbits();
    ++(*this);
  }
};

// "Visitor" objects for tree traversal operations. The object's operator()
// is applied to each tree node. These should be used with the TreeWalk
// iterator (see TreeWalk.h)

// Copy pattern to PatternTree object
class CopyPattern : public NodeVisitor {
public:
  CopyPattern( PatternTree* tree ) 
    : fTree(tree) { assert(fTree); }
  Int_t operator() ( const NodeDescriptor& nd );

private:
  PatternTree* fTree;    // Tree object to fill
  map<Pattern*,Int_t> fMap; // Index map for serializing 
};

// Write pattern to binary file
class WritePattern : public NodeVisitor {
public:
  WritePattern( const char* filename, size_t index_size = sizeof(Int_t) );
  ~WritePattern() { delete os; }
  Int_t operator() ( const NodeDescriptor& nd );

private:
  ofstream* os;        // Output file stream
  size_t    fIdxSiz;   // Required size for pattern count (1, 2, or 4)
  map<Pattern*,Int_t> fMap; // Index map for serializing 

  // Because of the pointer member, disallow copying and assignment
  WritePattern( const WritePattern& orig );
  WritePattern& operator=( const WritePattern& rhs );
};

// Count unique patterns (including shifts)
class CountPattern : public NodeVisitor {
public:
  CountPattern() : fCount(0) {}
  Int_t operator() ( const NodeDescriptor& nd ) { fCount++; return 0; }
  ULong64_t GetCount() const { return fCount; }

private:
  ULong64_t    fCount;    // Pattern count
};

// Pretty print to output stream (and count) all actual patterns
class PrintPattern : public NodeVisitor {
public:
  PrintPattern( ostream& ostr = cout, bool dump = false ) 
    : os(ostr), fCount(0), fDump(dump) {}
  Int_t operator() ( const NodeDescriptor& nd );
  ULong64_t GetCount() const { return fCount; }

private:
  ostream&     os;        // Destinaton stream
  ULong64_t    fCount;    // Pattern count
  Bool_t       fDump;     // Dump mode (one line per pattern)
};


//_____________________________________________________________________________
inline
ChildIter& ChildIter::operator++()
{ 
  // Iterator over all suitable child patterns of a given parent pattern
  // that occur when the bin resolution is doubled.
  // Child pattern bits are either 2*bit or 2*bit+1 of the parent bits,
  // yielding 2^nbits (=2^nplanes) different combinations.
  // The bits of suitable patterns must monotonically increase.
  // bit[0] is always zero (otherwise the pattern could be shifted).
  // Note that type() is an important part of the result.
  // type & 1 indicates a pattern shifted by one to the right
  // type & 2 indicates a mirrored pattern 
  // To recover the actual pattern, mirror first, then shift, as appropriate.
  // With the self-referential tree structure used here, mirrored patterns
  // only ever occur as children of the root of the tree. Simultaneously
  // mirrored and shifted patterns never occur.
  // Hence, type = 0, 1, and, very rarely, 2.

  if( fCount > 0 ) {
    while( fCount-- ) {
      Int_t maxbit = 0;
      Int_t minbit = 1;
      UInt_t nbits = fChild.GetNbits();
      for( UInt_t ibit = nbits; ibit--; ) {
	Int_t bit = fParent[ibit] << 1;
	if( fCount & (1<<ibit) )
	  ++bit;
	fChild[ibit] = bit;
	if( bit < minbit ) minbit = bit;
	if( bit > maxbit ) maxbit = bit;
      }
      Int_t width = fChild.GetWidth();
      if( maxbit-minbit > TMath::Abs(width) )
	continue;
      if( minbit == 0 )
	fType = 0;
      else {
	fType = 1;
	for( UInt_t ibit = nbits; ibit; )
	  --fChild[--ibit];
      }
      if( width < 0 ) {
	fType += 2;
	width = -width;
	for( UInt_t ibit = nbits; ibit--; )
	  fChild[ibit] = width-fChild[ibit];
      }
      break;
    }
  } else
    fCount = -1;
  return *this;
}

//_____________________________________________________________________________
//FIXME: TEST TEST
void print( int i )
{
  cout << "val = " << i << endl;
}

//_____________________________________________________________________________
inline
Int_t CopyPattern::operator() ( const NodeDescriptor& nd )
{
  Pattern* node = nd.link->GetPattern();
  map<Pattern*,Int_t>::iterator idx = fMap.find(node);
  if( idx == fMap.end() ) {
    map<Pattern*,Int_t>::size_type n = fMap.size();
    fMap[node] = n;
    // FIXME: TEST TEST
    print(-1);
    if( fTree->AddPattern(nd.link) )
      return -1;
    Int_t nchild = 0;
    Link* ln = node->GetChild();
    while( ln ) {
      nchild++;
      ln = ln->Next();
    }
    print(nchild);
    return 0;
  } else {
    print(nd.link->Type());
    print(idx->second);
    return 1;
  }
}

//_____________________________________________________________________________
WritePattern::WritePattern( const char* filename, size_t index_size )
  : os(0), fIdxSiz(index_size)
{
  static const char* here = "PatternGenerator::WritePattern";

  if( filename && *filename ) {
    os = new ofstream(filename, ios::out|ios::binary|ios::trunc);
    if( !os || !(*os) ) {
      ::Error( here, "Error opening treefile %s", filename );
      if( os )
	delete os;
      os = 0;
    }
  } else {
    ::Error( here, "Invalid file name" );
  }
  if( (fIdxSiz & (fIdxSiz-1)) != 0 ) {
    ::Error( here, "Invalid index_size. Must be a power of 2" );
    fIdxSiz = sizeof(Int_t);
  }    
}

//_____________________________________________________________________________
template< typename T>
inline
void swapped_binary_write( ostream& os, const T& data, size_t n = 1, 
			   size_t start = 0 )
{
  // Write "n" elements of "data" to "os" in binary big-endian (MSB) format.
  // "start" indicates an optional _byte_ skip count from the beginning of
  // the big-endian format data - designed to write only the non-trivial
  // part of scalar data for which the upper bytes are known to be always zero.
  size_t size = sizeof(T);
  const char* bytes = reinterpret_cast<const char*>( &data );
#ifdef R__BYTESWAP
  size_t k = size-1;
  for( size_t i = start; i < n * size; ++i )
    os.put( bytes[(i&~k)+(k-i&k)] );
#else
  os.write( bytes+start, n*size-start );
#endif
}

//_____________________________________________________________________________
inline
Int_t WritePattern::operator() ( const NodeDescriptor& nd )
{
  // Write the single pattern referenced by "nd" to the binary output stream.
  // In essence, this implements the serialization method for cyclical graphs
  // described in
  // http://www.parashift.com/c++-faq-lite/serialization.html#faq-36.11

  if( !os )
    return -1;
  Pattern* node = nd.link->GetPattern();
  map<Pattern*,Int_t>::iterator idx = fMap.find(node);
  if( idx == fMap.end() ) {
    map<Pattern*,Int_t>::size_type n = fMap.size();
    fMap[node] = n;
    // Header for new pattern: link type + 128 (=128-130)
    os->put( nd.link->Type() | 0x80 );
    if( os->fail() ) return -1;
    // Pattern data. NB: fBits[0] is always 0, so we can skip it
    swapped_binary_write( *os, node->GetBits()[1], node->GetNbits()-1 );
    UShort_t nchild = 0;
    Link* ln = node->GetChild();
    while( ln ) {
      nchild++;
      ln = ln->Next();
    }
    // Child node count
    swapped_binary_write( *os, nchild );
    if( os->fail() ) return -1;
    // Write child nodes
    return 0;
  } else {
    // Reference pattern header: the plain link type (=0-2)
    os->put( nd.link->Type() );
    // Reference index
    swapped_binary_write( *os, idx->second, 1, sizeof(Int_t)-fIdxSiz );
    if( os->fail() ) return -1;
    // Don't write child nodes
    return 1;
  }
}

//_____________________________________________________________________________
inline
Int_t PrintPattern::operator() ( const NodeDescriptor& nd )
{
  // Print actual (shifted & mirrored) pattern described by "nd".

  ++fCount;
  if( fDump )
    os << setw(2) << nd.depth;

  Pattern* node = nd.link->GetPattern();
  for( UInt_t i = 0; i < node->GetNbits(); i++ ) {
    UInt_t v = node->GetBits()[i];
    if( nd.mirrored )  // Mirrored pattern
      v = node->GetWidth() - v;
    v += nd.shift;

    // In dump mode, write out one pattern n-tuple per line
    if( fDump )
      os << " " << setw(5) << v;

    // Otherwise draw a pretty ASCII picture of the pattern
    else {
      UInt_t op = (nd.mirrored ? 2 : 0) + nd.link->Shift();
      os << static_cast<UInt_t>(nd.depth) << "-" << op;
      for( UInt_t k = 0; k < nd.depth; ++k )
	os << " ";
      os << " |";
      for( UInt_t k = 0; k < v; ++k )
	os << ".";
      os << "O";
      for( UInt_t k = (1<<nd.depth)-1; k > v; --k )
	os << ".";
      os << "|" << endl;
    }
  }
  os << endl;

  // Process child nodes
  return 0;
}

///////////////////////////////////////////////////////////////////////////////

} //end namespace

namespace TreeSearch {

//_____________________________________________________________________________
// Linked-list node for the hash table of base patterns

class HashNode {
  friend class PatternGenerator;
private:
  Pattern*   fPattern;    // Bit pattern treenode
  HashNode*  fNext;       // Next linked list element
  UInt_t     fMinDepth;   // Minimum valid depth for this pattern (<=16)

public:
  HashNode( Pattern* pat, HashNode* next )
    : fPattern(pat), fNext(next), fMinDepth(-1) {}
  Pattern*   GetPattern() const { return fPattern; }
  HashNode*  Next()       const { return fNext; }
  
  void       UsedAtDepth( UInt_t depth ) {
    if( depth < fMinDepth ) fMinDepth = depth;
  }
}; // end class HashNode

//_____________________________________________________________________________
PatternGenerator::PatternGenerator()
  : fNlevels(0), fNplanes(0), fMaxSlope(0)
{
  // Constructor

}

//_____________________________________________________________________________
PatternGenerator::~PatternGenerator()
{
  // Destructor

  DeleteTree();

}

//_____________________________________________________________________________
void PatternGenerator::DeleteTree()
{
  // Delete the build tree along with its hash table.
  // Internal utility function.

  for( vector<HashNode*>::iterator it = fHashTable.begin();
       it != fHashTable.end(); ++it ) {
    HashNode* hashnode = *it;
    while( hashnode ) {
      HashNode* cur_node = hashnode;
      hashnode = hashnode->Next();
      assert( cur_node->GetPattern() );
      delete cur_node->GetPattern();
      delete cur_node;
      break;
    }
  }
  fHashTable.clear();
}

//_____________________________________________________________________________
void PatternGenerator::CalcStatistics()
{
  // Collect statistics on the build tree. This is best done separately here
  // because some things (averages, memory requirements) can only be 
  // calculated once the tree is complete.

  memset( &fStats, 0, sizeof(Statistics_t) );

  for( vector<HashNode*>::const_iterator it = fHashTable.begin();
       it != fHashTable.end(); ++it ) {
    // Each hashnode points to a unique pattern by construction of the table
    HashNode* hashnode = *it;
    UInt_t hash_length = 0;
    while( hashnode ) {
      // Count patterns
      fStats.nPatterns++;
      // Count child nodes and length of child list
      Pattern* pat = hashnode->GetPattern();
      assert(pat);
      Link* ln = pat->fChild;
      UInt_t list_length = 0;
      while( ln ) {
	fStats.nLinks++;
	list_length++;
	ln = ln->Next();
      }
      if( list_length > fStats.MaxChildListLength )
	fStats.MaxChildListLength = list_length;
      // Count collision list depth
      hash_length++;

      hashnode = hashnode->Next();
    } // while hashnode

    if( hash_length > fStats.MaxHashDepth )
      fStats.MaxHashDepth = hash_length;

  } // hashtable elements

  fStats.nBytes = 
    fStats.nPatterns * sizeof(Pattern)
    + fStats.nPatterns * fNplanes * sizeof(UShort_t)
    + fStats.nLinks * sizeof(Link);
  fStats.nHashBytes = fHashTable.size() * sizeof(HashNode*)
    + fStats.nPatterns * sizeof(Link);
}

//_____________________________________________________________________________
void PatternGenerator::Print( Option_t* opt, ostream& os ) const
{
  // Print information about the tree, depending on option

  // Dump all stored patterns, using Pattern::print()
  if( *opt == 'D' ) {
    for( vector<HashNode*>::const_iterator it = fHashTable.begin();
	 it != fHashTable.end(); ++it ) {
      HashNode* hashnode = *it;
      while( hashnode ) {
	Pattern* pat = hashnode->GetPattern();
	pat->Print( true, os );
	hashnode = hashnode->Next();
      }
    }
    return;
  }

  Link root_link( fHashTable[0]->GetPattern(), 0, 0 );
  // Print ASCII pictures of ALL actual patterns in the tree
  if( *opt == 'P' ) {
    PrintPattern print(os);
    fTreeWalk( &root_link, print );
    return;
  }

  // Dump n-tuples of all actual patterns, one per line
  if( *opt == 'L' ) {
    PrintPattern print(os,true);
    fTreeWalk( &root_link, print );
    return;
  }

  // Count all actual patterns
  if( *opt == 'C' ) {
    CountPattern count;
    fTreeWalk( &root_link, count );
    os << "Total pattern count = " << count.GetCount() << endl;
    return;
  }

  // Basic info
  os << "tree: nlevels = " << fNlevels
     << ", nplanes = " << fNplanes
     << ", zpos = ";
  for( UInt_t i=0; i<fZ.size(); i++ ) {
    os << fZ[i];
    if( i+1 != fZ.size() )
      os << ",";
  }
  os << endl;

  os << "patterns = " << fStats.nPatterns
     << ", links = "   << fStats.nLinks
     << ", bytes = " << fStats.nBytes
     << endl;
  os << "maxlinklen = " << fStats.MaxChildListLength
     << ", maxhash = " << fStats.MaxHashDepth
     << ", hashbytes = " << fStats.nHashBytes
     << endl;
  os << "time = " << fStats.BuildTime << " s" << endl;
 
}

//_____________________________________________________________________________
Int_t PatternGenerator::Write( const char* filename )
{
  // Write tree to binary file

  // TODO: write header

  size_t index_size = sizeof(Int_t);
  if( fStats.nPatterns < (1U<<8) )
    index_size = 1;
  else if( fStats.nPatterns < (1U<<16) )
    index_size = 2;
  WritePattern write(filename,index_size);
  Link root_link( fHashTable[0]->GetPattern(), 0, 0 );
  return fTreeWalk( &root_link, write );
}

//_____________________________________________________________________________
HashNode* PatternGenerator::AddHash( Pattern* pat )
{
  // Add given pattern to the hash table

  assert(pat);
  UInt_t hashsize = fHashTable.size();
  if( hashsize == 0 ) {
    // Set the size of the hash table.
    // 2^(nlevels-1)*2^(nplanes-2) is the upper limit for the number of
    // patterns, so a size of 2^(nlevels-1) will give 2^(nplanes-2) collisions
    // per entry (i.e. 2, 4, 8), with which we can live. Anything better would
    // require a cleverer hash function.
    fHashTable.resize( 1<<(fNlevels-1), 0 );
    hashsize = fHashTable.size();
  }
  Int_t hash = pat->Hash()%hashsize;
  return fHashTable[hash] = new HashNode( pat, fHashTable[hash] );
}

//_____________________________________________________________________________
PatternTree* PatternGenerator::Generate( TreeParam_t parameters )
{
  // Generate a new pattern tree for the given parameters. Returns a pointer
  // to the generated tree, or zero if error

  DeleteTree();

  // Set parameters for the new build.
  if( PatternTree::Normalize(parameters) != 0 )
    return 0;

  fNlevels  = parameters.maxdepth+1;
  fZ        = parameters.zpos;
  fNplanes  = fZ.size();
  fMaxSlope = parameters.maxslope;

  fTreeWalk.SetNlevels( fNlevels );

  // Benchmark the build
  clock_t cpu_ticks = clock();

  // Start with the trivial all-zero root node at depth 0. 
  Pattern* root = new Pattern( fNplanes );
  HashNode* hroot = AddHash( root );

  // Generate the tree recursively
  MakeChildNodes( hroot, 1 );
  
  // Calculate tree statistics (number of patterns, links etc.)
  cpu_ticks = clock() - cpu_ticks;
  CalcStatistics();
  fStats.BuildTime = static_cast<Double_t>(cpu_ticks)/CLOCKS_PER_SEC;

  //FIXME: TEST
  // Print tree statistics
  Print();

  // Create a PatternTree object from the build tree
  PatternTree* tree = 0;
//   PatternTree* tree = new PatternTree( parameters,
// 				       fStats.nPatterns,
// 				       fStats.nLinks );
  if( tree ) {
//     Int_t (PatternTree::*CopyPattern)(int) = &PatternTree::AddPattern;
//     (tree->*CopyPattern)(1);
    
    Link root_link(root,0,0);
    CopyPattern copy(tree);
    fTreeWalk( &root_link, copy );
  }

  return tree;
}

//_____________________________________________________________________________
PatternTree* PatternGenerator::Generate( UInt_t maxdepth, 
					 Double_t detector_width, 
					 const char* zpos,
					 Double_t maxslope )
{
  // Convenience function for use at the command line and in scripts

  TreeParam_t param;
  param.maxdepth = maxdepth;
  param.width    = detector_width;
  param.maxslope = maxslope;
  
  TString zlist( zpos );
  TString tok;
  Ssiz_t pos = 0;
  while( zlist.Tokenize(tok, pos, ",") ) {
    if( tok.IsNull() )
      continue;
#if ROOT_VERSION_CODE < ROOT_VERSION(5,8,0)
    Double_t val = atof(tok.Data());
#else
    Double_t val = tok.Atof();
#endif
    param.zpos.push_back( val );
  }

  return Generate( param );
}

//_____________________________________________________________________________
bool PatternGenerator::TestSlope( const Pattern& pat, UInt_t depth )
{
  UInt_t width = pat.GetWidth();
  return ( width < 2 ||
	   TMath::Abs((double)(width-1) / (double)(1<<depth)) <= fMaxSlope );
}

//_____________________________________________________________________________
bool PatternGenerator::LineCheck( const Pattern& pat )
{
  // Check if the gievn bit pattern is consistent with a straight line.
  // The intersection plane positions are given by fZ[].
  // Assumes a normalized pattern, for which pat[0] is always zero.
  // Assumes identical bin sizes and positions in each plane.

  // FIXME FIXME: for certain z-values, the following can be _very_ sensitive 
  // to the floating point rounding behavior!

  assert(fNplanes);
  Double_t xL   = pat[fNplanes-1];
  Double_t xRm1 = xL;               // xR-1
  Double_t zL   = fZ[fNplanes-1];
  Double_t zR   = zL;

  for( Int_t i = fNplanes-2; i > 0; --i ) {
    // Compare the intersection point with the i-th plane of the left edge 
    // of the band, (xL-x0) * z[i]/zL, to the left edge of the bin, pat[i]-x0. 
    // If the difference is equal or larger than one bin width (=1), the bin is
    // outside of the allowed band.
    // Multiply with zL (to avoid division) and recall x0 = 0.
    Double_t dL = xL*fZ[i] - pat[i]*zL;
    if( TMath::Abs(dL) >= zL )
      return false;
    // Likewise for the right edge
    Double_t dR = xRm1*fZ[i] - pat[i]*zR;
    if( TMath::Abs(dR) >= zR )
      return false;

    if( i > 1 ) {
      // If dL>0, the right edge of the bin is inside the band,
      // so set a new right-side limit.
      if( dL > 0 ) {
	xRm1 = pat[i];
	zR   = fZ[i];
      }
      // Likewise for the left-side limit
      if( dR < 0 ) {
	xL = pat[i];
	zL = fZ[i];
      }
    }
  } // planes
  return true;
}

//_____________________________________________________________________________
HashNode* PatternGenerator::Find( const Pattern& pat )
{
  // Search for the given pattern in the current database

  UInt_t hashsize = fHashTable.size();
  assert(hashsize);
  Int_t hash = pat.Hash()%hashsize;
  HashNode* hashNode = fHashTable[hash];
  while( hashNode ) {
    Pattern* rhs = hashNode->GetPattern();
    if( pat == *rhs )
      return hashNode;
    hashNode = hashNode->Next();
  }
  return 0;
}

//_____________________________________________________________________________
void PatternGenerator::MakeChildNodes( HashNode* pnode, UInt_t depth )
{
  // Generate child nodes for the given parent pattern

  // Requesting child nodes for the parent at this depth implies that the 
  // parent is being used at the level above
  if( depth > 0 )
    pnode->UsedAtDepth( depth-1 );

  // Base case of the recursion: no child nodes beyond fNlevels-1
  if( depth >= fNlevels )
    return;

  // If not already done, generate the child patterns of this parent
  Pattern* parent = pnode->GetPattern();
  assert(parent);
  if( !parent->fChild ) {
    ChildIter it( *parent );
    while( it ) {
      Pattern& child = *it;

      // Pattern already exists?
      HashNode* node = Find( child );
      if( node ) {
	Pattern* pat = node->GetPattern();
	assert(pat);
	// If the pattern has only been tested at a higher depth, we need to
	// redo the slope test since the slope is larger now at lower depth
	if( depth >= node->fMinDepth || TestSlope(*pat, depth)) {
	  // Only add a reference to the existing pattern
	  parent->AddChild( pat, it.type() );
	}
      } else if( TestSlope(child, depth) && LineCheck(child) ) {
	// If the pattern is new, check it for consistency with maxslope and 
	// the straight line condition.
	Pattern* pat = new Pattern( child );
	AddHash( pat );
	parent->AddChild( pat, it.type() );
      }
      ++it;
    }
  }

  // Recursively generate child nodes down the tree
  Link* ln = parent->GetChild();
  while( ln ) {
    Pattern* pat = ln->GetPattern();
    // This Find() is necessary because we don't want to store the state 
    // parameter fMinDepth with the pattern itself - if we did so, we'd
    // get bigger patterns but minimally faster code here
    HashNode* node = Find( *pat );
    assert(node);
    // We only need to go deeper if either this pattern does not have children
    // yet OR (important!) children were previously generated from a deeper
    // location in the tree and so this pattern's subtree needs to be extended
    // deeper down now.
    if( !pat->fChild || node->fMinDepth > depth )
      MakeChildNodes( node, depth+1 );
    ln = ln->Next();
  }
}

///////////////////////////////////////////////////////////////////////////////

} // end namespace TreeSearch
