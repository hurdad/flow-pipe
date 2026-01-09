package service

import (
	"context"
	"log/slog"

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
	spec := r.GetSpec()
	flowName := ""
	if spec != nil {
		flowName = spec.GetName()
	}
	slog.DebugContext(ctx, "create flow request", slog.String("flow_name", flowName))
	return s.store.CreateFlow(ctx, r.Spec)
}

func (s *FlowServer) UpdateFlow(ctx context.Context, r *flowpipev1.UpdateFlowRequest) (*flowpipev1.Flow, error) {
	spec := r.GetSpec()
	specVersion := uint64(0)
	if spec != nil {
		specVersion = spec.GetVersion()
	}
	slog.DebugContext(ctx, "update flow request", slog.String("flow_name", r.GetName()), slog.Uint64("spec_version", specVersion))
	return s.store.UpdateFlow(ctx, r.Name, r.Spec)
}

func (s *FlowServer) GetFlow(ctx context.Context, r *flowpipev1.GetFlowRequest) (*flowpipev1.Flow, error) {
	slog.DebugContext(ctx, "get flow request", slog.String("flow_name", r.GetName()))
	return s.store.GetFlow(ctx, r.Name)
}

func (s *FlowServer) ListFlows(ctx context.Context, _ *flowpipev1.ListFlowsRequest) (*flowpipev1.ListFlowsResponse, error) {
	slog.DebugContext(ctx, "list flows request")
	flows, err := s.store.ListFlows(ctx)
	if err != nil {
		return nil, err
	}
	return &flowpipev1.ListFlowsResponse{Flows: flows}, nil
}

func (s *FlowServer) DeleteFlow(ctx context.Context, r *flowpipev1.DeleteFlowRequest) (*emptypb.Empty, error) {
	slog.DebugContext(ctx, "delete flow request", slog.String("flow_name", r.GetName()))
	return &emptypb.Empty{}, s.store.DeleteFlow(ctx, r.Name)
}

func (s *FlowServer) GetFlowStatus(ctx context.Context, r *flowpipev1.GetFlowStatusRequest) (*flowpipev1.FlowStatus, error) {
	slog.DebugContext(ctx, "get flow status request", slog.String("flow_name", r.GetName()))
	return s.store.GetFlowStatus(ctx, r.Name)
}

func (s *FlowServer) RollbackFlow(ctx context.Context, r *flowpipev1.RollbackFlowRequest) (*flowpipev1.Flow, error) {
	slog.DebugContext(ctx, "rollback flow request", slog.String("flow_name", r.GetName()), slog.Uint64("version", r.GetVersion()))
	return s.store.RollbackFlow(ctx, r.Name, r.Version)
}
