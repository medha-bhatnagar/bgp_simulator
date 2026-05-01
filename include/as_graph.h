#pragma once
#include "as_node.h"
#include <unordered_map>
#include <vector>
#include <string>
#include <memory>

class ASGraph {
public:
    // asn -> AS object
    std::unordered_map<uint32_t, std::unique_ptr<AS>> ases;

    // Propagation ranks: ranks[i] = list of AS* at rank i
    std::vector<std::vector<AS*>> ranks;

    // Load graph from CAIDA file. Returns false if a provider/customer cycle is found.
    bool load_caida(const std::string& filepath);

    // Mark a set of ASNs as ROV-enabled
    void apply_rov(const std::string& filepath);

    // Seed announcements from anns.csv into their origin ASes
    void seed_announcements(const std::string& filepath);

    // Run the full BGP propagation (up -> across -> down)
    void propagate();

    // Write output CSV
    void write_output(const std::string& filepath) const;

private:
    // Build the propagation rank vector (topological sort by provider depth)
    // Returns false if a provider/customer cycle is detected
    bool build_ranks();

    // Get or create an AS node
    AS* get_or_create(uint32_t asn);
};
