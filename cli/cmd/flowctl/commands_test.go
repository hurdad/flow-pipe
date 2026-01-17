package main

import (
	"context"
	"net"
	"testing"
	"time"

	flowpipev1 "github.com/hurdad/flow-pipe/gen/go/flowpipe/v1"
	"google.golang.org/grpc"
)

type testFlowService struct {
	flowpipev1.UnimplementedFlowServiceServer
	getReq      *flowpipev1.GetFlowRequest
	listCalled  bool
	statusReq   *flowpipev1.GetFlowStatusRequest
	rollbackReq *flowpipev1.RollbackFlowRequest
}

func (s *testFlowService) GetFlow(ctx context.Context, req *flowpipev1.GetFlowRequest) (*flowpipev1.Flow, error) {
	s.getReq = req
	return &flowpipev1.Flow{Name: req.Name, Version: 1}, nil
}

func (s *testFlowService) ListFlows(ctx context.Context, req *flowpipev1.ListFlowsRequest) (*flowpipev1.ListFlowsResponse, error) {
	s.listCalled = true
	return &flowpipev1.ListFlowsResponse{
		Flows: []*flowpipev1.FlowSummary{
			{
				Name:   "flow-a",
				Status: &flowpipev1.FlowStatus{State: flowpipev1.FlowState_FLOW_STATE_RUNNING},
			},
		},
	}, nil
}

func (s *testFlowService) GetFlowStatus(ctx context.Context, req *flowpipev1.GetFlowStatusRequest) (*flowpipev1.FlowStatus, error) {
	s.statusReq = req
	return &flowpipev1.FlowStatus{State: flowpipev1.FlowState_FLOW_STATE_RUNNING}, nil
}

func (s *testFlowService) RollbackFlow(ctx context.Context, req *flowpipev1.RollbackFlowRequest) (*flowpipev1.Flow, error) {
	s.rollbackReq = req
	return &flowpipev1.Flow{Name: req.Name, Version: req.Version}, nil
}

func startFlowService(t *testing.T, svc *testFlowService) (string, func()) {
	t.Helper()

	listener, err := net.Listen("tcp", "127.0.0.1:0")
	if err != nil {
		t.Fatalf("listen: %v", err)
	}

	server := grpc.NewServer()
	flowpipev1.RegisterFlowServiceServer(server, svc)

	go func() {
		_ = server.Serve(listener)
	}()

	cleanup := func() {
		server.Stop()
		_ = listener.Close()
	}

	return listener.Addr().String(), cleanup
}

func TestGetCommand(t *testing.T) {
	svc := &testFlowService{}
	addr, cleanup := startFlowService(t, svc)
	defer cleanup()

	getAPIAddr = addr
	getTimeout = time.Second

	if err := getCmd.RunE(getCmd, []string{"flow-one"}); err != nil {
		t.Fatalf("getCmd.RunE: %v", err)
	}

	if svc.getReq == nil || svc.getReq.Name != "flow-one" {
		t.Fatalf("expected get request for flow-one")
	}
}

func TestListCommand(t *testing.T) {
	svc := &testFlowService{}
	addr, cleanup := startFlowService(t, svc)
	defer cleanup()

	listAPIAddr = addr
	listTimeout = time.Second

	if err := listCmd.RunE(listCmd, []string{}); err != nil {
		t.Fatalf("listCmd.RunE: %v", err)
	}

	if !svc.listCalled {
		t.Fatalf("expected list flows call")
	}
}

func TestStatusCommand(t *testing.T) {
	svc := &testFlowService{}
	addr, cleanup := startFlowService(t, svc)
	defer cleanup()

	statusAPIAddr = addr
	statusTimeout = time.Second

	if err := statusCmd.RunE(statusCmd, []string{"flow-two"}); err != nil {
		t.Fatalf("statusCmd.RunE: %v", err)
	}

	if svc.statusReq == nil || svc.statusReq.Name != "flow-two" {
		t.Fatalf("expected status request for flow-two")
	}
}

func TestRollbackCommand(t *testing.T) {
	svc := &testFlowService{}
	addr, cleanup := startFlowService(t, svc)
	defer cleanup()

	rollbackAPIAddr = addr
	rollbackTimeout = time.Second
	rollbackVersion = 3

	if err := rollbackCmd.RunE(rollbackCmd, []string{"flow-three"}); err != nil {
		t.Fatalf("rollbackCmd.RunE: %v", err)
	}

	if svc.rollbackReq == nil || svc.rollbackReq.Name != "flow-three" || svc.rollbackReq.Version != 3 {
		t.Fatalf("expected rollback request for flow-three version 3")
	}
}
