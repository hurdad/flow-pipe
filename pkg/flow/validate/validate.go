package validate

import (
	"fmt"

	flowpipev1 "github.com/hurdad/flow-pipe/gen/go/flowpipe/v1"
)

// Validate performs semantic validation of a FlowSpec.
// The spec is expected to be normalized before validation.
func Validate(spec *flowpipev1.FlowSpec) error {
	if spec == nil {
		return &Error{"spec", "flow spec is nil"}
	}

	// --------------------------------------------------
	// Flow-level validation
	// --------------------------------------------------

	if spec.Name == "" {
		return &Error{"name", "flow name is required"}
	}

	if len(spec.Stages) == 0 {
		return &Error{
			Field:   "stages",
			Message: "flow must contain at least one stage",
		}
	}

	// --------------------------------------------------
	// Queues
	// --------------------------------------------------

	queueNames := map[string]struct{}{}

	for _, q := range spec.Queues {
		if q.Name == "" {
			return &Error{"queues.name", "queue name is required"}
		}

		if _, exists := queueNames[q.Name]; exists {
			return &Error{
				Field:   "queues.name",
				Message: fmt.Sprintf("duplicate queue name %q", q.Name),
			}
		}

		// Capacity sanity is handled by normalization
		queueNames[q.Name] = struct{}{}
	}

	// --------------------------------------------------
	// Stages
	// --------------------------------------------------

	stageNames := map[string]struct{}{}

	for _, s := range spec.Stages {
		if s.Name == "" {
			return &Error{"stages.name", "stage name is required"}
		}

		if _, exists := stageNames[s.Name]; exists {
			return &Error{
				Field:   "stages.name",
				Message: fmt.Sprintf("duplicate stage %q", s.Name),
			}
		}

		stageNames[s.Name] = struct{}{}

		if s.Type == "" {
			return &Error{
				Field:   "stages.type",
				Message: fmt.Sprintf("stage %q missing type", s.Name),
			}
		}

		if s.Threads < 1 {
			return &Error{
				Field:   "stages.threads",
				Message: fmt.Sprintf("stage %q must declare at least one thread", s.Name),
			}
		}

		// input_queue (optional string -> *string)
		if s.InputQueue != nil {
			q := *s.InputQueue
			if _, ok := queueNames[q]; !ok {
				return &Error{
					Field: "stages.input_queue",
					Message: fmt.Sprintf(
						"stage %q references unknown input queue %q",
						s.Name,
						q,
					),
				}
			}
		}

		// output_queue (optional string -> *string)
		if s.OutputQueue != nil {
			q := *s.OutputQueue
			if _, ok := queueNames[q]; !ok {
				return &Error{
					Field: "stages.output_queue",
					Message: fmt.Sprintf(
						"stage %q references unknown output queue %q",
						s.Name,
						q,
					),
				}
			}
		}
	}

	return nil
}
