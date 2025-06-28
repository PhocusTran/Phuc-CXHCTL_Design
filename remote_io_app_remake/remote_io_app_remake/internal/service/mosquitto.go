package service

import (
	"log"
	"os/exec"
)

func Run_mosquitto() {
    cmd := exec.Command("mosquitto", "-v" ,"-c", "./mosquitto.conf")

    err := cmd.Start()
    if err != nil { log.Fatalf("Failed to run command: %v", err) }
}
