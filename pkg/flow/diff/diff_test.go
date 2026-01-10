package diff_test

import (
	"testing"

	flowpipev1 "github.com/hurdad/flow-pipe/gen/go/flowpipe/v1"
	"github.com/hurdad/flow-pipe/pkg/flow/diff"
)

func TestDiffEmpty(t *testing.T) {
	spec := &flowpipev1.FlowSpec{Name: "flow"}
	out := diff.Diff(spec, spec)
	if len(out) != 0 {
		t.Fatalf("expected no diff, got %v", out)
	}
}

func TestDiffNameQueueStageCounts(t *testing.T) {
	a := &flowpipev1.FlowSpec{
		Name:   "flow-a",
		Queues: []*flowpipev1.Queue{{Name: "q1"}},
		Stages: []*flowpipev1.Stage{{Name: "s1", Type: "noop"}},
	}
	b := &flowpipev1.FlowSpec{
		Name:   "flow-b",
		Queues: []*flowpipev1.Queue{{Name: "q1"}, {Name: "q2"}},
		Stages: []*flowpipev1.Stage{{Name: "s1", Type: "noop"}, {Name: "s2", Type: "noop"}},
	}

	out := diff.Diff(a, b)
	if len(out) != 3 {
		t.Fatalf("expected 3 diff entries, got %v", out)
	}
	assertContains(t, out, "name: \"flow-a\" → \"flow-b\"")
	assertContains(t, out, "queues: 1 → 2")
	assertContains(t, out, "stages: 1 → 2")
}

func assertContains(t *testing.T, values []string, expected string) {
	t.Helper()

	for _, value := range values {
		if value == expected {
			return
		}
	}

	t.Fatalf("expected %q in %v", expected, values)
}
