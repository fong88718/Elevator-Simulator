#include <iostream>
#include <iomanip>
#include <thread>
#include <chrono>
#include <vector>
#include <cmath>
#include <fstream>
#include <string>
#include <mutex>
#include <condition_variable>
#include <algorithm>
#include <list>
#include <set>
#include <memory>
#include <limits>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <cstring>
#include <cstdio>

using namespace std;

const int MOVE_SPEED_MS = 1000;
const int DOOR_TIME_MS = 1000;
const int ANIMATION_DELAY_MS = 300;

int MAX_FLOORS = 10;

std::mutex display_mutex; 

// 置中字串的輔助函式
string center_string(const string& str, int width) 
{
    if (str.length() >= width) return str;
    int padding = width - str.length();
    int pad_left = padding / 2;
    int pad_right = padding - pad_left;
    return string(pad_left, ' ') + str + string(pad_right, ' ');
}

int current_input_row = 0;
int current_input_col = 0;

void restore_input_cursor() 
{
    if (current_input_row > 0) 
        cout << "\033[" << current_input_row << ";" << current_input_col << "H";
}

// --- Event Data Structure ---
struct Event {
    int id;
    int pickup;
    int target;
    int elevator_id;
    bool is_remote;
    enum Status { PENDING, ONBOARD, COMPLETE } status;
};

// --- EventManager Class (整合 Event Queue 與 UI 繪製) ---
class EventManager 
{
private:
    std::vector<std::shared_ptr<Event>> events;
    int next_event_id = 1;
    std::mutex mtx; // 專屬於 EventManager 的資料保護鎖

public:
    // 取得鎖的參考，讓 Elevator 在更新狀態時可以使用
    std::mutex& get_mutex() { return mtx; }

    // 新增事件並回傳指標
    std::shared_ptr<Event> add_event(int pickup, int target, int elevator_id, bool is_remote = false)
    {
        auto new_ev = std::make_shared<Event>();
        new_ev->id = next_event_id++;
        new_ev->pickup = pickup;
        new_ev->target = target;
        new_ev->elevator_id = elevator_id;
        new_ev->is_remote = is_remote;
        new_ev->status = Event::PENDING;
        events.push_back(new_ev);
        return new_ev;
    }

    // 繪製事件佇列
    void draw() 
    {
        std::lock_guard<std::mutex> disp_lock(display_mutex);
        std::lock_guard<std::mutex> ev_lock(mtx);
        
        int start_row = MAX_FLOORS + 6;
        cout << "\033[" << start_row << ";1H\033[K" << "=========== Event Queue ===========";
        
        int num_to_show = std::min(10, (int)events.size());
        int start_idx = events.size() - num_to_show;
        
        for (int i = 0; i < num_to_show; ++i) 
        {
            auto ev = events[start_idx + i];
            cout << "\033[" << (start_row + 1 + i) << ";1H\033[K";
            cout << "Req #" << ev->id << (ev->is_remote ? " [TCP]" : "      ") 
                 << " | " << ev->pickup << "F -> " << ev->target << "F [E" << ev->elevator_id << "] - ";
                 
            if (ev->status == Event::PENDING) cout << "PENDING";
            else if (ev->status == Event::ONBOARD) cout << "IN PROGRESS";
            else if (ev->status == Event::COMPLETE) cout << "\033[32mCOMPLETE\033[0m"; 
        }
        restore_input_cursor();
        cout << flush;
    }
};

// 實例化一個全域的 EventManager 供系統使用
EventManager event_manager;

// ------------------------------

class Elevator 
{
public:
    int current_floor;
    int id;
    static int totalElevator;

    enum class Direction { IDLE, UP, DOWN };
    Direction direction;

    std::set<int> up_requests;
    std::set<int, std::greater<int>> down_requests;
    std::vector<std::shared_ptr<Event>> tracked_events;

    std::mutex internal_mutex;
    std::condition_variable cv;

    Elevator() : current_floor(1), id(++totalElevator), direction(Direction::IDLE) {}

    void operate()
    {
        while (true) 
        {
            unique_lock<mutex> lock(internal_mutex);

            cv.wait(lock, [this] { return !up_requests.empty() || !down_requests.empty(); });
            if (direction == Direction::IDLE) 
                update_direction_if_needed();

            while (true) 
            {
                bool should_stop = should_stop_at_current_floor();
                if (should_stop) 
                {
                    lock.unlock();
                    stop_at_floor();
                    lock.lock(); 
                    continue;
                }

                if (update_direction_if_needed()) 
                    break; 

                lock.unlock();
                move_one_floor();
                lock.lock();
            }
        }
    }

