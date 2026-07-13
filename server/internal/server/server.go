package server

import (
	"encoding/json"
	"log"
	"pcmetrics-server/internal/config"
	"sync"

	"go.bug.st/serial"
	"go.bug.st/serial/enumerator"
)

var (
	port     serial.Port
	running  bool
	serverMu sync.RWMutex
	debugMu  sync.RWMutex
	sentData = false
)

// Start opens the serial port
func Start() {
	serverMu.Lock()
	defer serverMu.Unlock()

	if running {
		log.Println("[SERVER] Server already running")
		return
	}

	cfg := config.Get() // snapshot once
	portName := cfg.Serial.Port
	baud := cfg.Serial.BaudRate
	sentData = cfg.Serial.Debug

	if portName == "AUTO" || portName == "" {
		portName = autoDetectCOM()
		log.Println("[SERVER] Auto-detected COM Port:", portName)
	}

	mode := &serial.Mode{BaudRate: baud}

	p, err := serial.Open(portName, mode)
	if err != nil {
		log.Println("[SERIAL] Failed to open serial:", err)
		return
	}

	port = p
	running = true

	log.Println("[SERIAL] Serial connected")
	log.Println("[SERVER] Server started")
}

// Stop closes the serial port
func Stop() {
	serverMu.Lock()
	defer serverMu.Unlock()

	if !running {
		return
	}

	if port != nil {
		_ = port.Close()
	}
	port = nil
	running = false

	log.Println("[SERIAL] Serial disconnected")
	log.Println("[SERVER] Server stopped")
}

// IsRunning returns whether the server is active
func IsRunning() bool {
	serverMu.RLock()
	defer serverMu.RUnlock()
	return running
}

func SetDebug(debugSentData bool) {
	debugMu.Lock()
	sentData = debugSentData
	debugMu.Unlock()
}

// Send marshals the data and writes to serial
func Send(v interface{}) {
	serverMu.RLock()
	p := port
	isRunning := running && p != nil
	serverMu.RUnlock()

	if !isRunning {
		return
	}

	data, _ := json.Marshal(v)
	if _, err := p.Write(append(data, '\n')); err != nil {
		log.Println("[ERROR] Failed to write to serial:", err)
		return
	}

	debugMu.RLock()
	if sentData {
		log.Println("[DEBUG] Sent data:", string(data))
	}
	debugMu.RUnlock()
}

func autoDetectCOM() string {
	ports, err := enumerator.GetDetailedPortsList()
	if err != nil {
		log.Println("[SERVER] Failed to list COM ports:", err)
		return ""
	}
	for _, port := range ports {
		if port.IsUSB {
			return port.Name
		}
	}
	if len(ports) > 0 {
		return ports[0].Name
	}
	return ""
}
