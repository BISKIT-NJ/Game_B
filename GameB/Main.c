/* Codename: GameB
*  2022 Carlos "BISKIT" Garcia
*  Following along youtube series on making a game from scratch in C
*  by Joseph Ryan Ries
*/

#pragma warning(push, 3)
#include <stdio.h>
#include <windows.h>
#include <Psapi.h> // Process Memory Counters
#include <emmintrin.h> // to use register functions -- Intel® Intrinsics Guide
#pragma warning(pop)

#include <stdint.h> // header to use uint8_t & others
#include "Main.h" // header file with the function declarations

#pragma comment(lib, "Winmm.lib") // lib in order to set the timer resolution

/* GLOBAL VARIABLES */
HWND gGameWindow; // always starts off as zero-0
BOOL gGameIsRunning; // always starts off as FALSE
GAMEBITMAP gBackBuffer;
GAMEPERFDATA gPerformanceData;
PLAYER gPlayer;


INT __stdcall WinMain(HINSTANCE Instance, HINSTANCE PreviousInstance, 
    PSTR CommandLine, INT CmdShow){
    
    UNREFERENCED_PARAMETER(Instance);
    UNREFERENCED_PARAMETER(PreviousInstance);
    UNREFERENCED_PARAMETER(CommandLine);
    UNREFERENCED_PARAMETER(CmdShow);

    MSG Message = { 0 };
    int64_t FrameStart = 0;
    int64_t FrameEnd = 0;
    int64_t ElapsedMicroseconds =0;
    int64_t ElapsedMicrosecondsAccumulatorRaw = 0;
    int64_t ElapsedMicrosecondsAccumulatorCooked = 0;
    HMODULE NtDllModuleHandle = NULL;
    FILETIME ProcessCreationTime = { 0 };
    FILETIME ProcessExitTime = { 0 };
    int64_t CurrentUserCPUTime = 0;
    int64_t CurrentKernelCPUTime = 0;
    int64_t PreviousUserCPUTime = 0;
    int64_t PreviousKernelCPUTime = 0;
    HANDLE ProcessHandle = GetCurrentProcess();

    if ((NtDllModuleHandle = GetModuleHandleA("ntdll.dll")) == NULL) {
        MessageBoxA(NULL, "Couldn't load ntdll.dll!", "Error!", MB_ICONEXCLAMATION | MB_OK);
        goto Exit;
    }

    if ((NtQueryTimerResolution = (_NtQueryTimerResolution)GetProcAddress(NtDllModuleHandle, 
        "NtQueryTimerResolution")) == NULL) {

        MessageBoxA(NULL, "Couldn't find the NtQueryTimerResolution in ntdll.dll!", "Error!", 
            MB_ICONEXCLAMATION | MB_OK);
        goto Exit;
    }

    NtQueryTimerResolution(&gPerformanceData.MaximumTimerResolution, 
        &gPerformanceData.MaximumTimerResolution, &gPerformanceData.CurrentTimerResolution);

    GetSystemInfo(&gPerformanceData.SystemInfo); // Calling this to get how many processors the PC has.
    GetSystemTimeAsFileTime((FILETIME*)&gPerformanceData.PreviousSystemTime);

    if (GameIsAlreadyRunning() == TRUE) {
        MessageBoxA(NULL, "Another instance of this program is already running!", "Error!", 
            MB_ICONEXCLAMATION | MB_OK);
        goto Exit;
    }

    if (timeBeginPeriod(1) == TIMERR_NOCANDO) { // sets timer resolution to 1 MS
        MessageBoxA(NULL, "Failed to set global timer resolution!", "Error!",
            MB_ICONEXCLAMATION | MB_OK);
        goto Exit;
    }

    SetPriorityClass(ProcessHandle, HIGH_PRIORITY_CLASS);

    if (CreateMainGameWindow() != ERROR_SUCCESS) { goto Exit; }

    QueryPerformanceFrequency((LARGE_INTEGER*)&gPerformanceData.PerfFrequency);

    gPerformanceData.DisplayDebugInfo = TRUE; // defaults the frame rate data to show on screen
    
    gBackBuffer.BitmapInfo.bmiHeader.biSize = sizeof(gBackBuffer.BitmapInfo.bmiHeader);
    gBackBuffer.BitmapInfo.bmiHeader.biWidth = GAME_RES_WIDTH;
    gBackBuffer.BitmapInfo.bmiHeader.biHeight = GAME_RES_HEIGHT;
    gBackBuffer.BitmapInfo.bmiHeader.biBitCount = GAME_BPP;
    gBackBuffer.BitmapInfo.bmiHeader.biCompression = BI_RGB;
    gBackBuffer.BitmapInfo.bmiHeader.biPlanes = 1;
    
    if ((gBackBuffer.Memory = VirtualAlloc(NULL, GAME_DRAWING_AREA_MEMORY_SIZE, 
        MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE)) == NULL) {
        
        MessageBoxA(NULL, "Failed to allocate memory for drawing surface!", "Error!", 
            MB_ICONEXCLAMATION | MB_OK);
        goto Exit;
    }

    //memset(gBackBuffer.Memory, 0x7F, GAME_DRAWING_AREA_MEMORY_SIZE);

    gPlayer.ScreenPosX = 25;
    gPlayer.ScreenPosY = 25;

    gGameIsRunning = TRUE;

    while (gGameIsRunning == TRUE) { // MAIN GAME LOOP

        QueryPerformanceCounter((LARGE_INTEGER*)&FrameStart);

        while (PeekMessageA(&Message, gGameWindow, 0, 0, PM_REMOVE)) {
            DispatchMessageA(&Message);
        }

        ProcessPlayerInput();
        RenderFrameGraphics();
        QueryPerformanceCounter((LARGE_INTEGER*)&FrameEnd);
        ElapsedMicroseconds = FrameEnd- FrameStart;
        ElapsedMicroseconds *= 1000000;
        ElapsedMicroseconds /= gPerformanceData.PerfFrequency;
        gPerformanceData.TotalFramesRendered++;
        ElapsedMicrosecondsAccumulatorRaw += ElapsedMicroseconds;
        
        while (ElapsedMicroseconds < TARGET_MICROSECONDS_PER_FRAME) {
            
            ElapsedMicroseconds = FrameEnd - FrameStart;
            ElapsedMicroseconds *= 1000000;
            ElapsedMicroseconds /= gPerformanceData.PerfFrequency;
            QueryPerformanceCounter((LARGE_INTEGER*)&FrameEnd);

            if (ElapsedMicroseconds < (TARGET_MICROSECONDS_PER_FRAME * 0.75f)) { Sleep(1); }
        }

        ElapsedMicrosecondsAccumulatorCooked += ElapsedMicroseconds;

        if ((gPerformanceData.TotalFramesRendered % CALCULATE_AVG_FPS_EVERY_X_FRAMES) == 0) {
            
            GetProcessTimes(ProcessHandle, &ProcessCreationTime,
                &ProcessExitTime, (FILETIME*)&CurrentKernelCPUTime, 
                (FILETIME*)&CurrentUserCPUTime);
            
            gPerformanceData.CPUPercent = ((double)CurrentKernelCPUTime - (double)PreviousKernelCPUTime)\
                + ((double)CurrentUserCPUTime - (double)PreviousUserCPUTime);

            gPerformanceData.CPUPercent /= gPerformanceData.SystemInfo.dwNumberOfProcessors;

            gPerformanceData.CPUPercent *= 100;

            GetProcessHandleCount(ProcessHandle, &gPerformanceData.HandleCount);

            K32GetProcessMemoryInfo(ProcessHandle, (PROCESS_MEMORY_COUNTERS*)&gPerformanceData.MemInfo,
                sizeof(gPerformanceData.MemInfo));

            gPerformanceData.RawFPSAverage = 1.0f / (((float)ElapsedMicrosecondsAccumulatorRaw / \
                CALCULATE_AVG_FPS_EVERY_X_FRAMES) * 0.000001f);
            gPerformanceData.CookedFPSAverage = 1.0f / (((float)ElapsedMicrosecondsAccumulatorCooked / \
                CALCULATE_AVG_FPS_EVERY_X_FRAMES) * 0.000001f);

            ElapsedMicrosecondsAccumulatorRaw = 0;
            ElapsedMicrosecondsAccumulatorCooked = 0;

            PreviousKernelCPUTime = CurrentKernelCPUTime;
            PreviousUserCPUTime = CurrentUserCPUTime;
            gPerformanceData.PreviousSystemTime = gPerformanceData.CurrentSystemTime;
        }
    }

Exit:

    return 0;
}

