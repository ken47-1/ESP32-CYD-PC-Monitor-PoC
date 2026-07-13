package metrics

import (
	"pcmetrics-server/internal/config"
	"pcmetrics-server/internal/server"
	"sync"
	"time"
)

type Loop struct {
	mu      sync.Mutex
	stop    chan struct{}
	done    chan struct{}
	running bool
}

func (l *Loop) Start(send func()) {
	l.mu.Lock()
	if l.running {
		l.mu.Unlock()
		return
	}

	l.stop = make(chan struct{})
	l.done = make(chan struct{})
	l.running = true
	l.mu.Unlock()

	go func() {
		defer close(l.done)

		for {
			cfg := config.Get()
			interval := 500 * time.Millisecond
			if cfg.Metrics.IntervalMs > 0 {
				interval = time.Duration(cfg.Metrics.IntervalMs) * time.Millisecond
			}

			ticker := time.NewTicker(interval)

			select {
			case <-l.stop:
				ticker.Stop()
				return
			case <-ticker.C:
				ticker.Stop() // stop the current ticker; will recreate next loop

				l.mu.Lock()
				if !l.running {
					l.mu.Unlock()
					return
				}
				l.mu.Unlock()

				if server.IsRunning() {
					send()
				}
			}
		}
	}()
}

func (l *Loop) Stop() {
	l.mu.Lock()
	if !l.running {
		l.mu.Unlock()
		return
	}

	close(l.stop)
	l.running = false
	l.mu.Unlock()

	<-l.done
}
