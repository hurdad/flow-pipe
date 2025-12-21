package normalize

import (
	"sort"

	flowpipev1 "github.com/hurdad/flow-pipe/gen/go/flowpipe/v1"
)

func Normalize(in *flowpipev1.FlowSpec) *flowpipev1.FlowSpec {
	if in == nil {
		return nil
	}

	spec := *in

	// Sort queues by name
	sort.Slice(spec.Queues, func(i, j int) bool {
		return spec.Queues[i].Name < spec.Queues[j].Name
	})

	// Default capacities
	for _, q := range spec.Queues {
		if q.Capacity == 0 {
			q.Capacity = 128
		}
	}

	// Sort stages
	sort.Slice(spec.Stages, func(i, j int) bool {
		return spec.Stages[i].Name < spec.Stages[j].Name
	})

	return &spec
}
