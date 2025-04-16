#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include "TaskManagerUI.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <locale>
#include <vector>
#include <deque>
#include <string>
#include <commctrl.h>
#include <windowsx.h>
#include <shellapi.h>
#include <richedit.h>
#include <gdiplus.h>
#include <psapi.h>
#include <debugapi.h>
#include <commdlg.h>

// Определения недостающих макросов и констант
#ifndef LVM_GETITEMHEIGHT
#define LVM_GETITEMHEIGHT (LVM_FIRST + 44)
#endif

#ifndef ListView_GetItemHeight
#define ListView_GetItemHeight(hwnd) \
        (UINT)SendMessage((hwnd), LVM_GETITEMHEIGHT, 0, 0L)
#endif

#ifndef ListView_Scroll
#define ListView_Scroll(hwnd, dx, dy) \
        (BOOL)SendMessage((hwnd), LVM_SCROLL, (WPARAM)(int)(dx), (LPARAM)(int)(dy))
#endif

#ifndef LVM_ENSUREVISIBLE
#define LVM_ENSUREVISIBLE (LVM_FIRST + 19)
#endif

#ifndef ListView_EnsureVisible
#define ListView_EnsureVisible(hwnd, i, fPartialOK) \
        (BOOL)SendMessage((hwnd), LVM_ENSUREVISIBLE, (WPARAM)(int)(i), MAKELPARAM((BOOL)(fPartialOK), 0))
#endif

#ifndef ListView_GetTopIndex
#define ListView_GetTopIndex(hwnd) \
        (int)SendMessage((hwnd), LVM_GETTOPINDEX, 0, 0L)
#endif

#ifndef ListView_GetCountPerPage
#define ListView_GetCountPerPage(hwnd) \
        (int)SendMessage((hwnd), LVM_GETCOUNTPERPAGE, 0, 0L)
#endif

#ifndef ListView_GetNextItem
#define ListView_GetNextItem(hwnd, i, flags) \
        (int)SendMessage((hwnd), LVM_GETNEXTITEM, (WPARAM)(int)(i), MAKELPARAM((UINT)(flags), 0))
#endif

#ifndef ListView_GetItemCount
#define ListView_GetItemCount(hwnd) \
        (int)SendMessage((hwnd), LVM_GETITEMCOUNT, 0, 0L)
#endif

#ifndef ListView_GetItem
#define ListView_GetItem(hwnd, pitem) \
        (BOOL)SendMessage((hwnd), LVM_GETITEM, 0, (LPARAM)(LV_ITEM*)(pitem))
#endif

#ifndef ListView_SetItem
#define ListView_SetItem(hwnd, pitem) \
        (BOOL)SendMessage((hwnd), LVM_SETITEM, 0, (LPARAM)(LV_ITEM*)(pitem))
#endif

#ifndef ListView_DeleteItem
#define ListView_DeleteItem(hwnd, i) \
        (BOOL)SendMessage((hwnd), LVM_DELETEITEM, (WPARAM)(int)(i), 0L)
#endif

#ifndef ListView_DeleteAllItems
#define ListView_DeleteAllItems(hwnd) \
        (BOOL)SendMessage((hwnd), LVM_DELETEALLITEMS, 0, 0L)
#endif

#ifndef ListView_InsertItem
#define ListView_InsertItem(hwnd, pitem) \
        (int)SendMessage((hwnd), LVM_INSERTITEM, 0, (LPARAM)(const LV_ITEM*)(pitem))
#endif

#ifndef ListView_SetItemState
#define ListView_SetItemState(hwndLV, i, data, mask) \
{ LV_ITEM _lvi; \
  _lvi.stateMask = mask; \
  _lvi.state = data; \
  SendMessage((hwndLV), LVM_SETITEMSTATE, (WPARAM)(i), (LPARAM)(LV_ITEM *)&_lvi); \
}
#endif

#ifndef ListView_SetItemText
#define ListView_SetItemText(hwndLV, i, iSubItem, pszText) \
{ LV_ITEM _lvi; \
  _lvi.iSubItem = iSubItem; \
  _lvi.pszText = pszText; \
  SendMessage((hwndLV), LVM_SETITEMTEXT, (WPARAM)(i), (LPARAM)(LV_ITEM *)&_lvi); \
}
#endif

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "gdiplus.lib")

using namespace Gdiplus;

// Константы для идентификаторов меню
#define IDM_REFRESH          1001
#define IDM_TERMINATE        1002
#define IDM_START            1003
#define IDM_PERFORMANCE      1004
#define IDM_EXIT             1005

// Константы для ListView
#define IDC_LISTVIEW         2001
#define IDC_STATUSBAR        2002

// Константы для диалогов
#define IDD_TERMINATE        3001
#define IDD_START            3002
#define IDD_PERFORMANCE      3003
#define IDC_PID              3101
#define IDC_COMMAND          3102

// Имя класса окна
const wchar_t* CLASS_NAME_W = L"TaskManagerWindowClassW";

// Цвета для графиков
const COLORREF CPU_COLOR = RGB(235, 77, 75);    // Красный
const COLORREF MEM_COLOR = RGB(106, 176, 76);   // Зеленый
const COLORREF NET_SEND_COLOR = RGB(52, 152, 219);     // Синий для исходящего трафика
const COLORREF NET_RECV_COLOR = RGB(155, 89, 182);     // Фиолетовый для входящего трафика
const COLORREF GRID_COLOR = RGB(200, 200, 200); // Светло-серый
const COLORREF TEXT_COLOR = RGB(50, 50, 50);    // Темно-серый
const COLORREF BG_COLOR = RGB(240, 240, 240);   // Светло-серый фон

// Константа для идентификатора графика сети
#define IDC_NET_GRAPH 3204
#define IDC_STATIC_NETWORK 3205

// Конструктор
TaskManagerUI::TaskManagerUI() : 
    hInstance(NULL), 
    hWnd(NULL),
    hListView(NULL),
    hPerfDialog(NULL),
    bIsPerfDialogOpen(false),
    showBarGraphs(false)
{
    lastUserInteractionTime = GetTickCount();
    LogDebugMessage(L"[INFO] TaskManagerUI создан\n");
}

// Деструктор
TaskManagerUI::~TaskManagerUI() {
    if (hMenu != NULL) {
        DestroyMenu(hMenu);
    }
    
    if (hPerfDialog != NULL && IsWindow(hPerfDialog)) {
        DestroyWindow(hPerfDialog);
    }
}

// Главный цикл программы
void TaskManagerUI::Run() {    
    // Устанавливаем формат юникод для общих контролов
    BOOL bUnicodeFormat = IsWindowUnicode(NULL);
    // Установка локали для корректной работы с Unicode
    try {
        std::locale::global(std::locale("en_US.UTF-8")); // Или "ru_RU.UTF-8" если система поддерживает
    } catch (const std::runtime_error&) {
        // Фоллбэк, если UTF-8 не поддерживается
        setlocale(LC_ALL, "C"); 
        OutputDebugStringA("Failed to set UTF-8 locale, falling back to C locale.\n");
    }

    // Инициализируем GDI+
    GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);
    
    // Убедимся что история производительности пуста в начале
    perfHistory.Clear();
    LogDebugMessage(L"[INIT] История производительности очищена\n");

    // Инициализируем Common Controls
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_LISTVIEW_CLASSES | ICC_BAR_CLASSES;
    InitCommonControlsEx(&icex);

    // Инициализируем окно
    if (!InitWindow()) {
        MessageBoxW(NULL, L"Failed to initialize window!", L"Error", MB_ICONERROR);
        GdiplusShutdown(gdiplusToken);
        return;
    }

    // Цикл обработки сообщений
    MSG msg = {};
    while (GetMessageW(&msg, NULL, 0, 0)) {
        // Проверяем, не является ли сообщение для диалога производительности
        if (hPerfDialog == NULL || !IsDialogMessageW(hPerfDialog, &msg)) {
        TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    
    // Освобождаем ресурсы GDI+
    GdiplusShutdown(gdiplusToken);
}

// Инициализация окна
bool TaskManagerUI::InitWindow() {
    // Устанавливаем кодовую страницу UTF-8 для корректного отображения русских символов в Windows
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    
    hInstance = GetModuleHandleW(NULL); // Получаем HINSTANCE для Unicode
    RegisterWindowClass(hInstance);

    hWnd = CreateMainWindow(hInstance);
    if (!hWnd) {
        return false;
    }

    InitializeMenu();
    InitializeListView();
    InitializeStatusBar();

    ShowWindow(hWnd, SW_SHOW);
    UpdateWindow(hWnd);

    // Запускаем таймер для обновления данных (например, раз в секунду)
    SetTimer(hWnd, 1, 1000, NULL);
    
    UpdatePerformanceData(); // Первичное обновление
    RefreshProcessList(); // Первичное обновление списка

    return true;
}

// Регистрация класса окна
void TaskManagerUI::RegisterWindowClass(HINSTANCE hInstance) {
    WNDCLASSEXW wcex = {};
    wcex.cbSize = sizeof(WNDCLASSEXW);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WindowProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = NULL;
    wcex.lpszClassName = L"TaskManagerClass";
    wcex.hIconSm = LoadIcon(NULL, IDI_APPLICATION);

    if (!RegisterClassExW(&wcex)) {
         MessageBoxW(NULL, L"Ошибка при регистрации класса окна!", L"Ошибка", MB_ICONERROR);
    }
}

// Создание главного окна
HWND TaskManagerUI::CreateMainWindow(HINSTANCE hInstance) {
    HWND hwnd = CreateWindowExW(
        0,
        L"TaskManagerClass",
        L"Диспетчер задач",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        800, 600,
        NULL,
        NULL,
        hInstance,
        this
    );

    // Устанавливаем кодовую страницу UTF-8 для корректного отображения русских символов
    SetWindowTextW(hwnd, L"Диспетчер задач");

    return hwnd;
}

// Инициализация меню
void TaskManagerUI::InitializeMenu() {
    hMenu = CreateMenu();
    HMENU hFileMenu = CreatePopupMenu();
    HMENU hViewMenu = CreatePopupMenu();

    // Меню "Файл"
    AppendMenuW(hFileMenu, MF_STRING, IDM_REFRESH, L"Обновить");
    AppendMenuW(hFileMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hFileMenu, MF_STRING, IDM_TERMINATE, L"Завершить процесс...");
    AppendMenuW(hFileMenu, MF_STRING, IDM_START, L"Запустить новый процесс...");
    AppendMenuW(hFileMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hFileMenu, MF_STRING, IDM_EXIT, L"Выход");

    // Меню "Вид"
    AppendMenuW(hViewMenu, MF_STRING, IDM_PERFORMANCE, L"Производительность");

    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hFileMenu, L"Файл");
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hViewMenu, L"Вид");

    SetMenu(hWnd, hMenu);
}

// Инициализация ListView
void TaskManagerUI::InitializeListView() {
    hListView = CreateWindowExW(
        WS_EX_CLIENTEDGE,            // Расширенный стиль
        WC_LISTVIEWW,                // Имя класса ListView (Unicode)
        L"",                         // Текст (не используется)
        WS_VISIBLE | WS_CHILD | WS_BORDER | LVS_REPORT | LVS_SHOWSELALWAYS | LVS_SINGLESEL,
        0, 0, 0, 0,                  // Позиция и размер (устанавливаются в WM_SIZE)
        hWnd,                        // Родительское окно
        (HMENU)IDC_LISTVIEW,         // ID
        hInstance,                   // Дескриптор приложения
        NULL                         // Доп. параметры
    );

    // Настройка поддержки Unicode для ListView
    SendMessageW(hListView, CCM_SETUNICODEFORMAT, TRUE, 0);
    // Устанавливаем кодовую страницу CP_UTF8 для корректного отображения русских символов
    SendMessageW(hListView, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), MAKELPARAM(TRUE, 0));

    // Установка стиля отчета для ListView
    SendMessageW(hListView, LVM_SETEXTENDEDLISTVIEWSTYLE, 0, 
                LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);
    
    SetProcessListColumns();
}

