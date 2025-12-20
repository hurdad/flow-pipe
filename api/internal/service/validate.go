package service

import (
	"fmt"

	flowpipev1 "github.com/hurdad/flow-pipe/gen/go/flowpipe/v1"
)

// ValidateFlowSpec performs basic structural validation of a FlowSpec.
//
// This enforces:
//   - required fields
//   - name uniqueness
//   - sane numeric values
//
// It intentionally does NOT enforce:
//   - wiring semantics (not yet in schema)
//   - DAG constraints
//   - runtime policy
func ValidateFlowSpec(spec *flowpipev1.FlowSpec) error {
	if spec == nil {
		return fmt.Errorf("flow spec is nil")
	}

	if spec.Name == "" {
		return fmt.Errorf("flow name is required")
	}

	if len(spec.Stages) == 0 {
		return fmt.Errorf("flow must contain at least one stage")
	}

	if err := validateQueues(spec); err != nil {
		return err
	}

	if err := validateStages(spec); err != nil {
		return err
	}

	return nil
}

// --------------------------------------------------
// Queues
// --------------------------------------------------

func validateQueues(spec *flowpipev1.FlowSpec) error {
	seen := map[string]struct{}{}

	for _, q := range spec.Queues {
		if q.Name == "" {
			return fmt.Errorf("queue name is required")
		}

		if _, ok := seen[q.Name]; ok {
			return fmt.Errorf("duplicate queue name: %q", q.Name)
		}
		seen[q.Name] = struct{}{}

		if q.Capacity <= 0 {
			return fmt.Errorf("queue %q must have capacity > 0", q.Name)
		}
	}

	return nil
}

// --------------------------------------------------
// Stages
// --------------------------------------------------

func validateStages(spec *flowpipev1.FlowSpec) error {
	seen := map[string]struct{}{}

	for _, s := range spec.Stages {
		if s.Name == "" {
			return fmt.Errorf("stage name is required")
		}

		if _, ok := seen[s.Name]; ok {
			return fmt.Errorf("duplicate stage name: %q", s.Name)
		}
		seen[s.Name] = struct{}{}

		if s.Type == "" {
			return fmt.Errorf("stage %q must specify a type", s.Name)
		}

		if s.Threads <= 0 {
			return fmt.Errorf("stage %q must have threads > 0", s.Name)
		}
	}

	return nil
}
