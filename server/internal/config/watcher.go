package config

import (
	"log"
	"time"

	"github.com/fsnotify/fsnotify"
)

// WatchConfig monitors the config.json file and triggers the callback when updated
func WatchConfig(path string, onReload func()) {
	watcher, err := fsnotify.NewWatcher()
	if err != nil {
		log.Println("[ERROR] Failed to start watcher:", err)
		return
	}

	if err := watcher.Add(path); err != nil {
		log.Println("[ERROR] Failed to watch config:", err)
		return
	}

	var debounce time.Time

	for {
		select {
		case event := <-watcher.Events:
			if event.Op&(fsnotify.Write|fsnotify.Create|fsnotify.Rename) != 0 {
				if !debounce.IsZero() && time.Since(debounce) < 200*time.Millisecond {
					continue
				}
				debounce = time.Now()

				log.Println("[CONFIG] Detected config change, updating...")

				if err := Load(); err != nil {
					log.Println("[ERROR] Reload failed:", err)
					continue
				}

				// Re-add watcher in case file was replaced
				_ = watcher.Remove(path)
				_ = watcher.Add(path)

				log.Println("[CONFIG] Reloaded config.json successfully")

				if onReload != nil {
					onReload()
				}
			}
		case err := <-watcher.Errors:
			log.Println("[ERROR] Watcher error:", err)
		}
	}
}
