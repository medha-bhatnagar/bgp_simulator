#include "../include/policy.h"
#include <algorithm>

// Compare two announcements — returns true if challenger is BETTER than incumbent
// Priority: relationship > path length > next_hop_asn > full AS-path
bool Policy::is_better(const Announcement& challenger, const Announcement& incumbent) {

    int c_rel = static_cast<int>(challenger.recv_relationship);
    int i_rel = static_cast<int>(incumbent.recv_relationship);

    // 1. Relationship preference (CUSTOMER > PEER > PROVIDER )
    if (c_rel != i_rel) {
        return c_rel < i_rel;
    }

    // 2. Shorter AS-path wins
    if (challenger.as_path.size() != incumbent.as_path.size()) {
        return challenger.as_path.size() < incumbent.as_path.size();
    }

    // 3. Lower next-hop ASN wins
    if (challenger.next_hop_asn != incumbent.next_hop_asn) {
        return challenger.next_hop_asn < incumbent.next_hop_asn;
    }

    // Ensures deterministic selection identical to reference implementation
    return challenger.as_path < incumbent.as_path;
}

void Policy::process_incoming(uint32_t my_asn) {

    for (auto& [prefix, candidates] : recv_queue) {

        // sort candidates for deterministic behavior
        std::sort(candidates.begin(), candidates.end(),
            [](const Announcement& a, const Announcement& b) {
                if (a.recv_relationship != b.recv_relationship)
                    return a.recv_relationship < b.recv_relationship;

                if (a.as_path.size() != b.as_path.size())
                    return a.as_path.size() < b.as_path.size();

                if (a.next_hop_asn != b.next_hop_asn)
                    return a.next_hop_asn < b.next_hop_asn;

                return a.as_path < b.as_path;
            }
        );

        for (auto& ann : candidates) {

            if (!accept(ann)) continue;

            Announcement updated = ann;

            // prepend ASN
            updated.as_path.insert(updated.as_path.begin(), my_asn);

            auto it = local_rib.find(prefix);

            if (it == local_rib.end()) {
                local_rib[prefix] = updated;
            }
            else if (is_better(updated, it->second)) {
                it->second = updated;
            }
        }
    }

    recv_queue.clear();
}