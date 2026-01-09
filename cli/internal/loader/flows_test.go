package loader_test

import (
	"os"
	"path/filepath"
	"runtime"
	"strings"
	"testing"

	"github.com/hurdad/flow-pipe/cli/internal/loader"
	"github.com/hurdad/flow-pipe/pkg/flow/normalize"
	"github.com/hurdad/flow-pipe/pkg/flow/validate"
)

func TestRepositoryFlowsValidate(t *testing.T) {
	flowsDir := filepath.Join(repoRoot(t), "flows")
	entries, err := os.ReadDir(flowsDir)
	if err != nil {
		if os.IsNotExist(err) {
			t.Skipf("flows directory not present at %s", flowsDir)
		}
		t.Fatalf("read flows dir: %v", err)
	}

	for _, entry := range entries {
		if entry.IsDir() {
			continue
		}
		name := entry.Name()
		if !strings.HasSuffix(name, ".yaml") {
			continue
		}

		path := filepath.Join(flowsDir, name)
		spec, err := loader.LoadFlowSpec(path)
		if err != nil {
			t.Fatalf("load flow %s: %v", name, err)
		}

		spec = normalize.Normalize(spec)
		if err := validate.Validate(spec); err != nil {
			t.Fatalf("validate flow %s: %v", name, err)
		}
	}
}

func repoRoot(t *testing.T) string {
	_, filename, _, ok := runtime.Caller(0)
	if !ok {
		t.Fatal("resolve test file path")
	}

	return filepath.Clean(filepath.Join(filepath.Dir(filename), "..", "..", "..", ".."))
}
