# OS/II as Research: MSc and PhD Framing

## Research Context
OS/II investigates whether a restricted BEAM-inspired runtime can provide robust orchestration on MCU-class hardware while preserving deterministic I/O behavior through native driver ownership.

## Core Research Question
Can a bounded, register-based BEAM-style VM deliver practical control-plane benefits on microcontrollers without violating latency and resource constraints?

## MSc-Grade Thesis Scope (Strongly Feasible)

### Candidate Title
Design and Evaluation of a Deterministic BEAM-Inspired Runtime for nRF52840 Embedded Systems

### Objectives
1. Design a constrained VM model and VM/native contract.
2. Implement and validate selected peripheral orchestration paths.
3. Evaluate latency, memory, and fault behavior against baseline native control flows.

### Hypotheses
1. VM orchestration overhead remains acceptable for control-plane tasks.
2. Bounded mailbox + explicit ABI improves failure clarity and recovery behavior.
3. Developer iteration speed improves through explicit observability and typed events.

### Method
1. Implement staged subsystem support (M0-M5 model).
2. Run controlled benchmarks (latency, jitter, memory footprint, throughput where relevant).
3. Run failure-injection tests and recovery measurements.
4. Compare against equivalent Zephyr-native implementations.

### Evidence Artifacts
- Source code and reproducible build scripts.
- Acceptance tests per subsystem.
- Benchmark and fault-injection reports.
- Decision logs linked to implementation changes.

## PhD-Grade Extension (Possible With Added Formal Depth)

### Candidate Direction
Formally Constrained Actor-Like Runtime Architectures for Deterministic Embedded Control

### Additional Contributions Needed
1. Formal model of VM scheduling/queueing/resource bounds.
2. Proved invariants or model-checked properties for selected failure modes.
3. Multi-board, multi-workload comparative study.
4. Generalized runtime framework beyond one board family.

### Strong PhD Questions
1. Which VM/native partitioning strategies minimize jitter under load?
2. What formal guarantees are practical for mailbox-driven embedded orchestration?
3. How do actor-inspired control semantics compare to RTOS-task approaches under identical hardware constraints?

## Practitioner Ergonomics Value (for both MSc and PhD)
OS/II shortens debug cycles by exposing explicit control flow and typed runtime signals. Developers can trace the path from mailbox command to hardware effect and observe where failures occur (validation, bus, device response, or policy).

## Observability Plan
Required signal set:
- command ingress (`type,a,b,c,d`)
- dispatch status
- peripheral return code/value
- monotonic timestamp
- recovery transitions and retries

Collection:
- structured serial logs now
- optional binary trace stream later

## Suggested Publication Path
1. Workshop/demo paper: architecture + early hardware results.
2. Full MSc thesis: implementation + controlled evaluation.
3. Journal/PhD progression: formalism + generalized runtime + broader benchmarks.

## Suggested Next 90-Day Milestones
1. Freeze M1 contract and publish reproducible benchmark harness.
2. Add M2 subsystems (ADC/UART/timer) with acceptance tests.
3. Produce comparative results (VM vs native) and first research preprint draft.
