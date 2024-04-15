#include "reassembler.hh"
#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

using namespace std;

void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring )
{
  if ( is_last_substring && !is_last_substring_ ) {
    is_last_substring_ = true;
    max_index_ = first_index + data.size();
  }

  if ( data.empty() ) {
    if ( is_last_substring_ && first_unassembled_index_ == max_index_ ) {
      output_.writer().close();
    }
    return;
  }

  if ( writer().available_capacity() == 0 ) {
    return;
  }

  uint64_t last_index = first_index + data.size();

  auto reject_index = first_unassembled_index_ + output_.writer().available_capacity();

  // 字符串已经写入stream或者超出容量，则丢弃，注意左闭右开，这里取等号
  if ( last_index <= first_unassembled_index_ || first_index >= reject_index ) {
    return;
  }

  uint64_t offset = 0;
  if ( first_index < first_unassembled_index_ ) {
    offset = first_unassembled_index_ - first_index;
    first_index = first_unassembled_index_;
  }

  last_index = min( last_index, reject_index );

  string_view middle( data.data() + offset, last_index - first_index );

  string res;

  auto find_pos = first_index;
  auto it = buffer_.upper_bound( find_pos );
  auto begin_index = first_index;
  auto end_index = last_index;

  if ( first_index > first_unassembled_index_ ) {

    if ( buffer_.empty() ) {
      buffer_.emplace( first_index, data.substr( offset, last_index - first_index ) );
      bytes_pending_ += last_index - first_index;
      return;
    }

    auto prev = it;
    --prev;

    // 如果it为头 prev--后还是头
    begin_index = it == buffer_.begin() ? first_index : max( begin_index, prev->first + prev->second.size() );

    uint64_t right_index, len;

    while ( it != buffer_.end() && begin_index < end_index ) {
      right_index = max( begin_index, min( end_index, it->first ) );
      len = right_index - begin_index;
      if ( len > 0 ) {
        buffer_.emplace_hint(
          it, begin_index, data.substr( begin_index - first_index - offset, right_index - begin_index ) );
        bytes_pending_ += len;
      }
      begin_index = right_index == end_index ? end_index : max( begin_index, it->first + it->second.size() );
      ++it;
    }

    if ( begin_index < end_index ) {
      buffer_.emplace_hint(
        it, begin_index, data.substr( begin_index - first_index + offset, end_index - begin_index ) );
      bytes_pending_ += end_index - begin_index;
    }

    return;
  }

  // 如果buffer里面前面的字节都送过来了，写入bytestream

  res.reserve( bytes_pending_ );

  first_unassembled_index_ += middle.size();
  res.append( middle );

  while ( !buffer_.empty() && first_unassembled_index_ >= buffer_.begin()->first ) {
    if ( first_unassembled_index_ < buffer_.begin()->first + buffer_.begin()->second.size() ) {
      // 字符串中可能有\0，需要指定长度
      string_view tail( buffer_.begin()->second.data() + first_unassembled_index_ - buffer_.begin()->first,
                        buffer_.begin()->first + buffer_.begin()->second.size() - first_unassembled_index_ );
      first_unassembled_index_ += tail.size();
      res.append( tail );
    }
    bytes_pending_ -= buffer_.begin()->second.size();
    buffer_.erase( buffer_.begin() );
  }

  output_.writer().push( move( res ) );

  if ( is_last_substring_ && first_unassembled_index_ == max_index_ ) {
    output_.writer().close();
  }
}

uint64_t Reassembler::bytes_pending() const
{
  // Your code here.
  return bytes_pending_;
}
