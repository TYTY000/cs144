#include "tcp_sender.hh"
#include "tcp_config.hh"
#include "wrapping_integers.hh"
#include <algorithm>
#include <cstdint>
#include <locale>
#include <stdexcept>
#include <string_view>

using namespace std;

uint64_t TCPSender::sequence_numbers_in_flight() const
{
  // Your code here.
  return pending_seqno_;
}

uint64_t TCPSender::consecutive_retransmissions() const
{
  // Your code here.
  return retry_trans_;
}

void TCPSender::push( const TransmitFunction& transmit )
{
  // Your code here.
  if ( ( window_size_ and window_size_ <= pending_seqno_ ) or ( window_size_ == 0 and pending_seqno_ >= 1 ) ) {
    return;
  }

  Wrap32 seqno = Wrap32::wrap( abs_seqno_, isn_ );
  auto avai_window = window_size_ == 0 ? 1 : window_size_ - pending_seqno_ - static_cast<uint16_t>( isn_ == seqno );

  // preparing package.
  string str;
  while ( reader().bytes_buffered() and static_cast<uint16_t>( str.size() ) < avai_window ) {
    auto view = reader().peek();
    if ( view.empty() ) {
      throw runtime_error( "Broken package by peeking." );
    }
    view = view.substr( 0, avai_window - str.size() );
    str += view;
    input_.reader().pop( view.size() );
  }

  // pushing package.
  string_view view( str );
  size_t len;

  while ( !view.empty() or seqno == isn_ or ( !FIN and writer().is_closed() ) ) {
    len = min( view.size(), TCPConfig::MAX_PAYLOAD_SIZE );
    string payload( view.substr( 0, len ) );
    TCPSenderMessage message( seqno, seqno == isn_, move( payload ), false, writer().has_error() );
    if ( !FIN and writer().is_closed() and len == view.size()
         and ( pending_seqno_ + message.sequence_length() < window_size_
               or ( window_size_ == 0 and message.sequence_length() == 0 ) ) ) {
      message.FIN = FIN = true;
    }

    transmit( message );

    pending_seqno_ += message.sequence_length();
    abs_seqno_ += message.sequence_length();
    msg_queue_.emplace( move( message ) );

    // fin exceeds?
    if ( !FIN and writer().is_closed() and len == view.size() ) {
      break;
    }

    seqno = Wrap32::wrap( abs_seqno_, isn_ );
    view.remove_prefix( len );
  }
}

TCPSenderMessage TCPSender::make_empty_message() const
{
  // Your code here.
  return TCPSenderMessage( Wrap32::wrap( abs_seqno_, isn_ ), false, "", false, writer().has_error() );
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
  // Your code here.
  if ( msg.RST ) {
    writer().set_error();
    return;
  }

  window_size_ = msg.window_size;
  uint64_t abs_seqno = msg.ackno ? msg.ackno.value().unwrap( isn_, abs_prev_seqno_ ) : 0;
  if ( abs_seqno > abs_prev_seqno_ and abs_seqno <= abs_seqno_ ) {
    abs_prev_seqno_ = abs_seqno;
    timer_ms_ = 0;
    RTO_ms_ = initial_RTO_ms_;
    retry_trans_ = 0;
    uint64_t seqno { 0 };
    while ( !msg_queue_.empty() and seqno <= abs_seqno ) {
      seqno = msg_queue_.front().seqno.unwrap( isn_, abs_prev_seqno_ ) + msg_queue_.front().sequence_length();
      if ( seqno <= abs_seqno ) {
        pending_seqno_ -= msg_queue_.front().sequence_length();
        msg_queue_.pop();
      }
    }
  }
}

void TCPSender::tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit )
{
  // Your code here.
  if ( !msg_queue_.empty() ) {
    timer_ms_ += ms_since_last_tick;
  }
  if ( timer_ms_ >= RTO_ms_ ) {
    transmit( msg_queue_.front() );
    if ( window_size_ > 0 ) {
      retry_trans_++;
      RTO_ms_ <<= 1;
    }
    timer_ms_ = 0;
  }
}
