#pragma once
#include <string>
#include <windows.h>

// Структура для хранения информации о процессе
struct ProcessInfo {
    DWORD pid;
    std::wstring name;
    DWORD parentPid;
    SIZE_T memoryUsage;
    double cpuUsage;
    std::wstring status;
    ULONGLONG workingSetSize;
};

class ProcessInfoClass {
public:
    ProcessInfoClass(DWORD id, const std::wstring& name, DWORD parentId, SIZE_T memoryUsage, float cpuUsage, const std::wstring& status)
        : id(id), name(name), parentId(parentId), memoryUsage(memoryUsage), cpuUsage(cpuUsage), status(status) {}

    DWORD GetId() const { return id; }
    std::wstring GetName() const { return name; }
    DWORD GetParentId() const { return parentId; }
    SIZE_T GetMemoryUsage() const { return memoryUsage; }
    float GetCpuUsage() const { return cpuUsage; }
    std::wstring GetStatus() const { return status; }

    void SetCpuUsage(float usage) { cpuUsage = usage; }
    void SetMemoryUsage(SIZE_T memory) { memoryUsage = memory; }
    void SetStatus(const std::wstring& newStatus) { status = newStatus; }

private:
    DWORD id;
    std::wstring name;
    DWORD parentId;
    SIZE_T memoryUsage;
    float cpuUsage;
    std::wstring status;
}; 