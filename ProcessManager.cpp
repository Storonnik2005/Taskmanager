#include "ProcessManager.h"
#include <iostream>
#include <algorithm>
#include <codecvt>
#include <locale>
#include <vector>

// Определяем, что нам не нужны лишние компоненты Windows
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

// Сначала базовые заголовки Windows
#include <windows.h>
#include <TlHelp32.h>
#include <Psapi.h>
#include <iphlpapi.h>

// Конвертация std::string (ANSI) в std::wstring (Unicode)
std::wstring StringToWString(const std::string& str) {
    if (str.empty()) return std::wstring();
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
    std::wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
    return wstrTo;
}

ProcessManager::ProcessManager() {
    lastSystemTime.QuadPart = 0;
    lastNetTime.QuadPart = 0;
    lastNetInBytes = 0;
    lastNetOutBytes = 0;
    
    // Устанавливаем кодовую страницу UTF-8 для корректного отображения русских символов
    SetConsoleOutputCP(CP_UTF8); // CP_UTF8 = 65001
    SetConsoleCP(CP_UTF8);
    
    UpdateProcessList();
}

ProcessManager::~ProcessManager() {
    processes.clear();
}

void ProcessManager::UpdateProcessList() {
    processes.clear();
    
    HANDLE hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hProcessSnap == INVALID_HANDLE_VALUE) {
        return;
    }

    PROCESSENTRY32W pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32W);

    if (!Process32FirstW(hProcessSnap, &pe32)) {
        CloseHandle(hProcessSnap);
        return;
    }

    do {
        HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ, FALSE, pe32.th32ProcessID);
        
        // Получаем имя процесса из структуры PROCESSENTRY32W
        std::wstring processName;
        if (pe32.th32ProcessID == 0) {
            processName = L"System Idle Process";
        } else if (pe32.th32ProcessID == 4) {
            processName = L"System";
        } else if (pe32.szExeFile[0] != L'\0') {
            // Сначала пробуем использовать имя из pe32.szExeFile (уже в Unicode)
            processName = pe32.szExeFile;
            
            // Дополнительная проверка на корректность имени
            if (processName.length() == 0 || !IsTextUnicode(processName.c_str(), static_cast<int>(processName.length() * sizeof(wchar_t)), NULL)) {
                // Если имя не в Unicode, попробуем преобразовать
                processName = L"[Некорректное имя]";
            }
            
            // Если доступен дескриптор процесса, пытаемся получить полное имя
            if (hProcess) {
                wchar_t exePath[MAX_PATH] = {0};
                DWORD len = MAX_PATH;
                
                if (QueryFullProcessImageNameW(hProcess, 0, exePath, &len)) {
                    // Проверяем, что полученное имя валидное
                    if (exePath[0] != L'\0' && IsTextUnicode(exePath, static_cast<int>(len * sizeof(wchar_t)), NULL)) {
                        // Извлекаем только имя файла из полного пути
                        wchar_t* fileName = wcsrchr(exePath, L'\\');
                        if (fileName) {
                            processName = fileName + 1; // +1 чтобы пропустить сам символ '\'
                        }
                    }
                }
            }
        } else {
            processName = L"[Неизвестный процесс]";
        }

        SIZE_T memoryUsage = 0;
        if (hProcess) {
            PROCESS_MEMORY_COUNTERS pmc;
            if (GetProcessMemoryInfo(hProcess, &pmc, sizeof(pmc))) {
                memoryUsage = pmc.WorkingSetSize;
            }
        }

        // Получаем статус процесса
        std::wstring status = GetProcessStatus(pe32.th32ProcessID);

        // Создаем объект ProcessInfo и добавляем его в вектор
        ProcessInfo process;
        process.pid = pe32.th32ProcessID;
        process.name = processName;
        process.parentPid = pe32.th32ParentProcessID;
        process.memoryUsage = memoryUsage;
        process.cpuUsage = 0.0;  // CPU usage будет обновлен позже
        process.status = status;
        
        processes.push_back(process);

        if (hProcess) {
            CloseHandle(hProcess);
        }
    } while (Process32NextW(hProcessSnap, &pe32));

    CloseHandle(hProcessSnap);
    
    // Обновляем CPU usage для всех процессов
    UpdateCpuUsage();
}

const std::vector<ProcessInfo>& ProcessManager::GetProcesses() const {
    return processes;
}

std::vector<ProcessInfo> ProcessManager::GetProcessList() {
    return processes;
}

bool ProcessManager::TerminateProcess(DWORD processId) {
    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, processId);
    if (hProcess == NULL) {
        return false;
    }

    bool result = ::TerminateProcess(hProcess, 0);
    CloseHandle(hProcess);
    
    if (result) {
        UpdateProcessList();
    }
    
    return result;
}

