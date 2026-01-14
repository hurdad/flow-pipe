package controller

import "time"

type WorkQueue interface {
	Add(key string)
	Get() (string, time.Duration, bool)
	Forget(key string)
	Retry(key string)
	Len() int
	Shutdown()
}
