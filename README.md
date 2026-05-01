# bgp_simulator
BGP Simulator – CSE3150 Course Project
Overview
This project is a C++ implementation of a BGP (Border Gateway Protocol) simulator. It models how route announcements propagate through the Internet using a CAIDA-derived Autonomous System (AS) graph.
The simulator:
Builds an AS-level graph (providers, customers, peers)
Applies Route Origin Validation (ROV)
Simulates valley-free BGP propagation
Selects the best routes using standard BGP decision rules
Outputs each AS’s final routing table (RIB) to a CSV file
__________
Files
bgp_simulator/

—-----src/              # Core implementation
—---------main.cpp
—---------as_graph.cpp
—---------as_node.cpp
—---------policy.cpp

—----- include/          # Header files
—---------as_graph.h
—---------as_node.h
—--------- policy.h

—-----tests/
—--------- tests.cpp     # Unit/system tests
—-----Makefile          # Build system
__________
Compilation
From the project root (bgp_simulator/):
make clean
make
This produces:
bgp_sim (main simulator)
run_tests (test executable)
Compiler Flags
-std=c++17 
-O2 ( optimization for performance)
-Wall -Wextra  warnings for safer code
__________
Command Line Usage
./bgp_sim <relationships_file> <announcements_file> <rov_file> <output_file>
Example
./bgp_sim ../prefix/CAIDAASGraphCollector_2025.10.15.txt \
         ../prefix/anns.csv \
         ../prefix/rov_asns.csv \
         output.csv
__________
Input Format
1. CAIDA Relationships File
asn1|asn2|relationship|bgp
-1 > provider > customer
0 > peer <> peer
1 > customer > provider
__________
2. Announcements CSV
origin_asn,prefix,rov_invalid
Example:
15169,8.8.8.0/24,0
__________
3. ROV ASNs
<asn>
<asn>
...
ASNs listed here enforce ROV filtering.
__________
Output Format
asn,prefix,as_path
Example:
1,1.2.0.0/16,"(1, 11537, 10886, 27)"
__________
Code Structure
ASGraph
Stores all AS nodes
Builds relationships (providers, customers, peers)
Computes propagation ranks
ASNode
Represents a single AS
Stores:
providers
customers
peers
local RIB
received queue
Policy (BGP)
Handles:
announcement storage
tie-breaking logic
ROV filtering
main.cpp
Parses input files
Builds graph
Runs propagation:
Up
Peer
Down
Writes output
__________
Design Decisions
1. Graph Representation
Used adjacency lists for:
Memory efficiency (sparse graph ~100k nodes)
Fast traversal
Each AS stores:
providers
customers
peers
__________
2. Propagation Rank System
Flattened the DAG into ranks:
Rank 0 > no customers
Higher ranks > providers upward
This enables deterministic propagation:
Up (customers > providers)
Across (peer, one hop)
Down (providers -> customers)
__________
3. RIB + Received Queue Separation
Each AS maintains:
local_rib > best routes
recv_queue > incoming routes
This prevents:
mid-iteration modification bugs
Incorrect multi-hop propagation
__________
4. BGP Tie-Breaking Logic
Priority:
Relationship
Customer > Peer > Provider
Shortest AS-path
Lowest next-hop ASN
This models real-world economic routing behavior.
__________
5. AS-Path Construction
Each AS:
Prepends its ASN when storing a route
Updates next-hop on send
__________
6. ROV Filtering
If:
AS is ROV-enabled
announcement is invalid
 it is dropped immediately
__________
7. Valley-Free Routing Enforcement
Propagation strictly follows:
Up
Peer (single hop only)
Down
Prevents:
cycles
invalid routing paths
__________
8. Performance Considerations
Used:
unordered_map > O(1) lookup
rank-based iteration > avoids repeated traversals
batched processing > improves cache efficiency
__________
Testing
1. Provided Dataset Tests
Verified correctness using:
prefix
subprefix
many
Example:
./bgp_sim ... output.csv 
bash ../compare_output.sh output.csv ../prefix/ribs.csv
Result:
 Files match perfectly!
__________
2. Custom Tests
Implemented in tests/tests.cpp:
AS-path correctness
tie-breaking logic
propagation behavior
Run:
./run_tests
__________
3. Edge Cases Considered
Multiple announcements for same prefix
Equal path lengths (tie-breaking)
ROV filtering correctness
Empty or missing relationships
Large dataset performance
__________
Submission Notes
This project:
Fully implements BGP propagation
Passes all provided system tests
Produces correct output matching reference CSVs
__________
References
CAIDA AS Relationships Dataset
BGP RFC 4271
https://bgpsimulator.com


