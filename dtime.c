/*******************************************************************************************
*
* Discord Timestamp Generator - Native Win32 API Version
*
* This tool is written in pure C using the Windows API (Win32) without any
* external libraries besides what's provided by the OS.
*
* It supports three modes:
* 1. Default GUI mode.
* 2. Console-only mode (--nogui) to get the current timestamp.
* 3. Read mode (--read <input>) to convert between timestamp formats.
* - Input: "HH:MM:SS DD/MM/YYYY" -> Output: <t:123:F>
* - Input: <t:123:F> -> Output: Human-readable date
*
********************************************************************************************/

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h> // Required for Up-Down controls (spinners)
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>

// Define IDs for our controls
#define IDC_YEAR_EDIT       101
#define IDC_MONTH_EDIT      102
#define IDC_DAY_EDIT        103
#define IDC_HOUR_EDIT       104
#define IDC_MINUTE_EDIT     105
#define IDC_SECOND_EDIT     106
#define IDC_GENERATE_BUTTON 113
#define IDC_COPY_BUTTON     114
#define IDC_OUTPUT_EDIT     115

// Global handles to our controls
HWND hYear, hMonth, hDay, hHour, hMinute, hSecond, hOutput;

// Forward declaration of functions
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void GenerateTimestamp(HWND hwnd);
void CopyToClipboard(HWND hwnd);
int RunNoGuiMode(void);
int RunReadMode(const char* inputStr);


int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
        LPSTR cmdLine = lpCmdLine;

        // Trim leading whitespace from command line
        while (*cmdLine && (*cmdLine == ' ' || *cmdLine == '\t')) {
                cmdLine++;
        }

        // Check for the --read flag
        if (strncmp(cmdLine, "--read ", 7) == 0) {
                // Pass the rest of the command line string, which is the input
                return RunReadMode(cmdLine + 7);
        }
        // Check for the --nogui flag
        else if (strcmp(cmdLine, "--nogui") == 0) {
                return RunNoGuiMode();
        }

        // If no flags, proceed with GUI mode
        const char CLASS_NAME[]  = "DiscordTimestampWindowClass";

        WNDCLASS wc = { 0 };
        wc.lpfnWndProc   = WindowProc;
        wc.hInstance     = hInstance;
        wc.lpszClassName = CLASS_NAME;
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wc.hCursor       = LoadCursor(NULL, IDC_ARROW);

        RegisterClass(&wc);

        HWND hwnd = CreateWindowEx(
                0, "DiscordTimestampWindowClass", "Discord Timestamp Generator",
                WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                CW_USEDEFAULT, CW_USEDEFAULT, 440, 240,
                NULL, NULL, hInstance, NULL);

        if (hwnd == NULL) {
                return 0;
        }

        ShowWindow(hwnd, nCmdShow);

        MSG msg = { 0 };
        while (GetMessage(&msg, NULL, 0, 0)) {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
        }

        return 0;
}

int RunReadMode(const char* inputStr)
{
        char cleanInput[256];
        const char* start = inputStr;
        size_t len;

        // Attach to parent console to print output
        if (AttachConsole(ATTACH_PARENT_PROCESS)) {
                FILE* f;
                freopen_s(&f, "CONOUT$", "w", stdout);
        }

        // Trim leading whitespace from the input string
        while (*start && (*start == ' ' || *start == '\t')) {
                start++;
        }

        // Copy to a mutable buffer and remove quotes if they exist
        strncpy(cleanInput, start, sizeof(cleanInput) - 1);
        cleanInput[sizeof(cleanInput) - 1] = '\0';
        len = strlen(cleanInput);

        if (len > 0 && cleanInput[0] == '"') {
                // Shift everything left by one to remove the opening quote
                memmove(cleanInput, cleanInput + 1, len); 
                len--;
        }
        if (len > 0 && cleanInput[len - 1] == '"') {
                // Null-terminate to remove the closing quote
                cleanInput[len - 1] = '\0';
        }


        // Check if the input is a Discord timestamp format like <t:12345:F>
        if (strncmp(cleanInput, "<t:", 3) == 0) {
                // It's a Discord timestamp, convert it TO human-readable time
                const char* numStart = cleanInput + 3; // Move past "<t:"
                char* endPtr;
                long long unixTimestampLong = strtoll(numStart, &endPtr, 10);

                if (endPtr == numStart || (*endPtr != ':' && *endPtr != '>')) {
                        printf("Error: Could not parse number from timestamp.\n");
                        FreeConsole();
                        return 1;
                }

                time_t unixTimestamp = (time_t)unixTimestampLong;
                struct tm* timeinfo = localtime(&unixTimestamp);
                char buffer[128];

                strftime(buffer, sizeof(buffer), "%A, %B %d, %Y at %I:%M:%S %p", timeinfo);

                printf("Timestamp %s corresponds to:\n%s\n", cleanInput, buffer);
        } else {
                // It's a human-readable time, convert it TO a Discord timestamp
                struct tm timeinfo = { 0 };
                int year, month, day, hour, minute, second;

                int itemsParsed = sscanf(cleanInput, "%d:%d:%d %d/%d/%d",
                                         &hour, &minute, &second,
                                         &day, &month, &year);

                if (itemsParsed != 6) {
                        printf("Error: Invalid input format provided.\n");
                        printf("Please use \"HH:MM:SS DD/MM/YYYY\" or \"<t:TIMESTAMP:F>\"\n");
                        FreeConsole();
                        return 1;
                }

                timeinfo.tm_year = year - 1900;
                timeinfo.tm_mon = month - 1;
                timeinfo.tm_mday = day;
                timeinfo.tm_hour = hour;
                timeinfo.tm_min = minute;
                timeinfo.tm_sec = second;
                timeinfo.tm_isdst = -1;

                time_t timestamp = mktime(&timeinfo);

                if (timestamp == -1) {
                        printf("Error: Could not convert the provided date/time. It may be invalid.\n");
                        FreeConsole();
                        return 1;
                }

                char buffer[256];
                sprintf(buffer, "<t:%ld:F>", (long)timestamp);

                printf("Generated Timestamp: %s\n", buffer);

                const size_t len = strlen(buffer) + 1;
                HGLOBAL hMem =  GlobalAlloc(GMEM_MOVEABLE, len);
                memcpy(GlobalLock(hMem), buffer, len);
                GlobalUnlock(hMem);

                OpenClipboard(NULL);
                EmptyClipboard();
                SetClipboardData(CF_TEXT, hMem);
                CloseClipboard();

                printf("Timestamp copied to clipboard.\n");
        }

        FreeConsole();
        return 0;
}

