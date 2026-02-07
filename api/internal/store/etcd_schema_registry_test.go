package store

import (
	"context"
	"fmt"
	"net"
	"net/url"
	"testing"
	"time"

	"github.com/hurdad/flow-pipe/api/internal/model"
	flowpipev1 "github.com/hurdad/flow-pipe/gen/go/flowpipe/v1"
	clientv3 "go.etcd.io/etcd/client/v3"
	"go.etcd.io/etcd/server/v3/embed"
)

func TestSchemaRegistryLifecycle(t *testing.T) {
	clientURL, peerURL := newEtcdURLs(t)
	etcd := startEmbeddedEtcd(t, clientURL, peerURL)
	defer etcd.Close()

	cli, err := clientv3.New(clientv3.Config{
		Endpoints:   []string{clientURL.String()},
		DialTimeout: 5 * time.Second,
	})
	if err != nil {
		t.Fatalf("create etcd client: %v", err)
	}
	defer cli.Close()

	store := &EtcdStore{cli: cli}
	ctx := context.Background()

	first, err := store.CreateSchema(ctx, &model.SchemaDefinition{
		SchemaID:  "orders",
		Format:    flowpipev1.InMemorySchemaFormat_IN_MEMORY_SCHEMA_FORMAT_JSON,
		RawSchema: []byte(`{"type":"object"}`),
	})
	if err != nil {
		t.Fatalf("create schema v1: %v", err)
	}
	if first.Version != 1 {
		t.Fatalf("expected version 1, got %d", first.Version)
	}

	second, err := store.CreateSchema(ctx, &model.SchemaDefinition{
		SchemaID:  "orders",
		Format:    flowpipev1.InMemorySchemaFormat_IN_MEMORY_SCHEMA_FORMAT_JSON,
		RawSchema: []byte(`{"type":"object","version":2}`),
	})
	if err != nil {
		t.Fatalf("create schema v2: %v", err)
	}
	if second.Version != 2 {
		t.Fatalf("expected version 2, got %d", second.Version)
	}

	active, err := store.GetSchema(ctx, "orders", 0)
	if err != nil {
		t.Fatalf("get active schema: %v", err)
	}
	if active.Version != 2 {
		t.Fatalf("expected active version 2, got %d", active.Version)
	}

	versions, err := store.ListSchemaVersions(ctx, "orders")
	if err != nil {
		t.Fatalf("list schema versions: %v", err)
	}
	if len(versions) != 2 {
		t.Fatalf("expected 2 versions, got %d", len(versions))
	}
	if versions[0].Version != 1 || versions[1].Version != 2 {
		t.Fatalf("expected sorted versions [1,2], got [%d,%d]", versions[0].Version, versions[1].Version)
	}

	if err := store.DeleteSchema(ctx, "orders"); err != nil {
		t.Fatalf("delete schema: %v", err)
	}

	_, err = store.GetSchema(ctx, "orders", 0)
	if err == nil {
		t.Fatalf("expected error after delete")
	}
}

func startEmbeddedEtcd(t *testing.T, clientURL, peerURL url.URL) *embed.Etcd {
	t.Helper()

	cfg := embed.NewConfig()
	cfg.Dir = t.TempDir()
	cfg.LogLevel = "error"
	cfg.ListenClientUrls = []url.URL{clientURL}
	cfg.AdvertiseClientUrls = []url.URL{clientURL}
	cfg.ListenPeerUrls = []url.URL{peerURL}
	cfg.AdvertisePeerUrls = []url.URL{peerURL}
	cfg.InitialCluster = fmt.Sprintf("%s=%s", cfg.Name, peerURL.String())

	etcd, err := embed.StartEtcd(cfg)
	if err != nil {
		t.Fatalf("start embedded etcd: %v", err)
	}

	select {
	case <-etcd.Server.ReadyNotify():
	case <-time.After(10 * time.Second):
		etcd.Server.Stop()
		t.Fatalf("embedded etcd not ready")
	}

	return etcd
}

func newEtcdURLs(t *testing.T) (url.URL, url.URL) {
	t.Helper()

	clientPort := freePort(t)
	peerPort := freePort(t)

	clientURL := url.URL{Scheme: "http", Host: net.JoinHostPort("127.0.0.1", clientPort)}
	peerURL := url.URL{Scheme: "http", Host: net.JoinHostPort("127.0.0.1", peerPort)}

	return clientURL, peerURL
}

func freePort(t *testing.T) string {
	t.Helper()

	listener, err := net.Listen("tcp", "127.0.0.1:0")
	if err != nil {
		t.Fatalf("listen: %v", err)
	}
	defer listener.Close()

	_, port, err := net.SplitHostPort(listener.Addr().String())
	if err != nil {
		t.Fatalf("split host port: %v", err)
	}

	return port
}
