package main

import (
	"context"
	"fmt"
	"strings"

	"google.golang.org/grpc"
	"google.golang.org/grpc/credentials"
	"google.golang.org/grpc/credentials/insecure"
)

func dialAPI(ctx context.Context, addr string) (*grpc.ClientConn, error) {
	if addr == "" {
		return nil, fmt.Errorf("api address is required")
	}

	if grpcTLSEnabled {
		if strings.TrimSpace(grpcTLSCertFile) == "" {
			return nil, fmt.Errorf("grpc tls enabled but cert file is empty")
		}
		if strings.TrimSpace(grpcTLSServerName) == "" {
			return nil, fmt.Errorf("grpc tls enabled but server name is empty")
		}
	}

	opts := []grpc.DialOption{
		grpc.WithBlock(),
	}
	if grpcTLSEnabled {
		creds, err := credentials.NewClientTLSFromFile(grpcTLSCertFile, grpcTLSServerName)
		if err != nil {
			return nil, fmt.Errorf("grpc tls config: %w", err)
		}
		opts = append(opts, grpc.WithTransportCredentials(creds))
	} else {
		opts = append(opts, grpc.WithTransportCredentials(insecure.NewCredentials()))
	}

	if strings.TrimSpace(apiKey) != "" {
		opts = append(opts, grpc.WithPerRPCCredentials(apiKeyCredentials{
			token:                    formatBearerToken(apiKey),
			requireTransportSecurity: grpcTLSEnabled,
		}))
	}

	conn, err := grpc.DialContext(
		ctx,
		addr,
		opts...,
	)
	if err != nil {
		return nil, fmt.Errorf("connect to api %s: %w", addr, err)
	}

	return conn, nil
}

type apiKeyCredentials struct {
	token                    string
	requireTransportSecurity bool
}

func (c apiKeyCredentials) GetRequestMetadata(ctx context.Context, uri ...string) (map[string]string, error) {
	if c.token == "" {
		return nil, nil
	}
	return map[string]string{"authorization": c.token}, nil
}

func (c apiKeyCredentials) RequireTransportSecurity() bool {
	return c.requireTransportSecurity
}

func formatBearerToken(token string) string {
	value := strings.TrimSpace(token)
	if value == "" {
		return ""
	}
	if strings.HasPrefix(strings.ToLower(value), "bearer ") {
		return value
	}
	return fmt.Sprintf("Bearer %s", value)
}
