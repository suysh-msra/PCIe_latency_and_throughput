#include <algorithm>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <queue>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

struct Config {
  int transactions = 1000;
  int payload_bytes = 256;
  int queue_depth = 8;
  double link_gbps = 8.0;
  double fixed_ns = 70.0;
  double endpoint_ns = 120.0;
  double host_gap_ns = 20.0;
  double protocol_overhead_bytes = 24.0;
};

struct Sample {
  double issue_ns = 0.0;
  double admitted_ns = 0.0;
  double complete_ns = 0.0;
  double queue_ns = 0.0;
  double serialize_ns = 0.0;
  double endpoint_ns = 0.0;
};

struct Summary {
  double avg_latency_ns = 0.0;
  double p50_latency_ns = 0.0;
  double p95_latency_ns = 0.0;
  double p99_latency_ns = 0.0;
  double avg_queue_ns = 0.0;
  double avg_serialize_ns = 0.0;
  double throughput_gbps = 0.0;
  double tx_per_us = 0.0;
  double total_time_ns = 0.0;
};

double percentile(std::vector<double> values, double p) {
  if (values.empty()) {
    return 0.0;
  }
  std::sort(values.begin(), values.end());
  const double idx = p * static_cast<double>(values.size() - 1);
  const auto lo = static_cast<std::size_t>(idx);
  const auto hi = std::min(lo + 1, values.size() - 1);
  const double frac = idx - static_cast<double>(lo);
  return values[lo] * (1.0 - frac) + values[hi] * frac;
}

double parse_double(const std::string& value, const std::string& flag) {
  char* end = nullptr;
  const double parsed = std::strtod(value.c_str(), &end);
  if (end == value.c_str() || *end != '\0') {
    throw std::runtime_error("invalid value for " + flag + ": " + value);
  }
  return parsed;
}

int parse_int(const std::string& value, const std::string& flag) {
  char* end = nullptr;
  const long parsed = std::strtol(value.c_str(), &end, 10);
  if (end == value.c_str() || *end != '\0') {
    throw std::runtime_error("invalid value for " + flag + ": " + value);
  }
  return static_cast<int>(parsed);
}

Config parse_args(int argc, char** argv) {
  Config cfg;
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    auto require_value = [&](const std::string& flag) -> std::string {
      if (i + 1 >= argc) {
        throw std::runtime_error("missing value for " + flag);
      }
      return argv[++i];
    };

    if (arg == "--transactions") {
      cfg.transactions = parse_int(require_value(arg), arg);
    } else if (arg == "--payload") {
      cfg.payload_bytes = parse_int(require_value(arg), arg);
    } else if (arg == "--queue-depth") {
      cfg.queue_depth = parse_int(require_value(arg), arg);
    } else if (arg == "--link-gbps") {
      cfg.link_gbps = parse_double(require_value(arg), arg);
    } else if (arg == "--fixed-ns") {
      cfg.fixed_ns = parse_double(require_value(arg), arg);
    } else if (arg == "--endpoint-ns") {
      cfg.endpoint_ns = parse_double(require_value(arg), arg);
    } else if (arg == "--host-gap-ns") {
      cfg.host_gap_ns = parse_double(require_value(arg), arg);
    } else if (arg == "--protocol-overhead") {
      cfg.protocol_overhead_bytes = parse_double(require_value(arg), arg);
    } else if (arg == "--help") {
      std::cout
          << "Usage: pcie_sim [options]\n"
          << "  --transactions N\n"
          << "  --payload BYTES\n"
          << "  --queue-depth N\n"
          << "  --link-gbps GBPS\n"
          << "  --fixed-ns NS\n"
          << "  --endpoint-ns NS\n"
          << "  --host-gap-ns NS\n"
          << "  --protocol-overhead BYTES\n";
      std::exit(0);
    } else {
      throw std::runtime_error("unknown argument: " + arg);
    }
  }

  if (cfg.transactions <= 0 || cfg.payload_bytes <= 0 || cfg.queue_depth <= 0 ||
      cfg.link_gbps <= 0.0) {
    throw std::runtime_error("transactions, payload, queue-depth, and link-gbps must be positive");
  }
  return cfg;
}