    void add_request(int pickup_floor, int target_floor, std::shared_ptr<Event> ev) 
    {
        std::unique_lock<std::mutex> lock(internal_mutex);
        tracked_events.push_back(ev); 

        if (pickup_floor <= target_floor) 
            up_requests.insert(pickup_floor);
        else 
            down_requests.insert(pickup_floor);

        lock.unlock();
        cv.notify_one();
    }

    int getCurrentFloor() const { return current_floor; }
    int getId() const { return id; }
    Direction getDirection() const { return direction; }
    int getUpRequestsCount() const { return up_requests.size(); }
    int getDownRequestsCount() const { return down_requests.size(); }

    void clear_floor() const 
    {
        std::lock_guard<std::mutex> lock(display_mutex);
        int row = (MAX_FLOORS + 2) - current_floor;
        int col = 7 + (id - 1) * 8;
        cout << "\033[" << row << ";" << col << "H" << "       |";
        restore_input_cursor();
        cout << flush;
    }

    void display_floor() const 
    {
        std::lock_guard<std::mutex> lock(display_mutex);
        int row = (MAX_FLOORS + 2) - current_floor;
        int col = 7 + (id - 1) * 8;
        cout << "\033[" << (row) << ";" << col << "H";
        string e_str = "[E" + to_string(id) + "]";
        cout << center_string(e_str, 7) << "|";
        restore_input_cursor();
        cout << flush;
    }

    void move_one_floor() {
        clear_floor();
        if (direction == Direction::UP && current_floor < MAX_FLOORS) current_floor++;
        else if (direction == Direction::DOWN && current_floor > 1) current_floor--;
        display_floor();
        std::this_thread::sleep_for(std::chrono::milliseconds(MOVE_SPEED_MS));
    }

    void stop_at_floor() 
    {
        auto draw_frame = [this](const string& frame_str) 
        {
            std::lock_guard<std::mutex> lock(display_mutex);
            int row = (MAX_FLOORS + 2) - current_floor;
            int col = 7 + (id - 1) * 8;
            cout << "\033[" << row << ";" << col << "H";
            cout << center_string(frame_str, 7) << "|";
            restore_input_cursor();
            cout << flush;
        };

        // 開門動畫
        draw_frame("<E" + to_string(id) + ">");
        std::this_thread::sleep_for(std::chrono::milliseconds(ANIMATION_DELAY_MS));
        draw_frame("< E" + to_string(id) + " >");
        std::this_thread::sleep_for(std::chrono::milliseconds(ANIMATION_DELAY_MS));
        draw_frame("| E" + to_string(id) + " |");
        std::this_thread::sleep_for(std::chrono::milliseconds(ANIMATION_DELAY_MS));

        
        
        bool state_changed = false;
        {
            // 向 EventManager 取得鎖，以安全地更新事件狀態
            std::lock_guard<std::mutex> ev_lock(event_manager.get_mutex());
            for (auto& ev : tracked_events) 
            {
                if (ev->status == Event::PENDING && current_floor == ev->pickup) 
                {
                    bool passenger_goes_up = (ev->target >= ev->pickup);
                    if ((passenger_goes_up && direction == Direction::UP) || (!passenger_goes_up && direction == Direction::DOWN) || direction == Direction::IDLE) 
                    {
                        ev->status = Event::ONBOARD;
                        state_changed = true;

                        // ===== 修正: 乘客真正上車後，才將他的目的地加入電梯任務佇列 =====
                        std::lock_guard<std::mutex> int_lock(internal_mutex);
                        if (ev->target != current_floor) 
                        {
                            if (passenger_goes_up) 
                                up_requests.insert(ev->target);
                            else 
                                down_requests.insert(ev->target);
                        }
                    }
                }
                if (ev->status == Event::ONBOARD && current_floor == ev->target) 
                {
                    ev->status = Event::COMPLETE;
                    state_changed = true;
                }
            }
        }
        
        // 使用 event_manager.draw() 繪製畫面
        if (state_changed) event_manager.draw();

        std::this_thread::sleep_for(std::chrono::milliseconds(DOOR_TIME_MS));
        
        // 關門動畫
        draw_frame("> E" + to_string(id) + " <");
        std::this_thread::sleep_for(std::chrono::milliseconds(ANIMATION_DELAY_MS));
        draw_frame(">E" + to_string(id) + "<");
        std::this_thread::sleep_for(std::chrono::milliseconds(ANIMATION_DELAY_MS));
        display_floor(); 
        std::this_thread::sleep_for(std::chrono::milliseconds(DOOR_TIME_MS));
    }