// Настройка колонок для ListView
void TaskManagerUI::SetProcessListColumns() {
    LVCOLUMNW lvc = {0};
    lvc.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
    lvc.fmt = LVCFMT_LEFT;

    // Меняем порядок колонок: Имя процесса, PID, ...
    struct ColumnData { int id; int width; const wchar_t* name; };
    ColumnData columns[] = {
        {0, 250, L"Имя процесса"}, // Было {1, 250, L"Имя процесса"}
        {1, 70,  L"PID"},          // Было {0, 70,  L"PID"}
        {2, 80,  L"ЦП (%)"},
        {3, 120, L"Память"},
        {4, 120, L"Статус"},
        {5, 100, L"Родительский PID"}
    };

    for (const auto& col : columns) {
        lvc.iSubItem = col.id;
        lvc.cx = col.width;
        lvc.pszText = const_cast<LPWSTR>(col.name);
        
        // Заменяем ListView_InsertColumn на прямой вызов SendMessage
        if (SendMessageW(hListView, LVM_INSERTCOLUMNW, col.id, (LPARAM)&lvc) == -1) {
            MessageBoxW(hWnd, L"Не удалось добавить колонку", L"Ошибка", MB_OK);
        }
    }
}

// Инициализация статус-бара
void TaskManagerUI::InitializeStatusBar() {
    hStatusBar = CreateWindowExW(
        0, STATUSCLASSNAMEW, NULL,
        SBARS_SIZEGRIP | WS_CHILD | WS_VISIBLE, 
        0, 0, 0, 0, 
        hWnd, (HMENU)IDC_STATUSBAR, hInstance, NULL
    );
    UpdateStatusBar();
}

// Обновление статус-бара
void TaskManagerUI::UpdateStatusBar() {
    wchar_t statusText[256];
    SIZE_T totalPhysMem = 0, availPhysMem = 0;
    SIZE_T usedMem = processManager.GetSystemMemoryInfo(totalPhysMem, availPhysMem);
    float cpuUsage = processManager.GetSystemCpuUsage();
    int processCount = static_cast<int>(processManager.GetProcesses().size());
    
    swprintf_s(statusText, L"Процессы: %d | ЦП: %.1f%% | Память: %s / %s",
            processCount,
            cpuUsage,
            FormatMemorySize(usedMem).c_str(),
            FormatMemorySize(totalPhysMem).c_str());
    
    SendMessageW(hStatusBar, SB_SETTEXTW, 0, (LPARAM)statusText); 
}

// Обновление списка процессов
void TaskManagerUI::RefreshProcessList() {
    LogDebugMessage(L"[DEBUG] Обновляем список процессов\n");
    
    HWND hList = GetDlgItem(hWnd, IDC_LISTVIEW);
    if (!hList) return;
    
    // Сохраняем текущую позицию прокрутки и выбранный элемент перед обновлением
    int selectedIndex = ListView_GetNextItem(hList, -1, LVNI_SELECTED);
    int scrollPos = ListView_GetTopIndex(hList);
    
    // Получаем выбранный процесс ID, если есть выбранный элемент
    DWORD selectedPID = 0;
    if (selectedIndex != -1) {
        LVITEM lvItem = {0};
        lvItem.mask = LVIF_PARAM;
        lvItem.iItem = selectedIndex;
        lvItem.iSubItem = 0;
        ListView_GetItem(hList, &lvItem);
        selectedPID = static_cast<DWORD>(lvItem.lParam);
    }
    
    // Обновляем информацию о процессах
    processManager.UpdateProcessList();
    
    // Используем LockWindowUpdate для предотвращения мерцания и множественных перерисовок
    LockWindowUpdate(hList);
    
    // Сохраняем текущее количество элементов
    int itemCount = ListView_GetItemCount(hList);
    
    // Получаем идентификаторы всех прежних процессов для их поиска
    std::unordered_map<DWORD, int> oldPIDs;
    for (int i = 0; i < itemCount; i++) {
        LVITEM item = {0};
        item.mask = LVIF_PARAM;
        item.iItem = i;
        item.iSubItem = 0;
        if (ListView_GetItem(hList, &item)) {
            oldPIDs[static_cast<DWORD>(item.lParam)] = i;
        }
    }
    
    // Временно отключаем перерисовку
    SendMessageW(hList, WM_SETREDRAW, FALSE, 0);
    
    const std::vector<ProcessInfo>& processes = processManager.GetProcesses();
    std::unordered_map<DWORD, int> newIndexMap;
    int newSelectedIndex = -1;
    
    // Проходим по всем процессам и обновляем элементы списка
    for (size_t i = 0; i < processes.size(); i++) {
        const ProcessInfo& proc = processes[i];
        newIndexMap[proc.pid] = static_cast<int>(i);
        
        // Сохраняем новый индекс выбранного элемента
        if (proc.pid == selectedPID) {
            newSelectedIndex = static_cast<int>(i);
        }

        // Проверяем, существует ли уже этот процесс в списке
        auto it = oldPIDs.find(proc.pid);

        LVITEM lvItem = {0};
        lvItem.mask = LVIF_TEXT | LVIF_PARAM;
        lvItem.iItem = static_cast<int>(i); // Предварительный индекс для нового элемента
        lvItem.lParam = static_cast<LPARAM>(proc.pid);
        lvItem.pszText = const_cast<LPWSTR>(proc.name.c_str()); // Имя процесса теперь в колонке 0
        lvItem.iSubItem = 0; // Указываем, что это текст для колонки 0

        if (it != oldPIDs.end()) {
            // Обновляем существующий элемент
            lvItem.iItem = it->second; // Используем старый индекс
            // Устанавливаем Имя (колонка 0) и lParam
            SendMessageW(hList, LVM_SETITEMW, 0, (LPARAM)&lvItem);

            // Обновляем данные в подэлементах
            wchar_t pidStr[32];
            swprintf_s(pidStr, L"%lu", proc.pid);
            ListView_SetItemText(hList, lvItem.iItem, 1, pidStr); // PID теперь в колонке 1

            wchar_t cpuStr[32];
            swprintf_s(cpuStr, L"%.1f%%", proc.cpuUsage);
            ListView_SetItemText(hList, lvItem.iItem, 2, cpuStr);

            std::wstring memStr = FormatMemorySize(proc.memoryUsage);
            ListView_SetItemText(hList, lvItem.iItem, 3, const_cast<LPWSTR>(memStr.c_str()));

            ListView_SetItemText(hList, lvItem.iItem, 4, const_cast<LPWSTR>(proc.status.c_str()));

            wchar_t parentPidStr[32];
            swprintf_s(parentPidStr, L"%lu", proc.parentPid);
            ListView_SetItemText(hList, lvItem.iItem, 5, parentPidStr);

        } else {
            // Добавляем новый элемент
            // Вставляем Имя (колонка 0) и lParam
            int idx = static_cast<int>(SendMessageW(hList, LVM_INSERTITEMW, 0, (LPARAM)&lvItem));

            if (idx != -1) {
                wchar_t pidStr[32];
                swprintf_s(pidStr, L"%lu", proc.pid);
                ListView_SetItemText(hList, idx, 1, pidStr); // PID теперь в колонке 1

                wchar_t cpuStr[32];
                swprintf_s(cpuStr, L"%.1f%%", proc.cpuUsage);
                ListView_SetItemText(hList, idx, 2, cpuStr);

                std::wstring memStr = FormatMemorySize(proc.memoryUsage);
                ListView_SetItemText(hList, idx, 3, const_cast<LPWSTR>(memStr.c_str()));

                ListView_SetItemText(hList, idx, 4, const_cast<LPWSTR>(proc.status.c_str()));

                wchar_t parentPidStr[32];
                swprintf_s(parentPidStr, L"%lu", proc.parentPid);
                ListView_SetItemText(hList, idx, 5, parentPidStr);
            }
        }
    }
    
    // Удаляем процессы, которых больше нет
    for (int i = itemCount - 1; i >= 0; i--) {
        LVITEM item = {0};
        item.mask = LVIF_PARAM;
        item.iItem = i;
        item.iSubItem = 0;
        if (ListView_GetItem(hList, &item)) {
            DWORD pid = static_cast<DWORD>(item.lParam);
            if (newIndexMap.find(pid) == newIndexMap.end()) {
                ListView_DeleteItem(hList, i);
            }
        }
    }
    
    // --- Логика восстановления выделения и прокрутки --- 
    if (newSelectedIndex != -1) {
        // Восстанавливаем выделение
        ListView_SetItemState(hList, newSelectedIndex, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
        // Обеспечиваем видимость, только если это необходимо (оптимизация)
        if (!ListView_EnsureVisible(hList, newSelectedIndex, TRUE)) { // TRUE - разрешить частичную видимость
            // Если элемент уже виден, пытаемся сохранить позицию прокрутки
            if (scrollPos >= 0 && scrollPos < ListView_GetItemCount(hList)) {
                 ListView_Scroll(hList, 0, (scrollPos - ListView_GetTopIndex(hList)) * ListView_GetItemHeight(hList));
            }
        }
    } else if (scrollPos >= 0 && scrollPos < ListView_GetItemCount(hList)) {
        // Если не было выбрано элемента или он исчез, восстанавливаем позицию прокрутки
        // Проверяем, чтобы не прокручивать за пределы списка
        int currentTop = ListView_GetTopIndex(hList);
        int itemCountAfter = ListView_GetItemCount(hList);
        // Заменяем std::min/max на явные проверки из-за возможного конфликта/ошибки линтера
        int countPerPage = ListView_GetCountPerPage(hList);
        int maxTopIndex = (itemCountAfter > countPerPage) ? (itemCountAfter - countPerPage) : 0;
        int targetScroll = scrollPos;
        if (targetScroll > maxTopIndex) targetScroll = maxTopIndex;
        if (targetScroll < 0) targetScroll = 0;
        if (targetScroll != currentTop) {
            ListView_Scroll(hList, 0, (targetScroll - currentTop) * ListView_GetItemHeight(hList));
        }
    } 
    // --- Конец логики восстановления ---

    // Включаем перерисовку
    SendMessageW(hList, WM_SETREDRAW, TRUE, 0);
    // Разблокируем окно
    LockWindowUpdate(NULL);
    // Форсируем перерисовку элемента
    InvalidateRect(hList, NULL, TRUE);
    
    // Обновляем строку состояния
    UpdateStatusBar();
}

// Добавление процесса в ListView
void TaskManagerUI::AddProcessToListView(const ProcessInfo* process, int index) {
    LVITEMW lvi = {0};
    lvi.mask = LVIF_TEXT;
    lvi.iItem = index;
    
    wchar_t buffer[260];
    
    // PID
    swprintf_s(buffer, L"%lu", process->pid);
    lvi.iSubItem = 0;
    lvi.pszText = buffer;
    // Заменяем ListView_InsertItem на прямой вызов SendMessage
    SendMessageW(hListView, LVM_INSERTITEMW, 0, (LPARAM)&lvi);
    
    // Проверяем и преобразуем имя процесса, если нужно
    std::wstring processName = process->name;
    if (processName.empty()) {
        processName = L"[Неизвестный процесс]";
    } else if (!IsTextUnicode(processName.c_str(), static_cast<int>(processName.length() * sizeof(wchar_t)), NULL)) {
        processName = L"[Некорректное имя]";
    }
    
    // Имя процесса
    lvi.iSubItem = 1;
    lvi.pszText = const_cast<LPWSTR>(processName.c_str());
    // Заменяем ListView_SetItem на прямой вызов SendMessage
    SendMessageW(hListView, LVM_SETITEMTEXTW, index, (LPARAM)&lvi);
    
    // CPU (%)
    swprintf_s(buffer, L"%.1f", static_cast<float>(process->cpuUsage));
    lvi.iSubItem = 2;
    lvi.pszText = buffer;
    SendMessageW(hListView, LVM_SETITEMTEXTW, index, (LPARAM)&lvi);
    
    // Память
    lvi.iSubItem = 3;
    std::wstring memoryStr = FormatMemorySize(process->memoryUsage);
    lvi.pszText = const_cast<LPWSTR>(memoryStr.c_str());
    SendMessageW(hListView, LVM_SETITEMTEXTW, index, (LPARAM)&lvi);
    
    // Получаем и проверяем статус
    std::wstring statusText = process->status;
    if (statusText.empty() || !IsTextUnicode(statusText.c_str(), static_cast<int>(statusText.length() * sizeof(wchar_t)), NULL)) {
        // Если статус пустой или некорректный, устанавливаем безопасное значение
        statusText = L"---";
    }
    
    // Статус
    lvi.iSubItem = 4;
    // Убедимся, что текст корректный и правильной длины для UI
    if (statusText.length() > 15) {
        statusText = statusText.substr(0, 15); // Обрезаем, если слишком длинный
    }
    lvi.pszText = const_cast<LPWSTR>(statusText.c_str());
    SendMessageW(hListView, LVM_SETITEMTEXTW, index, (LPARAM)&lvi);
    
    // PID родителя
    swprintf_s(buffer, L"%lu", process->parentPid);
    lvi.iSubItem = 5;
    lvi.pszText = buffer;
    SendMessageW(hListView, LVM_SETITEMTEXTW, index, (LPARAM)&lvi);
}

// Показ диалога завершения процесса
void TaskManagerUI::ShowTerminateProcessDialog() {
    int selectedItem = (int)SendMessageW(hListView, LVM_GETNEXTITEM, -1, MAKELPARAM(LVNI_SELECTED, 0));
    if (selectedItem == -1) {
        MessageBoxW(hWnd, L"Пожалуйста, выберите процесс для завершения.", L"Завершение процесса", MB_OK | MB_ICONINFORMATION);
        return;
    }
    
    wchar_t nameBuffer[MAX_PATH] = {0};

    LVITEMW lvItem = {0};

    // Получаем lParam (где хранится PID)
    lvItem.mask = LVIF_PARAM;
    lvItem.iItem = selectedItem;
    lvItem.iSubItem = 0; // SubItem не важен для lParam
    if (!SendMessageW(hListView, LVM_GETITEMW, selectedItem, (LPARAM)&lvItem)) {
        MessageBoxW(hWnd, L"Не удалось получить информацию о процессе.", L"Ошибка", MB_ICONERROR);
        return;
    }
    DWORD pidToTerminate = static_cast<DWORD>(lvItem.lParam);

    // Получаем имя процесса (для сообщения)
    lvItem.mask = LVIF_TEXT;
    lvItem.iSubItem = 0; // Колонка с именем
    lvItem.pszText = nameBuffer;
    lvItem.cchTextMax = sizeof(nameBuffer) / sizeof(wchar_t);
    SendMessageW(hListView, LVM_GETITEMTEXTW, selectedItem, (LPARAM)&lvItem);

    // Проверяем, что PID не 0 (на всякий случай)
    if (pidToTerminate == 0) {
        MessageBoxW(hWnd, L"Невозможно завершить процесс с PID 0.", L"Ошибка", MB_ICONERROR);
        return;
    }

    wchar_t message[512];
    swprintf_s(message, L"Вы уверены, что хотите завершить процесс '%s' (PID: %lu)?", nameBuffer, pidToTerminate);
    
    int result = MessageBoxW(hWnd, message, L"Подтверждение завершения процесса", MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2);
    
    if (result == IDYES) {
        bool success = processManager.TerminateProcess(pidToTerminate);
                    if (success) {
            MessageBoxW(hWnd, L"Процесс успешно завершен.", L"Информация", MB_ICONINFORMATION);
                        RefreshProcessList();
                    } else {
            DWORD error = GetLastError();
            wchar_t errorMsg[256];
            FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                           NULL, error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                           errorMsg, sizeof(errorMsg)/sizeof(wchar_t), NULL);
            wchar_t finalMsg[512];
            swprintf_s(finalMsg, L"Не удалось завершить процесс (PID: %lu).\nОшибка %lu: %s", pidToTerminate, error, errorMsg);
            MessageBoxW(hWnd, finalMsg, L"Ошибка", MB_ICONERROR);
        }
    }
}

