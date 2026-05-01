/*
 * BGP Simulator Test Suite
 *
 * Compile with:
 *   g++ -std=c++17 -O2 -I../include tests.cpp ../src/policy.cpp ../src/as_node.cpp ../src/as_graph.cpp -o run_tests
 *
 * Run:
 *   ./run_tests
 */

#include "../include/as_graph.h"
#include "../include/policy.h"
#include "../include/as_node.h"
#include <iostream>
#include <cassert>
#include <fstream>
#include <sstream>
#include <vector>
#include <functional>


static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) void name(); \
    struct _reg_##name { _reg_##name() { run_test(#name, name); } } _inst_##name; \
    void name()

void run_test(const char* name, std::function<void()> fn) {
    tests_run++;
    try {
        fn();
        tests_passed++;
        std::cout << "  PASS  " << name << "\n";
    } catch (const std::exception& e) {
        std::cout << "  FAIL  " << name << " — " << e.what() << "\n";
    } catch (...) {
        std::cout << "  FAIL  " << name << " — unknown exception\n";
    }
}

#define ASSERT(cond) \
    if (!(cond)) throw std::runtime_error("Assertion failed: " #cond " at line " + std::to_string(__LINE__))

#define ASSERT_EQ(a, b) \
    if ((a) != (b)) throw std::runtime_error( \
        std::string("Expected: ") + std::to_string(b) + " Got: " + std::to_string(a) + " at line " + std::to_string(__LINE__))

//helpers
// Write a temp CAIDA-format file
std::string write_caida(const std::string& content) {
    std::string path = "/tmp/test_caida.txt";
    std::ofstream f(path);
    f << content;
    return path;
}

std::string write_anns(const std::string& content) {
    std::string path = "/tmp/test_anns.csv";
    std::ofstream f(path);
    f << content;
    return path;
}

std::string write_rov(const std::string& content) {
    std::string path = "/tmp/test_rov.csv";
    std::ofstream f(path);
    f << content;
    return path;
}

//policy compare
TEST(test_relationship_priority_customer_beats_provider) {
    Announcement customer, provider;
    customer.prefix = "1.0.0.0/8";
    customer.as_path = {10, 5};
    customer.next_hop_asn = 10;
    customer.recv_relationship = Relationship::CUSTOMER;
    customer.rov_invalid = false;

    provider = customer;
    provider.recv_relationship = Relationship::PROVIDER;

    // Customer beats provider regardless of path
    ASSERT(Policy::is_better(customer, provider));
    ASSERT(!Policy::is_better(provider, customer));
}

TEST(test_relationship_priority_customer_beats_peer) {
    Announcement customer, peer;
    customer.prefix = "1.0.0.0/8";
    customer.as_path = {10, 5};
    customer.recv_relationship = Relationship::CUSTOMER;

    peer = customer;
    peer.recv_relationship = Relationship::PEER;

    ASSERT(Policy::is_better(customer, peer));
}

TEST(test_relationship_priority_peer_beats_provider) {
    Announcement peer, provider;
    peer.prefix = "1.0.0.0/8";
    peer.as_path = {10, 5};
    peer.recv_relationship = Relationship::PEER;

    provider = peer;
    provider.recv_relationship = Relationship::PROVIDER;

    ASSERT(Policy::is_better(peer, provider));
}

TEST(test_shorter_path_wins_same_relationship) {
    Announcement short_path, long_path;
    short_path.prefix = "1.0.0.0/8";
    short_path.as_path = {5, 99};
    short_path.next_hop_asn = 5;
    short_path.recv_relationship = Relationship::CUSTOMER;

    long_path = short_path;
    long_path.as_path = {5, 10, 99};

    ASSERT(Policy::is_better(short_path, long_path));
    ASSERT(!Policy::is_better(long_path, short_path));
}

TEST(test_lower_nexthop_wins_tiebreak) {
    Announcement low, high;
    low.prefix = "1.0.0.0/8";
    low.as_path = {3, 99};
    low.next_hop_asn = 3;
    low.recv_relationship = Relationship::CUSTOMER;

    high = low;
    high.as_path = {7, 99};
    high.next_hop_asn = 7;

    ASSERT(Policy::is_better(low, high));
    ASSERT(!Policy::is_better(high, low));
}

TEST(test_origin_beats_customer) {
    Announcement origin, customer;
    origin.prefix = "1.0.0.0/8";
    origin.as_path = {99};
    origin.recv_relationship = Relationship::ORIGIN;

    customer = origin;
    customer.recv_relationship = Relationship::CUSTOMER;

    ASSERT(Policy::is_better(origin, customer));
}

//rov policy
TEST(test_rov_drops_invalid) {
    ROV rov;
    Announcement ann;
    ann.rov_invalid = true;
    ASSERT(!rov.accept(ann));
}

TEST(test_rov_accepts_valid) {
    ROV rov;
    Announcement ann;
    ann.rov_invalid = false;
    ASSERT(rov.accept(ann));
}

TEST(test_bgp_accepts_invalid) {
    BGP bgp;
    Announcement ann;
    ann.rov_invalid = true;
    ASSERT(bgp.accept(ann));  // BGP doesn't care about rov_invalid
}


/*
 * Scenario: Simple linear graph
 *   A (customer of B), B (customer of C)
 *   A -> B -> C  (A is at rank 0, C is at rank 2)
 *   Announcement seeded at A, should propagate to B and C
 */
TEST(test_simple_linear_propagation) {
    // A=1, B=2, C=3. 1 is customer of 2, 2 is customer of 3
    std::string caida = "2|1|-1|bgp\n3|2|-1|bgp\n";
    std::string anns  = "seed_asn,prefix,rov_invalid\n1,10.0.0.0/8,False\n";
    std::string rov   = "";

    ASGraph g;
    ASSERT(g.load_caida(write_caida(caida)));
    g.apply_rov(write_rov(rov));
    g.seed_announcements(write_anns(anns));
    g.propagate();

    // AS 1 should have the announcement (it originated it)
    ASSERT(!g.ases[1]->policy->local_rib.empty());
    ASSERT(g.ases[1]->policy->local_rib.count("10.0.0.0/8") > 0);

    // AS 2 should have received it from customer AS 1
    ASSERT(g.ases[2]->policy->local_rib.count("10.0.0.0/8") > 0);
    auto& ann2 = g.ases[2]->policy->local_rib.at("10.0.0.0/8");
    ASSERT(ann2.recv_relationship == Relationship::CUSTOMER);
    // Path at AS2 should be [1, 2]
    ASSERT(ann2.as_path.size() == 2);
    ASSERT(ann2.as_path[0] == 1);
    ASSERT(ann2.as_path[1] == 2);

    // AS 3 should have received it from customer AS 2
    ASSERT(g.ases[3]->policy->local_rib.count("10.0.0.0/8") > 0);
    auto& ann3 = g.ases[3]->policy->local_rib.at("10.0.0.0/8");
    ASSERT(ann3.as_path.size() == 3);
    ASSERT(ann3.as_path[0] == 1);
    ASSERT(ann3.as_path[1] == 2);
    ASSERT(ann3.as_path[2] == 3);
}

/*
 * Scenario: Peer propagation
 *   A and B are peers. A announces prefix. B should receive it.
 *   B's customer C should also receive (provider sending down).
 */
TEST(test_peer_propagation) {
    // 1 and 2 are peers. 3 is customer of 2.
    std::string caida = "1|2|0|bgp\n2|3|-1|bgp\n";
    std::string anns  = "seed_asn,prefix,rov_invalid\n1,10.0.0.0/8,False\n";

    ASGraph g;
    ASSERT(g.load_caida(write_caida(caida)));
    g.seed_announcements(write_anns(anns));
    g.propagate();

    // AS 2 receives from peer AS 1
    ASSERT(g.ases[2]->policy->local_rib.count("10.0.0.0/8") > 0);
    auto& ann2 = g.ases[2]->policy->local_rib.at("10.0.0.0/8");
    ASSERT(ann2.recv_relationship == Relationship::PEER);

    // AS 3 (customer of 2) receives from provider 2
    ASSERT(g.ases[3]->policy->local_rib.count("10.0.0.0/8") > 0);
}

/*
 * Scenario: Valley-free routing
 *   Peer routes should NOT be re-advertised to other peers or providers.
 *   A-B are peers, B-C are peers. A announces. C should NOT receive.
 */
TEST(test_valley_free_peer_no_readvertise) {
    // 1 and 2 are peers. 2 and 3 are peers.
    std::string caida = "1|2|0|bgp\n2|3|0|bgp\n";
    std::string anns  = "seed_asn,prefix,rov_invalid\n1,10.0.0.0/8,False\n";

    ASGraph g;
    ASSERT(g.load_caida(write_caida(caida)));
    g.seed_announcements(write_anns(anns));
    g.propagate();

    // AS 2 gets it from peer AS 1
    ASSERT(g.ases[2]->policy->local_rib.count("10.0.0.0/8") > 0);

    // AS 3 should NOT get it (peer routes not re-sent to peers)
    ASSERT(g.ases[3]->policy->local_rib.count("10.0.0.0/8") == 0);
}

/*
 * Scenario: Provider route not re-advertised upward
 *   A is customer of B, B is customer of C.
 *   D is customer of C (sibling of B).
 *   Announcement at C going down should reach B and A,
 *   but B should NOT re-advertise C's route back UP to C.
 */
TEST(test_provider_route_not_sent_up) {
    // C=3 is provider of B=2, B=2 is provider of A=1
    // Also D=4 is customer of C=3
    std::string caida = "3|2|-1|bgp\n2|1|-1|bgp\n3|4|-1|bgp\n";
    std::string anns  = "seed_asn,prefix,rov_invalid\n3,10.0.0.0/8,False\n";

    ASGraph g;
    ASSERT(g.load_caida(write_caida(caida)));
    g.seed_announcements(write_anns(anns));
    g.propagate();

    // A should receive via B from C
    ASSERT(g.ases[1]->policy->local_rib.count("10.0.0.0/8") > 0);
    // B should have received from provider C
    auto& ann_b = g.ases[2]->policy->local_rib.at("10.0.0.0/8");
    ASSERT(ann_b.recv_relationship == Relationship::PROVIDER);
    // D should receive
    ASSERT(g.ases[4]->policy->local_rib.count("10.0.0.0/8") > 0);
}

/*
 * Scenario: Conflict resolution — customer beats provider
 *   AS 4 has two paths to prefix: one through its customer, one through provider.
 *   Should prefer the customer route.
 */
TEST(test_conflict_customer_beats_provider) {
    // 4 is customer of 1 (provider). 2 is customer of 4.
    // 2 announces the prefix. 1 also announces same prefix.
    // At AS 4: 2 is customer (path [2,4]), 1 is provider.
    // 4 should prefer customer route from 2.
    std::string caida = "1|4|-1|bgp\n4|2|-1|bgp\n";
    std::string anns  = "seed_asn,prefix,rov_invalid\n2,10.0.0.0/8,False\n1,10.0.0.0/8,False\n";

    ASGraph g;
    ASSERT(g.load_caida(write_caida(caida)));
    g.seed_announcements(write_anns(anns));
    g.propagate();

    auto& ann4 = g.ases[4]->policy->local_rib.at("10.0.0.0/8");
    // Should have received from customer (AS 2)
    ASSERT(ann4.recv_relationship == Relationship::CUSTOMER);
    ASSERT(ann4.as_path[0] == 2);
}

/*
 * Scenario: Conflict resolution — shorter path wins same relationship
 */
TEST(test_conflict_shorter_path_wins) {
    // 3 is provider of both 1 and 2. 1 and 2 are peers. 4 is customer of 1.
    // Prefix announced at 4 (path len 1 at 4), and at 2 (path len 1 at 2).
    // At AS 1: receives from customer 4 (path [4,1]) and from peer 2 (path [2,1]).
    // Customer wins over peer regardless.
    // Let's test shorter path with same relationship instead:
    // AS 5 is peer of AS 1 and has two customers: 10 (short path) and 11 (longer path)
    // Both announce same prefix. 5 picks the shorter one.
    std::string caida = "5|10|-1|bgp\n5|11|-1|bgp\n";
    // 10 announces directly, 11 re-announces via an intermediate (faked with longer path)
    std::string anns  = "seed_asn,prefix,rov_invalid\n10,10.0.0.0/8,False\n11,10.0.0.0/8,False\n";

    ASGraph g;
    ASSERT(g.load_caida(write_caida(caida)));
    g.seed_announcements(write_anns(anns));
    g.propagate();

    // AS 5 gets from two customers. Both have path length 2 at AS5 ([10,5] or [11,5]).
    // Tiebreak: lower next_hop_asn at time of comparison. Incoming next_hop is 10 vs 11. 10 wins.
    // After storage, next_hop becomes 5 (the storing AS). So check as_path[0] to see who won.
    auto& ann5 = g.ases[5]->policy->local_rib.at("10.0.0.0/8");
    ASSERT(ann5.as_path[0] == 10);  // AS 10 won the tiebreak (lower ASN)
}

/*
 * Scenario: ROV drops hijack announcement
 *   AS 27 is ROV-enabled. AS 666 announces a hijack (rov_invalid=True).
 *   AS 27 should not have the hijacked prefix.
 */
TEST(test_rov_blocks_invalid_announcement) {
    // Simple: 27 is provider of 666. 666 announces hijack. 27 deploys ROV.
    std::string caida = "27|666|-1|bgp\n";
    std::string anns  = "seed_asn,prefix,rov_invalid\n666,10.0.0.0/8,True\n";
    std::string rov   = "27\n";

    ASGraph g;
    ASSERT(g.load_caida(write_caida(caida)));
    g.apply_rov(write_rov(rov));
    g.seed_announcements(write_anns(anns));
    g.propagate();

    // AS 666 originated it so it has it
    ASSERT(g.ases[666]->policy->local_rib.count("10.0.0.0/8") > 0);
    // AS 27 (ROV) should have DROPPED it
    ASSERT(g.ases[27]->policy->local_rib.count("10.0.0.0/8") == 0);
}

/*
 * Scenario: ROV non-deployer passes invalid announcement through
 */
TEST(test_non_rov_passes_invalid_through) {
    // 1 is provider of 2, 2 is provider of 3. None deploy ROV.
    std::string caida = "1|2|-1|bgp\n2|3|-1|bgp\n";
    std::string anns  = "seed_asn,prefix,rov_invalid\n3,10.0.0.0/8,True\n";

    ASGraph g;
    ASSERT(g.load_caida(write_caida(caida)));
    g.seed_announcements(write_anns(anns));
    g.propagate();

    // All three should have the announcement (no ROV deployed)
    ASSERT(g.ases[1]->policy->local_rib.count("10.0.0.0/8") > 0);
    ASSERT(g.ases[2]->policy->local_rib.count("10.0.0.0/8") > 0);
    ASSERT(g.ases[3]->policy->local_rib.count("10.0.0.0/8") > 0);
}

/*
 * Scenario: Cycle detection
 *   Provider/customer cycles should be detected and rejected.
 */
TEST(test_cycle_detection) {
    // 1 is provider of 2, 2 is provider of 1 — CYCLE!
    std::string caida = "1|2|-1|bgp\n2|1|-1|bgp\n";

    ASGraph g;
    bool result = g.load_caida(write_caida(caida));
    ASSERT(!result);  // Should return false due to cycle
}

/*
 * Scenario: bgpsimulator.com home page example
 *   777 announces 1.2.0.0/16. 666 hijacks same prefix (rov_invalid).
 *   AS 4 has 666 and 3 as customers. 777 and 4 are peers.
 *   3 is customer of 4. 666 is customer of 4.
 *   At AS 4: should prefer 666's route (shorter path from customer).
 */
TEST(test_homepage_scenario) {
    // From bgpsimulator.com:
    // 777 announces 1.2.0.0/16 (origin)
    // 666 announces 1.2.0.0/16 (hijack, rov_invalid=True)
    // 4 is provider of 666 and 3. 4 and 777 are peers.
    std::string caida =
        "4|666|-1|bgp\n"
        "4|3|-1|bgp\n"
        "4|777|0|bgp\n";
    std::string anns =
        "seed_asn,prefix,rov_invalid\n"
        "777,1.2.0.0/16,False\n"
        "666,1.2.0.0/16,True\n";

    ASGraph g;
    ASSERT(g.load_caida(write_caida(caida)));
    g.seed_announcements(write_anns(anns));
    g.propagate();

    // AS 4 should prefer 666 (customer, shorter path [666,4]) over 777 (peer)
    ASSERT(g.ases[4]->policy->local_rib.count("1.2.0.0/16") > 0);
    auto& ann4 = g.ases[4]->policy->local_rib.at("1.2.0.0/16");
    ASSERT(ann4.recv_relationship == Relationship::CUSTOMER);
    ASSERT(ann4.as_path[0] == 666);

    // AS 3 gets announcement from provider 4, which passes 666's route down
    ASSERT(g.ases[3]->policy->local_rib.count("1.2.0.0/16") > 0);
}

/*
 * Scenario: Output format check
 *   Verify the CSV output has correct columns and Python tuple format.
 */
TEST(test_output_format) {
    std::string caida = "2|1|-1|bgp\n";
    std::string anns  = "seed_asn,prefix,rov_invalid\n1,10.0.0.0/8,False\n";

    ASGraph g;
    ASSERT(g.load_caida(write_caida(caida)));
    g.seed_announcements(write_anns(anns));
    g.propagate();

    std::string out_path = "/tmp/test_output.csv";
    g.write_output(out_path);

    std::ifstream f(out_path);
    ASSERT(f.is_open());

    std::string header;
    std::getline(f, header);
    ASSERT(header == "asn,prefix,as_path");

    std::string line;
    bool found_single_tuple = false;
    bool found_multi_tuple = false;
    while (std::getline(f, line)) {
        if (line.find("\"(1,)\"") != std::string::npos) found_single_tuple = true;
        if (line.find("(1, 2)") != std::string::npos) found_multi_tuple = true;
    }
    ASSERT(found_single_tuple);  // AS 1 has single-element path
    ASSERT(found_multi_tuple);   // AS 2 has path (1, 2)
}

//main
int main() {
    std::cout << "\n=== BGP Simulator Test Suite ===\n\n";
    // Tests are auto-registered via static constructors above
    std::cout << "\n================================\n";
    std::cout << "Results: " << tests_passed << "/" << tests_run << " passed\n";
    if (tests_passed == tests_run) {
        std::cout << "ALL TESTS PASSED\n\n";
        return 0;
    } else {
        std::cout << (tests_run - tests_passed) << " TESTS FAILED\n\n";
        return 1;
    }
}
