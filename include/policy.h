#pragma once
#include "types.h"
#include <unordered_map>
#include <vector>
#include <string>

// Abstract base policy — every AS holds one of these
class Policy {
public:
    // local RIB: prefix -> best announcement
    std::unordered_map<std::string, Announcement> local_rib;

    // received queue: prefix -> list of candidates received this round
    std::unordered_map<std::string, std::vector<Announcement>> recv_queue;

    virtual ~Policy() = default;

    // Process all announcements in recv_queue into local_rib, then clear queue
    virtual void process_incoming(uint32_t my_asn);

    // ROV / filtering hook
    virtual bool accept(const Announcement& ann) const { return true; }

public:
    // Compare two announcements — return true if challenger is better
    static bool is_better(const Announcement& challenger, const Announcement& incumbent);
};

// Standard BGP policy
class BGP : public Policy {
public:
    bool accept(const Announcement& ann) const override {
        return true;
    }
};

// ROV-enabled policy — drops invalid announcements
class ROV : public BGP {
public:
    bool accept(const Announcement& ann) const override {
        // Only filter based on announcement flag
        return !ann.rov_invalid;
    }
};