bool ProcessManager::StartNewProcess(const std::wstring& processPath) {
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;

    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    // CreateProcessW требует изменяемый буфер для командной строки
    std::vector<wchar_t> commandLine(processPath.begin(), processPath.end());
    commandLine.push_back(L'\0'); // Добавляем null-терминатор

    bool result = CreateProcessW(
        NULL,                // Имя приложения (используем командную строку)
        commandLine.data(),  // Командная строка (должна быть изменяемой)
        NULL,                // Атрибуты безопасности процесса
        NULL,                // Атрибуты безопасности потока
        FALSE,               // Наследование дескрипторов
        0,                   // Флаги создания
        NULL,                // Блок окружения родителя
        NULL,                // Текущий каталог родителя
        &si,                 // Указатель на STARTUPINFOW
        &pi                  // Указатель на PROCESS_INFORMATION
    );

    // Закрываем дескрипторы процесса и потока
    if (result) {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        UpdateProcessList();
    }

    return result;
}

double ProcessManager::GetSystemCpuUsage() {
    FILETIME idleTime, kernelTime, userTime;
    if (!GetSystemTimes(&idleTime, &kernelTime, &userTime)) {
        return 0.0;
    }

    ULARGE_INTEGER idle, kernel, user;
    idle.LowPart = idleTime.dwLowDateTime;
    idle.HighPart = idleTime.dwHighDateTime;
    kernel.LowPart = kernelTime.dwLowDateTime;
    kernel.HighPart = kernelTime.dwHighDateTime;
    user.LowPart = userTime.dwLowDateTime;
    user.HighPart = userTime.dwHighDateTime;

    static ULARGE_INTEGER prevIdle = {0}, prevKernel = {0}, prevUser = {0};
    
    ULARGE_INTEGER idleDiff, kernelDiff, userDiff;
    idleDiff.QuadPart = idle.QuadPart - prevIdle.QuadPart;
    kernelDiff.QuadPart = kernel.QuadPart - prevKernel.QuadPart;
    userDiff.QuadPart = user.QuadPart - prevUser.QuadPart;

    // Обновляем предыдущие значения
    prevIdle = idle;
    prevKernel = kernel;
    prevUser = user;

    // Разница в рабочем времени (kernel включает idle)
    ULARGE_INTEGER totalDiff;
    totalDiff.QuadPart = kernelDiff.QuadPart + userDiff.QuadPart;

    // Вычисляем процент использования CPU
    double cpuUsage = 0.0;
    if (totalDiff.QuadPart > 0) {
        cpuUsage = 100.0 - ((idleDiff.QuadPart * 100.0) / totalDiff.QuadPart);
    }

    return cpuUsage;
}

SIZE_T ProcessManager::GetSystemMemoryInfo(SIZE_T& totalPhysMem, SIZE_T& availPhysMem) {
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    GlobalMemoryStatusEx(&memInfo);

    totalPhysMem = memInfo.ullTotalPhys;
    availPhysMem = memInfo.ullAvailPhys;
    
    return totalPhysMem - availPhysMem; // Используемая память
}

