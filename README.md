# BGP Simulator – CSE3150 Course Project

## Overview
This project is a C++ implementation of a **BGP (Border Gateway Protocol)** simulator. It models how route announcements propagate through the Internet using a CAIDA-derived Autonomous System (AS) graph.

**The simulator:**
* Builds an AS-level graph (providers, customers, peers).
* Applies Route Origin Validation (ROV).
* Simulates valley-free BGP propagation.
* Selects the best routes using standard BGP decision rules.
* Outputs each AS’s final routing table (RIB) to a CSV file.

---

## Files
```text
bgp_simulator/
├── src/                # Core implementation
│   ├── main.cpp
│   ├── as_graph.cpp
│   ├── as_node.cpp
│   └── policy.cpp
├── include/            # Header files
│   ├── as_graph.h
│   ├── as_node.h
│   └── policy.h
├── tests/
│   └── tests.cpp       # Unit/system tests
└── Makefile            # Build system
```

---

## Compilation
From the project root (`bgp_simulator/`):
```bash
make clean
make
```

**This produces:**
* `bgp_sim`: Main simulator executable.
* `run_tests`: Test suite executable.

### Compiler Flags
* `-std=c++17`
* `-O2`: Optimization for performance.
* `-Wall -Wextra`: Warnings for safer code.

---

## Usage
```bash
./bgp_sim <relationships_file> <announcements_file> <rov_file> <output_file>
```

### Example
```bash
./bgp_sim ../prefix/CAIDAASGraphCollector_2025.10.15.txt \
           ../prefix/anns.csv \
           ../prefix/rov_asns.csv \
           output.csv
```

---

## Data Formats

### 1. CAIDA Relationships File
`asn1|asn2|relationship|bgp`
* `-1`: provider $\rightarrow$ customer
* `0`: peer $\leftrightarrow$ peer
* `1`: customer $\rightarrow$ provider

### 2. Announcements CSV
`origin_asn,prefix,rov_invalid`

### 3. ROV ASNs
A simple list of ASNs that enforce ROV filtering.

---

## Design Decisions

### 1. Graph Representation
Used **Adjacency Lists** for memory efficiency (sparse graph ~100k nodes) and fast traversal.

### 2. Propagation Rank System
Flattened the DAG into ranks to enable deterministic propagation:
* **Up:** (Customers $\rightarrow$ Providers)
* **Across:** (Peer, one hop)
* **Down:** (Providers $\rightarrow$ Customers)

### 3. RIB + Received Queue Separation
Each AS maintains a `local_rib` (best routes) and a `recv_queue` (incoming routes). This prevents mid-iteration modification bugs and incorrect multi-hop propagation.

### 4. BGP Tie-Breaking Logic
Priority order for route selection:
1. **Relationship:** Customer > Peer > Provider
2. **Shortest AS-path**
3. **Lowest next-hop ASN**

5. AS-Path Construction

To maintain a valid record of the route's journey, the simulator handles path updates at each hop:

Prepending: When an AS accepts a route into its local_rib, it prepends its own ASN to the AS-path.

Next-Hop Tracking: Updates the next-hop ASN during the send phase so the recipient can identify the immediate source.

Loop Prevention: The simulator checks if an AS's own ASN is already present in the path; if a loop is detected, the announcement is rejected.

6. ROV Filtering

The simulator models Route Origin Validation to simulate RPKI security:

Enforcement: If an AS is listed in the rov_file, it acts as an "ROV-enabled" node.

Drop Logic: Any announcement marked as invalid in the input CSV is dropped immediately by ROV-enabled nodes.

Impact: This prevents hijacked or incorrect prefixes from propagating further down the chain from that node.

7. Valley-Free Routing Enforcement

To model real-world economic incentives, the simulator strictly enforces the Gao-Rexford model:

Valid Paths: Routes can flow from Customer → Provider, Peer ↔ Peer, or Provider → Customer.

The "Valley" Constraint: A route received from a provider or a peer cannot be exported to another provider or peer.

Result: This ensures that no AS provides free transit for its providers or peers, preventing "valleys" in the routing graph.

8. Performance Considerations

Given the potential size of the CAIDA dataset (~100k+ nodes), the following optimizations were implemented:

std::unordered_map: Used for O(1) lookups of AS nodes by their ASN.

Rank-Based Iteration: Instead of a full graph traversal for every announcement, the rank system allows the simulator to process the graph in a single linear pass per direction (Up, Across, Down).

Memory Management: Adjacency lists were used instead of matrices to keep the memory footprint low for sparse Internet graphs.


---

## Testing

### 1. Provided Dataset Tests
Verified correctness using prefix, subprefix, and large-scale datasets:
```bash
./bgp_sim ... output.csv 
bash ../compare_output.sh output.csv ../prefix/ribs.csv
```

### 2. Custom Tests
Implemented in `tests/tests.cpp`:
* AS-path correctness
* Tie-breaking logic
* Propagation behavior

---

## References
* [CAIDA AS Relationships Dataset](https://www.caida.org/data/as-relationships/)
* BGP RFC 4271
* [BGP Simulator](https://bgpsimulator.com)




