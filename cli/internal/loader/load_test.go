package loader

import (
	"os"
	"path/filepath"
	"testing"
)

func TestLoadFlowSpec(t *testing.T) {
	dir := t.TempDir()
	path := filepath.Join(dir, "flow.yaml")
	input := []byte("name: example-flow\n")
	if err := os.WriteFile(path, input, 0o600); err != nil {
		t.Fatalf("write temp flow: %v", err)
	}

	spec, err := LoadFlowSpec(path)
	if err != nil {
		t.Fatalf("LoadFlowSpec error: %v", err)
	}
	if spec == nil || spec.Name != "example-flow" {
		t.Fatalf("expected flow name %q, got %+v", "example-flow", spec)
	}
}

func TestLoadFlowSpecInvalidYAML(t *testing.T) {
	dir := t.TempDir()
	path := filepath.Join(dir, "flow.yaml")
	input := []byte("name: [oops\n")
	if err := os.WriteFile(path, input, 0o600); err != nil {
		t.Fatalf("write temp flow: %v", err)
	}

	if _, err := LoadFlowSpec(path); err == nil {
		t.Fatal("expected error for invalid yaml")
	}
}
