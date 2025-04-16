cmake_minimum_required(VERSION 3.10)
project(TaskManager VERSION 1.0)

# Устанавливаем стандарт C++
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Добавляем исполняемый файл
add_executable(TaskManager
    TaskManager.cpp
    ProcessInfo.h
    ProcessManager.h
    ProcessManager.cpp
    TaskManagerUI.h
    TaskManagerUI.cpp
)

# Делаем Windows GUI приложение вместо консольного
if(MSVC)
    set_target_properties(TaskManager PROPERTIES
        WIN32_EXECUTABLE TRUE
    )
endif()

# Указываем необходимые библиотеки Windows
target_link_libraries(TaskManager 
    psapi 
    comctl32
    gdiplus
    shlwapi
    iphlpapi
    Ws2_32
)

# Устанавливаем кодировку UTF-8 для исходников
if(MSVC)
    # Добавляем опцию UTF-8 для исходного кода
    target_compile_options(TaskManager PRIVATE 
        "/utf-8"
        # Отключаем некоторые предупреждения
        "/wd4996"  # Отключение предупреждений о устаревших функциях
        "/wd4244"  # Отключение предупреждений о преобразовании типов
    )
endif()

# Добавляем определение для использования общих контролов
target_compile_definitions(TaskManager PRIVATE
    _UNICODE
    UNICODE
    _WIN32_IE=0x0600
    WINVER=0x0601
    _WIN32_WINNT=0x0601
) 