// Показ диалога запуска процесса
void TaskManagerUI::ShowStartProcessDialog() {
    wchar_t command[MAX_PATH] = {0};

    OPENFILENAMEW ofn = {0};
    ofn.lStructSize = sizeof(OPENFILENAMEW);
        ofn.hwndOwner = hWnd;
    ofn.lpstrFilter = L"Исполняемые файлы (*.exe)\0*.exe\0Все файлы (*.*)\0*.*\0";
        ofn.lpstrFile = command;
        ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_PATHMUSTEXIST;
    ofn.lpstrTitle = L"Запустить новый процесс";
    ofn.lpstrDefExt = L"exe";
        
    if (GetOpenFileNameW(&ofn)) {
            bool success = processManager.StartNewProcess(command);
            if (success) {
            MessageBoxW(hWnd, L"Процесс успешно запущен.", L"Информация", MB_ICONINFORMATION);
                RefreshProcessList();
            } else {
            DWORD error = GetLastError();
            wchar_t errorMsg[256];
            FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                           NULL, error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                           errorMsg, sizeof(errorMsg)/sizeof(wchar_t), NULL);
             wchar_t finalMsg[MAX_PATH + 256];
            swprintf_s(finalMsg, L"Не удалось запустить процесс:\n%s\nОшибка %lu: %s", command, error, errorMsg);
            MessageBoxW(hWnd, finalMsg, L"Ошибка", MB_ICONERROR);
        }
    }
}

// Обновление данных производительности
void TaskManagerUI::UpdatePerformanceData() {
    // Не обновляем если диалог не открыт или не создан
    if (!bIsPerfDialogOpen || !hPerfDialog || !IsWindow(hPerfDialog)) {
        return;
    }
    
    // Получаем данные системы для отладки
    SystemInfo sysInfo = processManager.GetSystemInfo();
    LogDebugMessage(L"[DEBUG] Производительность: CPU=%.1f%%, Память=%.1f%%, Сеть: отправка=%.1f КБ/с, прием=%.1f КБ/с\n",
        sysInfo.cpuUsage, sysInfo.memoryUsagePercent, 
        sysInfo.networkSendSpeedKBps, sysInfo.networkReceiveSpeedKBps);
    
    // Обновляем строку состояния в основном окне
    UpdateStatusBar();
}

// Рисование графика CPU
void TaskManagerUI::DrawCpuGraph(HDC hdc, RECT rect) {
    LogDebugMessage(L"[DRAW] Отрисовка графика CPU, точек данных: %d\n", 
                   static_cast<int>(perfHistory.cpuHistory.size()));
    
    // Заполняем фон
    HBRUSH hBgBrush = CreateSolidBrush(BG_COLOR);
    FillRect(hdc, &rect, hBgBrush);
    DeleteObject(hBgBrush);
    
    // Вычисляем размеры области для рисования
    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;
    int padding = 10; // Отступ от краев
    
    // Рисуем сетку
    HPEN hGridPen = CreatePen(PS_DOT, 1, GRID_COLOR);
    HPEN hOldPen = (HPEN)SelectObject(hdc, hGridPen);
    
    // Горизонтальные линии сетки - шаг 10%
    for (int i = 1; i < 10; i++) {
        int y = rect.bottom - padding - (i * (height - 2 * padding) / 10);
        MoveToEx(hdc, rect.left + padding, y, NULL);
        LineTo(hdc, rect.right - padding, y);
        
        // Подписи процентов
        wchar_t label[10];
        swprintf_s(label, L"%d%%", i * 10);
        SetTextColor(hdc, TEXT_COLOR);
        SetBkMode(hdc, TRANSPARENT);
        TextOutW(hdc, rect.left + 2, y - 7, label, static_cast<int>(wcslen(label)));
    }
    
    // Рисуем график только если есть данные
    if (perfHistory.cpuHistory.size() > 1) {
        HPEN hDataPen = CreatePen(PS_SOLID, 2, CPU_COLOR);
        SelectObject(hdc, hDataPen);
        
        // Определяем максимальное количество точек
        int pointCount = static_cast<int>(perfHistory.cpuHistory.size());
        int maxPoints = std::min(pointCount, width - 2 * padding);
        
        // Начальный индекс в истории
        int startIndex = std::max(0, pointCount - maxPoints);
        
        // Первая точка
        int x1 = rect.left + padding;
        int y1 = rect.bottom - padding - static_cast<int>((perfHistory.cpuHistory[startIndex] * (height - 2 * padding)) / 100.0);
        MoveToEx(hdc, x1, y1, NULL);
        
        // Соединяем все точки линиями
        for (int i = 1; i < maxPoints; i++) {
            int x2 = rect.left + padding + (i * (width - 2 * padding) / maxPoints);
            double cpuValue = perfHistory.cpuHistory[startIndex + i];
            // Ограничиваем значение между 0 и 100
            cpuValue = std::max(0.0, std::min(100.0, cpuValue));
            int y2 = rect.bottom - padding - static_cast<int>((cpuValue * (height - 2 * padding)) / 100.0);
            LineTo(hdc, x2, y2);
        }
        
        // Освобождаем ресурсы
        SelectObject(hdc, hOldPen);
        DeleteObject(hDataPen);
    } else {
        // Если нет данных, пишем сообщение
        SetTextColor(hdc, RGB(150, 150, 150));
        TextOutW(hdc, rect.left + padding, rect.top + height/2 - 10, L"Нет данных для отображения", 24);
    }
    
    // Рисуем рамку
    HPEN hBorderPen = CreatePen(PS_SOLID, 1, RGB(150, 150, 150));
    SelectObject(hdc, hBorderPen);
    Rectangle(hdc, rect.left, rect.top, rect.right, rect.bottom);
    SelectObject(hdc, hOldPen);
    DeleteObject(hGridPen);
    DeleteObject(hBorderPen);
    
    // Добавляем текст с текущим значением
    if (!perfHistory.cpuHistory.empty()) {
        double currentValue = perfHistory.cpuHistory.back();
        wchar_t valueText[32];
        swprintf_s(valueText, L"Текущее значение: %.1f%%", currentValue);
        SetTextColor(hdc, TEXT_COLOR);
        TextOutW(hdc, rect.left + padding, rect.top + padding, valueText, static_cast<int>(wcslen(valueText)));
    }
}

