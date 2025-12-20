package flow

import (
	"context"

	"github.com/hurdad/flow-pipe/api/internal/store"
	flowpipev1 "github.com/hurdad/flow-pipe/gen/go/flowpipe/v1"
	"google.golang.org/protobuf/types/known/emptypb"
)

type FlowServer struct {
	flowpipev1.UnimplementedFlowServiceServer
	store store.Store
}

func NewFlowServer(store store.Store) *FlowServer {
	return &FlowServer{store: store}
}

func (s *FlowServer) CreateFlow(ctx context.Context, r *flowpipev1.CreateFlowRequest) (*flowpipev1.Flow, error) {
	return s.store.CreateFlow(ctx, r.Spec)
}

func (s *FlowServer) UpdateFlow(ctx context.Context, r *flowpipev1.UpdateFlowRequest) (*flowpipev1.Flow, error) {
	return s.store.UpdateFlow(ctx, r.Name, r.Spec)
}

func (s *FlowServer) GetFlow(ctx context.Context, r *flowpipev1.GetFlowRequest) (*flowpipev1.Flow, error) {
	return s.store.GetFlow(ctx, r.Name)
}

func (s *FlowServer) ListFlows(ctx context.Context, _ *flowpipev1.ListFlowsRequest) (*flowpipev1.ListFlowsResponse, error) {
	flows, err := s.store.ListFlows(ctx)
	if err != nil {
		return nil, err
	}
	return &flowpipev1.ListFlowsResponse{Flows: flows}, nil
}

func (s *FlowServer) DeleteFlow(ctx context.Context, r *flowpipev1.DeleteFlowRequest) (*emptypb.Empty, error) {
	return &emptypb.Empty{}, s.store.DeleteFlow(ctx, r.Name)
}

func (s *FlowServer) GetFlowStatus(ctx context.Context, r *flowpipev1.GetFlowStatusRequest) (*flowpipev1.FlowStatus, error) {
	return s.store.GetFlowStatus(ctx, r.Name)
}

func (s *FlowServer) RollbackFlow(ctx context.Context, r *flowpipev1.RollbackFlowRequest) (*flowpipev1.Flow, error) {
	return s.store.RollbackFlow(ctx, r.Name, r.Version)
}
