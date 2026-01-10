package fingerprint_test

import (
	"testing"

	flowpipev1 "github.com/hurdad/flow-pipe/gen/go/flowpipe/v1"
	"github.com/hurdad/flow-pipe/pkg/flow/fingerprint"
)

func TestFingerprintStable(t *testing.T) {
	spec := &flowpipev1.FlowSpec{Name: "flow"}
	first := fingerprint.Fingerprint(spec)
	second := fingerprint.Fingerprint(spec)

	if first != second {
		t.Fatalf("expected stable fingerprint, got %q and %q", first, second)
	}
}

func TestFingerprintDifferentSpecs(t *testing.T) {
	first := fingerprint.Fingerprint(&flowpipev1.FlowSpec{Name: "flow-a"})
	second := fingerprint.Fingerprint(&flowpipev1.FlowSpec{Name: "flow-b"})

	if first == second {
		t.Fatalf("expected different fingerprints")
	}
}