Summary simulate(const Config& cfg) {
  std::queue<double> completion_times;
  std::vector<Sample> samples;
  samples.reserve(cfg.transactions);

  double link_available_ns = 0.0;
  double endpoint_available_ns = 0.0;
  double total_bits = 0.0;

  for (int i = 0; i < cfg.transactions; ++i) {
    const double issue_ns = static_cast<double>(i) * cfg.host_gap_ns;

    while (!completion_times.empty() && completion_times.front() <= issue_ns) {
      completion_times.pop();
    }

    double admitted_ns = issue_ns;
    if (static_cast<int>(completion_times.size()) >= cfg.queue_depth) {
      admitted_ns = completion_times.front();
      while (!completion_times.empty() && completion_times.front() <= admitted_ns) {
        completion_times.pop();
      }
    }

    const double queue_ns = admitted_ns - issue_ns;
    const double total_bytes = static_cast<double>(cfg.payload_bytes) + cfg.protocol_overhead_bytes;
    const double serialize_ns = (total_bytes * 8.0) / cfg.link_gbps;
    const double link_start_ns = std::max(admitted_ns + cfg.fixed_ns, link_available_ns);
    const double link_done_ns = link_start_ns + serialize_ns;
    link_available_ns = link_done_ns;

    const double endpoint_start_ns = std::max(link_done_ns, endpoint_available_ns);
    const double complete_ns = endpoint_start_ns + cfg.endpoint_ns;
    endpoint_available_ns = complete_ns;

    completion_times.push(complete_ns);
    total_bits += static_cast<double>(cfg.payload_bytes) * 8.0;

    samples.push_back(Sample{
        .issue_ns = issue_ns,
        .admitted_ns = admitted_ns,
        .complete_ns = complete_ns,
        .queue_ns = queue_ns,
        .serialize_ns = serialize_ns,
        .endpoint_ns = cfg.endpoint_ns,
    });
  }

  std::vector<double> latencies;
  latencies.reserve(samples.size());
  Summary summary;

  for (const auto& sample : samples) {
    const double latency = sample.complete_ns - sample.issue_ns;
    latencies.push_back(latency);
    summary.avg_latency_ns += latency;
    summary.avg_queue_ns += sample.queue_ns;
    summary.avg_serialize_ns += sample.serialize_ns;
  }

  summary.avg_latency_ns /= static_cast<double>(samples.size());
  summary.avg_queue_ns /= static_cast<double>(samples.size());
  summary.avg_serialize_ns /= static_cast<double>(samples.size());
  summary.p50_latency_ns = percentile(latencies, 0.50);
  summary.p95_latency_ns = percentile(latencies, 0.95);
  summary.p99_latency_ns = percentile(latencies, 0.99);
  summary.total_time_ns = samples.back().complete_ns - samples.front().issue_ns;
  summary.throughput_gbps = total_bits / summary.total_time_ns;
  summary.tx_per_us = static_cast<double>(cfg.transactions) / (summary.total_time_ns / 1000.0);
  return summary;
}

void print_summary(const Config& cfg, const Summary& summary) {
  std::cout << std::fixed << std::setprecision(2);
  std::cout << "config"
            << " transactions=" << cfg.transactions
            << " payload_bytes=" << cfg.payload_bytes
            << " queue_depth=" << cfg.queue_depth
            << " link_gbps=" << cfg.link_gbps
            << " fixed_ns=" << cfg.fixed_ns
            << " endpoint_ns=" << cfg.endpoint_ns
            << " host_gap_ns=" << cfg.host_gap_ns
            << "\n";
  std::cout << "results"
            << " avg_latency_ns=" << summary.avg_latency_ns
            << " p50_latency_ns=" << summary.p50_latency_ns
            << " p95_latency_ns=" << summary.p95_latency_ns
            << " p99_latency_ns=" << summary.p99_latency_ns
            << " avg_queue_ns=" << summary.avg_queue_ns
            << " avg_serialize_ns=" << summary.avg_serialize_ns
            << " throughput_gbps=" << summary.throughput_gbps
            << " tx_per_us=" << summary.tx_per_us
            << " total_time_ns=" << summary.total_time_ns
            << "\n";
}

}  // namespace

int main(int argc, char** argv) {
  try {
    const Config cfg = parse_args(argc, argv);
    const Summary summary = simulate(cfg);
    print_summary(cfg, summary);
    return 0;
  } catch (const std::exception& ex) {
    std::cerr << "error: " << ex.what() << "\n";
    return 1;
  }
}
