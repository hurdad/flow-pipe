package api

import "time"

// --------------------------------------------------
// Top-level Flow object (desired state)
// --------------------------------------------------

// Flow represents a declarative pipeline definition
// stored in etcd and reconciled by the controller.
type Flow struct {
	Metadata Metadata   `json:"metadata"`
	Spec     FlowSpec   `json:"spec"`
	Status   FlowStatus `json:"status,omitempty"`
}

// --------------------------------------------------
// Metadata
// --------------------------------------------------

type Metadata struct {
	Name      string            `json:"name"`
	Namespace string            `json:"namespace"`
	Labels    map[string]string `json:"labels,omitempty"`
	CreatedAt time.Time         `json:"createdAt"`
}

// --------------------------------------------------
// Spec (desired state)
// --------------------------------------------------

type FlowSpec struct {
	// Stages define the dataflow graph
	Stages []StageSpec `json:"stages"`

	// Queues connect stages
	Queues []QueueSpec `json:"queues,omitempty"`

	// Optional execution mode
	// - "service"  -> Deployment
	// - "job"      -> Kubernetes Job
	Mode ExecutionMode `json:"mode"`

	// Resource defaults (can be overridden per stage)
	Resources ResourceSpec `json:"resources,omitempty"`
}

// --------------------------------------------------
// Stage
// --------------------------------------------------

type StageSpec struct {
	Name string `json:"name"`

	// Implementation type (csv_reader, sqs_listener, etc.)
	Type string `json:"type"`

	// Concurrency model
	Threads int `json:"threads,omitempty"`

	// Optional input/output (sources or sinks may omit)
	Input  *string `json:"input,omitempty"`
	Output *string `json:"output,omitempty"`

	// Arbitrary config passed to the runtime
	Config map[string]string `json:"config,omitempty"`

	// Optional CPU pinning / affinity
	CPUs []int `json:"cpus,omitempty"`

	// Stage-specific resources
	Resources *ResourceSpec `json:"resources,omitempty"`
}

// --------------------------------------------------
// Queues
// --------------------------------------------------

type QueueSpec struct {
	Name     string    `json:"name"`
	Type     QueueType `json:"type"`
	Capacity int       `json:"capacity"`
}

type QueueType string

const (
	QueueMPSC QueueType = "mpsc"
	QueueMPMC QueueType = "mpmc"
)

// --------------------------------------------------
// Resources
// --------------------------------------------------

type ResourceSpec struct {
	CPU    string `json:"cpu,omitempty"`    // e.g. "500m"
	Memory string `json:"memory,omitempty"` // e.g. "1Gi"
}

// --------------------------------------------------
// Execution mode
// --------------------------------------------------

type ExecutionMode string

const (
	ExecutionModeService ExecutionMode = "service"
	ExecutionModeJob     ExecutionMode = "job"
)

// --------------------------------------------------
// Status (observed state)
// --------------------------------------------------

type FlowStatus struct {
	Phase   FlowPhase `json:"phase,omitempty"`
	Message string    `json:"message,omitempty"`
}

type FlowPhase string

const (
	FlowPending   FlowPhase = "Pending"
	FlowRunning   FlowPhase = "Running"
	FlowFailed    FlowPhase = "Failed"
	FlowCompleted FlowPhase = "Completed"
)
