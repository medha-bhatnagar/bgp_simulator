#include "../include/as_graph.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <stdexcept>
#include <queue>

AS* ASGraph::get_or_create(uint32_t asn) {
    auto it = ases.find(asn);
    if (it != ases.end()) return it->second.get();
    auto [ins, _] = ases.emplace(asn, std::make_unique<AS>(asn));
    return ins->second.get();
}

bool ASGraph::load_caida(const std::string& filepath) {
    std::ifstream f(filepath);
    if (!f.is_open()) {
        std::cerr << "ERROR: Cannot open CAIDA file: " << filepath << "\n";
        return false;
    }

    std::string line;
    while (std::getline(f, line)) {
        if (line.empty() || line[0] == '#') continue;

        // Format: asn1|asn2|relationship|source
        // relationship: -1 = asn1 is provider of asn2, 0 = peers
        uint32_t asn1, asn2;
        int rel;
        std::istringstream ss(line);
        std::string tok;

        // Parse manually to avoid locale issues
        if (!std::getline(ss, tok, '|')) continue;
        asn1 = static_cast<uint32_t>(std::stoul(tok));
        if (!std::getline(ss, tok, '|')) continue;
        asn2 = static_cast<uint32_t>(std::stoul(tok));
        if (!std::getline(ss, tok, '|')) continue;
        rel = std::stoi(tok);

        AS* a1 = get_or_create(asn1);
        AS* a2 = get_or_create(asn2);

        if (rel == -1) {
            // asn1 is provider of asn2
            a2->providers.push_back(a1);
            a1->customers.push_back(a2);
        } else if (rel == 0) {
            // peers
            a1->peers.push_back(a2);
            a2->peers.push_back(a1);
        }
    }

    return build_ranks();
}

bool ASGraph::build_ranks() {
    // Kahn's algorithm traversing UPWARD (customer -> provider direction)
    //
    // Assign ranks bottom-up:
    //   rank 0 = stub ASes with NO customers (leaf nodes, like home ISPs)
    //   rank N = ASes whose all customers have already been assigned ranks
    //
    // In-degree for this BFS = number of CUSTOMERS an AS has
    // When all customers of an AS are processed, the AS is ready to process
    // Notify the AS's PROVIDERS that one of their customers is done

    // customer_count[asn] = how many customers this AS has (its in-degree)
    std::unordered_map<uint32_t, int> customer_count;
    for (auto& [asn, as_ptr] : ases) {
        customer_count[asn] = static_cast<int>(as_ptr->customers.size());
    }

    // Start with ASes that have no customers (stubs / rank 0)
    std::queue<AS*> q;
    for (auto& [asn, as_ptr] : ases) {
        if (as_ptr->customers.empty()) {
            as_ptr->propagation_rank = 0;
            q.push(as_ptr.get());
        }
    }

    int max_rank = 0;
    int processed = 0;

    while (!q.empty()) {
        AS* cur = q.front(); q.pop();
        processed++;

        // cur's providers get a rank at least cur->rank + 1
        int next_rank = cur->propagation_rank + 1;

        for (AS* provider : cur->providers) {
            // Provider's rank = max(current rank, next_rank)
            if (next_rank > provider->propagation_rank) {
                provider->propagation_rank = next_rank;
                if (next_rank > max_rank) max_rank = next_rank;
            }
            // Decrement customer_count — one more of provider's customers is done
            customer_count[provider->asn]--;
            if (customer_count[provider->asn] == 0) {
                q.push(provider);
            }
        }
    }

    // If not all ASes were processed, there is a provider/customer cycle
    if (processed != static_cast<int>(ases.size())) {
        std::cerr << "ERROR: Provider/customer cycle detected in AS graph!\n";
        return false;
    }

    // Build rank vectors
    ranks.resize(max_rank + 1);
    for (auto& [asn, as_ptr] : ases) {
        int r = as_ptr->propagation_rank;
        ranks[r].push_back(as_ptr.get());
    }

    return true;
}

void ASGraph::apply_rov(const std::string& filepath) {
    std::ifstream f(filepath);
    if (!f.is_open()) {
        std::cerr << "WARNING: Cannot open ROV file: " << filepath << "\n";
        return;
    }
    std::string line;
    while (std::getline(f, line)) {
        if (line.empty()) continue;
        // Strip \r
        if (!line.empty() && line.back() == '\r') line.pop_back();
        uint32_t asn = static_cast<uint32_t>(std::stoul(line));
        auto it = ases.find(asn);
        if (it != ases.end()) {
            it->second->policy = std::make_unique<ROV>();
        }
    }
}