// Рисование графика памяти
void TaskManagerUI::DrawMemGraph(HDC hdc, RECT rect) {
    LogDebugMessage(L"[DRAW] Отрисовка графика памяти, точек данных: %d\n", 
                   static_cast<int>(perfHistory.memHistory.size()));
    
    // Заполняем фон
    HBRUSH hBgBrush = CreateSolidBrush(BG_COLOR);
    FillRect(hdc, &rect, hBgBrush);
    DeleteObject(hBgBrush);
    
    // Вычисляем размеры области для рисования
    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;
    int padding = 10; // Отступ от краев
    
    // Рисуем сетку
    HPEN hGridPen = CreatePen(PS_DOT, 1, GRID_COLOR);
    HPEN hOldPen = (HPEN)SelectObject(hdc, hGridPen);
    
    // Горизонтальные линии сетки - шаг 10%
    for (int i = 1; i < 10; i++) {
        int y = rect.bottom - padding - (i * (height - 2 * padding) / 10);
        MoveToEx(hdc, rect.left + padding, y, NULL);
        LineTo(hdc, rect.right - padding, y);
        
        // Подписи процентов
        wchar_t label[10];
        swprintf_s(label, L"%d%%", i * 10);
        SetTextColor(hdc, TEXT_COLOR);
        SetBkMode(hdc, TRANSPARENT);
        TextOutW(hdc, rect.left + 2, y - 7, label, static_cast<int>(wcslen(label)));
    }
    
    // Рисуем график только если есть данные
    if (perfHistory.memHistory.size() > 1) {
        HPEN hDataPen = CreatePen(PS_SOLID, 2, MEM_COLOR);
        SelectObject(hdc, hDataPen);
        
        // Определяем максимальное количество точек
        int pointCount = static_cast<int>(perfHistory.memHistory.size());
        int maxPoints = std::min(pointCount, width - 2 * padding);
        
        // Начальный индекс в истории
        int startIndex = std::max(0, pointCount - maxPoints);
        
        // Первая точка
        int x1 = rect.left + padding;
        int y1 = rect.bottom - padding - static_cast<int>((perfHistory.memHistory[startIndex] * (height - 2 * padding)) / 100.0);
        MoveToEx(hdc, x1, y1, NULL);
        
        // Соединяем все точки линиями
        for (int i = 1; i < maxPoints; i++) {
            int x2 = rect.left + padding + (i * (width - 2 * padding) / maxPoints);
            double memValue = perfHistory.memHistory[startIndex + i];
            // Ограничиваем значение между 0 и 100
            memValue = std::max(0.0, std::min(100.0, memValue));
            int y2 = rect.bottom - padding - static_cast<int>((memValue * (height - 2 * padding)) / 100.0);
            LineTo(hdc, x2, y2);
        }
        
        // Освобождаем ресурсы
        SelectObject(hdc, hOldPen);
        DeleteObject(hDataPen);
    } else {
        // Если нет данных, пишем сообщение
        SetTextColor(hdc, RGB(150, 150, 150));
        TextOutW(hdc, rect.left + padding, rect.top + height/2 - 10, L"Нет данных для отображения", 24);
    }
    
    // Рисуем рамку
    HPEN hBorderPen = CreatePen(PS_SOLID, 1, RGB(150, 150, 150));
    SelectObject(hdc, hBorderPen);
    Rectangle(hdc, rect.left, rect.top, rect.right, rect.bottom);
    SelectObject(hdc, hOldPen);
    DeleteObject(hGridPen);
    DeleteObject(hBorderPen);
    
    // Добавляем текст с текущим значением
    if (!perfHistory.memHistory.empty()) {
        double currentValue = perfHistory.memHistory.back();
        wchar_t valueText[32];
        swprintf_s(valueText, L"Текущее значение: %.1f%%", currentValue);
        SetTextColor(hdc, TEXT_COLOR);
        TextOutW(hdc, rect.left + padding, rect.top + padding, valueText, static_cast<int>(wcslen(valueText)));
    }
}

// Рисование графика сети
void TaskManagerUI::DrawNetworkGraph(HDC hdc, RECT rect) {
    LogDebugMessage(L"[DRAW] Отрисовка графика сети, точек отправки: %d, точек приема: %d\n", 
                   static_cast<int>(perfHistory.netSendHistory.size()),
                   static_cast<int>(perfHistory.netReceiveHistory.size()));
    
    // Заполняем фон
    HBRUSH hBgBrush = CreateSolidBrush(BG_COLOR);
    FillRect(hdc, &rect, hBgBrush);
    DeleteObject(hBgBrush);
    
    // Вычисляем размеры области для рисования
    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;
    int padding = 10; // Отступ от краев
    
    // Рисуем сетку
    HPEN hGridPen = CreatePen(PS_DOT, 1, GRID_COLOR);
    HPEN hOldPen = (HPEN)SelectObject(hdc, hGridPen);
    
    // Находим максимальное значение скорости для масштабирования
    double maxSendValue = 0.1; // Минимальное значение для случая, если нет данных или все 0
    double maxRecvValue = 0.1; 
    
    for (const auto& val : perfHistory.netSendHistory) {
        maxSendValue = std::max(maxSendValue, val);
    }
    
    for (const auto& val : perfHistory.netReceiveHistory) {
        maxRecvValue = std::max(maxRecvValue, val);
    }
    
    double maxValue = std::max(maxSendValue, maxRecvValue);
    
    // Округляем максимум до "красивого" числа для легкости чтения шкалы
    if (maxValue <= 10.0) {
        maxValue = 10.0; // 10 КБ/с
    } else if (maxValue <= 100.0) {
        maxValue = 100.0; // 100 КБ/с
    } else if (maxValue <= 1000.0) {
        maxValue = 1000.0; // 1 МБ/с
    } else if (maxValue <= 10000.0) {
        maxValue = 10000.0; // 10 МБ/с
    } else {
        maxValue = ceil(maxValue / 10000.0) * 10000.0; // Округлить до следующего целого числа десятков МБ/с
    }
    
    // Горизонтальные линии сетки - 5 линий
    for (int i = 1; i <= 5; i++) {
        int y = rect.bottom - padding - (i * (height - 2 * padding) / 5);
        MoveToEx(hdc, rect.left + padding, y, NULL);
        LineTo(hdc, rect.right - padding, y);
        
        // Подписи скорости
        wchar_t label[32];
        double value = (i * maxValue / 5);
        if (value < 1000.0) {
            swprintf_s(label, L"%.1f КБ/с", value);
        } else {
            swprintf_s(label, L"%.1f МБ/с", value / 1024.0);
        }
        
        SetTextColor(hdc, TEXT_COLOR);
        SetBkMode(hdc, TRANSPARENT);
        TextOutW(hdc, rect.left + 2, y - 7, label, static_cast<int>(wcslen(label)));
    }
    
    // Если нет данных, выводим сообщение
    if (perfHistory.netSendHistory.size() <= 1 && perfHistory.netReceiveHistory.size() <= 1) {
        SetTextColor(hdc, RGB(150, 150, 150));
        TextOutW(hdc, rect.left + padding, rect.top + height/2 - 10, L"Нет данных для отображения", 24);
        
        // Рисуем рамку и возвращаемся
        HPEN hBorderPen = CreatePen(PS_SOLID, 1, RGB(150, 150, 150));
        SelectObject(hdc, hBorderPen);
        Rectangle(hdc, rect.left, rect.top, rect.right, rect.bottom);
        SelectObject(hdc, hOldPen);
        DeleteObject(hGridPen);
        DeleteObject(hBorderPen);
        return;
    }
    
    // Количество точек для рисования
    int maxPoints = width - 2 * padding;
    
    // Рисуем график исходящего трафика
    if (perfHistory.netSendHistory.size() > 1) {
        HPEN hSendPen = CreatePen(PS_SOLID, 2, NET_SEND_COLOR);
        SelectObject(hdc, hSendPen);
        
        int pointCount = static_cast<int>(perfHistory.netSendHistory.size());
        int pointsToUse = std::min(pointCount, maxPoints);
        int startIndex = std::max(0, pointCount - pointsToUse);
        
        // Первая точка
        int x1 = rect.left + padding;
        int y1 = rect.bottom - padding - static_cast<int>((perfHistory.netSendHistory[startIndex] * (height - 2 * padding)) / maxValue);
        MoveToEx(hdc, x1, y1, NULL);
        
        // Соединяем все точки линиями
        for (int i = 1; i < pointsToUse; i++) {
            int x2 = rect.left + padding + (i * (width - 2 * padding) / pointsToUse);
            double sendValue = perfHistory.netSendHistory[startIndex + i];
            // Ограничиваем значение между 0 и maxValue
            sendValue = std::max(0.0, std::min(maxValue, sendValue));
            int y2 = rect.bottom - padding - static_cast<int>((sendValue * (height - 2 * padding)) / maxValue);
            LineTo(hdc, x2, y2);
        }
        
        SelectObject(hdc, hOldPen);
        DeleteObject(hSendPen);
    }
    
    // Рисуем график входящего трафика
    if (perfHistory.netReceiveHistory.size() > 1) {
        HPEN hRecvPen = CreatePen(PS_SOLID, 2, NET_RECV_COLOR);
        SelectObject(hdc, hRecvPen);
        
        int pointCount = static_cast<int>(perfHistory.netReceiveHistory.size());
        int pointsToUse = std::min(pointCount, maxPoints);
        int startIndex = std::max(0, pointCount - pointsToUse);
        
        // Первая точка
        int x1 = rect.left + padding;
        int y1 = rect.bottom - padding - static_cast<int>((perfHistory.netReceiveHistory[startIndex] * (height - 2 * padding)) / maxValue);
        MoveToEx(hdc, x1, y1, NULL);
        
        // Соединяем все точки линиями
        for (int i = 1; i < pointsToUse; i++) {
            int x2 = rect.left + padding + (i * (width - 2 * padding) / pointsToUse);
            double recvValue = perfHistory.netReceiveHistory[startIndex + i];
            // Ограничиваем значение между 0 и maxValue
            recvValue = std::max(0.0, std::min(maxValue, recvValue));
            int y2 = rect.bottom - padding - static_cast<int>((recvValue * (height - 2 * padding)) / maxValue);
            LineTo(hdc, x2, y2);
        }
        
        SelectObject(hdc, hOldPen);
        DeleteObject(hRecvPen);
    }
    
    // Рисуем рамку
    HPEN hBorderPen = CreatePen(PS_SOLID, 1, RGB(150, 150, 150));
    SelectObject(hdc, hBorderPen);
    Rectangle(hdc, rect.left, rect.top, rect.right, rect.bottom);
    SelectObject(hdc, hOldPen);
    DeleteObject(hGridPen);
    DeleteObject(hBorderPen);
    
    // Добавляем легенду
    if (!perfHistory.netSendHistory.empty() && !perfHistory.netReceiveHistory.empty()) {
        double sendValue = perfHistory.netSendHistory.back();
        double recvValue = perfHistory.netReceiveHistory.back();
        
        wchar_t sendText[64];
        wchar_t recvText[64];
        
        if (sendValue < 1000.0) {
            swprintf_s(sendText, L"Отправка: %.1f КБ/с", sendValue);
        } else {
            swprintf_s(sendText, L"Отправка: %.2f МБ/с", sendValue / 1024.0);
        }
        
        if (recvValue < 1000.0) {
            swprintf_s(recvText, L"Прием: %.1f КБ/с", recvValue);
        } else {
            swprintf_s(recvText, L"Прием: %.2f МБ/с", recvValue / 1024.0);
        }
        
        // Отрисовка легенды для отправки
        HPEN hPen = CreatePen(PS_SOLID, 2, NET_SEND_COLOR);
        SelectObject(hdc, hPen);
        MoveToEx(hdc, rect.left + padding, rect.top + padding + 7, NULL);
        LineTo(hdc, rect.left + padding + 20, rect.top + padding + 7);
        SetTextColor(hdc, TEXT_COLOR);
        TextOutW(hdc, rect.left + padding + 25, rect.top + padding, sendText, static_cast<int>(wcslen(sendText)));
        DeleteObject(hPen);
        
        // Отрисовка легенды для приема
        hPen = CreatePen(PS_SOLID, 2, NET_RECV_COLOR);
        SelectObject(hdc, hPen);
        MoveToEx(hdc, rect.left + padding + 180, rect.top + padding + 7, NULL);
        LineTo(hdc, rect.left + padding + 200, rect.top + padding + 7);
        TextOutW(hdc, rect.left + padding + 205, rect.top + padding, recvText, static_cast<int>(wcslen(recvText)));
        DeleteObject(hPen);
    }
}

