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

### 5. AS-Path Construction
Each AS:
**Prepends its ASN when storing a route***
***Updates next-hop on send***

### 6. ROV Filtering
if:
###AS is ROV-enabled###
###announcement is invalid###
it is dropped immediately

###7. Valley-Free Routing Enforcement
Propagation strictly follows:
###Up###
###Peer(single hop only)###
###Down###
Prevents:
###cycles###
###invalid routing paths###

###Performance 
###unordered_map>O(1) lookup###
###rank-based iteration > avoide repeated traversals###
###batched processing > improves cache efficiency###


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




