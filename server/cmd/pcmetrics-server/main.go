package main

import (
	"log"
	"os"
	"os/signal"
	"pcmetrics-server/internal/config"
	"pcmetrics-server/internal/control"
	"pcmetrics-server/internal/server"
	"pcmetrics-server/internal/tray"
	"syscall"
)

func main() {
	if err := config.Load(); err != nil {
		log.Fatal("Config load error:", err)
	}

	cfg := config.Get()
	server.SetDebug(cfg.Serial.Debug)

	control.StartAll()

	// Watch for config changes
	go config.WatchConfig("config.json", func() {
		log.Println("[CONFIG] Restarting after config change")
		cfg := config.Get()
		server.SetDebug(cfg.Serial.Debug)
		control.RestartAll()
	})

	// Run system tray
	go tray.RunTray()

	// Handle Ctrl+C / SIGTERM
	sig := make(chan os.Signal, 1)
	signal.Notify(sig, os.Interrupt, syscall.SIGTERM)
	<-sig

	log.Println("[SERVER] Shutdown initiated (Ctrl+C/SIGTERM)")
	control.StopAll()
	log.Println("[SERVER] Shutdown complete")
}
