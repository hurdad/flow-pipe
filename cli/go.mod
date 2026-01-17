module github.com/hurdad/flow-pipe/cli

go 1.22.0

toolchain go1.22.2

require (
	github.com/hurdad/flow-pipe v0.0.0
	github.com/spf13/cobra v1.8.1
	google.golang.org/grpc v1.71.1
	google.golang.org/protobuf v1.36.5
	gopkg.in/yaml.v3 v3.0.1
)

require (
	github.com/grpc-ecosystem/grpc-gateway/v2 v2.23.0 // indirect
	github.com/inconshreveable/mousetrap v1.1.0 // indirect
	github.com/kr/text v0.2.0 // indirect
	github.com/rogpeppe/go-internal v1.12.0 // indirect
	github.com/spf13/pflag v1.0.5 // indirect
	golang.org/x/net v0.34.0 // indirect
	golang.org/x/sys v0.29.0 // indirect
	golang.org/x/text v0.21.0 // indirect
	google.golang.org/genproto/googleapis/api v0.0.0-20250106144421-5f5ef82da422 // indirect
	google.golang.org/genproto/googleapis/rpc v0.0.0-20250115164207-1a7da9e5054f // indirect
)

replace github.com/hurdad/flow-pipe => ..
