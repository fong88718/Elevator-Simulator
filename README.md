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

🏗 系統架構說明
核心類別

Elevator: 控制電梯實體的行為，包含移動邏輯、門控動畫、請求處理。

EventManager: 管理所有乘客請求的狀態，並負責繪製底部的 Event Queue 表格。

dispatch_elevator: 全域調度函式，計算各電梯的「成本分數」來決定由誰接單。

調度邏輯 (Cost Function)

系統會考慮以下因素來決定派發哪部電梯：

距離：電梯目前樓層與起點的距離。

方向一致性：順向載客的成本最低；逆向或已過目標樓層的成本較高。

負載量：若電梯已有大量請求，會增加權重，避免單一電梯過載。

視覺化代號

[E1]：電梯 1 正常運作中。

<E1> / |E1|：電梯正在開門、等待或關門。

顏色標示：完成後的請求會以綠色 COMPLETE 顯示。

⚠️ 注意事項
本程式使用 ANSI 控制碼，請在支援的終端機（如 Linux Terminal, macOS iTerm2, 或 Windows PowerShell/WSL）中執行，以獲得正確的視覺效果。

預設樓層上限為 10 樓，可透過修改原始碼中的 MAX_FLOORS 變數調整。