    bool should_stop_at_current_floor() 
   {
        bool stopped = false;
        
        bool has_req_above = false, has_req_below = false;
        for (int f : up_requests) 
        { 
            if (f > current_floor) has_req_above = true; 
            if (f < current_floor) has_req_below = true; 
        }
        for (int f : down_requests) 
        { 
            if (f > current_floor) has_req_above = true; 
            if (f < current_floor) has_req_below = true; 
        }

        if (direction == Direction::UP || direction == Direction::IDLE) 
        {
            if (up_requests.count(current_floor)) 
            {
                up_requests.erase(current_floor);
                stopped = true;
                direction = Direction::UP; 
            } 
            else if (!has_req_above && down_requests.count(current_floor)) 
            {
                down_requests.erase(current_floor);
                stopped = true;
                direction = Direction::DOWN; 
            }
        }
        
        if (!stopped && (direction == Direction::DOWN || direction == Direction::IDLE)) 
        {
            if (down_requests.count(current_floor)) 
            {
                down_requests.erase(current_floor);
                stopped = true;
                direction = Direction::DOWN;
            } 
            else if (!has_req_below && up_requests.count(current_floor)) 
            {
                up_requests.erase(current_floor);
                stopped = true;
                direction = Direction::UP; 
            }
        }
        return stopped;
    }

    bool update_direction_if_needed() 
    {
        if (up_requests.empty() && down_requests.empty()) 
        {
            direction = Direction::IDLE;
            return true; 
        }

        bool has_req_above = false, has_req_below = false;
        for (int f : up_requests) 
        { 
            if (f > current_floor) has_req_above = true; 
            if (f < current_floor) has_req_below = true; 
        }
        for (int f : down_requests) 
        { 
            if (f > current_floor) has_req_above = true; 
            if (f < current_floor) has_req_below = true; 
        }

        if (direction == Direction::UP) 
        {
            if (has_req_above) return false; 
            else if (has_req_below) direction = Direction::DOWN; 
            else direction = Direction::IDLE;
        } 
        else if (direction == Direction::DOWN) 
        {
            if (has_req_below) return false; 
            else if (has_req_above) direction = Direction::UP; 
            else direction = Direction::IDLE;
        }
        else 
        { 
            if (has_req_above) direction = Direction::UP;
            else if (has_req_below) direction = Direction::DOWN;
        }
        
        return (direction == Direction::IDLE); 
    }
};

int Elevator::totalElevator = 0;

bool valid(int current, int floor)
{
    std::lock_guard<std::mutex> lock(display_mutex);
    cout << "\033[" << (MAX_FLOORS + 4) << ";1H" << "\033[K";
    if (1 <= current && current <= MAX_FLOORS && 1 <= floor && floor <= MAX_FLOORS)
        return true;
    cout << "\033[" << (MAX_FLOORS + 4) << ";1H" << "Invalid floor. Please enter a number between 1 and " << MAX_FLOORS << ".";
    restore_input_cursor();
    cout << flush;
    return false;
}


int dispatch_elevator(int pickup, int target, const vector<unique_ptr<Elevator>>& elevators) 
{
    int closest_idx = 0;
    int min_cost = 1e9;
    
    bool request_goes_up = (target >= pickup);

    for (size_t i = 0; i < elevators.size(); i++) 
    {
        int cost = 0;
        int e_current_floor = elevators[i]->getCurrentFloor();
        Elevator::Direction e_dir = elevators[i]->getDirection();
        
        int up_count = elevators[i]->getUpRequestsCount();
        int down_count = elevators[i]->getDownRequestsCount();
        int queue_size = up_count + down_count;

        if (e_dir == Elevator::Direction::IDLE) 
        {
            cost = abs(e_current_floor - pickup); 
        } 
        else if (e_dir == Elevator::Direction::UP) 
        {
            if (request_goes_up) 
            {
                if (e_current_floor <= pickup) 
                    cost = abs(e_current_floor - pickup) + queue_size * 2; 
                else 
                    cost = 1000 + abs(e_current_floor - pickup) + queue_size * 5;
            } 
            else 
            {
                if (up_count == 0 && e_current_floor <= pickup) 
                    cost = abs(e_current_floor - pickup) + queue_size * 2;
                else 
                    cost = 1000 + abs(e_current_floor - pickup) + queue_size * 5;
            }
        } 
        else if (e_dir == Elevator::Direction::DOWN) 
        {
            if (!request_goes_up) 
            {
                if (e_current_floor >= pickup) 
                    cost = abs(e_current_floor - pickup) + queue_size * 2;
                else 
                    cost = 1000 + abs(e_current_floor - pickup) + queue_size * 5;
            } 
            else 
            {
                if (down_count == 0 && e_current_floor >= pickup) 
                    cost = abs(e_current_floor - pickup) + queue_size * 2;
                else
                    cost = 1000 + abs(e_current_floor - pickup) + queue_size * 5;
            }
        }

        if (cost < min_cost) 
        {
            min_cost = cost;
            closest_idx = i;
        }
    }
    return closest_idx;
}

