package main

import (
	"context"
	"fmt"
	"strings"

	"google.golang.org/grpc"
	"google.golang.org/grpc/credentials/insecure"
)

func dialAPI(ctx context.Context, addr string) (*grpc.ClientConn, error) {
	if addr == "" {
		return nil, fmt.Errorf("api address is required")
	}

	opts := []grpc.DialOption{
		grpc.WithTransportCredentials(insecure.NewCredentials()),
		grpc.WithBlock(),
	}

	if strings.TrimSpace(apiKey) != "" {
		opts = append(opts, grpc.WithPerRPCCredentials(apiKeyCredentials{
			token: formatBearerToken(apiKey),
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
	token string
}

func (c apiKeyCredentials) GetRequestMetadata(ctx context.Context, uri ...string) (map[string]string, error) {
	if c.token == "" {
		return nil, nil
	}
	return map[string]string{"authorization": c.token}, nil
}

func (c apiKeyCredentials) RequireTransportSecurity() bool {
	return false
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
