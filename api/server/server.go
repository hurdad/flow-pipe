package server

import (
	"context"

	flowpipev1 "github.com/hurdad/flow-pipe/gen/go/flowpipe/v1"
	//"google.golang.org/protobuf/types/known/timestamppb"
)

type FlowServer struct {
	flowpipev1.UnimplementedFlowServiceServer
	store *Store
}

func NewFlowServer(store *Store) *FlowServer {
	return &FlowServer{store: store}
}

func (s *FlowServer) CreateFlow(
	ctx context.Context,
	req *flowpipev1.CreateFlowRequest,
) (*flowpipev1.Flow, error) {

	spec := req.Spec
	spec.Version = 1
//
// 	flow := &flowpipev1.Flow{
// 		Name:    spec.Name,
// 		Version: spec.Version,
// 		Spec:    spec,
// 		Status: &flowpipev1.FlowStatus{
// 			State:         flowpipev1.FlowState_FLOW_STATE_PENDING,
// 			ActiveVersion: spec.Version,
// 			LastUpdated:   timestamppb.Now(),
// 		},
// 	}
//
// 	s.store.mu.Lock()
// 	defer s.store.mu.Unlock()
//
// 	s.store.flows[spec.Name] = flow
 	return nil, nil
}

func (s *FlowServer) GetFlow(
	ctx context.Context,
	req *flowpipev1.GetFlowRequest,
) (*flowpipev1.Flow, error) {

	s.store.mu.RLock()
	defer s.store.mu.RUnlock()

	return s.store.flows[req.Name], nil
}

func (s *FlowServer) ListFlows(
	ctx context.Context,
	_ *flowpipev1.ListFlowsRequest,
) (*flowpipev1.ListFlowsResponse, error) {

	s.store.mu.RLock()
	defer s.store.mu.RUnlock()

	resp := &flowpipev1.ListFlowsResponse{}
	for _, f := range s.store.flows {
		resp.Flows = append(resp.Flows, f)
	}
	return resp, nil
}
