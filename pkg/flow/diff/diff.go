package diff

import (
	"fmt"

	flowpipev1 "github.com/hurdad/flow-pipe/gen/go/flowpipe/v1"
)

// Diff returns a human-readable structural diff between two FlowSpecs.
// Specs must already be normalized.
func Diff(a, b *flowpipev1.FlowSpec) []string {
	var out []string

	if a.Name != b.Name {
		out = append(out, fmt.Sprintf("name: %q → %q", a.Name, b.Name))
	}

	if len(a.Queues) != len(b.Queues) {
		out = append(out, fmt.Sprintf("queues: %d → %d", len(a.Queues), len(b.Queues)))
	}

	if len(a.Stages) != len(b.Stages) {
		out = append(out, fmt.Sprintf("stages: %d → %d", len(a.Stages), len(b.Stages)))
	}

	// (Intentionally minimal — expand later)
	return out
}
