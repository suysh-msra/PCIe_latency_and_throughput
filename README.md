# PCIe_latency_and_throughput
Sim for exploring two approachable PCIe themes:  - latency of host-to-device transactions - throughput bottlenecks caused by payload size, queue depth, and service rate.

It uses:

- `C++` for a fast transaction-level simulator
- `Python` for experiment sweeps and summary output
- `Verilog` for a tiny endpoint model with queue/backpressure behavior

## What This Simulates

The C++ model is intentionally simple. It does not implement full PCIe packets, credit rules, LTSSM, or a realistic root complex. Instead, it models a stream of posted transactions through a link and endpoint pipeline so we can inspect:

- fixed per-transaction overhead
- serialization time from payload size and link bandwidth
- endpoint service time
- queueing delay from limited in-flight capacity

That makes it useful for fast intuition:

- Why do small transactions have terrible effective bandwidth?
- When does a deeper queue help?
- When does the endpoint become the bottleneck instead of the link?

## Layout

- `src/pcie_sim.cpp`: C++ simulator
- `scripts/run_experiments.py`: build/run helper and experiment sweeps
- `rtl/simple_pcie_endpoint.v`: minimal queueing endpoint model
- `rtl/tb_simple_pcie_endpoint.v`: Verilog testbench

## Quick Start

Build and run the default experiments:

```bash
python3 scripts/run_experiments.py
```

Run one custom simulation:

```bash
./build/pcie_sim --transactions 256 --payload 128 --queue-depth 8 --link-gbps 8 --endpoint-ns 110
```

Run the Verilog model:

```bash
iverilog -g2012 -o build/endpoint_tb rtl/simple_pcie_endpoint.v rtl/tb_simple_pcie_endpoint.v
vvp build/endpoint_tb
```

## Useful Parameters

- `--payload`: payload bytes per transaction
- `--queue-depth`: maximum in-flight requests
- `--link-gbps`: effective one-direction link bandwidth in Gbit/s
- `--fixed-ns`: fixed request overhead
- `--endpoint-ns`: endpoint processing time per request
- `--host-gap-ns`: host issue gap between new requests

## First Experiments To Try

1. Fix bandwidth and endpoint time. Sweep payload size from 16B to 1024B.
2. Fix payload size. Sweep queue depth from 1 to 32.
3. Lower endpoint service time until the link becomes the bottleneck.
4. Increase host issue gap and see when queueing disappears.

## Interpretation

If `avg_queue_ns` dominates, you are queue-limited.
If `avg_serialize_ns` dominates, you are link-limited.
If `endpoint_ns` dominates, the device pipeline is the bottleneck.

This is not a cycle-accurate PCIe model. It is a small, inspectable lab for building intuition quickly.

