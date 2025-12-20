package store

import "fmt"

func FlowRoot(name string) string {
	return fmt.Sprintf("/flowpipe/flows/%s", name)
}

func FlowMetaKey(name string) string {
	return fmt.Sprintf("%s/meta", FlowRoot(name))
}

func FlowActiveKey(name string) string {
	return fmt.Sprintf("%s/active", FlowRoot(name))
}

func FlowStatusKey(name string) string {
	return fmt.Sprintf("%s/status", FlowRoot(name))
}

func FlowVersionSpecKey(name string, version uint64) string {
	return fmt.Sprintf("%s/versions/%d/spec", FlowRoot(name), version)
}

func FlowVersionsPrefix(name string) string {
	return fmt.Sprintf("%s/versions/", FlowRoot(name))
}