int RunNoGuiMode(void)
{
        // Attach to the parent console (e.g., cmd.exe) to allow printing output
        if (AttachConsole(ATTACH_PARENT_PROCESS)) {
                FILE* f;
                freopen_s(&f, "CONOUT$", "w", stdout); // Redirect stdout to the console
        }

        // Get current time
        time_t now = time(NULL);
        char buffer[256];
        
        // Format the timestamp string
        sprintf(buffer, "<t:%ld:F>", (long)now);

        // Print to console
        printf("Generated Timestamp: %s\n", buffer);

        // Copy the string to the clipboard
        const size_t len = strlen(buffer) + 1;
        HGLOBAL hMem =  GlobalAlloc(GMEM_MOVEABLE, len);
        memcpy(GlobalLock(hMem), buffer, len);
        GlobalUnlock(hMem);

        OpenClipboard(NULL); // We can pass NULL since we don't have a window handle
        EmptyClipboard();
        SetClipboardData(CF_TEXT, hMem);
        CloseClipboard();

        printf("Timestamp copied to clipboard.\n");

        // Clean up console
        FreeConsole();

        return 0;
}


LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
        switch (uMsg) {
        case WM_CREATE:
                {
                        HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);

                        CreateWindow("STATIC", "Year", WS_CHILD | WS_VISIBLE, 10, 15, 80, 20, hwnd, NULL, NULL, NULL);
                        CreateWindow("STATIC", "Month", WS_CHILD | WS_VISIBLE, 100, 15, 80, 20, hwnd, NULL, NULL, NULL);
                        CreateWindow("STATIC", "Day", WS_CHILD | WS_VISIBLE, 190, 15, 80, 20, hwnd, NULL, NULL, NULL);
                        CreateWindow("STATIC", "Hour", WS_CHILD | WS_VISIBLE, 10, 65, 80, 20, hwnd, NULL, NULL, NULL);
                        CreateWindow("STATIC", "Minute", WS_CHILD | WS_VISIBLE, 100, 65, 80, 20, hwnd, NULL, NULL, NULL);
                        CreateWindow("STATIC", "Second", WS_CHILD | WS_VISIBLE, 190, 65, 80, 20, hwnd, NULL, NULL, NULL);

                        time_t now = time(NULL);
                        struct tm *local = localtime(&now);
                        char buffer[10];

                        sprintf(buffer, "%d", local->tm_year + 1900);
                        hYear = CreateWindow("EDIT", buffer, WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER | ES_CENTER, 10, 35, 80, 20, hwnd, (HMENU)IDC_YEAR_EDIT, NULL, NULL);
                        sprintf(buffer, "%d", local->tm_mon + 1);
                        hMonth = CreateWindow("EDIT", buffer, WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER | ES_CENTER, 100, 35, 80, 20, hwnd, (HMENU)IDC_MONTH_EDIT, NULL, NULL);
                        sprintf(buffer, "%d", local->tm_mday);
                        hDay = CreateWindow("EDIT", buffer, WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER | ES_CENTER, 190, 35, 80, 20, hwnd, (HMENU)IDC_DAY_EDIT, NULL, NULL);
                        sprintf(buffer, "%d", local->tm_hour);
                        hHour = CreateWindow("EDIT", buffer, WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER | ES_CENTER, 10, 85, 80, 20, hwnd, (HMENU)IDC_HOUR_EDIT, NULL, NULL);
                        sprintf(buffer, "%d", local->tm_min);
                        hMinute = CreateWindow("EDIT", buffer, WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER | ES_CENTER, 100, 85, 80, 20, hwnd, (HMENU)IDC_MINUTE_EDIT, NULL, NULL);
                        sprintf(buffer, "%d", local->tm_sec);
                        hSecond = CreateWindow("EDIT", buffer, WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER | ES_CENTER, 190, 85, 80, 20, hwnd, (HMENU)IDC_SECOND_EDIT, NULL, NULL);

                        hOutput = CreateWindow("EDIT", "Generated timestamp will appear here.", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_READONLY | ES_CENTER, 10, 120, 400, 25, hwnd, (HMENU)IDC_OUTPUT_EDIT, NULL, NULL);

                        HWND hGenerate = CreateWindow("BUTTON", "Generate Timestamp", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, 10, 160, 195, 30, hwnd, (HMENU)IDC_GENERATE_BUTTON, NULL, NULL);
                        HWND hCopy = CreateWindow("BUTTON", "Copy to Clipboard", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, 215, 160, 195, 30, hwnd, (HMENU)IDC_COPY_BUTTON, NULL, NULL);

                        SendMessage(hYear, WM_SETFONT, (WPARAM)hFont, TRUE);
                        SendMessage(hMonth, WM_SETFONT, (WPARAM)hFont, TRUE);
                        SendMessage(hDay, WM_SETFONT, (WPARAM)hFont, TRUE);
                        SendMessage(hHour, WM_SETFONT, (WPARAM)hFont, TRUE);
                        SendMessage(hMinute, WM_SETFONT, (WPARAM)hFont, TRUE);
                        SendMessage(hSecond, WM_SETFONT, (WPARAM)hFont, TRUE);
                        SendMessage(hOutput, WM_SETFONT, (WPARAM)hFont, TRUE);
                        SendMessage(hGenerate, WM_SETFONT, (WPARAM)hFont, TRUE);
                        SendMessage(hCopy, WM_SETFONT, (WPARAM)hFont, TRUE);
                }
                return 0;

        case WM_COMMAND:
                {
                        if (LOWORD(wParam) == IDC_GENERATE_BUTTON && HIWORD(wParam) == BN_CLICKED) {
                                GenerateTimestamp(hwnd);
                        }
                        else if (LOWORD(wParam) == IDC_COPY_BUTTON && HIWORD(wParam) == BN_CLICKED) {
                                CopyToClipboard(hwnd);
                        }
                }
                return 0;

        case WM_DESTROY:
                PostQuitMessage(0);
                return 0;
        }
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void GenerateTimestamp(HWND hwnd)
{
        char buffer[20];
        struct tm timeinfo = { 0 };

        GetWindowText(hYear, buffer, 20); timeinfo.tm_year = atoi(buffer) - 1900;
        GetWindowText(hMonth, buffer, 20); timeinfo.tm_mon = atoi(buffer) - 1;
        GetWindowText(hDay, buffer, 20); timeinfo.tm_mday = atoi(buffer);
        GetWindowText(hHour, buffer, 20); timeinfo.tm_hour = atoi(buffer);
        GetWindowText(hMinute, buffer, 20); timeinfo.tm_min = atoi(buffer);
        GetWindowText(hSecond, buffer, 20); timeinfo.tm_sec = atoi(buffer);
        timeinfo.tm_isdst = -1;

        time_t timestamp = mktime(&timeinfo);

        if (timestamp != -1) {
                sprintf(buffer, "<t:%ld:F>", (long)timestamp);
        } else {
                sprintf(buffer, "Error: Invalid date/time.");
        }

        SetWindowText(hOutput, buffer);
}

void CopyToClipboard(HWND hwnd)
{
        char buffer[256];
        GetWindowText(hOutput, buffer, 256);

        if (strstr(buffer, "<t:") == NULL) {
                return;
        }

        const size_t len = strlen(buffer) + 1;
        HGLOBAL hMem =  GlobalAlloc(GMEM_MOVEABLE, len);
        memcpy(GlobalLock(hMem), buffer, len);
        GlobalUnlock(hMem);

        OpenClipboard(hwnd);
        EmptyClipboard();
        SetClipboardData(CF_TEXT, hMem);
        CloseClipboard();
}