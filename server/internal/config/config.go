package config

import (
	"encoding/json"
	"log"
	"os"
	"sync/atomic"
)

type SerialConfig struct {
	Port     string `json:"port"`
	BaudRate int    `json:"baud_rate"`
	Debug    bool   `json:"debug"`
}

type NetworkConfig struct {
	Host string `json:"host"`
}

type MetricsConfig struct {
	IntervalMs int `json:"interval_ms"`
}

type TrayConfig struct {
	PollIntervalMs int `json:"poll_interval_ms"`
}

type SensorsConfig struct {
	CPULoadName  string `json:"cpu_load_name"`
	CPUTempName  string `json:"cpu_temp_name"`
	GPULoadName  string `json:"gpu_load_name"`
	GPUTempName  string `json:"gpu_temp_name"`
	RAMUsedName  string `json:"ram_used_name"`
	RAMAvailName string `json:"ram_avail_name"`
}

type Config struct {
	Serial  SerialConfig  `json:"serial"`
	Network NetworkConfig `json:"network"`
	Metrics MetricsConfig `json:"metrics"`
	Tray    TrayConfig    `json:"tray"`
	Sensors SensorsConfig `json:"sensors"`
}

// atomic storage for config
var configValue atomic.Value

func Get() Config {
	if v := configValue.Load(); v != nil {
		return v.(Config)
	}
	return Config{}
}

func Load() error {
	if _, err := os.Stat("config.json"); os.IsNotExist(err) {
		defaultConfig := Config{
			Serial: SerialConfig{
				Port:     "AUTO",
				BaudRate: 115200,
				Debug:    false,
			},
			Network: NetworkConfig{
				Host: "8.8.8.8",
			},
			Metrics: MetricsConfig{
				IntervalMs: 500,
			},
			Tray: TrayConfig{
				PollIntervalMs: 500,
			},
			Sensors: SensorsConfig{
				CPULoadName:  "CPU Total",
				CPUTempName:  "CPU Package",
				GPULoadName:  "GPU Core",
				GPUTempName:  "GPU Core",
				RAMUsedName:  "Memory Used",
				RAMAvailName: "Memory Available",
			},
		}

		data, _ := json.MarshalIndent(defaultConfig, "", "  ")
		_ = os.WriteFile("config.json", data, 0644)
		configValue.Store(defaultConfig)
		log.Println("[CONFIG] Created default config.json")
		return nil
	}

	data, err := os.ReadFile("config.json")
	if err != nil {
		return err
	}

	var cfg Config
	if err := json.Unmarshal(data, &cfg); err != nil {
		return err
	}

	configValue.Store(cfg)
	log.Println("[CONFIG] Config loaded")
	return nil
}
