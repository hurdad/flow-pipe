package controller

type WorkQueue interface {
	Add(key string)
	Get() (string, bool)
	Forget(key string)
	Retry(key string)
	Shutdown()
}
