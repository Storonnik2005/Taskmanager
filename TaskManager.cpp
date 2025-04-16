#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <iostream>
#include <vector>
#include <string>
#include <iomanip>
#include <algorithm>
#include <memory>
#include "ProcessInfo.h"
#include "TaskManagerUI.h"

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow) {
    // Launch Task Manager interface
    TaskManagerUI taskManager;
    taskManager.Run();
    
    return 0;
} 