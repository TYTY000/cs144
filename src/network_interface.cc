#include <cstddef>
#include <iostream>
#include <utility>

#include "arp_message.hh"
#include "ethernet_header.hh"
#include "exception.hh"
#include "network_interface.hh"
#include "parser.hh"

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface( string_view name,
                                    shared_ptr<OutputPort> port,
                                    const EthernetAddress& ethernet_address,
                                    const Address& ip_address )
  : name_( name )
  , port_( notnull( "OutputPort", move( port ) ) )
  , ethernet_address_( ethernet_address )
  , ip_address_( ip_address )
{
  cerr << "DEBUG: Network interface has Ethernet address " << to_string( ethernet_address ) << " and IP address "
       << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but
//! may also be another host if directly connected to the same network as the destination) Note: the Address type
//! can be converted to a uint32_t (raw 32-bit IP address) by using the Address::ipv4_numeric() method.
void NetworkInterface::send_datagram( const InternetDatagram& dgram, const Address& next_hop )
{
  // Your code here.
  EthernetFrame eframe;
  eframe.header.src = ethernet_address_;
  if ( !map_addr.contains( next_hop.ipv4_numeric() ) ) {
    if ( map_arp.contains( next_hop.ipv4_numeric() ) ) {
      return;
    }
    eframe.header.dst = ETHERNET_BROADCAST;
    eframe.header.type = EthernetHeader::TYPE_ARP;
    ARPMessage arpMsg;
    arpMsg.opcode = ARPMessage::OPCODE_REQUEST;
    arpMsg.sender_ethernet_address = ethernet_address_;
    arpMsg.sender_ip_address = ip_address_.ipv4_numeric();
    arpMsg.target_ip_address = next_hop.ipv4_numeric();
    eframe.payload = serialize( arpMsg );
    map_arp.emplace( arpMsg.target_ip_address, pair<size_t, EthernetFrame>( time, eframe ) );
    datagrams_received_.emplace( dgram );
    transmit( eframe );
    return;
  }
  eframe.header.dst = map_addr[next_hop.ipv4_numeric()].second;
  eframe.header.type = EthernetHeader::TYPE_IPv4;
  eframe.payload = serialize( dgram );
  transmit( eframe );
}

//! \param[in] frame the incoming Ethernet frame
void NetworkInterface::recv_frame( const EthernetFrame& frame )
{
  // Your code here.
  ARPMessage arpMsg;
  if ( parse( arpMsg, frame.payload ) ) { // ARPMessage
    if ( arpMsg.target_ip_address != ip_address_.ipv4_numeric() ) {
      return;
    }

    EthernetFrame eframe;
    if ( arpMsg.opcode == ARPMessage::OPCODE_REQUEST ) {
      map_addr[arpMsg.sender_ip_address] = pair<size_t, EthernetAddress>( time, arpMsg.sender_ethernet_address );

      arpMsg.opcode = ARPMessage::OPCODE_REPLY;
      arpMsg.target_ethernet_address = ethernet_address_;
      swap( arpMsg.sender_ethernet_address, arpMsg.target_ethernet_address );
      swap( arpMsg.sender_ip_address, arpMsg.target_ip_address );

      eframe.header.type = EthernetHeader::TYPE_ARP;
      eframe.header.src = ethernet_address_;
      eframe.header.dst = arpMsg.target_ethernet_address;
      eframe.payload = serialize( arpMsg );
      transmit( eframe );
    } else if ( arpMsg.opcode == ARPMessage::OPCODE_REPLY ) {
      eframe.header.type = EthernetHeader::TYPE_IPv4;
      eframe.header.dst = arpMsg.sender_ethernet_address;
      eframe.header.src = ethernet_address_;
      eframe.payload = serialize( datagrams_received_.front() );
      datagrams_received_.pop();
      if ( map_arp.contains( arpMsg.sender_ip_address ) ) {
        map_arp.erase( arpMsg.sender_ip_address );
      }
      map_addr.emplace( arpMsg.sender_ip_address, pair( time, arpMsg.sender_ethernet_address ) );
      transmit( eframe );
      return;
    }
  }
  // IPv4
  if ( frame.header.dst != ethernet_address_ ) {
    return;
  }
  InternetDatagram dgram;
  if ( parse( dgram, frame.payload ) ) {
    datagrams_received_.emplace( dgram );
  }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick( const size_t ms_since_last_tick )
{
  // Your code here.
  time += ms_since_last_tick;
  for ( auto it = map_addr.begin(); it != map_addr.end(); ) {
    // IP-to-Ethernet mappings hold 30s
    if ( time - it->second.first >= 30000 ) {
      it = map_addr.erase( it );
    } else {
      ++it;
    }
  }

  for ( auto& it : map_arp ) {
    if ( time - it.second.first >= 5000 ) {
      it.second.first = time;
      transmit( it.second.second );
    }
  }
}
