package telegrambot

import (
	"log"
	"os"
	"strconv"
	"time"

	tele "gopkg.in/telebot.v4"
)

var Chat tele.Chat
var Bot *tele.Bot

func Telebot_Init(){
    Chat_ID, err := strconv.ParseInt(os.Getenv("CHAT_ID"), 10, 64)
    Chat = tele.Chat{ID: int64(Chat_ID)}

	if err != nil { log.Fatal(err) }


	pref := tele.Settings{
		Token:  os.Getenv("TOKEN"),
		Poller: &tele.LongPoller{Timeout: 10 * time.Second},
	}

	Bot, err = tele.NewBot(pref)
	if err != nil {
		log.Fatal(err)
	}

    time.Sleep(500 * time.Millisecond)

    go Bot.Start()
}


func Send(msg string){
    _, err := Bot.Send(&Chat, msg)
    if err!=nil{log.Printf("Send Telegram message error: %s\n", err)}
}