void ProcessManager::UpdateCpuUsage() {
    ULARGE_INTEGER currentSystemTimeUI;
    FILETIME dummy; // Не используется, но требуется GetSystemTimes
    GetSystemTimeAsFileTime((LPFILETIME)&dummy); // Получаем текущее системное время
    GetSystemTimes(&dummy, (LPFILETIME)&dummy, (LPFILETIME)&currentSystemTimeUI);
    
    // Задержка между обновлениями системного времени
    ULONGLONG systemTimeDiff = 0;
    if (lastSystemTime.QuadPart != 0) { // Проверка на первый вызов
        systemTimeDiff = currentSystemTimeUI.QuadPart - lastSystemTime.QuadPart;
    }

    for (auto& process : processes) {
        HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, process.pid);
        if (hProcess) {
            FILETIME createTime, exitTime, kernelTime, userTime;
            if (GetProcessTimes(hProcess, &createTime, &exitTime, &kernelTime, &userTime)) {
                ULARGE_INTEGER currentKernel, currentUser, processTotalTime;
                currentKernel.LowPart = kernelTime.dwLowDateTime;
                currentKernel.HighPart = kernelTime.dwHighDateTime;
                currentUser.LowPart = userTime.dwLowDateTime;
                currentUser.HighPart = userTime.dwHighDateTime;
                processTotalTime.QuadPart = currentKernel.QuadPart + currentUser.QuadPart;

                float cpuUsage = 0.0f;
                auto it = lastCPUUsage.find(process.pid);
                
                if (it != lastCPUUsage.end() && systemTimeDiff > 0) { // Проверяем наличие предыдущих данных и разницу во времени
                    ULONGLONG processTimeDiff = processTotalTime.QuadPart - it->second.QuadPart;
                    cpuUsage = (float)(processTimeDiff * 100.0 / systemTimeDiff);
                }
                
                // Обновляем последнее время процесса
                lastCPUUsage[process.pid] = processTotalTime;
                
                // Коррекция на количество процессоров
                SYSTEM_INFO sysInfo;
                ::GetSystemInfo(&sysInfo);  // Явно указываем глобальную функцию Windows API
                if (sysInfo.dwNumberOfProcessors > 0) {
                    cpuUsage /= sysInfo.dwNumberOfProcessors;
                }

                // Ограничиваем значение
                 if (cpuUsage < 0.0f) cpuUsage = 0.0f;
                 if (cpuUsage > 100.0f) cpuUsage = 100.0f; // Может превышать 100% временно

                process.cpuUsage = cpuUsage;
            }
             CloseHandle(hProcess);
        } else {
             process.cpuUsage = 0.0f; // Если не удалось открыть процесс
        }
    }
    
    // Обновляем последнее системное время
    lastSystemTime = currentSystemTimeUI;
    
     // Удаляем данные о CPU для процессов, которые больше не существуют
    std::vector<DWORD> pidsToRemove;
    for(auto const& [pid, val] : lastCPUUsage) {
        bool found = false;
        for (const auto& p : processes) {
            if (p.pid == pid) {
                found = true;
                break;
            }
        }
        if (!found) {
            pidsToRemove.push_back(pid);
        }
    }
    for (DWORD pid : pidsToRemove) {
        lastCPUUsage.erase(pid);
    }
}

std::wstring ProcessManager::GetProcessStatus(DWORD processId) {
    if (processId == 0 || processId == 4) {
        return L"Системный";
    }
    
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);
    if (hProcess == NULL) {
        // Для случаев, когда не можем получить дескриптор
        DWORD err = GetLastError();
        if (err == ERROR_ACCESS_DENIED) {
            return L"Системный";
        } else {
            return L"Недоступен";
        }
    }
    
    DWORD exitCode = 0;
    if (GetExitCodeProcess(hProcess, &exitCode)) {
        CloseHandle(hProcess);
        
        if (exitCode == STILL_ACTIVE) {
            return L"Активен";
        } else {
            return L"Завершен";
        }
    }
    
    CloseHandle(hProcess);
    return L"---"; // Более короткая строка, если не удалось получить код завершения
}

SystemInfo ProcessManager::GetSystemInfo()
{
    SystemInfo info = {0}; // Инициализируем нулями
    
    // Получаем загрузку ЦП
    info.cpuUsage = GetSystemCpuUsage();
    
    // Получаем информацию о памяти
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    if (GlobalMemoryStatusEx(&memInfo)) {
        info.totalMemory = memInfo.ullTotalPhys;
        info.memoryAvailable = memInfo.ullAvailPhys;
        info.memoryUsed = info.totalMemory - info.memoryAvailable;
        info.memoryUsagePercent = static_cast<double>(memInfo.dwMemoryLoad);
    }
    
    // --- ПОЛУЧАЕМ ИНФОРМАЦИЮ О СЕТИ БОЛЕЕ ПРОСТЫМ СПОСОБОМ ---
    // Получаем информацию о сети с помощью GetIfTable (проще, чем GetIfTable2)
    PMIB_IFTABLE pIfTable = NULL;
    DWORD dwSize = 0;
    DWORD dwRetVal = 0;
    
    // Сначала получаем размер буфера
    dwRetVal = GetIfTable(NULL, &dwSize, FALSE);
    if (dwRetVal == ERROR_INSUFFICIENT_BUFFER) {
        pIfTable = (MIB_IFTABLE *)malloc(dwSize);
        if (pIfTable == NULL) {
            return info; // Не смогли выделить память, возвращаем существующую информацию
        }
        
        // Теперь получаем данные
        dwRetVal = GetIfTable(pIfTable, &dwSize, FALSE);
        if (dwRetVal == NO_ERROR) {
            ULONGLONG totalInBytes = 0;
            ULONGLONG totalOutBytes = 0;
            
            // Суммируем байты по всем интерфейсам
            for (DWORD i = 0; i < pIfTable->dwNumEntries; i++) {
                MIB_IFROW *pIfRow = &pIfTable->table[i];
                // Пропускаем loopback и незадействованные интерфейсы
                if (pIfRow->dwType != IF_TYPE_SOFTWARE_LOOPBACK && pIfRow->dwOperStatus == IF_OPER_STATUS_OPERATIONAL) {
                    totalInBytes += pIfRow->dwInOctets;
                    totalOutBytes += pIfRow->dwOutOctets;
                }
            }
            
            // Вычисляем скорость передачи данных
            ULARGE_INTEGER currentTimeUI;
            FILETIME dummyTime;
            GetSystemTimeAsFileTime(&dummyTime);
            currentTimeUI.LowPart = dummyTime.dwLowDateTime;
            currentTimeUI.HighPart = dummyTime.dwHighDateTime;
            
            double timeDiffSeconds = 0.0;
            if (lastNetTime.QuadPart != 0) {
                ULONGLONG timeDiff = currentTimeUI.QuadPart - lastNetTime.QuadPart;
                timeDiffSeconds = static_cast<double>(timeDiff) / 10000000.0; // FILETIME в 100-наносекундных интервалах
            }
            
            if (timeDiffSeconds > 0.0 && lastNetInBytes > 0 && lastNetOutBytes > 0) {
                ULONGLONG inDiff = (totalInBytes > lastNetInBytes) ? (totalInBytes - lastNetInBytes) : 0;
                ULONGLONG outDiff = (totalOutBytes > lastNetOutBytes) ? (totalOutBytes - lastNetOutBytes) : 0;
                
                // Переводим Байты/сек в КБайты/сек
                info.networkReceiveSpeedKBps = (static_cast<double>(inDiff) / timeDiffSeconds) / 1024.0;
                info.networkSendSpeedKBps = (static_cast<double>(outDiff) / timeDiffSeconds) / 1024.0;
            } else {
                // Если прошло недостаточно времени или это первый замер
                info.networkReceiveSpeedKBps = 0.0;
                info.networkSendSpeedKBps = 0.0;
            }
            
            // Обновляем последние значения для следующего замера
            lastNetInBytes = totalInBytes;
            lastNetOutBytes = totalOutBytes;
            lastNetTime = currentTimeUI;
        }
        
        // Освобождаем память
        free(pIfTable);
    }
    
    return info;
}

