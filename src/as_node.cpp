#include "../include/as_node.h"
#include <algorithm>

void AS::seed(const Announcement& ann) {
    // Origin announcements go straight into local_rib — they always win
    policy->local_rib[ann.prefix] = ann;
}

void AS::send_to(AS* neighbor, const Announcement& ann, Relationship rel) {
    // Build the announcement as it will appear when received by neighbor
    Announcement outgoing = ann;
    outgoing.recv_relationship = rel;
    outgoing.next_hop_asn = this->asn;
    // NOTE: neighbor will prepend its own ASN when it processes (in policy)
    neighbor->policy->recv_queue[ann.prefix].push_back(std::move(outgoing));
}

void AS::propagate_to_providers() {
    for (auto& [prefix, ann] : policy->local_rib) {
        // only send to providers if we learned from a customer or are the origin
        // (Valley-free: don't re-advertise provider/peer routes upward)
        if (ann.recv_relationship == Relationship::PROVIDER ||
            ann.recv_relationship == Relationship::PEER) {
            continue;
        }
        for (AS* provider : providers) {
            send_to(provider, ann, Relationship::CUSTOMER);
        }
    }
}

void AS::propagate_to_customers() {
    for (auto& [prefix, ann] : policy->local_rib) {
        for (AS* customer : customers) {
            send_to(customer, ann, Relationship::PROVIDER);
        }
    }
}

void AS::propagate_to_peers() {
    for (auto& [prefix, ann] : policy->local_rib) {
        // Valley-free: only send customer/origin routes to peers
        if (ann.recv_relationship == Relationship::PROVIDER ||
            ann.recv_relationship == Relationship::PEER) {
            continue;
        }
        for (AS* peer : peers) {
            send_to(peer, ann, Relationship::PEER);
        }
    }
}