LRESULT CALLBACK MainWindowProc(_In_ HWND WindowHandle, _In_ UINT Message, 
    _In_ WPARAM WParam, _In_ LPARAM LParam) {

    LRESULT Result = 0;

    switch (Message) {
        case WM_CLOSE: {
            gGameIsRunning = FALSE;
            PostQuitMessage(0);
        }
        default: { Result = DefWindowProcA(WindowHandle, Message, WParam, LParam); }
    }
    return (Result);
}

DWORD CreateMainGameWindow(void) {

    DWORD Result = ERROR_SUCCESS;

    WNDCLASSEXA WindowClass = { 0 };

    WindowClass.cbSize = sizeof(WNDCLASSEXA);
    WindowClass.style = 0;  // style of window, we will fix this later
    WindowClass.lpfnWndProc = MainWindowProc; // 
    WindowClass.cbClsExtra = 0;
    WindowClass.cbWndExtra = 0;
    WindowClass.hInstance = GetModuleHandleA(NULL);
    WindowClass.hIcon = LoadIconA(NULL, IDI_APPLICATION); // this is a bult in windows icon
    WindowClass.hCursor = LoadCursor(NULL, IDC_ARROW);
    WindowClass.hbrBackground = CreateSolidBrush(RGB(255, 0, 222)); // hot pinkish color
    WindowClass.lpszMenuName = NULL;
    WindowClass.lpszClassName = GAME_NAME "_WindowClass"; // pointer to a string
    WindowClass.hIconSm = LoadIconA(NULL, IDI_APPLICATION); // handle to a small icon

    if (RegisterClassExA(&WindowClass) == 0) {
        
        Result = GetLastError();

        MessageBoxA(NULL, "Window Registration Failed!", "Error!", 
            MB_ICONEXCLAMATION | MB_OK);

        goto Exit;
    }

    gGameWindow = CreateWindowExA(0, WindowClass.lpszClassName, 
        "Window Title!!!", WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, 
        640, 480, NULL, NULL, GetModuleHandleA(NULL), NULL);

    if (gGameWindow == NULL) {
        
        Result = GetLastError();
        
        MessageBox(NULL, "Window Creation Failed!", "Error!", MB_ICONEXCLAMATION | MB_OK);
        goto Exit;
    }

    gPerformanceData.MonitorInfo.cbSize = sizeof(MONITORINFO);

    if (GetMonitorInfoA(MonitorFromWindow(gGameWindow, MONITOR_DEFAULTTOPRIMARY), 
        &gPerformanceData.MonitorInfo) == 0) {

        Result = ERROR_MONITOR_NO_DESCRIPTOR;
        goto Exit;
    }

    gPerformanceData.MonitorWidth = gPerformanceData.MonitorInfo.rcMonitor.right - \
        gPerformanceData.MonitorInfo.rcMonitor.left;
    gPerformanceData.MonitorHeight = gPerformanceData.MonitorInfo.rcMonitor.bottom - \
        gPerformanceData.MonitorInfo.rcMonitor.top;

    if (SetWindowLongPtrA(gGameWindow, GWL_STYLE, WS_VISIBLE) == 0) {

        Result = GetLastError();
        goto Exit;
    }

    if (SetWindowPos(gGameWindow, HWND_TOP, gPerformanceData.MonitorInfo.rcMonitor.left,
        gPerformanceData.MonitorInfo.rcMonitor.top, gPerformanceData.MonitorWidth, 
        gPerformanceData.MonitorHeight, SWP_FRAMECHANGED) == 0) {

        Result = GetLastError();
        goto Exit;
    }

Exit:

    return(Result);
}

