```markdown
# Non-Goals

flow-pipe explicitly does NOT aim to be:

## A Workflow Engine
- No DAG execution semantics
- No step ordering
- No retries or branching
- No execution state machine

## A Stream Processor
- No windows
- No watermarks
- No exactly-once guarantees
- No stateful operators

## A Scheduler
- No internal scheduling
- No task placement logic
- Kubernetes handles lifecycle

## A UI Platform
- No dashboards
- No visual editors

These are deliberate exclusions to keep the system simple, reliable, and composable.
