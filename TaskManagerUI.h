#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include <memory>
#include <deque>
#include <unordered_map>
#include "ProcessManager.h"
#include <algorithm> // Нужно для std::sort, std::min, std::max
#include <commctrl.h>
#include <debugapi.h> // Для OutputDebugString

// Предварительное объявление функции диалога
INT_PTR CALLBACK TerminateProcessDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK PerformanceDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);

// Constants for charts
#define MAX_HISTORY_POINTS 60  // 1 minute history (update every second)
#define PERFORMANCE_TIMER 1001
#define IDC_CPU_GRAPH     3201
#define IDC_MEM_GRAPH     3202
#define IDD_PERFORMANCE   3003
#define IDC_STATIC_MEMORY 3203
#define IDC_NET_GRAPH     3204
#define IDC_STATIC_NETWORK 3205

// Идентификаторы для столбчатых графиков
#define IDC_CPU_BAR_GRAPH     3301
#define IDC_MEM_BAR_GRAPH     3302
#define IDC_NET_BAR_GRAPH     3303
#define IDC_VIEW_SWITCH       3304  // Переключатель типа отображения

// Переместим имя класса окна сюда
extern const wchar_t* CLASS_NAME_W;

enum class MenuOption {
    ShowProcesses,
    ShowPerformance,
    TerminateProcess,
    StartProcess,
    RefreshData,
    Exit
};

// Предварительно объявим функцию логирования, если ее еще нет в заголовочном файле
inline void LogDebugMessage(const wchar_t* format, ...) {
    static wchar_t buffer[1024];
    va_list args;
    va_start(args, format);
    vswprintf_s(buffer, format, args);
    va_end(args);
    OutputDebugStringW(buffer);
}

// Structure for storing performance history
struct PerformanceHistory {
    std::deque<double> cpuHistory;
    std::deque<double> memHistory;
    std::deque<double> netSendHistory;    // История отправки (КБ/с)
    std::deque<double> netReceiveHistory; // История приема (КБ/с)
    
    void AddData(double cpu, double mem, double netSend = 0.0, double netRecv = 0.0) {
        // Логируем добавление данных
        LogDebugMessage(L"[HISTORY] Добавляем данные: CPU=%.1f%%, Mem=%.1f%%, Net Send=%.1f КБ/с, Net Recv=%.1f КБ/с\n",
                      cpu, mem, netSend, netRecv);
                      
        cpuHistory.push_back(cpu);
        memHistory.push_back(mem);
        netSendHistory.push_back(netSend);
        netReceiveHistory.push_back(netRecv);
        
        // Limit history size
        while (cpuHistory.size() > MAX_HISTORY_POINTS) cpuHistory.pop_front();
        while (memHistory.size() > MAX_HISTORY_POINTS) memHistory.pop_front();
        while (netSendHistory.size() > MAX_HISTORY_POINTS) netSendHistory.pop_front();
        while (netReceiveHistory.size() > MAX_HISTORY_POINTS) netReceiveHistory.pop_front();
        
        // Логируем размеры после обновления
        LogDebugMessage(L"[HISTORY] Размеры после обновления: CPU=%d, Mem=%d, Net Send=%d, Net Recv=%d\n",
                      static_cast<int>(cpuHistory.size()),
                      static_cast<int>(memHistory.size()),
                      static_cast<int>(netSendHistory.size()),
                      static_cast<int>(netReceiveHistory.size()));
    }
    
    void Clear() {
        cpuHistory.clear();
        memHistory.clear();
        netSendHistory.clear();
        netReceiveHistory.clear();
    }
};

class TaskManagerUI {
public:
    TaskManagerUI();
    ~TaskManagerUI();

    void Run();

    // Declare dialogue functions as friends
    friend INT_PTR CALLBACK TerminateProcessDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
    friend INT_PTR CALLBACK PerformanceDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
    
    ProcessManager processManager; // Сделаем публичным для доступа из диалогов
    HINSTANCE hInstance;     // Application instance handle - тоже публичный
    HWND hWnd;               // Main window handle - тоже публичный
    HWND hListView;          // ListView control handle
    void RefreshProcessList(); // Для вызова из диалога
    
    // Переменные, используемые в UI
    HWND hPerfDialog;      // Performance dialog handle
    bool bIsPerfDialogOpen; // Флаг открытого диалога производительности
    PerformanceHistory perfHistory;  // Performance history for charts
    bool showBarGraphs;   // Флаг для отображения столбчатых графиков вместо линейных
    
    // Утилиты, доступные из диалогов
    std::wstring FormatMemorySize(SIZE_T size);

    void InitializeUI(); // Добавляем объявление метода

private:
    HWND hStatusBar;         // Status bar handle
    HMENU hMenu;             // Menu handle
    
    bool running;
    
    // Время последнего взаимодействия пользователя
    DWORD lastUserInteractionTime;
    
    DWORD selectedPID; // Добавляем переменную для хранения PID выбранного процесса
    int selectedIndex; // Добавляем переменную для хранения индекса выбранного элемента

    // Переменные для сортировки ListView
    int sortColumn = -1; // -1 = нет сортировки, 0 = Имя, 1 = PID, 2 = ЦП, 3 = Память и т.д.
    bool sortAscending = true;
    
    // Window registration and creation
    bool InitWindow();
    void RegisterWindowClass(HINSTANCE hInstance);
    HWND CreateMainWindow(HINSTANCE hInstance);
    void InitializeMenu();
    void InitializeListView();
    void InitializeStatusBar();
    
    // Window message handling
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    void HandleCommand(int commandId);
    
    // Interface updates
    void UpdateStatusBar();
    void UpdatePerformanceData();
    
    // Chart drawing
    void DrawCpuGraph(HDC hdc, RECT rect);
    void DrawMemGraph(HDC hdc, RECT rect);
    void DrawNetworkGraph(HDC hdc, RECT rect);
    
    // Столбчатые графики (гистограммы)
    void DrawCpuBarGraph(HDC hdc, RECT rect);
    void DrawMemBarGraph(HDC hdc, RECT rect);
    void DrawNetworkBarGraph(HDC hdc, RECT rect);
    
    // Dialogs
    void ShowTerminateProcessDialog();
    void ShowStartProcessDialog();
    void ShowPerformanceDialog();
    
    // Utilities
    void SetProcessListColumns();
    void AddProcessToListView(const ProcessInfo* process, int index);
    void SortAndRefreshProcessList();
}; 