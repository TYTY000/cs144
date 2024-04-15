#include "tcp_receiver.hh"
#include "byte_stream.hh"
#include "tcp_receiver_message.hh"
#include <cstdint>
#include <limits>
#include <optional>

using namespace std;

void TCPReceiver::receive( TCPSenderMessage message )
{
  // Your code here.
  if ( message.RST ) {
    reader().set_error();
    return;
  }

  if ( !zero_point_ ) {
    if ( !message.SYN ) {
      return;
    }
    zero_point_ = message.seqno;
  }
  uint64_t abs_seq = message.seqno.unwrap( zero_point_.value(), writer().bytes_pushed() );
  reassembler_.insert( message.SYN ? 0 : abs_seq - 1, move( message.payload ), message.FIN );
}

TCPReceiverMessage TCPReceiver::send() const
{
  // Your code here.
  if ( reassembler_.reader().has_error() ) {
    return { nullopt, 0, true };
  }
  TCPReceiverMessage message;
  message.window_size = ( writer().available_capacity() >= numeric_limits<uint16_t>::max() )
                          ? numeric_limits<uint16_t>::max()
                          : writer().available_capacity();
  if ( zero_point_.has_value() ) {
    message.ackno = Wrap32::wrap( writer().bytes_pushed() + 1 + writer().is_closed(), zero_point_.value() );
  }
  return message;
}
