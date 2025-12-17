# flow-pipe Examples

These examples demonstrate how to extend the flow-pipe runtime with
custom C++ stages.

Each example:
- Builds a standalone executable
- Registers stages at runtime
- Uses bounded queues and JOB semantics

## Examples

- custom_stage: minimal transform example
- csv_pipeline: CSV → struct → output
- fanout_fanin: parallel fan-out and aggregation
