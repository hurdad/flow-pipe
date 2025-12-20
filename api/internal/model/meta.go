package model

import "time"

type FlowMeta struct {
	UID       string            `json:"uid"`
	CreatedAt time.Time         `json:"created_at"`
	Labels    map[string]string `json:"labels,omitempty"`
	Deleted   bool              `json:"deleted,omitempty"`
}
