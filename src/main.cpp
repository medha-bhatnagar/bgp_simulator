#include "../include/as_graph.h"
#include <iostream>
#include <string>

static void print_usage(const char* prog) {
    std::cerr << "Usage: " << prog
              << " <caida_file> <anns_csv> <rov_asns_csv> <output_csv>\n"
              << "Example:\n"
              << "  " << prog << " CAIDAASGraphCollector.txt anns.csv rov_asns.csv output.csv\n";
}

int main(int argc, char* argv[]) {
    if (argc != 5) {
        print_usage(argv[0]);
        return 1;
    }

    std::string caida_file = argv[1];
    std::string anns_file  = argv[2];
    std::string rov_file   = argv[3];
    std::string out_file   = argv[4];

    std::cout << "[1/5] Loading AS graph from: " << caida_file << "\n";
    ASGraph graph;
    if (!graph.load_caida(caida_file)) {
         return 2;
    }
    std::cout << "      Loaded " << graph.ases.size() << " ASes, "
              << graph.ranks.size() << " propagation ranks\n";

    std::cout << "[2/5] Applying ROV from: " << rov_file << "\n";
    graph.apply_rov(rov_file);

    std::cout << "[3/5] Seeding announcements from: " << anns_file << "\n";
    graph.seed_announcements(anns_file);

    std::cout << "[4/5] Propagating announcements...\n";
    graph.propagate();

    std::cout << "[5/5] Writing output to: " << out_file << "\n";
    graph.write_output(out_file);

    std::cout << "Done.\n";
    return 0;
}
