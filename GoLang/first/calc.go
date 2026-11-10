package main

import "fmt"

func add(a, b float64) (float64, error) {
	return a + b, nil
}
func sub(a, b float64) (float64, error) {
	return a - b, nil
}
func mul(a, b float64) (float64, error) {
	return a * b, nil
}
func div(a, b float64) (float64, error) {
	if b == 0 {
		return 0, fmt.Errorf("cant divide by zero")
	}
	return a / b, nil
}




func main(){
	type Operation func(float64, float64) (float64, error)
	actions := map[string]Operation{
		"add": add,
		"sub": sub,
		"mul": mul,
		"div": div,
		"divide": div,
		"multiply": mul,
		"substraction": sub,
		"+": add,
		"-": sub,
		"*": mul,
		"x": mul,
		":": div,
		"/": div,
	}
	var action string
	var a,b float64
	fmt.Print("\n Input your ecuastion:")
	fmt.Scanln(&a, &action, &b)
	
	operation, exists := actions[action]
	if !exists {
    	fmt.Printf("Unknown action: %s\n", action)
    	return
	}
	result, err := operation(a, b)

	if err != nil {
		fmt.Println("Error:", err)
		return
	}
	fmt.Println(a, action, b, "=", result)

}