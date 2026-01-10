package codec_test

import (
	"testing"

	flowpipev1 "github.com/hurdad/flow-pipe/gen/go/flowpipe/v1"
	"github.com/hurdad/flow-pipe/pkg/flow/codec"
	"google.golang.org/protobuf/proto"
)

func TestJSONRoundTrip(t *testing.T) {
	spec := &flowpipev1.FlowSpec{
		Name: "flow",
		Queues: []*flowpipev1.Queue{
			{Name: "in", Capacity: 128},
		},
		Stages: []*flowpipev1.Stage{
			{Name: "stage", Type: "noop"},
		},
	}

	data, err := codec.ToJSON(spec)
	if err != nil {
		t.Fatalf("ToJSON error: %v", err)
	}

	var decoded flowpipev1.FlowSpec
	if err := codec.FromJSON(data, &decoded); err != nil {
		t.Fatalf("FromJSON error: %v", err)
	}

	if !proto.Equal(spec, &decoded) {
		t.Fatalf("expected round-trip spec to match")
	}
}
