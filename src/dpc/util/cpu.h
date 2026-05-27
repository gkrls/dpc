#ifndef DPC_CPU_H
#define DPC_CPU_H

#include <string>
#include <vector>

namespace dpc::cpu {

// Cores allowed by the current cpuset (taskset/cgroups respected).
std::vector<int> get_available_cores();

// Cores pinned by dpc workers
std::vector<int> get_pinned_cores();

// Cores on a NUMA node. Empty if node < 0.
std::vector<int> get_cores_on_numa_node(int node);

// NIC-NUMA cores intersected with effective cpuset.
// Falls back to effective cpuset if intersection is empty or no NUMA info.
std::vector<int> get_nic_local_cores(const std::string &ifname);

// NUMA node of the NIC's PCI device. -1 if unknown.
int get_numa_node_for_iface(const std::string &ifname);

// Pin calling thread to a single core. Throws on failure.
void pin_to_core(int core);

// Pin worker thread to a single core in the numa node of the NIC. Throws on failure.
void pin_worker(size_t tid, size_t n, const std::string &ifname = "");

} // namespace dpc::cpu

#endif // !DPC_CPU_H
