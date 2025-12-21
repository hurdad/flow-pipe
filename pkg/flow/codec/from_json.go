package codec

import (
	"google.golang.org/protobuf/encoding/protojson"
	"google.golang.org/protobuf/proto"
)

func FromJSON(data []byte, msg proto.Message) error {
	return protojson.Unmarshal(data, msg)
}
