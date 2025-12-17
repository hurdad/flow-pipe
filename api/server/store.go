package server

import (
	"sync"

	flowpipev1 "github.com/hurdad/flow-pipe/gen/go/flowpipe/v1"
)

type Store struct {
	mu    sync.RWMutex
	flows map[string]*flowpipev1.Flow
}

func NewStore() *Store {
	return &Store{
		flows: make(map[string]*flowpipev1.Flow),
	}
}
