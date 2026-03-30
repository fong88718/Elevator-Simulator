#include <iostream>
#include <iomanip>
#include <thread> 
#include <chrono> 

using namespace std;

const int moveSpeed = 1000;

class Elevator 
{
private:
    int current_floor;
    int id;
    static int totalElevator;
public:
    Elevator() 
    {
        current_floor = 1;
        totalElevator++;
        id = totalElevator;
    };
    
    void display_floor(const Elevator &other) 
    {
        cout << "\033[H";
        cout << "===========Elevator Status============\n";
        for(int i = 10 ; i >= 1 ; i--)
        {
            if(id == 1)
                cout << setw(2) << i << "F | " << (current_floor == i ? "[E1]" : "    ") << " | " << (other.getCurrentFloor() == i ? "[E2]" : "    ") << " | " << endl;
            else    
                cout << setw(2) << i << "F | " << (other.getCurrentFloor() == i ? "[E1]" : "    ") << " | " << (current_floor == i ? "[E2]" : "    ") << " | " << endl;
        }  
        cout << "\033[14;28H" << flush;
    };

    void move(int current, int floor, const Elevator &other) 
    {
        while(current_floor != current)
        {   
            if (current_floor < current)
                current_floor++;
            else
                current_floor--;
            std::this_thread::sleep_for(std::chrono::milliseconds(moveSpeed));
            display_floor(other);
        }
      
        while(current_floor != floor)
        {   
            if (current_floor < floor)
                current_floor++;
            else
                current_floor--;
            
            std::this_thread::sleep_for(std::chrono::milliseconds(moveSpeed));
            display_floor(other);
        }
    };
    int getCurrentFloor() const 
    {
        return current_floor;
    };
};

int Elevator::totalElevator = 0;

bool valid(int current, int floor) 
{
    cout << "\033[13;1H" << "\033[K";
    if (1 <= current && current <= 10 && 1 <= floor && floor <= 10)
        return true;
    cout << "\033[13;1H" << "Please enter valid number" << endl;
    return false;
}

int main(void) 
{
    std::cout << "\033[2J\033[H" << std::flush;
    Elevator e1, e2;
    e1.display_floor(e2);
    int current = 1, floor = 1;

    while(1)
    {   
        // clear
        do 
        {
            cout << "\033[14;1H" << "\033[K";
            cout << "\033[15;1H" << "\033[K";

            cout << "\033[14;1H" << "Enter current floor (1-10): ";
            if (!(cin >> current))
            {
                cin.clear();
                cin.ignore(1000, '\n');
                continue;
            }
          
            cout << "\033[15;1H" << "Enter target floor (1-10): ";
            if (!(cin >> floor))
            {
                cin.clear();
                cin.ignore(1000, '\n');
                continue;
            }
        } while (!valid(current, floor));

        if (abs(e1.getCurrentFloor() - current) <= abs(e2.getCurrentFloor() - current))
            e1.move(current, floor, e2);
        else
            e2.move(current, floor, e1);
    }
    
    return 0;
} 
