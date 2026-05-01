#pragma once
#include <cstdint>
#include <string>
#include <vector>

// Relationship types from CAIDA file and BGP logic
enum class Relationship : int8_t {
    ORIGIN   = 0,   // originated here (highest priority)
    CUSTOMER = 1,   // received from a customer
    PEER     = 2,   // received from a peer
    PROVIDER = 3,   // received from a provider (lowest priority)
    UNKNOWN  = 4
};

// A BGP announcement traveling through the graph
struct Announcement {
    std::string prefix;
    std::vector<uint32_t> as_path;   // as_path[0] = origin AS, back = most recent
    uint32_t next_hop_asn;
    Relationship recv_relationship;
    bool rov_invalid;

    // Default constructor
    Announcement() : next_hop_asn(0), recv_relationship(Relationship::UNKNOWN), rov_invalid(false) {}

    Announcement(const std::string& pfx, uint32_t seed_asn, bool invalid)
        : prefix(pfx), as_path({seed_asn}), next_hop_asn(seed_asn),
          recv_relationship(Relationship::ORIGIN), rov_invalid(invalid) {}
};
