#pragma once
#include "types.h"
#include "policy.h"
#include <vector>
#include <memory>
#include <cstdint>

struct AS {
    uint32_t asn;
    int propagation_rank = -1;

    // Neighbor lists store raw pointers for speed (graph owns all AS objects)
    std::vector<AS*> providers;
    std::vector<AS*> customers;
    std::vector<AS*> peers;

    // Policy object (BGP or ROV)
    std::unique_ptr<Policy> policy;

    explicit AS(uint32_t asn_) : asn(asn_), policy(std::make_unique<BGP>()) {}

    // Seed an announcement directly into local RIB (origin)
    void seed(const Announcement& ann);

    // Send announcement to a neighbor's recv_queue
    void send_to(AS* neighbor, const Announcement& ann, Relationship rel);

    // Push all local_rib entries to providers
    void propagate_to_providers();

    // Push all local_rib entries to customers
    void propagate_to_customers();

    // Push all local_rib entries to peers
    void propagate_to_peers();

    // Process recv_queue into local_rib
    void process_incoming() { policy->process_incoming(asn); }

    // Convenience accessors
    const std::unordered_map<std::string, Announcement>& local_rib() const {
        return policy->local_rib;
    }
};
