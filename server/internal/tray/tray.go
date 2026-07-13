package tray

import (
	"pcmetrics-server/internal/config"
	"pcmetrics-server/internal/control"
	"pcmetrics-server/internal/server"
	"sync/atomic"
	"time"

	"github.com/getlantern/systray"
)

var (
	menuStartStop *systray.MenuItem
	menuStatus    *systray.MenuItem

	lastStatus   string
	starting     atomic.Bool
	shuttingDown atomic.Bool
)

// RunTray launches the system tray
func RunTray() {
	systray.Run(onReady, onExit)
}

func updateMenuState() {
	var status string
	switch {
	case shuttingDown.Load():
		status = "Shutting down…"
	case starting.Load():
		status = "Starting…"
	case server.IsRunning():
		status = "Running"
	default:
		status = "Stopped"
	}

	if status == lastStatus {
		return
	}
	lastStatus = status

	menuStatus.SetTitle("Status: " + status)

	if server.IsRunning() || starting.Load() {
		menuStartStop.SetTitle("Stop Server")
	} else {
		menuStartStop.SetTitle("Start Server")
	}
}

func onReady() {
	systray.SetTitle("PCMetrics-Server")
	systray.SetTooltip("PCMetrics Server")

	menuStatus = systray.AddMenuItem("Status: Unknown", "Server status")
	systray.AddSeparator()
	menuStartStop = systray.AddMenuItem("Start Server", "Start/Stop the server")
	systray.AddSeparator()
	exit := systray.AddMenuItem("Exit", "Exit the program")

	// ---- Menu actions ----
	go func() {
		for {
			select {
			case <-menuStartStop.ClickedCh:
				menuStartStop.Disable()

				if server.IsRunning() {
					shuttingDown.Store(true)
					control.StopAll()
					shuttingDown.Store(false)
				} else {
					starting.Store(true)
					control.StartAll()
					starting.Store(false)
				}

				updateMenuState()
				menuStartStop.Enable()

			case <-exit.ClickedCh:
				shuttingDown.Store(true)
				control.StopAll()
				systray.Quit()
				return
			}
		}
	}()

	// ---- Status polling ----
	cfg := config.Get()
	interval := 500 * time.Millisecond
	if cfg.Tray.PollIntervalMs > 0 {
		interval = time.Duration(cfg.Tray.PollIntervalMs) * time.Millisecond
	}

	go func() {
		ticker := time.NewTicker(interval)
		defer ticker.Stop()

		for range ticker.C {
			updateMenuState()
		}
	}()

	lastStatus = ""
	updateMenuState()
}

func onExit() {
	shuttingDown.Store(true)
	control.StopAll()
}
