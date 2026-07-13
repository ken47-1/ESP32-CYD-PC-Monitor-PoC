package control

import (
	"log"
	"pcmetrics-server/internal/metrics"
	"pcmetrics-server/internal/server"
	"sync"
	"sync/atomic"
)

var (
	metricsLoop  = &metrics.Loop{}
	mu           sync.Mutex
	stopCh       chan struct{}
	shuttingDown atomic.Bool
)

func StartAll() {
	mu.Lock()
	defer mu.Unlock()

	if server.IsRunning() {
		return
	}

	shuttingDown.Store(false)    // reset shutdown flag
	stopCh = make(chan struct{}) // reset stop channel

	server.Start()
	metrics.StartNetworkMetrics()

	metricsLoop.Start(func() {
		if shuttingDown.Load() || !server.IsRunning() {
			return
		}

		stats, err := metrics.GetMetrics()
		if err != nil {
			log.Println("[ERROR] Metrics error:", err)
			return
		}
		server.Send(stats)
	})
}

func StopAll() {
	mu.Lock()
	defer mu.Unlock()

	shuttingDown.Store(true) // mark shutdown
	close(stopCh)

	metrics.StopNetworkMetrics()
	metricsLoop.Stop()
	server.Stop()
}

func RestartAll() {
	mu.Lock()
	defer mu.Unlock()

	// shutdown
	shuttingDown.Store(true)
	close(stopCh)
	metrics.StopNetworkMetrics()
	metricsLoop.Stop()
	server.Stop()

	// restart
	shuttingDown.Store(false)
	stopCh = make(chan struct{})
	server.Start()
	metrics.StartNetworkMetrics()

	metricsLoop.Start(func() {
		if shuttingDown.Load() || !server.IsRunning() {
			return
		}
		stats, err := metrics.GetMetrics()
		if err != nil {
			log.Println("[ERROR] Metrics error:", err)
			return
		}
		server.Send(stats)
	})
}
