package server

import (
	"context"
	"fmt"
	"net/http"
	"strings"

	"google.golang.org/grpc"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/metadata"
	"google.golang.org/grpc/status"

	"github.com/hurdad/flow-pipe/api/internal/config"
)

const bearerPrefix = "bearer "

func ensureAPIKeyConfigured(cfg config.Config) error {
	if !cfg.AuthEnabled {
		return nil
	}
	if strings.TrimSpace(cfg.APIKey) == "" {
		return fmt.Errorf("api auth enabled but api key is empty")
	}
	return nil
}

func ensureHTTPServerTLSConfigured(cfg config.Config) error {
	if !cfg.HTTPTLSEnabled {
		return nil
	}
	if strings.TrimSpace(cfg.HTTPTLSCertFile) == "" {
		return fmt.Errorf("http tls enabled but cert file is empty")
	}
	if strings.TrimSpace(cfg.HTTPTLSKeyFile) == "" {
		return fmt.Errorf("http tls enabled but key file is empty")
	}
	return nil
}

func ensureGRPCServerTLSConfigured(cfg config.Config) error {
	if !cfg.GRPCTLSEnabled {
		return nil
	}
	if strings.TrimSpace(cfg.GRPCTLSCertFile) == "" {
		return fmt.Errorf("grpc tls enabled but cert file is empty")
	}
	if strings.TrimSpace(cfg.GRPCTLSKeyFile) == "" {
		return fmt.Errorf("grpc tls enabled but key file is empty")
	}
	if strings.TrimSpace(cfg.GRPCTLSServerName) == "" {
		return fmt.Errorf("grpc tls enabled but server name is empty")
	}
	return nil
}

func apiKeyUnaryInterceptor(cfg config.Config) grpc.UnaryServerInterceptor {
	if !cfg.AuthEnabled {
		return func(ctx context.Context, req interface{}, info *grpc.UnaryServerInfo, handler grpc.UnaryHandler) (interface{}, error) {
			return handler(ctx, req)
		}
	}

	return func(ctx context.Context, req interface{}, info *grpc.UnaryServerInfo, handler grpc.UnaryHandler) (interface{}, error) {
		md, _ := metadata.FromIncomingContext(ctx)
		token := firstBearerToken(md.Get("authorization"))
		if token == "" {
			return nil, status.Error(codes.Unauthenticated, "missing api key")
		}
		if token != cfg.APIKey {
			return nil, status.Error(codes.Unauthenticated, "invalid api key")
		}
		return handler(ctx, req)
	}
}

func apiKeyHTTPMiddleware(cfg config.Config, next http.Handler) http.Handler {
	if !cfg.AuthEnabled {
		return next
	}

	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		token := firstBearerToken([]string{r.Header.Get("Authorization")})
		if token == "" || token != cfg.APIKey {
			http.Error(w, "unauthorized", http.StatusUnauthorized)
			return
		}
		next.ServeHTTP(w, r)
	})
}

func firstBearerToken(values []string) string {
	for _, value := range values {
		if token := parseBearerToken(value); token != "" {
			return token
		}
	}
	return ""
}

func parseBearerToken(value string) string {
	value = strings.TrimSpace(value)
	if value == "" {
		return ""
	}
	lower := strings.ToLower(value)
	if !strings.HasPrefix(lower, bearerPrefix) {
		return ""
	}
	token := strings.TrimSpace(value[len(bearerPrefix):])
	return token
}
