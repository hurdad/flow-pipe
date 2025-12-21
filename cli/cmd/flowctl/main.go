package main

import (
	"github.com/hurdad/flow-pipe/cli/internal"
)

func main() {
	if err := internal.Execute(); err != nil {
		// Cobra already prints errors
		// Non-zero exit for CI / scripting
		panic(err)
	}
}
