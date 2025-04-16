#pragma once
#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <vector>
#include <unordered_map>
#include <memory>
#include <string>
#include <map>
#include <iphlpapi.h>
#include "ProcessInfo.h"

// Структура для хранения системной информации
struct SystemInfo {
    double cpuUsage;
    double memoryUsagePercent;
    SIZE_T totalMemory;
    SIZE_T memoryUsed;
    SIZE_T memoryAvailable;
    double networkSendSpeedKBps;
    double networkReceiveSpeedKBps;
};

class ProcessManager {
public:
    ProcessManager();
    ~ProcessManager();

    void UpdateProcessList();
    const std::vector<ProcessInfo>& GetProcesses() const;
    std::vector<ProcessInfo> GetProcessList();
    
    bool TerminateProcess(DWORD processId);
    bool StartNewProcess(const std::wstring& processPath);
    
    double GetSystemCpuUsage();
    SIZE_T GetSystemMemoryInfo(SIZE_T& totalPhysMem, SIZE_T& availPhysMem);
    
    // Новый метод для получения системной информации в структуре
    SystemInfo GetSystemInfo();

    std::wstring GetProcessStatus(DWORD processId);

private:
    std::vector<ProcessInfo> processes;
    std::map<DWORD, ULARGE_INTEGER> lastCPUUsage;
    ULARGE_INTEGER lastSystemTime;

    ULONGLONG lastNetInBytes;
    ULONGLONG lastNetOutBytes;
    ULARGE_INTEGER lastNetTime;

    void UpdateCpuUsage();
    double CalculateCpuUsage(HANDLE hProcess, DWORD processID);
}; 