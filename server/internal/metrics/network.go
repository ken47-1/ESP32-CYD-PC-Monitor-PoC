package metrics

import (
	"log"
	"pcmetrics-server/internal/config"
	"sync/atomic"
	"time"

	probing "github.com/prometheus-community/pro-bing"
	"github.com/yusufpapurcu/wmi"
)

var (
	netRunning    atomic.Bool
	netType       atomic.Value
	netPingMs     atomic.Int32
	netJitterMs   atomic.Int32
	netPacketLoss atomic.Int32
	netDone       chan struct{}
)

func init() {
	netType.Store("Unknown")
	netPingMs.Store(-1)
	netJitterMs.Store(-1)
	netPacketLoss.Store(-1)
}

// WMI struct for Windows network adapters
type WinAdapter struct {
	Name                string
	NetConnectionStatus *uint16
	AdapterType         *string
	PhysicalAdapter     bool
}

// GetActiveNetworkType queries WMI for active Ethernet/Wi-Fi adapters
func GetActiveNetworkType() string {
	var adapters []WinAdapter
	err := wmi.Query("SELECT Name, NetConnectionStatus, AdapterType, PhysicalAdapter FROM Win32_NetworkAdapter WHERE NetEnabled=true", &adapters)
	if err != nil {
		log.Println("[NET] WMI query failed:", err)
		return "Unknown"
	}

	for _, ad := range adapters {
		if ad.NetConnectionStatus == nil || !ad.PhysicalAdapter {
			continue
		}

		if ad.AdapterType != nil && *ad.AdapterType == "Ethernet 802.3" {
			return "Ethernet"
		}
		if ad.AdapterType != nil && *ad.AdapterType == "Wireless" {
			return "Wi-Fi"
		}
	}

	return "Unknown"
}

// Start network metrics collection
func StartNetworkMetrics() {
	if netRunning.Load() {
		return
	}

	netRunning.Store(true)
	netDone = make(chan struct{})

	go func() {
		defer close(netDone)
		defer netRunning.Store(false)

		for netRunning.Load() {
			cfg := config.Get()
			host := cfg.Network.Host
			if host == "" {
				host = "8.8.8.8"
			}

			pinger, err := probing.NewPinger(host)
			if err != nil {
				log.Println("[NET] Pinger error:", err)
				time.Sleep(2 * time.Second)
				continue
			}

			pinger.SetPrivileged(true)
			pinger.Interval = 200 * time.Millisecond
			pinger.Count = 10

			if err := pinger.Run(); err != nil {
				log.Println("[NET] Ping failed:", err)
				time.Sleep(1 * time.Second)
				continue
			}

			stats := pinger.Statistics()
			netType.Store(GetActiveNetworkType())
			netPingMs.Store(int32(stats.AvgRtt.Milliseconds()))
			netJitterMs.Store(int32(stats.StdDevRtt.Milliseconds()))
			netPacketLoss.Store(int32(stats.PacketLoss))

			// dynamic interval
			sleepInterval := time.Second
			if cfg.Metrics.IntervalMs > 0 {
				sleepInterval = time.Duration(cfg.Metrics.IntervalMs) * time.Millisecond
			}
			time.Sleep(sleepInterval)
		}
	}()
}

// Stop network metrics collection
func StopNetworkMetrics() {
	if !netRunning.Load() {
		return
	}

	netRunning.Store(false)
	if netDone != nil {
		<-netDone // wait for network goroutine to exit
	}

	// reset stats
	netType.Store("Unknown")
	netPingMs.Store(-1)
	netJitterMs.Store(-1)
	netPacketLoss.Store(-1)
}