// 處理來自 TCP Socket 的遠端呼叫背景執行緒
void tcp_server_thread(const vector<unique_ptr<Elevator>>& elevators) 
{
    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);
    char buffer[1024] = {0};

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) return;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) return;

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(8888); // 與手機端連接的預設通訊埠

    if (::bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) return;
    if (listen(server_fd, 3) < 0) return;

    while (true) 
    {
        // 1. 等待手機連入 (這部分不變)
        new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);
        if (new_socket < 0) continue;

        while (true) 
        {
            memset(buffer, 0, 1024);
            if (read(new_socket, buffer, 1024) <= 0) 
                break; 
            
            int pickup = 0, target = 0;
            if (sscanf(buffer, "%d %d", &pickup, &target) == 2) 
            {
                if (pickup >= 1 && pickup <= MAX_FLOORS && target >= 1 && target <= MAX_FLOORS) 
                {
                    int closest_idx = dispatch_elevator(pickup, target, elevators);
                    auto new_ev = event_manager.add_event(pickup, target, elevators[closest_idx]->getId(), true);
                    event_manager.draw();
                    elevators[closest_idx]->add_request(pickup, target, new_ev);
                }
            }
        }
        close(new_socket); 
    }
}


int main(void) 
{
    std::cout << "\033[2J\033[H" << std::flush;
    
    int num_elevators = 0;
    cout << "Enter number of elevators: ";
    while (!(cin >> num_elevators) || num_elevators <= 0)
    {
        cin.clear();
        cin.ignore(1000, '\n');
        cout << "\033[1;1H\033[K";
        cout << "Please enter a valid number of elevators: ";
    }
    std::cout << "\033[2J\033[H" << std::flush;

    vector<unique_ptr<Elevator>> elevators;
    vector<thread> elevator_threads;
    for (int i = 0; i < num_elevators; i++) 
        elevators.emplace_back(new Elevator());
    for (int i = 0; i < num_elevators; i++) 
        elevator_threads.emplace_back(&Elevator::operate, elevators[i].get());

    // 啟動 TCP 背景執行緒以監聽遠端指令，並脫離主執行緒獨立運行
    std::thread remote_thread(tcp_server_thread, std::ref(elevators));
    remote_thread.detach();

    cout << "\033[H";
    cout << "===========Elevator Status============\n";
    for(int i = MAX_FLOORS ; i >= 1 ; i--)
    {
        if (i < 10) cout << " " << i << "F | ";
        else cout << i << "F | ";
        for(int j = 1; j <= num_elevators; j++) cout << "       |";
        cout << "\n";
    }

    for (const auto& e : elevators) e->display_floor();
    event_manager.draw(); // 透過 EventManager 繪製初始狀態

    int pickup = 1, target = 1;

    while(1)
    {   
        {
            std::lock_guard<std::mutex> lock(display_mutex);
            cout << "\033[" << (MAX_FLOORS + 2) << ";1H" << "\033[K";
            cout << "\033[" << (MAX_FLOORS + 3) << ";1H" << "\033[K";
            cout << "\033[" << (MAX_FLOORS + 4) << ";1H" << "\033[K";
        }

        do 
        {
            {
                std::lock_guard<std::mutex> lock(display_mutex);
                string prompt = "Enter pickup floor (1-" + to_string(MAX_FLOORS) + ") ";
                cout << "\033[" << (MAX_FLOORS + 2) << ";1H" << prompt;
                current_input_row = MAX_FLOORS + 2;
                current_input_col = prompt.length() + 1;
                cout << flush;
            }
            if (!(cin >> pickup))
            {
                cin.clear();
                cin.ignore(numeric_limits<streamsize>::max(), '\n');
                continue;
            }
            
            {
                std::lock_guard<std::mutex> lock(display_mutex);
                string prompt = "Enter target floor (1-" + to_string(MAX_FLOORS) + "): ";
                cout << "\033[" << (MAX_FLOORS + 3) << ";1H" << prompt;
                current_input_row = MAX_FLOORS + 3;
                current_input_col = prompt.length() + 1;
                cout << flush;
            }
            if (!(cin >> target))
            {
                cin.clear();
                cin.ignore(numeric_limits<streamsize>::max(), '\n');
                continue;
            }
        } while (!valid(pickup, target));

        int closest_idx = dispatch_elevator(pickup, target, elevators);
        auto new_ev = event_manager.add_event(pickup, target, elevators[closest_idx]->getId());
        event_manager.draw();
        elevators[closest_idx]->add_request(pickup, target, new_ev);
    }
    return 0;
}
