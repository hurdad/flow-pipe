package main

import (
	"context"
	"log"
	"net"
	"net/http"

	"github.com/grpc-ecosystem/grpc-gateway/v2/runtime"
	"github.com/hurdad/flow-pipe/api/server"
	flowpipev1 "github.com/hurdad/flow-pipe/gen/go/flowpipe/v1"
	"google.golang.org/grpc"
)

const (
	grpcAddr = ":9090"
	httpAddr = ":8080"
)

func main() {
	store := server.NewStore()
	flowServer := server.NewFlowServer(store)

	// -----------------------------
	// gRPC server
	// -----------------------------
	grpcLis, err := net.Listen("tcp", grpcAddr)
	if err != nil {
		log.Fatalf("failed to listen: %v", err)
	}

	grpcServer := grpc.NewServer()
	flowpipev1.RegisterFlowServiceServer(grpcServer, flowServer)

	go func() {
		log.Printf("gRPC listening on %s", grpcAddr)
		if err := grpcServer.Serve(grpcLis); err != nil {
			log.Fatal(err)
		}
	}()

	// -----------------------------
	// REST gateway
	// -----------------------------
	ctx := context.Background()
	mux := runtime.NewServeMux()

	err = flowpipev1.RegisterFlowServiceHandlerFromEndpoint(
		ctx,
		mux,
		grpcAddr,
		[]grpc.DialOption{grpc.WithInsecure()},
	)
	if err != nil {
		log.Fatalf("failed to register gateway: %v", err)
	}

	log.Printf("HTTP REST listening on %s", httpAddr)
	log.Fatal(http.ListenAndServe(httpAddr, mux))
}
