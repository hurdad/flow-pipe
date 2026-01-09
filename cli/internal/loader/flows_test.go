package loader

import (
	"os"
	"path/filepath"
	"runtime"
	"strings"
	"testing"

	"github.com/hurdad/flow-pipe/pkg/flow/normalize"
	"github.com/hurdad/flow-pipe/pkg/flow/validate"
)

func TestRepositoryFlowsValidate(t *testing.T) {
	_, filePath, _, ok := runtime.Caller(0)
	if !ok {
		t.Fatalf("failed to locate test file path")
	}

	repoRoot := filepath.Clean(filepath.Join(filepath.Dir(filePath), "..", "..", ".."))
	flowsDir := filepath.Join(repoRoot, "flows")
	entries, err := os.ReadDir(flowsDir)
	if err != nil {
		t.Fatalf("read flows dir: %v", err)
	}

	for _, entry := range entries {
		if entry.IsDir() {
			continue
		}
		if !strings.HasSuffix(entry.Name(), ".yaml") && !strings.HasSuffix(entry.Name(), ".yml") {
			continue
		}

		path := filepath.Join(flowsDir, entry.Name())
		spec, err := LoadFlowSpec(path)
		if err != nil {
			t.Fatalf("load flow %s: %v", entry.Name(), err)
		}

		if spec.Name == "" {
			base := strings.TrimSuffix(entry.Name(), filepath.Ext(entry.Name()))
			spec.Name = base
		}

		spec = normalize.Normalize(spec)
		if err := validate.Validate(spec); err != nil {
			t.Fatalf("validate flow %s: %v", entry.Name(), err)
		}
	}
}