BOOL GameIsAlreadyRunning(void) {
    
    HANDLE Mutex = NULL;

    Mutex = CreateMutexA(NULL, FALSE, GAME_NAME "_GameMutex");

    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        return(TRUE);
    } else {
        return(FALSE);
    }
}

void ProcessPlayerInput(void) {

    int16_t EscapeKeyIsDown = GetAsyncKeyState(VK_ESCAPE);
    int16_t DebugKeyIsDown = GetAsyncKeyState(VK_F1);
    int16_t LeftKeyIsDown = GetAsyncKeyState(VK_LEFT) | GetAsyncKeyState('A');
    int16_t RightKeyIsDown = GetAsyncKeyState(VK_RIGHT) | GetAsyncKeyState('D');
    int16_t UpKeyIsDown = GetAsyncKeyState(VK_UP) | GetAsyncKeyState('W');
    int16_t DownKeyIsDown = GetAsyncKeyState(VK_DOWN) | GetAsyncKeyState('S');

    static int16_t DebugKeyWasDown;
    static int16_t LeftKeyWasDown;
    static int16_t RightKeyWasDown;
    static int16_t UpKeyWasDown;
    static int16_t DownKeyWasDown;
    
    if (EscapeKeyIsDown) { SendMessageA(gGameWindow, WM_CLOSE, 0, 0); }

    if (DebugKeyIsDown && !DebugKeyWasDown) {
        gPerformanceData.DisplayDebugInfo = !gPerformanceData.DisplayDebugInfo;
    }

    if (LeftKeyIsDown ) {
        if (gPlayer.ScreenPosX > 0) {
            gPlayer.ScreenPosX--;
        }
    }

    if (RightKeyIsDown ) {
        if (gPlayer.ScreenPosX < GAME_RES_WIDTH - 16) {
            gPlayer.ScreenPosX++;
        }
    }

    if (DownKeyIsDown) {
        if (gPlayer.ScreenPosY < GAME_RES_HEIGHT - 16) {
            gPlayer.ScreenPosY++;
        }
    }

    if (UpKeyIsDown) {
        if (gPlayer.ScreenPosY > 0) {
            gPlayer.ScreenPosY--;
        }
    }

    DebugKeyWasDown = DebugKeyIsDown;
    LeftKeyWasDown = LeftKeyIsDown;
    RightKeyWasDown = RightKeyIsDown;
    UpKeyWasDown = UpKeyIsDown;
    DownKeyWasDown = DownKeyIsDown;
}

