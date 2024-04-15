#include "router.hh"
#include "ipv4_datagram.hh"

#include <iostream>
#include <ranges>
#include <utility>

using namespace std;

// route_prefix: The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
// prefix_length: For this route to be applicable, how many high-order (most-significant) bits of
//    the route_prefix will need to match the corresponding bits of the datagram's destination address?
// next_hop: The IP address of the next hop. Will be empty if the network is directly attached to the router (in
//    which case, the next hop address should be the datagram's final destination).
// interface_num: The index of the interface to send the datagram out on.
void Router::add_route( const uint32_t route_prefix,
                        const uint8_t prefix_length,
                        const optional<Address> next_hop,
                        const size_t interface_num )
{
  cerr << "DEBUG: adding route " << Address::from_ipv4_numeric( route_prefix ).ip() << "/"
       << static_cast<int>( prefix_length ) << " => " << ( next_hop.has_value() ? next_hop->ip() : "(direct)" )
       << " on interface " << interface_num << "\n";

  routing_table_[prefix_length][route_prefix >> ( 32 - prefix_length )] = { interface_num, next_hop };
  // Your code here.
}

// Go through all the interfaces, and route every incoming datagram to its proper outgoing interface.
void Router::route()
{
  // Your code here.
  for ( auto interface : _interfaces ) {
    auto&& dgrams { interface->datagrams_received() };
    while ( !dgrams.empty() ) {
      InternetDatagram dgram { move( dgrams.front() ) };
      dgrams.pop();
      if ( dgram.header.ttl <= 1 ) {
        continue;
      }
      --dgram.header.ttl;
      dgram.header.ver = 4;
      dgram.header.compute_checksum();
      const optional<T>& res { match( dgram.header.dst ) };
      if ( not res.has_value() ) {
        continue;
      }
      const auto& [num, next_hop] { res.value() };
      _interfaces[num]->send_datagram( dgram, next_hop.value_or( Address::from_ipv4_numeric( dgram.header.dst ) ) );
    }
  }
}

[[nodiscard]] auto Router::match( uint32_t ip ) const noexcept -> std::optional<T>
{
  auto get_entry = views::filter( [&ip]( const auto& table ) { return table.contains( ip >>= 1 ); } )
                   | views::transform( [&ip]( const auto& subtable ) -> T { return subtable.at( ip ); } );
  auto matched = routing_table_ | views::reverse | get_entry | views::take( 1 );
  return matched.empty() ? nullopt : optional<T> { matched.front() };
}
