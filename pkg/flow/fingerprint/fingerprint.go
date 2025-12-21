package fingerprint

import (
	"crypto/sha256"
	"encoding/hex"

	"google.golang.org/protobuf/proto"
)

// Fingerprint returns a stable hash of a protobuf message.
// The message MUST be normalized first.
func Fingerprint(msg proto.Message) string {
	b, _ := proto.Marshal(msg)
	sum := sha256.Sum256(b)
	return hex.EncodeToString(sum[:])
}
