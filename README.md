Multi-Threaded Elevator Simulation System (C++)
這是一個基於 C++ 開發的多執行緒電梯調度模擬系統。系統支援即時終端機動畫顯示、智慧調度演算法，並提供 TCP Socket 介面以接收遠端（如手機端）的乘車請求。

🌟 核心功能
多執行緒架構：每部電梯運行在獨立的執行緒中，互不干擾。

智慧調度演算法：根據電梯當前位置、運行方向及負載量（佇列長度）計算最佳成本，指派最合適的電梯。

即時視覺化界面：利用 ANSI 控制碼在終端機繪製電梯移動、開關門動畫及事件狀態佇列。

TCP 遠端呼叫：內建 TCP Server 執行緒，監聽 Port 8888，可接收格式為 "PickupFloor TargetFloor" 的遠端指令。

事件追蹤系統：完整的生命週期管理（PENDING -> IN PROGRESS -> COMPLETE），並即時更新於 UI。

🛠 技術棧
語言：C++11 或更高版本

併發處理：std::thread, std::mutex, std::condition_variable

網路：POSIX Sockets (TCP/IP)

UI：ANSI Terminal Control Sequences (VT100)

🚀 快速上手
1. 編譯

由於使用了執行緒函式庫，編譯時請加上 -pthread 參數：

Bash
g++ -o elevator_sim main.cpp -pthread
2. 執行

Bash
./elevator_sim
3. 操作說明

初始化：啟動後輸入電梯總數量。

本地輸入：在終端機下方依照提示輸入 Pickup floor (起點) 與 Target floor (終點)。

遠端呼叫：您可以使用 telnet 或自定義 TCP Client 傳送指令：

Bash
# 範例：從 3 樓要去 8 樓
echo "3 8" | nc localhost 8888
