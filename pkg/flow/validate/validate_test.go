package validate_test

import (
	"errors"
	"testing"

	flowpipev1 "github.com/hurdad/flow-pipe/gen/go/flowpipe/v1"
	"github.com/hurdad/flow-pipe/pkg/flow/validate"
)

func TestValidateNilSpec(t *testing.T) {
	err := validate.Validate(nil)
	assertValidationError(t, err, "spec", "flow spec is nil")
}

func TestValidateMissingName(t *testing.T) {
	err := validate.Validate(&flowpipev1.FlowSpec{})
	assertValidationError(t, err, "name", "flow name is required")
}

func TestValidateMissingStages(t *testing.T) {
	err := validate.Validate(&flowpipev1.FlowSpec{Name: "flow"})
	assertValidationError(t, err, "stages", "flow must contain at least one stage")
}

func TestValidateDuplicateQueue(t *testing.T) {
	spec := &flowpipev1.FlowSpec{
		Name: "flow",
		Queues: []*flowpipev1.Queue{
			{Name: "queue"},
			{Name: "queue"},
		},
		Stages: []*flowpipev1.Stage{{Name: "stage", Type: "noop"}},
	}
	err := validate.Validate(spec)
	assertValidationError(t, err, "queues.name", "duplicate queue \"queue\"")
}

func TestValidateDuplicateStage(t *testing.T) {
	spec := &flowpipev1.FlowSpec{
		Name:   "flow",
		Queues: []*flowpipev1.Queue{{Name: "queue"}},
		Stages: []*flowpipev1.Stage{
			{Name: "stage", Type: "noop"},
			{Name: "stage", Type: "noop"},
		},
	}
	err := validate.Validate(spec)
	assertValidationError(t, err, "stages.name", "duplicate stage \"stage\"")
}

func TestValidateMissingStageType(t *testing.T) {
	spec := &flowpipev1.FlowSpec{
		Name:   "flow",
		Queues: []*flowpipev1.Queue{{Name: "queue"}},
		Stages: []*flowpipev1.Stage{{Name: "stage"}},
	}
	err := validate.Validate(spec)
	assertValidationError(t, err, "stages.type", "stage \"stage\" missing type")
}

func TestValidateUnknownInputQueue(t *testing.T) {
	spec := &flowpipev1.FlowSpec{
		Name:   "flow",
		Queues: []*flowpipev1.Queue{{Name: "queue"}},
		Stages: []*flowpipev1.Stage{
			{
				Name:       "stage",
				Type:       "noop",
				Threads:    1,
				InputQueue: strPtr("missing"),
			},
		},
	}
	err := validate.Validate(spec)
	assertValidationError(t, err, "stages.input_queue", "stage \"stage\" references unknown input queue \"missing\"")
}

func TestValidateUnknownOutputQueue(t *testing.T) {
	spec := &flowpipev1.FlowSpec{
		Name:   "flow",
		Queues: []*flowpipev1.Queue{{Name: "queue"}},
		Stages: []*flowpipev1.Stage{
			{
				Name:        "stage",
				Type:        "noop",
				Threads:     1,
				OutputQueue: strPtr("missing"),
			},
		},
	}
	err := validate.Validate(spec)
	assertValidationError(t, err, "stages.output_queue", "stage \"stage\" references unknown output queue \"missing\"")
}

func TestValidateZeroThreads(t *testing.T) {
	spec := &flowpipev1.FlowSpec{
		Name:   "flow",
		Queues: []*flowpipev1.Queue{{Name: "queue"}},
		Stages: []*flowpipev1.Stage{
			{
				Name:    "stage",
				Type:    "noop",
				Threads: 0,
			},
		},
	}
	err := validate.Validate(spec)
	assertValidationError(t, err, "stages.threads", "stage \"stage\" must declare at least one thread")
}

func TestValidateSuccess(t *testing.T) {
	spec := &flowpipev1.FlowSpec{
		Name: "flow",
		Queues: []*flowpipev1.Queue{
			{Name: "in"},
			{Name: "out"},
		},
		Stages: []*flowpipev1.Stage{
			{
				Name:        "stage",
				Type:        "noop",
				Threads:     1,
				InputQueue:  strPtr("in"),
				OutputQueue: strPtr("out"),
			},
		},
	}

	if err := validate.Validate(spec); err != nil {
		t.Fatalf("expected nil error, got %v", err)
	}
}

func TestValidationErrorFormatting(t *testing.T) {
	err := (&validate.Error{Field: "field", Message: "message"}).Error()
	if err != "field: message" {
		t.Fatalf("expected formatted error, got %q", err)
	}

	err = (&validate.Error{Message: "message"}).Error()
	if err != "message" {
		t.Fatalf("expected message only, got %q", err)
	}
}

func assertValidationError(t *testing.T, err error, field, message string) {
	t.Helper()

	if err == nil {
		t.Fatalf("expected error %q", message)
	}

	var vErr *validate.Error
	if !errors.As(err, &vErr) {
		t.Fatalf("expected validate.Error, got %T", err)
	}

	if vErr.Field != field {
		t.Fatalf("expected field %q, got %q", field, vErr.Field)
	}
	if vErr.Message != message {
		t.Fatalf("expected message %q, got %q", message, vErr.Message)
	}
}

func strPtr(value string) *string {
	return &value
}
