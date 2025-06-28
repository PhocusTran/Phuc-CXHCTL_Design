package routes

import (
	"log"
	"net/http"
    "app/internal/utils"
)


func Wifi_config_handle(w http.ResponseWriter, r *http.Request){
    if err := utils.RenderTemplate(w, "wifi_config.html", "base.html", nil); err != nil {
        log.Fatal(err)
    }
}

func Dashboard_handle(w http.ResponseWriter, r *http.Request){
    if err := utils.RenderTemplate(w, "dashboard.html", "base.html", nil); err != nil {
        log.Fatal(err)
    }
}

func ChatSetting_handle(w http.ResponseWriter, r *http.Request){
    if err := utils.RenderTemplate(w, "chatsetting.html", "base.html", nil); err != nil {
        log.Fatal(err)
    }
}

func TelegramSetting_handle(w http.ResponseWriter, r *http.Request){
    if err := utils.RenderTemplate(w, "chatbot.html", "base.html", nil); err != nil {
        log.Fatal(err)
    }
}

