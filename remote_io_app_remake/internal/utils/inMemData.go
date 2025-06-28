package utils

import (
	"fmt"
	"slices"
	"sync"
)


type Device_Dwin struct {
    ID      string
	Ins     []string
    Outs    []string
}

type Temp_Storage struct {
	mu      sync.Mutex
    Devices  []Device_Dwin
}

func (s *Temp_Storage) Add(id string, t rune, data string) {
	s.mu.Lock()
	defer s.mu.Unlock()

    fmt.Println(id, t, data)

    for id_d, device := range s.Devices {
        if id == device.ID {
            if t=='i'{
                for id_in, in := range device.Ins {
                    if in[0]==data[0]{
                        device.Ins[id_in] = data
                        fmt.Println("-----------", s)
                        return
                    } 
                }
                s.Devices[id_d].Ins = append(device.Ins, data)
                fmt.Println("aaaaaaaaaaa", s)
            } else if t=='o' {
                for out_id, out := range device.Outs {
                    if out[0]==data[0]{
                        device.Outs[out_id] = data
                        fmt.Println("-----------", s)
                        return
                    } 
                }
                s.Devices[id_d].Outs = append(device.Outs, data)
                fmt.Println("aaaaaaaaaaa", s)
            }
            return
        }
    }

    // no match found
    var new_d Device_Dwin
    new_d.ID = id
    if t=='i'{
        new_d.Ins = append(new_d.Ins, data)
    }

    if t=='o'{
        new_d.Outs = append(new_d.Outs, data)
    }

    s.Devices = append(s.Devices, new_d)
    fmt.Println(s)
}

func (s *Temp_Storage) Remove(id string) {
    s.mu.Lock()
    defer s.mu.Unlock()

    for i, d := range s.Devices {
        if d.ID == id{
            s.Devices = slices.Delete(s.Devices, i, i+1)
        }
    }
    fmt.Println(s)
}