void RenderFrameGraphics(void) {
    
    __m128i QuadPixel = { 0x7F, 0x00, 0x00, 0x7F, 0x7F, 0x00, 0x00, 0x7F, 
        0x7F, 0x00, 0x00, 0x7F, 0x7F, 0x00, 0x00, 0x00};

    ClearScreen(QuadPixel);

    int32_t ScreenX = gPlayer.ScreenPosX;
    int32_t ScreenY = gPlayer.ScreenPosY;
    int32_t StartingScreenPixel = ((GAME_RES_WIDTH * GAME_RES_HEIGHT) - GAME_RES_WIDTH) - \
        (GAME_RES_WIDTH * ScreenY) + ScreenX;

    for (int32_t y = 0; y < 16; y++) {
        for (int32_t x = 0; x < 16; x++) {
            memset((PIXEL32*)gBackBuffer.Memory + (uintptr_t)StartingScreenPixel + x - \
                ((uintptr_t)GAME_RES_WIDTH * y), 0xFF, sizeof(PIXEL32));
        }
    }

    HDC DeviceContext = GetDC(gGameWindow);

    StretchDIBits(DeviceContext, 0, 0, gPerformanceData.MonitorWidth, 
        gPerformanceData.MonitorHeight, 0, 0, GAME_RES_WIDTH, GAME_RES_HEIGHT, 
        gBackBuffer.Memory, &gBackBuffer.BitmapInfo, DIB_RGB_COLORS, SRCCOPY);

    if (gPerformanceData.DisplayDebugInfo == TRUE) {
        SelectObject(DeviceContext, (HFONT)GetStockObject(ANSI_FIXED_FONT));

        char DebugTextBuffer[64] = { 0 };

        sprintf_s(DebugTextBuffer, _countof(DebugTextBuffer),
            "FPS Raw:    %.01f", gPerformanceData.RawFPSAverage);

        TextOutA(DeviceContext, 0, 0, DebugTextBuffer, (int)strlen(DebugTextBuffer));

        sprintf_s(DebugTextBuffer, _countof(DebugTextBuffer),
            "FPS Cooked: %.01f", gPerformanceData.CookedFPSAverage);

        TextOutA(DeviceContext, 0, 18, DebugTextBuffer, (int)strlen(DebugTextBuffer));

        sprintf_s(DebugTextBuffer, _countof(DebugTextBuffer),
            "Min Timer Res: %.02f", gPerformanceData.MinimumTimerResolution / 10000.0f);

        TextOutA(DeviceContext, 0, 36, DebugTextBuffer, (int)strlen(DebugTextBuffer));

        sprintf_s(DebugTextBuffer, _countof(DebugTextBuffer),
            "Max Timer Res: %.02f", gPerformanceData.MaximumTimerResolution / 10000.0f);

        TextOutA(DeviceContext, 0, 54, DebugTextBuffer, (int)strlen(DebugTextBuffer));

        sprintf_s(DebugTextBuffer, _countof(DebugTextBuffer),
            "Curr Timer Res: %.02f", gPerformanceData.CurrentTimerResolution / 10000.0f);

        TextOutA(DeviceContext, 0, 72, DebugTextBuffer, (int)strlen(DebugTextBuffer));

        sprintf_s(DebugTextBuffer, _countof(DebugTextBuffer),
            "Handles: %lu", gPerformanceData.HandleCount);

        TextOutA(DeviceContext, 0, 90, DebugTextBuffer, (int)strlen(DebugTextBuffer));

        sprintf_s(DebugTextBuffer, _countof(DebugTextBuffer),
            "Memory: %lu KB", gPerformanceData.MemInfo.PrivateUsage/1024);

        TextOutA(DeviceContext, 0, 108, DebugTextBuffer, (int)strlen(DebugTextBuffer));

        sprintf_s(DebugTextBuffer, _countof(DebugTextBuffer),
            "CPU:      %.02f %%", gPerformanceData.CPUPercent);

        TextOutA(DeviceContext, 0, 126, DebugTextBuffer, (int)strlen(DebugTextBuffer));
    }

    ReleaseDC(gGameWindow, DeviceContext);
}

__forceinline void ClearScreen(_In_ __m128i Color) {

    for (int x = 0; x < GAME_RES_WIDTH * GAME_RES_HEIGHT; x += 4) {

        _mm_store_si128((PIXEL32*)gBackBuffer.Memory + x, Color);
    }
}