// Показ диалога производительности
void TaskManagerUI::ShowPerformanceDialog() {
    if (hPerfDialog != NULL && IsWindow(hPerfDialog)) {
        LogDebugMessage(L"[DEBUG] Окно производительности уже открыто\n");
        // Если окно уже открыто, просто показываем его
        ShowWindow(hPerfDialog, SW_SHOW);
        SetForegroundWindow(hPerfDialog);
        return;
    }

    LogDebugMessage(L"[DEBUG] Создаем окно производительности\n");
    
    // Создаем класс окна для диалога производительности, если он еще не зарегистрирован
    WNDCLASSEXW wcDlg = {0};
    wcDlg.cbSize = sizeof(WNDCLASSEXW);
    wcDlg.style = CS_HREDRAW | CS_VREDRAW;
    wcDlg.lpfnWndProc = DefWindowProcW;
    wcDlg.hInstance = hInstance;
    wcDlg.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
    wcDlg.lpszClassName = L"PerfDialogClass";
    
    // Проверяем, зарегистрирован ли уже класс окна
    WNDCLASSEXW checkClass = {0};
    checkClass.cbSize = sizeof(WNDCLASSEXW);
    if (!GetClassInfoExW(hInstance, L"PerfDialogClass", &checkClass)) {
        RegisterClassExW(&wcDlg);
    }

    bIsPerfDialogOpen = true; // Устанавливаем флаг, что диалог открыт
    
    // Увеличиваем высоту окна для третьего графика (сеть)
    hPerfDialog = CreateWindowExW(
        WS_EX_DLGMODALFRAME,
        L"PerfDialogClass",
        L"Монитор производительности",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        100, 100, 620, 630, // Еще увеличиваем высоту для переключателя
        hWnd,
        NULL,
        hInstance,
        NULL
    );

    if (hPerfDialog == NULL) {
        bIsPerfDialogOpen = false;
        MessageBoxW(hWnd, L"Не удалось создать окно производительности.", L"Ошибка", MB_ICONERROR);
        return;
    }

    // Устанавливаем оконную процедуру после создания окна
    SetWindowLongPtrW(hPerfDialog, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    SetWindowLongPtrW(hPerfDialog, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(PerformanceDialogProc));
    
    // Создаем элементы управления
    HWND hCpuLabel = CreateWindowW(L"STATIC", L"Загрузка ЦП:", WS_CHILD | WS_VISIBLE | SS_LEFT, 
                                  10, 10, 200, 20, hPerfDialog, (HMENU)(IDC_CPU_GRAPH + 100), hInstance, NULL);
    HWND hCpuGraph = CreateWindowW(L"STATIC", NULL, WS_CHILD | WS_VISIBLE | SS_OWNERDRAW | WS_BORDER, 
                                  10, 35, 590, 150, hPerfDialog, (HMENU)IDC_CPU_GRAPH, hInstance, NULL);
    
    HWND hMemLabel = CreateWindowW(L"STATIC", L"Использование памяти:", WS_CHILD | WS_VISIBLE | SS_LEFT, 
                                  10, 200, 200, 20, hPerfDialog, (HMENU)(IDC_MEM_GRAPH + 100), hInstance, NULL);
    HWND hMemDetails = CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_LEFT, 
                                    210, 200, 380, 20, hPerfDialog, (HMENU)IDC_STATIC_MEMORY, hInstance, NULL);
    HWND hMemGraph = CreateWindowW(L"STATIC", NULL, WS_CHILD | WS_VISIBLE | SS_OWNERDRAW | WS_BORDER, 
                                  10, 225, 590, 150, hPerfDialog, (HMENU)IDC_MEM_GRAPH, hInstance, NULL);
                                  
    // Добавляем элементы для графика сети
    HWND hNetLabel = CreateWindowW(L"STATIC", L"Сетевая активность:", WS_CHILD | WS_VISIBLE | SS_LEFT, 
                                  10, 390, 200, 20, hPerfDialog, (HMENU)(IDC_NET_GRAPH + 100), hInstance, NULL);
    HWND hNetDetails = CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_LEFT, 
                                    210, 390, 380, 20, hPerfDialog, (HMENU)IDC_STATIC_NETWORK, hInstance, NULL);
    HWND hNetGraph = CreateWindowW(L"STATIC", NULL, WS_CHILD | WS_VISIBLE | SS_OWNERDRAW | WS_BORDER, 
                                  10, 415, 590, 150, hPerfDialog, (HMENU)IDC_NET_GRAPH, hInstance, NULL);
                                  
    // Добавляем переключатель типа графиков
    HWND hViewSwitch = CreateWindowW(L"BUTTON", L"Переключить на столбчатые графики", 
                                    WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 
                                    200, 575, 220, 25, hPerfDialog, (HMENU)IDC_VIEW_SWITCH, hInstance, NULL);

    if (!hCpuLabel || !hCpuGraph || !hMemLabel || !hMemDetails || !hMemGraph ||
        !hNetLabel || !hNetDetails || !hNetGraph || !hViewSwitch) {
        DestroyWindow(hPerfDialog);
        hPerfDialog = NULL;
        bIsPerfDialogOpen = false;
        MessageBoxW(hWnd, L"Не удалось создать элементы окна производительности.", L"Ошибка", MB_ICONERROR);
        return;
    }

    HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    SendMessageW(hCpuLabel, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessageW(hMemLabel, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessageW(hMemDetails, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessageW(hNetLabel, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessageW(hNetDetails, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessageW(hViewSwitch, WM_SETFONT, (WPARAM)hFont, TRUE);

    ShowWindow(hPerfDialog, SW_SHOW);
    UpdateWindow(hPerfDialog);

    // Устанавливаем таймер для обновления данных с ID, отличным от основного
    SetTimer(hPerfDialog, PERFORMANCE_TIMER, 500, NULL); // Обновляем 2 раза в секунду
    
    // Обновляем первый раз данные для графиков
    SystemInfo sysInfo = processManager.GetSystemInfo();
    perfHistory.AddData(
        sysInfo.cpuUsage, 
        sysInfo.memoryUsagePercent, 
        sysInfo.networkSendSpeedKBps, 
        sysInfo.networkReceiveSpeedKBps
    );
    UpdatePerformanceData();
}

// Обновим обработчик PerformanceDialogProc чтобы включить обработку сетевого графика
INT_PTR CALLBACK PerformanceDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    TaskManagerUI* ui = reinterpret_cast<TaskManagerUI*>(GetWindowLongPtrW(hDlg, GWLP_USERDATA));
    
    if (!ui && (message != WM_CREATE && message != WM_INITDIALOG)) {
        return DefWindowProcW(hDlg, message, wParam, lParam);
    }

    switch (message) {
        case WM_TIMER:
            if (wParam == PERFORMANCE_TIMER && ui) {
                // Получаем новые данные о производительности и добавляем в историю
                SystemInfo sysInfo = ui->processManager.GetSystemInfo();
                
                // Проверяем наличие данных о сети и добавляем в историю
                double netSend = sysInfo.networkSendSpeedKBps >= 0.0 ? sysInfo.networkSendSpeedKBps : 0.0;
                double netRecv = sysInfo.networkReceiveSpeedKBps >= 0.0 ? sysInfo.networkReceiveSpeedKBps : 0.0;
                
                // Логируем полученные данные
                LogDebugMessage(L"[PERF_TIMER] Получены данные: CPU=%.1f%%, Mem=%.1f%%, Net Send=%.1f КБ/с, Net Recv=%.1f КБ/с\n",
                              sysInfo.cpuUsage, sysInfo.memoryUsagePercent, netSend, netRecv);
                
                ui->perfHistory.AddData(
                    sysInfo.cpuUsage, 
                    sysInfo.memoryUsagePercent, 
                    netSend,
                    netRecv
                );
                
                // Обновляем тексты с форматированием до 1 знака после запятой
                wchar_t cpuTextFormatted[100];
                swprintf_s(cpuTextFormatted, L"Загрузка ЦП: %.1f%%", sysInfo.cpuUsage);

                wchar_t memTextFormatted[200];
                std::wstring memUsedStr = ui->FormatMemorySize(sysInfo.memoryUsed);
                std::wstring memTotalStr = ui->FormatMemorySize(sysInfo.totalMemory);
                swprintf_s(memTextFormatted, L"Память: %.1f%% (%s / %s)",
                           sysInfo.memoryUsagePercent,
                           memUsedStr.c_str(),
                           memTotalStr.c_str());
                           
                // Форматируем текст для сетевой активности
                wchar_t netTextFormatted[200];
                if (netSend < 1000.0 && netRecv < 1000.0) {
                    swprintf_s(netTextFormatted, L"Сеть: ↑ %.1f КБ/с, ↓ %.1f КБ/с",
                        netSend, netRecv);
                } else {
                    swprintf_s(netTextFormatted, L"Сеть: ↑ %.2f МБ/с, ↓ %.2f МБ/с",
                        netSend / 1024.0, netRecv / 1024.0);
                }

                // Находим дочерние окна для текста
                HWND hCpuLabel = GetDlgItem(hDlg, IDC_CPU_GRAPH + 100);
                HWND hMemDetails = GetDlgItem(hDlg, IDC_STATIC_MEMORY);
                HWND hNetDetails = GetDlgItem(hDlg, IDC_STATIC_NETWORK);

                // Устанавливаем обновленный текст
                if (hCpuLabel) SetWindowTextW(hCpuLabel, cpuTextFormatted);
                if (hMemDetails) SetWindowTextW(hMemDetails, memTextFormatted);
                if (hNetDetails) SetWindowTextW(hNetDetails, netTextFormatted);

                // Инвалидируем области ГРАФИКОВ для перерисовки (не всего диалога)
                HWND hCpuGraph = GetDlgItem(hDlg, IDC_CPU_GRAPH);
                HWND hMemGraph = GetDlgItem(hDlg, IDC_MEM_GRAPH);
                HWND hNetGraph = GetDlgItem(hDlg, IDC_NET_GRAPH);

                if (hCpuGraph) {
                    InvalidateRect(hCpuGraph, NULL, TRUE);
                }

                if (hMemGraph) {
                    InvalidateRect(hMemGraph, NULL, TRUE);
                }
                
                if (hNetGraph) {
                    InvalidateRect(hNetGraph, NULL, TRUE);
                }
            }
            return TRUE;
            
        case WM_COMMAND:
            if (LOWORD(wParam) == IDC_VIEW_SWITCH && ui) {
                // Переключаем режим отображения
                ui->showBarGraphs = !ui->showBarGraphs;
                
                // Меняем текст кнопки
                HWND hViewSwitch = GetDlgItem(hDlg, IDC_VIEW_SWITCH);
                if (hViewSwitch) {
                    SetWindowTextW(hViewSwitch, ui->showBarGraphs ? 
                        L"Переключить на линейные графики" : 
                        L"Переключить на столбчатые графики");
                }
                
                // Инвалидируем все графики для перерисовки
                HWND hCpuGraph = GetDlgItem(hDlg, IDC_CPU_GRAPH);
                HWND hMemGraph = GetDlgItem(hDlg, IDC_MEM_GRAPH);
                HWND hNetGraph = GetDlgItem(hDlg, IDC_NET_GRAPH);
                
                if (hCpuGraph) InvalidateRect(hCpuGraph, NULL, TRUE);
                if (hMemGraph) InvalidateRect(hMemGraph, NULL, TRUE);
                if (hNetGraph) InvalidateRect(hNetGraph, NULL, TRUE);
                
                LogDebugMessage(L"[UI] Тип графика переключен на %s\n", 
                    ui->showBarGraphs ? L"столбчатый" : L"линейный");
                return TRUE;
            }
            break;
            
        case WM_PAINT: { // Оставляем пустым, т.к. рисование идет в WM_DRAWITEM
            PAINTSTRUCT ps;
            BeginPaint(hDlg, &ps);
            EndPaint(hDlg, &ps);
            return TRUE;
        }
            
        case WM_DRAWITEM: {
            if (!ui) return FALSE;
            
            LPDRAWITEMSTRUCT lpDrawItem = (LPDRAWITEMSTRUCT)lParam;
            LogDebugMessage(L"[DRAWITEM] Обработка WM_DRAWITEM для CtlID: %d\n", lpDrawItem->CtlID);
            
            if (lpDrawItem->CtlID == IDC_CPU_GRAPH) {
                if (ui->showBarGraphs) {
                    ui->DrawCpuBarGraph(lpDrawItem->hDC, lpDrawItem->rcItem);
                } else {
                    ui->DrawCpuGraph(lpDrawItem->hDC, lpDrawItem->rcItem);
                }
                return TRUE;
            }
            if (lpDrawItem->CtlID == IDC_MEM_GRAPH) {
                if (ui->showBarGraphs) {
                    ui->DrawMemBarGraph(lpDrawItem->hDC, lpDrawItem->rcItem);
                } else {
                    ui->DrawMemGraph(lpDrawItem->hDC, lpDrawItem->rcItem);
                }
                return TRUE;
            }
            if (lpDrawItem->CtlID == IDC_NET_GRAPH) {
                if (ui->showBarGraphs) {
                    ui->DrawNetworkBarGraph(lpDrawItem->hDC, lpDrawItem->rcItem);
                } else {
                    ui->DrawNetworkGraph(lpDrawItem->hDC, lpDrawItem->rcItem);
                }
                return TRUE;
            }
            break;
        }
            
        case WM_CLOSE:
            if (ui) {
                KillTimer(hDlg, PERFORMANCE_TIMER);
                ui->bIsPerfDialogOpen = false;
                ui->hPerfDialog = NULL;
            }
            DestroyWindow(hDlg);
            return TRUE;
    }
    
    return DefWindowProcW(hDlg, message, wParam, lParam);
}

// Обработка команд меню
void TaskManagerUI::HandleCommand(int commandId) {
    switch (commandId) {
        case IDM_REFRESH:
            RefreshProcessList();
            break;
            
        case IDM_TERMINATE:
            ShowTerminateProcessDialog();
            break;
            
        case IDM_START:
            ShowStartProcessDialog();
            break;
            
        case IDM_PERFORMANCE:
            ShowPerformanceDialog();
            break;
            
        case IDM_EXIT:
            DestroyWindow(hWnd);
            break;
    }
}

// Обработка сообщений окна
LRESULT TaskManagerUI::HandleMessage(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_CREATE:
            // Изначальная установка таймера на более долгий интервал для улучшения отзывчивости UI
            SetTimer(hwnd, 1, 2000, NULL);
            lastUserInteractionTime = GetTickCount(); // Инициализируем время взаимодействия
            return 0;
            
        case WM_TIMER:
            if (wParam == 1) {
                static DWORD lastRefreshTime = 0;
                DWORD currentTime = GetTickCount();

                // Проверяем, прошло ли достаточно времени с последнего взаимодействия
                // и с последнего обновления, чтобы избежать обновления во время скролла
                if ((currentTime - lastUserInteractionTime > 1500) && // Ждем 1.5 сек после скролла
                    (currentTime - lastRefreshTime > 3000)) // Обновляем не чаще чем раз в 3 сек
                {
                    RefreshProcessList();
                    lastRefreshTime = currentTime;
                }

                // Обновляем показатели производительности всегда по таймеру
                if (bIsPerfDialogOpen && hPerfDialog && IsWindow(hPerfDialog)) {
                    SystemInfo sysInfo = processManager.GetSystemInfo();
                    
                    // Проверяем наличие данных сети и добавляем в историю
                    double netSend = sysInfo.networkSendSpeedKBps >= 0.0 ? sysInfo.networkSendSpeedKBps : 0.0;
                    double netRecv = sysInfo.networkReceiveSpeedKBps >= 0.0 ? sysInfo.networkReceiveSpeedKBps : 0.0;
                    
                    // Логируем полученные данные
                    LogDebugMessage(L"[TIMER] Получены данные: CPU=%.1f%%, Mem=%.1f%%, Net Send=%.1f КБ/с, Net Recv=%.1f КБ/с\n",
                        sysInfo.cpuUsage, sysInfo.memoryUsagePercent, netSend, netRecv);
                    
                    // Добавляем данные в историю
                    perfHistory.AddData(sysInfo.cpuUsage, sysInfo.memoryUsagePercent, netSend, netRecv);
                    
                    // Запрашиваем перерисовку, если окно производительности открыто
                    if (hPerfDialog && IsWindow(hPerfDialog)) {
                        // Инвалидируем области графиков для перерисовки
                        HWND hCpuGraph = GetDlgItem(hPerfDialog, IDC_CPU_GRAPH);
                        if (hCpuGraph) InvalidateRect(hCpuGraph, NULL, TRUE);
                        
                        HWND hMemGraph = GetDlgItem(hPerfDialog, IDC_MEM_GRAPH);
                        if (hMemGraph) InvalidateRect(hMemGraph, NULL, TRUE);
                        
                        HWND hNetGraph = GetDlgItem(hPerfDialog, IDC_NET_GRAPH);
                        if (hNetGraph) InvalidateRect(hNetGraph, NULL, TRUE);
                    }
                }
                
                UpdatePerformanceData();
            }
            return 0;
        
        case WM_MOUSEWHEEL:
        case WM_VSCROLL: // Добавляем обработку стандартного скролл-бара
        case WM_LBUTTONDOWN: // Добавляем нажатие ЛКМ (например, на скролл-бар)
            // Запоминаем время последнего взаимодействия пользователя
            lastUserInteractionTime = GetTickCount();
            break;
            
        case WM_SIZE: {
            RECT rcClient;
            GetClientRect(hwnd, &rcClient);
            
            int statusBarHeight = 20;
            
            HWND hListViewCtrl = GetDlgItem(hwnd, IDC_LISTVIEW);
            HWND hStatusBarCtrl = GetDlgItem(hwnd, IDC_STATUSBAR);
            
            MoveWindow(hListViewCtrl, 0, 0, rcClient.right, rcClient.bottom - statusBarHeight, TRUE);
            SendMessageW(hStatusBarCtrl, WM_SIZE, 0, 0);
            
            return 0;
        }
            
        case WM_COMMAND:
            HandleCommand(LOWORD(wParam));
            return 0;
            
        case WM_NOTIFY: {
            LPNMHDR pnmh = (LPNMHDR)lParam;
            
            if (pnmh->idFrom == IDC_LISTVIEW) {
                switch (pnmh->code) {
                    case NM_DBLCLK: {
                        int selectedItem = (int)SendMessageW(hListView, LVM_GETNEXTITEM, -1, MAKELPARAM(LVNI_SELECTED, 0));
                        if (selectedItem != -1) {
                            wchar_t pidBuffer[20] = {0};
                            wchar_t nameBuffer[MAX_PATH] = {0};
                            wchar_t statusBuffer[50] = {0};
                            
                            LVITEMW lvItem = {0};
                            lvItem.mask = LVIF_TEXT;
                            
                            lvItem.iSubItem = 0;
                            lvItem.pszText = pidBuffer;
                            lvItem.cchTextMax = sizeof(pidBuffer) / sizeof(wchar_t);
                            lvItem.iItem = selectedItem;
                            SendMessageW(hListView, LVM_GETITEMTEXTW, selectedItem, (LPARAM)&lvItem);
                            
                            lvItem.iSubItem = 1;
                            lvItem.pszText = nameBuffer;
                            lvItem.cchTextMax = sizeof(nameBuffer) / sizeof(wchar_t);
                            SendMessageW(hListView, LVM_GETITEMTEXTW, selectedItem, (LPARAM)&lvItem);
                            
                            lvItem.iSubItem = 4;
                            lvItem.pszText = statusBuffer;
                            lvItem.cchTextMax = sizeof(statusBuffer) / sizeof(wchar_t);
                            SendMessageW(hListView, LVM_GETITEMTEXTW, selectedItem, (LPARAM)&lvItem);
                            
                            wchar_t infoMsg[1024];
                            swprintf_s(infoMsg, L"Информация о процессе:\n\nPID: %s\nИмя: %s\nСтатус: %s", 
                                       pidBuffer, nameBuffer, statusBuffer);
                                       
                            MessageBoxW(hwnd, infoMsg, L"Информация о процессе", MB_ICONINFORMATION);
                        }
                        break;
                    }
                     case LVN_COLUMNCLICK: { 
                        LPNMLISTVIEW pnmv = (LPNMLISTVIEW)lParam;
                        int clickedColumn = pnmv->iSubItem;

                        // Обновляем параметры сортировки
                        if (clickedColumn == sortColumn) {
                            sortAscending = !sortAscending; // Меняем направление
                        } else {
                            sortColumn = clickedColumn; // Новая колонка
                            sortAscending = true;       // По умолчанию по возрастанию
                        }

                        // Выполняем сортировку и обновление списка
                        SortAndRefreshProcessList();
                        break;
                    }
                }
            }
            
            break;
        }
            
        case WM_CLOSE:
            // Корректно закрываем все дочерние окна перед завершением
            if (hPerfDialog != NULL && IsWindow(hPerfDialog)) {
                // Сначала убиваем таймер диалога производительности
                KillTimer(hPerfDialog, PERFORMANCE_TIMER);
                // Затем разрушаем само окно
                DestroyWindow(hPerfDialog);
                hPerfDialog = NULL;
                bIsPerfDialogOpen = false;
            }
            
            // Убиваем таймер основного окна
            KillTimer(hwnd, 1);
            
            DestroyWindow(hwnd);
            return 0;
            
        case WM_DESTROY:
            // Корректно закрываем все ресурсы при уничтожении окна
            if (hMenu != NULL) {
                DestroyMenu(hMenu);
                hMenu = NULL;
            }
            
            PostQuitMessage(0);
            return 0;
    }
    
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

// Статическая функция для обработки сообщений окна
LRESULT CALLBACK TaskManagerUI::WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    TaskManagerUI* ui = NULL;
    if (message == WM_NCCREATE) {
        CREATESTRUCTW* pCreate = reinterpret_cast<CREATESTRUCTW*>(lParam);
        ui = reinterpret_cast<TaskManagerUI*>(pCreate->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(ui));
        if(ui) ui->hWnd = hwnd;
        return DefWindowProcW(hwnd, message, wParam, lParam); 
    } else {
        ui = reinterpret_cast<TaskManagerUI*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }
    
    if (ui) {
        return ui->HandleMessage(hwnd, message, wParam, lParam);
    } else {
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }
}

// Форматирование размера памяти
std::wstring TaskManagerUI::FormatMemorySize(SIZE_T size) {
    static const wchar_t* suffixes[] = {L"B", L"KB", L"MB", L"GB", L"TB"};
    
    if (size == 0) {
        return L"0 B";
    }
    
    int suffixIndex = 0;
    double dSize = static_cast<double>(size);
    
    while (dSize >= 1024 && suffixIndex < 4) {
        dSize /= 1024;
        suffixIndex++;
    }
    
    std::wostringstream woss;
    woss << std::fixed << std::setprecision(1) << dSize << L" " << suffixes[suffixIndex];
    
    return woss.str();
}

// --- Новая функция для сортировки и обновления --- 
void TaskManagerUI::SortAndRefreshProcessList() {
    HWND hList = GetDlgItem(hWnd, IDC_LISTVIEW);
    if (!hList || sortColumn < 0) return; // Не сортируем, если колонка не выбрана

    // Получаем текущие данные (НЕ обновляем список процессов здесь, чтобы не сбить сортировку)
    std::vector<ProcessInfo> processes = processManager.GetProcesses(); // Используем кэшированные данные

    // Компаратор для std::sort
    auto comparator = [&](const ProcessInfo& a, const ProcessInfo& b) {
        bool result = false;
        switch (sortColumn) {
            case 0: // Имя процесса
                result = (lstrcmpiW(a.name.c_str(), b.name.c_str()) < 0);
                break;
            case 1: // PID
                result = (a.pid < b.pid);
                break;
            case 2: // ЦП
                result = (a.cpuUsage < b.cpuUsage);
                break;
            case 3: // Память
                result = (a.memoryUsage < b.memoryUsage);
                break;
            case 4: // Статус
                result = (lstrcmpiW(a.status.c_str(), b.status.c_str()) < 0);
                break;
            case 5: // Родительский PID
                result = (a.parentPid < b.parentPid);
                break;
            default:
                return false; // Неизвестная колонка
        }
        return sortAscending ? result : !result; // Учитываем направление
    };

    // Сортируем вектор
    std::sort(processes.begin(), processes.end(), comparator);

    // --- Обновляем ListView БЕЗ полного пересоздания --- 
    LockWindowUpdate(hList);
    SendMessageW(hList, WM_SETREDRAW, FALSE, 0);

    int itemCount = ListView_GetItemCount(hList);
    if (itemCount != static_cast<int>(processes.size())) {
         // Если количество изменилось (маловероятно при сортировке, но возможно), 
         // делаем полный Refresh для безопасности
        SendMessageW(hList, WM_SETREDRAW, TRUE, 0);
        LockWindowUpdate(NULL);
        RefreshProcessList(); // Вызываем полный Refresh
        return;
    }

    // Просто обновляем текст и lParam для каждого существующего элемента
    for (int i = 0; i < itemCount; ++i) {
        const ProcessInfo& proc = processes[i];
        LVITEMW lvItem = {0};
        lvItem.mask = LVIF_PARAM | LVIF_TEXT;
        lvItem.iItem = i;
        lvItem.lParam = static_cast<LPARAM>(proc.pid);
        lvItem.pszText = const_cast<LPWSTR>(proc.name.c_str()); // Имя в колонку 0
        lvItem.iSubItem = 0;
        SendMessageW(hList, LVM_SETITEMW, 0, (LPARAM)&lvItem);

        // Обновляем остальные колонки
        wchar_t textBuffer[MAX_PATH];
        swprintf_s(textBuffer, L"%lu", proc.pid);
        ListView_SetItemText(hList, i, 1, textBuffer);
        swprintf_s(textBuffer, L"%.1f%%", proc.cpuUsage);
        ListView_SetItemText(hList, i, 2, textBuffer);
        std::wstring memStr = FormatMemorySize(proc.memoryUsage);
        ListView_SetItemText(hList, i, 3, const_cast<LPWSTR>(memStr.c_str()));
        ListView_SetItemText(hList, i, 4, const_cast<LPWSTR>(proc.status.c_str()));
        swprintf_s(textBuffer, L"%lu", proc.parentPid);
        ListView_SetItemText(hList, i, 5, textBuffer);
    }

    SendMessageW(hList, WM_SETREDRAW, TRUE, 0);
    LockWindowUpdate(NULL);
    InvalidateRect(hList, NULL, TRUE);
    // Не нужно восстанавливать прокрутку/выделение здесь, т.к. элементы только переупорядочены
}

// Реализация столбчатого графика для CPU
void TaskManagerUI::DrawCpuBarGraph(HDC hdc, RECT rect) {
    LogDebugMessage(L"[DRAW] Отрисовка столбчатого графика CPU, точек данных: %d\n", 
                   static_cast<int>(perfHistory.cpuHistory.size()));
    
    // Заполняем фон белым цветом
    HBRUSH hBgBrush = CreateSolidBrush(RGB(255, 255, 255));
    FillRect(hdc, &rect, hBgBrush);
    DeleteObject(hBgBrush);
    
    // Вычисляем размеры области для рисования
    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;
    int padding = 10; // Отступ от краев
    
    // Рисуем сетку
    HPEN hGridPen = CreatePen(PS_DOT, 1, GRID_COLOR);
    HPEN hOldPen = (HPEN)SelectObject(hdc, hGridPen);
    
    // Горизонтальные линии сетки - шаг 10%
    for (int i = 0; i <= 10; i++) {
        int y = rect.bottom - padding - (i * (height - 2 * padding) / 10);
        MoveToEx(hdc, rect.left + padding, y, NULL);
        LineTo(hdc, rect.right - padding, y);
        
        // Подписи процентов
        wchar_t label[10];
        swprintf_s(label, L"%d%%", i * 10);
        SetTextColor(hdc, TEXT_COLOR);
        SetBkMode(hdc, TRANSPARENT);
        TextOutW(hdc, rect.left + 2, y - 7, label, static_cast<int>(wcslen(label)));
    }
    
    // Рисуем столбцы только если есть данные
    if (!perfHistory.cpuHistory.empty()) {
        // Создаем кисть для столбцов
        HBRUSH hBarBrush = CreateSolidBrush(CPU_COLOR);
        HPEN hBarPen = CreatePen(PS_SOLID, 1, RGB(180, 30, 30)); // Более темный красный для контура
        SelectObject(hdc, hBarPen);
        
        // Определяем количество столбцов, которые хотим показать
        int maxBars = std::min(20, static_cast<int>(perfHistory.cpuHistory.size()));
        int startIndex = static_cast<int>(perfHistory.cpuHistory.size()) - maxBars;
        if (startIndex < 0) startIndex = 0;
        
        // Ширина одного столбца и расстояние между ними
        int barWidth = (width - 2 * padding) / maxBars;
        int barPadding = barWidth / 5; // 20% от ширины столбца для промежутка
        if (barPadding < 1) barPadding = 1;
        
        // Рисуем каждый столбец
        for (int i = 0; i < maxBars; i++) {
            double cpuValue = perfHistory.cpuHistory[startIndex + i];
            
            // Ограничиваем значение между 0 и 100
            cpuValue = std::max(0.0, std::min(100.0, cpuValue));
            
            // Вычисляем размеры столбца
            int barHeight = static_cast<int>((cpuValue * (height - 2 * padding)) / 100.0);
            int barLeft = rect.left + padding + (i * barWidth) + barPadding;
            int barRight = rect.left + padding + ((i + 1) * barWidth) - barPadding;
            int barTop = rect.bottom - padding - barHeight;
            int barBottom = rect.bottom - padding;
            
            // Рисуем столбец: сначала контур, потом заливка
            RECT barRect = {barLeft, barTop, barRight, barBottom};
            FillRect(hdc, &barRect, hBarBrush);
            
            // Рисуем рамку вокруг столбца
            MoveToEx(hdc, barLeft, barTop, NULL);
            LineTo(hdc, barRight, barTop);
            LineTo(hdc, barRight, barBottom);
            LineTo(hdc, barLeft, barBottom);
            LineTo(hdc, barLeft, barTop);
            
            // Если это последний столбец, добавляем метку с процентом
            if (i == maxBars - 1) {
                wchar_t valueLabel[10];
                swprintf_s(valueLabel, L"%.1f%%", cpuValue);
                TextOutW(hdc, barRight - 35, barTop - 15, valueLabel, static_cast<int>(wcslen(valueLabel)));
            }
        }
        
        // Освобождаем ресурсы
        SelectObject(hdc, hOldPen);
        DeleteObject(hBarBrush);
        DeleteObject(hBarPen);
    } else {
        // Если нет данных, пишем сообщение
        SetTextColor(hdc, RGB(150, 150, 150));
        TextOutW(hdc, rect.left + padding, rect.top + height/2 - 10, L"Нет данных для отображения", 24);
    }
    
    // Рисуем рамку
    HPEN hBorderPen = CreatePen(PS_SOLID, 1, RGB(150, 150, 150));
    SelectObject(hdc, hBorderPen);
    Rectangle(hdc, rect.left, rect.top, rect.right, rect.bottom);
    SelectObject(hdc, hOldPen);
    DeleteObject(hGridPen);
    DeleteObject(hBorderPen);
    
    // Добавляем заголовок с текущим значением
    if (!perfHistory.cpuHistory.empty()) {
        double currentValue = perfHistory.cpuHistory.back();
        wchar_t valueText[32];
        swprintf_s(valueText, L"Текущее значение: %.1f%%", currentValue);
        SetTextColor(hdc, TEXT_COLOR);
        TextOutW(hdc, rect.left + padding, rect.top + padding, valueText, static_cast<int>(wcslen(valueText)));
    }
}

// Реализация столбчатого графика для памяти
void TaskManagerUI::DrawMemBarGraph(HDC hdc, RECT rect) {
    LogDebugMessage(L"[DRAW] Отрисовка столбчатого графика памяти, точек данных: %d\n", 
                   static_cast<int>(perfHistory.memHistory.size()));
    
    // Заполняем фон белым цветом
    HBRUSH hBgBrush = CreateSolidBrush(RGB(255, 255, 255));
    FillRect(hdc, &rect, hBgBrush);
    DeleteObject(hBgBrush);
    
    // Вычисляем размеры области для рисования
    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;
    int padding = 10; // Отступ от краев
    
    // Рисуем сетку
    HPEN hGridPen = CreatePen(PS_DOT, 1, GRID_COLOR);
    HPEN hOldPen = (HPEN)SelectObject(hdc, hGridPen);
    
    // Горизонтальные линии сетки - шаг 10%
    for (int i = 0; i <= 10; i++) {
        int y = rect.bottom - padding - (i * (height - 2 * padding) / 10);
        MoveToEx(hdc, rect.left + padding, y, NULL);
        LineTo(hdc, rect.right - padding, y);
        
        // Подписи процентов
        wchar_t label[10];
        swprintf_s(label, L"%d%%", i * 10);
        SetTextColor(hdc, TEXT_COLOR);
        SetBkMode(hdc, TRANSPARENT);
        TextOutW(hdc, rect.left + 2, y - 7, label, static_cast<int>(wcslen(label)));
    }
    
    // Рисуем столбцы только если есть данные
    if (!perfHistory.memHistory.empty()) {
        // Создаем кисть для столбцов
        HBRUSH hBarBrush = CreateSolidBrush(MEM_COLOR);
        HPEN hBarPen = CreatePen(PS_SOLID, 1, RGB(60, 130, 40)); // Более темный зеленый для контура
        SelectObject(hdc, hBarPen);
        
        // Определяем количество столбцов, которые хотим показать
        int maxBars = std::min(20, static_cast<int>(perfHistory.memHistory.size()));
        int startIndex = static_cast<int>(perfHistory.memHistory.size()) - maxBars;
        if (startIndex < 0) startIndex = 0;
        
        // Ширина одного столбца и расстояние между ними
        int barWidth = (width - 2 * padding) / maxBars;
        int barPadding = barWidth / 5; // 20% от ширины столбца для промежутка
        if (barPadding < 1) barPadding = 1;
        
        // Рисуем каждый столбец
        for (int i = 0; i < maxBars; i++) {
            double memValue = perfHistory.memHistory[startIndex + i];
            
            // Ограничиваем значение между 0 и 100
            memValue = std::max(0.0, std::min(100.0, memValue));
            
            // Вычисляем размеры столбца
            int barHeight = static_cast<int>((memValue * (height - 2 * padding)) / 100.0);
            int barLeft = rect.left + padding + (i * barWidth) + barPadding;
            int barRight = rect.left + padding + ((i + 1) * barWidth) - barPadding;
            int barTop = rect.bottom - padding - barHeight;
            int barBottom = rect.bottom - padding;
            
            // Рисуем столбец: сначала заливка, потом контур
            RECT barRect = {barLeft, barTop, barRight, barBottom};
            FillRect(hdc, &barRect, hBarBrush);
            
            // Рисуем рамку вокруг столбца
            MoveToEx(hdc, barLeft, barTop, NULL);
            LineTo(hdc, barRight, barTop);
            LineTo(hdc, barRight, barBottom);
            LineTo(hdc, barLeft, barBottom);
            LineTo(hdc, barLeft, barTop);
            
            // Если это последний столбец, добавляем метку с процентом
            if (i == maxBars - 1) {
                wchar_t valueLabel[10];
                swprintf_s(valueLabel, L"%.1f%%", memValue);
                TextOutW(hdc, barRight - 35, barTop - 15, valueLabel, static_cast<int>(wcslen(valueLabel)));
            }
        }
        
        // Освобождаем ресурсы
        SelectObject(hdc, hOldPen);
        DeleteObject(hBarBrush);
        DeleteObject(hBarPen);
    } else {
        // Если нет данных, пишем сообщение
        SetTextColor(hdc, RGB(150, 150, 150));
        TextOutW(hdc, rect.left + padding, rect.top + height/2 - 10, L"Нет данных для отображения", 24);
    }
    
    // Рисуем рамку
    HPEN hBorderPen = CreatePen(PS_SOLID, 1, RGB(150, 150, 150));
    SelectObject(hdc, hBorderPen);
    Rectangle(hdc, rect.left, rect.top, rect.right, rect.bottom);
    SelectObject(hdc, hOldPen);
    DeleteObject(hGridPen);
    DeleteObject(hBorderPen);
    
    // Добавляем заголовок с текущим значением
    if (!perfHistory.memHistory.empty()) {
        double currentValue = perfHistory.memHistory.back();
        wchar_t valueText[32];
        swprintf_s(valueText, L"Текущее значение: %.1f%%", currentValue);
        SetTextColor(hdc, TEXT_COLOR);
        TextOutW(hdc, rect.left + padding, rect.top + padding, valueText, static_cast<int>(wcslen(valueText)));
    }
} 

// Реализация столбчатого графика для сети
void TaskManagerUI::DrawNetworkBarGraph(HDC hdc, RECT rect) {
    LogDebugMessage(L"[DRAW] Отрисовка столбчатого графика сети, точек отправки: %d, точек приема: %d\n", 
                   static_cast<int>(perfHistory.netSendHistory.size()),
                   static_cast<int>(perfHistory.netReceiveHistory.size()));
    
    // Заполняем фон белым цветом
    HBRUSH hBgBrush = CreateSolidBrush(RGB(255, 255, 255));
    FillRect(hdc, &rect, hBgBrush);
    DeleteObject(hBgBrush);
    
    // Вычисляем размеры области для рисования
    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;
    int padding = 10; // Отступ от краев
    
    // Находим максимальное значение скорости для масштабирования
    double maxSendValue = 0.1; // Минимальное значение для случая, если нет данных или все 0
    double maxRecvValue = 0.1; 
    
    for (const auto& val : perfHistory.netSendHistory) {
        maxSendValue = std::max(maxSendValue, val);
    }
    
    for (const auto& val : perfHistory.netReceiveHistory) {
        maxRecvValue = std::max(maxRecvValue, val);
    }
    
    double maxValue = std::max(maxSendValue, maxRecvValue);
    
    // Округляем максимум до "красивого" числа для легкости чтения шкалы
    if (maxValue <= 10.0) {
        maxValue = 10.0; // 10 КБ/с
    } else if (maxValue <= 100.0) {
        maxValue = 100.0; // 100 КБ/с
    } else if (maxValue <= 1000.0) {
        maxValue = 1000.0; // 1 МБ/с
    } else if (maxValue <= 10000.0) {
        maxValue = 10000.0; // 10 МБ/с
    } else {
        maxValue = ceil(maxValue / 10000.0) * 10000.0; // Округлить до следующего целого числа десятков МБ/с
    }
    
    // Рисуем сетку
    HPEN hGridPen = CreatePen(PS_DOT, 1, GRID_COLOR);
    HPEN hOldPen = (HPEN)SelectObject(hdc, hGridPen);
    
    // Горизонтальные линии сетки - 5 линий
    for (int i = 0; i <= 5; i++) {
        int y = rect.bottom - padding - (i * (height - 2 * padding) / 5);
        MoveToEx(hdc, rect.left + padding, y, NULL);
        LineTo(hdc, rect.right - padding, y);
        
        // Подписи скорости
        wchar_t label[32];
        double value = (i * maxValue / 5);
        if (value < 1000.0) {
            swprintf_s(label, L"%.1f КБ/с", value);
        } else {
            swprintf_s(label, L"%.1f МБ/с", value / 1024.0);
        }
        
        SetTextColor(hdc, TEXT_COLOR);
        SetBkMode(hdc, TRANSPARENT);
        TextOutW(hdc, rect.left + 2, y - 7, label, static_cast<int>(wcslen(label)));
    }
    
    // Если нет данных, выводим сообщение
    if (perfHistory.netSendHistory.size() <= 1 && perfHistory.netReceiveHistory.size() <= 1) {
        SetTextColor(hdc, RGB(150, 150, 150));
        TextOutW(hdc, rect.left + padding, rect.top + height/2 - 10, L"Нет данных для отображения", 24);
        
        // Рисуем рамку и возвращаемся
        HPEN hBorderPen = CreatePen(PS_SOLID, 1, RGB(150, 150, 150));
        SelectObject(hdc, hBorderPen);
        Rectangle(hdc, rect.left, rect.top, rect.right, rect.bottom);
        SelectObject(hdc, hOldPen);
        DeleteObject(hGridPen);
        DeleteObject(hBorderPen);
        return;
    }
    
    // Определяем количество столбцов, которые хотим показать
    int maxBars = std::min(15, static_cast<int>(std::max(perfHistory.netSendHistory.size(), perfHistory.netReceiveHistory.size())));
    int sendStartIndex = static_cast<int>(perfHistory.netSendHistory.size()) - maxBars;
    int recvStartIndex = static_cast<int>(perfHistory.netReceiveHistory.size()) - maxBars;
    if (sendStartIndex < 0) sendStartIndex = 0;
    if (recvStartIndex < 0) recvStartIndex = 0;
    
    // Ширина одного столбца и расстояние между ними
    int barGroupWidth = (width - 2 * padding) / maxBars;
    int barPaddingExternal = barGroupWidth / 10; // Внешний отступ между группами столбцов 
    if (barPaddingExternal < 1) barPaddingExternal = 1;
    
    // Ширина каждого столбца в группе (отправка + прием)
    int singleBarWidth = (barGroupWidth - 2 * barPaddingExternal) / 2;
    if (singleBarWidth < 2) singleBarWidth = 2;
    
    // Создаем кисти и перья для столбцов
    HBRUSH hSendBrush = CreateSolidBrush(NET_SEND_COLOR);
    HPEN hSendPen = CreatePen(PS_SOLID, 1, RGB(30, 100, 180)); // Более темный синий для контура
    HBRUSH hRecvBrush = CreateSolidBrush(NET_RECV_COLOR);
    HPEN hRecvPen = CreatePen(PS_SOLID, 1, RGB(100, 50, 140)); // Более темный фиолетовый для контура
    
    // Рисуем столбцы отправленных данных
    if (perfHistory.netSendHistory.size() > 1) {
        SelectObject(hdc, hSendPen);
        
        for (int i = 0; i < maxBars && (sendStartIndex + i) < static_cast<int>(perfHistory.netSendHistory.size()); i++) {
            double sendValue = perfHistory.netSendHistory[sendStartIndex + i];
            sendValue = std::min(sendValue, maxValue); // Ограничиваем значение до максимального
            
            int barHeight = static_cast<int>((sendValue * (height - 2 * padding)) / maxValue);
            if (barHeight < 1) barHeight = 1; // Минимальная высота столбца 1 пиксель
            
            int barLeft = rect.left + padding + (i * barGroupWidth) + barPaddingExternal;
            int barRight = barLeft + singleBarWidth;
            int barTop = rect.bottom - padding - barHeight;
            int barBottom = rect.bottom - padding;
            
            // Рисуем столбец отправки: сначала заливка, потом контур
            RECT barRect = {barLeft, barTop, barRight, barBottom};
            FillRect(hdc, &barRect, hSendBrush);
            
            // Рисуем рамку вокруг столбца
            MoveToEx(hdc, barLeft, barTop, NULL);
            LineTo(hdc, barRight, barTop);
            LineTo(hdc, barRight, barBottom);
            LineTo(hdc, barLeft, barBottom);
            LineTo(hdc, barLeft, barTop);
            
            // Если это последний столбец, добавляем подпись
            if (i == maxBars - 1 || (sendStartIndex + i) == static_cast<int>(perfHistory.netSendHistory.size()) - 1) {
                wchar_t valueLabel[16];
                if (sendValue < 1000.0) {
                    swprintf_s(valueLabel, L"%.1f КБ/с", sendValue);
                } else {
                    swprintf_s(valueLabel, L"%.2f МБ/с", sendValue / 1024.0);
                }
                TextOutW(hdc, barLeft, barTop - 15, valueLabel, static_cast<int>(wcslen(valueLabel)));
            }
        }
    }
    
    // Рисуем столбцы принятых данных
    if (perfHistory.netReceiveHistory.size() > 1) {
        SelectObject(hdc, hRecvPen);
        
        for (int i = 0; i < maxBars && (recvStartIndex + i) < static_cast<int>(perfHistory.netReceiveHistory.size()); i++) {
            double recvValue = perfHistory.netReceiveHistory[recvStartIndex + i];
            recvValue = std::min(recvValue, maxValue); // Ограничиваем значение до максимального
            
            int barHeight = static_cast<int>((recvValue * (height - 2 * padding)) / maxValue);
            if (barHeight < 1) barHeight = 1; // Минимальная высота столбца 1 пиксель
            
            int barLeft = rect.left + padding + (i * barGroupWidth) + singleBarWidth + barPaddingExternal * 2;
            int barRight = barLeft + singleBarWidth;
            int barTop = rect.bottom - padding - barHeight;
            int barBottom = rect.bottom - padding;
            
            // Рисуем столбец приема: сначала заливка, потом контур
            RECT barRect = {barLeft, barTop, barRight, barBottom};
            FillRect(hdc, &barRect, hRecvBrush);
            
            // Рисуем рамку вокруг столбца
            MoveToEx(hdc, barLeft, barTop, NULL);
            LineTo(hdc, barRight, barTop);
            LineTo(hdc, barRight, barBottom);
            LineTo(hdc, barLeft, barBottom);
            LineTo(hdc, barLeft, barTop);
            
            // Если это последний столбец, добавляем подпись
            if (i == maxBars - 1 || (recvStartIndex + i) == static_cast<int>(perfHistory.netReceiveHistory.size()) - 1) {
                wchar_t valueLabel[16];
                if (recvValue < 1000.0) {
                    swprintf_s(valueLabel, L"%.1f КБ/с", recvValue);
                } else {
                    swprintf_s(valueLabel, L"%.2f МБ/с", recvValue / 1024.0);
                }
                TextOutW(hdc, barLeft, barTop - 15, valueLabel, static_cast<int>(wcslen(valueLabel)));
            }
        }
    }
    
    // Освобождаем ресурсы
    SelectObject(hdc, hOldPen);
    DeleteObject(hSendBrush);
    DeleteObject(hSendPen);
    DeleteObject(hRecvBrush);
    DeleteObject(hRecvPen);
    
    // Рисуем рамку
    HPEN hBorderPen = CreatePen(PS_SOLID, 1, RGB(150, 150, 150));
    SelectObject(hdc, hBorderPen);
    Rectangle(hdc, rect.left, rect.top, rect.right, rect.bottom);
    SelectObject(hdc, hOldPen);
    DeleteObject(hGridPen);
    DeleteObject(hBorderPen);
    
    // Добавляем легенду
    if (!perfHistory.netSendHistory.empty() && !perfHistory.netReceiveHistory.empty()) {
        double sendValue = perfHistory.netSendHistory.back();
        double recvValue = perfHistory.netReceiveHistory.back();
        
        wchar_t sendText[64];
        wchar_t recvText[64];
        
        if (sendValue < 1000.0) {
            swprintf_s(sendText, L"Отправка: %.1f КБ/с", sendValue);
        } else {
            swprintf_s(sendText, L"Отправка: %.2f МБ/с", sendValue / 1024.0);
        }
        
        if (recvValue < 1000.0) {
            swprintf_s(recvText, L"Прием: %.1f КБ/с", recvValue);
        } else {
            swprintf_s(recvText, L"Прием: %.2f МБ/с", recvValue / 1024.0);
        }
        
        // Рисуем цветные квадратики для легенды
        HBRUSH hSendLegendBrush = CreateSolidBrush(NET_SEND_COLOR);
        HBRUSH hRecvLegendBrush = CreateSolidBrush(NET_RECV_COLOR);
        
        RECT sendLegendRect = {rect.left + padding, rect.top + padding, 
                         rect.left + padding + 15, rect.top + padding + 15};
        RECT recvLegendRect = {rect.left + padding + 180, rect.top + padding, 
                         rect.left + padding + 180 + 15, rect.top + padding + 15};
        
        FillRect(hdc, &sendLegendRect, hSendLegendBrush);
        FillRect(hdc, &recvLegendRect, hRecvLegendBrush);
        
        // Рисуем рамки вокруг цветных квадратиков
        HPEN hBlackPen = CreatePen(PS_SOLID, 1, RGB(0, 0, 0));
        SelectObject(hdc, hBlackPen);
        Rectangle(hdc, sendLegendRect.left, sendLegendRect.top, sendLegendRect.right, sendLegendRect.bottom);
        Rectangle(hdc, recvLegendRect.left, recvLegendRect.top, recvLegendRect.right, recvLegendRect.bottom);
        DeleteObject(hBlackPen);
        
        // Добавляем текст легенды
        SetTextColor(hdc, TEXT_COLOR);
        TextOutW(hdc, rect.left + padding + 20, rect.top + padding, sendText, static_cast<int>(wcslen(sendText)));
        TextOutW(hdc, rect.left + padding + 200, rect.top + padding, recvText, static_cast<int>(wcslen(recvText)));
        
        // Освобождаем ресурсы
        DeleteObject(hSendLegendBrush);
        DeleteObject(hRecvLegendBrush);
    }
}