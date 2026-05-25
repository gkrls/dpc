#include "dpc/util/cpu.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <mutex>
#include <pthread.h>
#include <sched.h>
#include <sstream>
#include <stdexcept>

namespace dpc::cpu {

namespace {
std::vector<int> g_pinned;
std::mutex g_pinned_mu;
} // namespace

std::vector<int> get_available_cores() {
  std::vector<int> out;
  cpu_set_t set;
  CPU_ZERO(&set);
  if (sched_getaffinity(0, sizeof(set), &set) != 0) return out;
  for (int i = 0; i < CPU_SETSIZE; ++i)
    if (CPU_ISSET(i, &set)) out.push_back(i);
  return out;
}

std::vector<int> get_pinned_cores() { return g_pinned; }

std::vector<int> get_cores_on_numa_node(int node) {
  std::vector<int> out;
  if (node < 0) return out;
  std::ifstream f("/sys/devices/system/node/node" + std::to_string(node) + "/cpulist");
  if (!f) return out;
  std::string s;
  std::getline(f, s);
  std::stringstream ss(s);
  std::string tok;
  while (std::getline(ss, tok, ',')) {
    if (tok.empty()) continue;
    auto dash = tok.find('-');
    if (dash == std::string::npos) {
      out.push_back(std::stoi(tok));
    } else {
      int lo = std::stoi(tok.substr(0, dash));
      int hi = std::stoi(tok.substr(dash + 1));
      for (int i = lo; i <= hi; ++i) out.push_back(i);
    }
  }
  return out;
}

int get_numa_node_for_iface(const std::string &ifname) {
  std::ifstream f("/sys/class/net/" + ifname + "/device/numa_node");
  if (!f) return -1;
  int node = -1;
  f >> node;
  return node;
}

std::vector<int> get_nic_local_cores(const std::string &ifname) {
  auto allowed = get_available_cores();
  if (allowed.empty()) throw std::runtime_error("no available cores");
  auto numa = get_cores_on_numa_node(get_numa_node_for_iface(ifname));
  if (numa.empty()) return allowed;
  std::sort(allowed.begin(), allowed.end());
  std::sort(numa.begin(), numa.end());
  std::vector<int> out;
  std::set_intersection(allowed.begin(), allowed.end(), numa.begin(), numa.end(), std::back_inserter(out));
  return out.empty() ? allowed : out;
}

void pin_to_core(int core) {
  cpu_set_t set;
  CPU_ZERO(&set);
  CPU_SET(core, &set);
  int rc = pthread_setaffinity_np(pthread_self(), sizeof(set), &set);
  if (rc != 0) throw std::runtime_error("pin_to_core(" + std::to_string(core) + ") failed: " + std::strerror(rc));
}

void pin_worker(size_t tid, size_t n, const std::string &ifname) {
  if (n == 0 || tid >= n) throw std::runtime_error("pin_worker: bad tid/n");

  auto pool = ifname.empty() ? get_available_cores() : get_nic_local_cores(ifname);
  if (n > pool.size())
    throw std::runtime_error("pin_worker: " + std::to_string(n) + " workers but only " + std::to_string(pool.size()) +
                             " cores available");

  int core = pool[tid];
  pin_to_core(core);

  std::lock_guard<std::mutex> lk(g_pinned_mu);
  g_pinned.push_back(core);
}

} // namespace dpc::cpu