double ProcessManager::CalculateCpuUsage(HANDLE hProcess, DWORD processID) {
    FILETIME createTime, exitTime, kernelTime, userTime;
    if (!GetProcessTimes(hProcess, &createTime, &exitTime, &kernelTime, &userTime)) {
        return 0.0;
    }
    
    ULARGE_INTEGER kernelTimeValue, userTimeValue;
    kernelTimeValue.LowPart = kernelTime.dwLowDateTime;
    kernelTimeValue.HighPart = kernelTime.dwHighDateTime;
    userTimeValue.LowPart = userTime.dwLowDateTime;
    userTimeValue.HighPart = userTime.dwHighDateTime;
    
    ULARGE_INTEGER processTotalTime;
    processTotalTime.QuadPart = kernelTimeValue.QuadPart + userTimeValue.QuadPart;
    
    // Получаем системное время
    FILETIME sysIdleTime, sysKernelTime, sysUserTime;
    if (!GetSystemTimes(&sysIdleTime, &sysKernelTime, &sysUserTime)) {
        return 0.0;
    }
    
    ULARGE_INTEGER sysKernelTimeValue, sysUserTimeValue;
    sysKernelTimeValue.LowPart = sysKernelTime.dwLowDateTime;
    sysKernelTimeValue.HighPart = sysKernelTime.dwHighDateTime;
    sysUserTimeValue.LowPart = sysUserTime.dwLowDateTime;
    sysUserTimeValue.HighPart = sysUserTime.dwHighDateTime;
    
    ULARGE_INTEGER systemTime;
    systemTime.QuadPart = sysKernelTimeValue.QuadPart + sysUserTimeValue.QuadPart;
    
    // Считаем разницу во времени с последнего обновления
    double cpuUsage = 0.0;
    if (lastSystemTime.QuadPart != 0) {
        ULARGE_INTEGER systemTimeDiff;
        systemTimeDiff.QuadPart = systemTime.QuadPart - lastSystemTime.QuadPart;
        
        // Если есть разница во времени, считаем загрузку процессора
        if (systemTimeDiff.QuadPart > 0) {
            auto it = lastCPUUsage.find(processID);
            
            if (it != lastCPUUsage.end()) {
                ULARGE_INTEGER processTimeDiff;
                processTimeDiff.QuadPart = processTotalTime.QuadPart - it->second.QuadPart;
                
                // Вычисляем процент
                cpuUsage = (processTimeDiff.QuadPart * 100.0) / systemTimeDiff.QuadPart;
            }
        }
    }
    
    // Обновляем значения для следующего расчета
    if (lastSystemTime.QuadPart != systemTime.QuadPart) {
        lastCPUUsage[processID] = processTotalTime;
    }
    
    return cpuUsage;
} 