package main

import (
	"encoding/json"
	"fmt"
	"io"
	"log"
	"net"
	"os"
	"sync"
)

var mutex sync.Mutex

func appendToJSONMaster(filename string, newItems string) error {
	var currentItems []string

	file, err := os.OpenFile(filename, os.O_RDWR|os.O_CREATE, 0644)
	if err != nil {
		return err
	}
	defer file.Close()

	stat, err := file.Stat()
	if err != nil {
		return err
	}

	if stat.Size() > 0 {
		err = json.NewDecoder(file).Decode(&currentItems)
		if err != nil && err != io.EOF {
			return err
		}
	}

	currentItems = append(currentItems, newItems)

	_, err = file.Seek(0, 0) //whats seek
	if err != nil {
		return err
	}
	err = file.Truncate(0) //whats truncate
	if err != nil {
		return err
	}

	encoder := json.NewEncoder(file)    // i understand this
	encoder.SetIndent("", "  ")         //this too
	return encoder.Encode(currentItems) //this too
}

func main() {
	listener, err := net.Listen("tcp", ":8110")
	if err != nil {
		fmt.Printf("Error occured. %s", err)
		return
	}
	defer listener.Close()

	for {

		conn, err := listener.Accept()
		if err != nil {
			log.Println("Error accepting conn:", err)
			continue
		}

		go handleConnection(conn)
	}
}

func saveMessage(filename string, msg string) error {
	mutex.Lock()
	defer mutex.Unlock()

	return appendToJSONMaster(filename, msg)
}

func handleConnection(conn net.Conn) {
	defer conn.Close()
	filename := "msgs.json"
	for {
		data := make([]byte, 80)
		byteAmount, err := conn.Read(data)
		if err == io.EOF {
			break
		}
		if err != nil {
			log.Println("error", err)
			break
		}
		msg := string(data[:byteAmount])
		if msg == "exit" || msg == "exit\n" {
			break
		}
		err = saveMessage(filename, msg)
		if err != nil {
			log.Println("error: ", err)
		}
		fmt.Printf("bytes read: %d\n", byteAmount)
		fmt.Printf("result: %s\n", msg)
	}

}
