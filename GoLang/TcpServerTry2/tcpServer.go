package main

import (
	"bufio"
	"encoding/json"
	"fmt"
	"io"
	"log"
	"net"
	"os"
	"sync"
)

var fileMutex sync.Mutex
var clientMutex sync.Mutex
var clients []net.Conn

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
		clientMutex.Lock()
		clients = append(clients, conn)
		clientMutex.Unlock()
		go handleConnection(conn)
	}
}

func saveMessage(filename string, msg string) error {
	fileMutex.Lock()
	defer fileMutex.Unlock()

	return appendToJSONMaster(filename, msg)
}
func remover(conn net.Conn) {
	clientMutex.Lock()
	removeClient(conn)
	clientMutex.Unlock()
}

func handleConnection(conn net.Conn) {
	filename := "msgs.json"
	reader := bufio.NewReader(conn)

	for {
		msg, err := reader.ReadString('\n')
		if err == io.EOF {
			remover(conn)
			break
		}
		if err != nil {
			log.Printf("Error: %v", err)
			remover(conn)
			break
		}
		if msg == "exit" || msg == "exit\n" {
			remover(conn)
			break
		}
		err = saveMessage(filename, msg)
		if err != nil {
			log.Println("Error: ", err)
		}
		fmt.Printf("Read %d\n", len(msg))
		log.Println("Msg: ", msg)
		sendMsg(conn, msg)

	}

}
func removeClient(conn net.Conn) {
	defer conn.Close()
	for i, value := range clients {
		if value == conn {
			clients = append(clients[:i], clients[i+1:]...)
			break
		}
	}
}
func sendMsg(currentConn net.Conn, msg string) {
	clientMutex.Lock()
	defer clientMutex.Unlock()

	msg = currentConn.RemoteAddr().String() + " Sent: " + msg

	for _, value := range clients {
		if value == currentConn {
			continue
		}

		_, err := value.Write([]byte(msg))
		if err != nil {
			log.Printf("Server write error: %v", err)
			continue
		}
	}
}
