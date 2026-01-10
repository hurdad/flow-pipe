package normalize_test

import (
	"testing"

	flowpipev1 "github.com/hurdad/flow-pipe/gen/go/flowpipe/v1"
	"github.com/hurdad/flow-pipe/pkg/flow/normalize"
)

func TestNormalizeNilSpec(t *testing.T) {
	if got := normalize.Normalize(nil); got != nil {
		t.Fatalf("expected nil spec, got %v", got)
	}
}

func TestNormalizeSortsAndDefaults(t *testing.T) {
	spec := &flowpipev1.FlowSpec{
		Queues: []*flowpipev1.Queue{
			{Name: "z-queue", Capacity: 0},
			{Name: "a-queue", Capacity: 256},
			{Name: "m-queue"},
		},
		Stages: []*flowpipev1.Stage{
			{Name: "stage-z"},
			{Name: "stage-a"},
			{Name: "stage-m"},
		},
	}

	got := normalize.Normalize(spec)
	if got == nil {
		t.Fatal("expected normalized spec")
	}

	if got.Queues[0].Name != "a-queue" || got.Queues[1].Name != "m-queue" || got.Queues[2].Name != "z-queue" {
		t.Fatalf("queues not sorted: %+v", got.Queues)
	}
	if got.Queues[0].Capacity != 256 {
		t.Fatalf("expected a-queue capacity 256, got %d", got.Queues[0].Capacity)
	}
	if got.Queues[1].Capacity != 128 {
		t.Fatalf("expected m-queue capacity 128, got %d", got.Queues[1].Capacity)
	}
	if got.Queues[2].Capacity != 128 {
		t.Fatalf("expected z-queue capacity 128, got %d", got.Queues[2].Capacity)
	}

	if got.Stages[0].Name != "stage-a" || got.Stages[1].Name != "stage-m" || got.Stages[2].Name != "stage-z" {
		t.Fatalf("stages not sorted: %+v", got.Stages)
	}
}