void ASGraph::seed_announcements(const std::string& filepath) {
    std::ifstream f(filepath);
    if (!f.is_open()) {
        std::cerr << "ERROR: Cannot open announcements file: " << filepath << "\n";
        return;
    }

    std::string line;
    bool header = true;
    while (std::getline(f, line)) {
        if (line.empty()) continue;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (header) { header = false; continue; }

        // Format: seed_asn,prefix,rov_invalid
        std::istringstream ss(line);
        std::string tok;
        std::getline(ss, tok, ',');
        uint32_t seed_asn = static_cast<uint32_t>(std::stoul(tok));
        std::string prefix;
        std::getline(ss, prefix, ',');
        std::string rov_str;
        std::getline(ss, rov_str, ',');
        bool rov_invalid = (rov_str == "True" || rov_str == "true" || rov_str == "1");

        AS* as_node = get_or_create(seed_asn);
        Announcement ann(prefix, seed_asn, rov_invalid);
        as_node->seed(ann);
    }
}

void ASGraph::propagate() {
    // Phase 1: propagate UP (customer -> provider), from rank 0 upward
    // rank 0 = stubs (no customers), highest rank = tier-1s (no providers)
    for (int r = 0; r < static_cast<int>(ranks.size()); r++) {
        // First: all ASes at this rank send to providers
        for (AS* as : ranks[r]) {
            as->propagate_to_providers();
        }
        // Then: all ASes at rank r+1 process their queues
        if (r + 1 < static_cast<int>(ranks.size())) {
            for (AS* as : ranks[r + 1]) {
                as->process_incoming();
            }
        }
    }

    // Phase 2: propagate ACROSS (peer -> peer), exactly one hop
    // All ASes send to peers first, then all process
    for (auto& [asn, as_ptr] : ases) {
        as_ptr->propagate_to_peers();
    }
    for (auto& [asn, as_ptr] : ases) {
        as_ptr->process_incoming();
    }

    // Phase 3: propagate DOWN (provider -> customer), from highest rank downward
    for (int r = static_cast<int>(ranks.size()) - 1; r >= 0; r--) {
        // Send to customers
        for (AS* as : ranks[r]) {
            as->propagate_to_customers();
        }
        // Customers at rank r-1 process
        if (r - 1 >= 0) {
            for (AS* as : ranks[r - 1]) {
                as->process_incoming();
            }
        }
    }
}

void ASGraph::write_output(const std::string& filepath) const {
    std::ofstream out(filepath);
    if (!out.is_open()) {
        std::cerr << "ERROR: Cannot open output file: " << filepath << "\n";
        return;
    }

    out << "asn,prefix,as_path\n";

    // Collect and sort by ASN for deterministic output
    std::vector<const AS*> sorted_ases;
    sorted_ases.reserve(ases.size());
    for (auto& [asn, as_ptr] : ases) {
        sorted_ases.push_back(as_ptr.get());
    }
    std::sort(sorted_ases.begin(), sorted_ases.end(),
              [](const AS* a, const AS* b) { return a->asn < b->asn; });

    for (const AS* as : sorted_ases) {
        const auto& rib = as->policy->local_rib;
        if (rib.empty()) continue;

        // Sort prefixes for deterministic output
        std::vector<const std::string*> prefixes;
        prefixes.reserve(rib.size());
        for (auto& [pfx, ann] : rib) prefixes.push_back(&pfx);
        std::sort(prefixes.begin(), prefixes.end(),
                  [](const std::string* a, const std::string* b) { return *a < *b; });

        for (const std::string* pfx : prefixes) {
            const Announcement& ann = rib.at(*pfx);

            // Format: asn,prefix,"(a, b, c)"
            // Single-element paths use Python tuple notation: "(a,)"
            out << as->asn << "," << *pfx << ",\"(";
            for (size_t i = 0; i < ann.as_path.size(); i++) {
                if (i > 0) out << ", ";
                out << ann.as_path[i];
            }
            if (ann.as_path.size() == 1) {
                out << ",";  // Python single-element tuple trailing comma
            }
            out << ")\"\n";
        }
    }
}
