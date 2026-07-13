package metrics

import (
	"math"
	"pcmetrics-server/internal/config"
	"strings"

	"github.com/shirou/gopsutil/v4/mem"
	"github.com/yusufpapurcu/wmi"
)

type Sensor struct {
	Identifier string
	Name       string
	SensorType string
	Value      *float32
}

type NetworkStats struct {
	NetworkType string  `json:"network_type"`
	Ping        int     `json:"ping"`
	Jitter      float32 `json:"jitter"`
	PacketLoss  float32 `json:"packet_loss"`
}

type CPUStats struct {
	Load int `json:"load"`
	Temp int `json:"temp"`
}

type GPUStats struct {
	Load int `json:"load"`
	Temp int `json:"temp"`
}

type RAMStats struct {
	UsedGB  float32 `json:"used_gb"`
	TotalGB float32 `json:"total_gb"`
	Percent int     `json:"percent"`
}

type Stats struct {
	Network NetworkStats `json:"network"`
	CPU     CPUStats     `json:"cpu"`
	GPU     GPUStats     `json:"gpu"`
	RAM     RAMStats     `json:"ram"`
}

func round1(f float32) float32 {
	return float32(math.Round(float64(f)*10)) / 10
}

func metricInt(v float32) int {
	if v < 0 {
		return -1 // N/A sentinel
	}
	return int(math.Round(float64(v)))
}

func querySensors() ([]Sensor, error) {
	var dst []Sensor
	q := wmi.CreateQuery(&dst, "WHERE Value IS NOT NULL")
	return dst, wmi.QueryNamespace(q, &dst, "root\\LibreHardwareMonitor")
}

func contains(a, b string) bool {
	return strings.Contains(strings.ToLower(a), strings.ToLower(b))
}

// Get Network stats
func getNetworkStats() NetworkStats {
	nt := netType.Load()
	ntStr := ""
	if nt != nil {
		ntStr = nt.(string)
	}

	return NetworkStats{
		NetworkType: ntStr,
		Ping:        int(netPingMs.Load()),
		Jitter:      float32(netJitterMs.Load()),
		PacketLoss:  float32(netPacketLoss.Load()),
	}
}

// Get Metrics from Sensors
func getMetric(s []Sensor, t, partial string) float32 {
	for _, sens := range s {
		if sens.SensorType == t && contains(sens.Name, partial) {
			if sens.Value != nil {
				return *sens.Value
			}
			return -1
		}
	}
	return -1
}

// Return metrics
// RAM used comes from LHM, total comes from gopsutil for accuracy.
func GetMetrics() (Stats, error) {
	sensors, err := querySensors()
	if err != nil {
		return Stats{}, err
	}

	cfg := config.Get()

	// Memory used from LHM
	ramUsed := round1(getMetric(sensors, "Data", cfg.Sensors.RAMUsedName))

	// Total memory from gopsutil
	vm, err := mem.VirtualMemory()
	var ramTotal float32
	if err == nil {
		ramTotal = round1(float32(vm.Total) / (1024 * 1024 * 1024)) // bytes -> GB
	} else {
		// fallback: estimate total from used + available if gopsutil fails
		ramAvail := round1(getMetric(sensors, "Data", cfg.Sensors.RAMAvailName))
		ramTotal = ramUsed + ramAvail
	}

	// RAM percent
	percent := -1
	if ramUsed >= 0 && ramTotal > 0 {
		percent = int(math.Round(float64(ramUsed / ramTotal * 100)))
	}

	return Stats{
		/*
			Network: NetworkStats{
				NetworkType: "Ethernet",
				Ping:        500,
				Jitter:      150,
				PacketLoss:  100,
			},
		*/
		Network: getNetworkStats(),
		CPU: CPUStats{
			Load: metricInt(getMetric(sensors, "Load", cfg.Sensors.CPULoadName)),
			Temp: metricInt(getMetric(sensors, "Temperature", cfg.Sensors.CPUTempName)),
		},
		GPU: GPUStats{
			Load: metricInt(getMetric(sensors, "Load", cfg.Sensors.GPULoadName)),
			Temp: metricInt(getMetric(sensors, "Temperature", cfg.Sensors.GPUTempName)),
		},
		RAM: RAMStats{
			UsedGB:  ramUsed,
			TotalGB: ramTotal,
			Percent: percent,
		},
	}, nil
}
