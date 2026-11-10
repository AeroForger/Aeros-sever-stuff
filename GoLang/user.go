package main

import (
    "fmt"
    "strings"
)

var users []string

type BankAcc struct  {
	Name string
	Age int
	Balance float64
}

func (u *BankAcc) changeName(newName string){
	for i, value := range users {
		if u.Name == value {
			users[i] = newName
			break;
		}
	}
	u.Name = newName
}
func (u *BankAcc) withdraw(amount float64) error {
    if amount <= 0 {
        return fmt.Errorf("withdrawal must be positive")
    }

    if amount > u.Balance {
        return fmt.Errorf("insufficient funds")
    }

    u.Balance -= amount
    return nil
}
func (u *BankAcc) deposit(amount float64) error {
    if amount <= 0 {
        return fmt.Errorf("deposit must be positive")
    }

    u.Balance += amount
    return nil
}
func (u *BankAcc) changeAge(newAge int) {
	if newAge < u.Age {
		fmt.Println("Cant change the age as it is younger than the current age")
	} else {
		u.Age = newAge
	}

}
func (johnny *BankAcc) info(){
	fmt.Println("Name: ", johnny.Name, "\nAge: ", johnny.Age, "\nBalance: ", johnny.Balance)
}

func mainLoop(user *BankAcc){
	for {
		var operation string
		fmt.Println("\n1. Change name \n2. Change Age \n3. Change balance \n4.Deposit")
		action := map[string]string {
			"change name": "1",
			"change age": "2",
			"change balance": "3",
			"deposit": "4",
			"1": "1",
			"2": "2",
			"3": "3",
			"4": "4",
		}
		fmt.Scanln(&operation)
		operation, exists := action[strings.ToLower(operation)]
		if !exists {
			fmt.Println("This action doesnt exit")
			continue
		}
		if operation == "1" {
			var newName string
			fmt.Print("\nNew name: ")
			fmt.Scanln(&newName)
			user.changeName(newName)
			user.info()
			continue
		} else if operation == "2" {
			var newAge int
			fmt.Print("\nNew age: ")
			fmt.Scanln(&newAge)
			user.changeAge(newAge)
			user.info()
			continue
		} else if operation == "3" {
			var newBalance float64
			fmt.Print("\n new balance: ")
			fmt.Scanln(&newBalance)
			err := user.withdraw(newBalance)
			if err != nil {
				fmt.Printf("Error: %s\n", err)
				continue
			}
			user.info()
			continue
		} else if operation == "4" {
			var amount float64
			fmt.Print("\n amount: ")
			fmt.Scanln(&amount)
			err := user.deposit(amount)
			if err != nil {
				fmt.Printf("Error: %s\n", err)
				continue
			}
			user.info()
			continue
		}

	}
}


func main(){


	johnny := BankAcc {
		Name: "johnny",
		Age: 19,
		Balance: 1259.45,
	}
	users = append(users, "johnny")
	var name string
	fmt.Print("Whats your name: ")
	fmt.Scanln(&name)
	found := 0
	for _, value := range users {
		if name == value {
			fmt.Println("\nFound your name in the database!")
			found = 1
		}
	}
	if found == 0 {
		var username BankAcc
		fmt.Println("\ninput your credentials (name, age, balance)")
		fmt.Scanln(
			&username.Name,
			&username.Age,
			&username.Balance,
		)
		users = append(users, username.Name)
		username.info()
		mainLoop(&username)
	} else {
		johnny.info()
		mainLoop(&johnny)
	}

}