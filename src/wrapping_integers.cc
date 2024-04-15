#include "wrapping_integers.hh"
#include <cstdint>
#include <limits>

using namespace std;

Wrap32 Wrap32::wrap( uint64_t n, Wrap32 zero_point )
{
  // Your code here.
  return Wrap32 { static_cast<uint32_t>( n + zero_point.raw_value_ ) };
}

uint64_t Wrap32::unwrap( Wrap32 zero_point, uint64_t checkpoint ) const
{
  // Your code here.
  uint64_t diff = raw_value_ - static_cast<uint32_t>( checkpoint + zero_point.raw_value_ );
  if ( diff < a or checkpoint + diff < b ) {
    return checkpoint + diff;
  } else {
    return checkpoint + diff - b;
  }
}
