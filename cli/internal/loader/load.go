package loader

import (
	"encoding/json"
	"fmt"
	"os"

	flowpipev1 "github.com/hurdad/flow-pipe/gen/go/flowpipe/v1"
	"google.golang.org/protobuf/encoding/protojson"
	"gopkg.in/yaml.v3"
)

// LoadFlowSpec loads a YAML flow file into a FlowSpec protobuf.
func LoadFlowSpec(path string) (*flowpipev1.FlowSpec, error) {
	data, err := os.ReadFile(path)
	if err != nil {
		return nil, fmt.Errorf("read %s: %w", path, err)
	}

	// YAML → generic map
	var raw map[string]any
	if err := yaml.Unmarshal(data, &raw); err != nil {
		return nil, fmt.Errorf("yaml decode: %w", err)
	}

	// YAML → JSON
	jsonBytes, err := yamlToJSON(raw)
	if err != nil {
		return nil, err
	}

	// JSON → protobuf
	var spec flowpipev1.FlowSpec
	if err := protojson.Unmarshal(jsonBytes, &spec); err != nil {
		return nil, fmt.Errorf("decode flow spec: %w", err)
	}

	return &spec, nil
}

// yamlToJSON converts YAML-parsed data into canonical JSON bytes.
func yamlToJSON(v any) ([]byte, error) {
	return json.Marshal(v)
}
