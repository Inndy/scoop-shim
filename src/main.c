#include <windows.h>
#include <string.h>
#include <stdarg.h>

void __attribute__((noreturn)) fatalf(LPCWSTR fmt, ...) {
    va_list args;
    va_start(args, fmt);
    static WCHAR buf[1024];
    wvsprintfW(buf, fmt, args);
    MessageBoxW(NULL, buf, L"Error", MB_ICONERROR | MB_SETFOREGROUND);
    va_end(args);
    ExitProcess(1);
}

void WinMainCRTStartup() {
	const WCHAR new_suffix[] = L".shim";
    static WCHAR path[4096];

    DWORD n = GetModuleFileNameW(NULL, path, ARRAYSIZE(path) - ARRAYSIZE(new_suffix));
    if (!n) {
        fatalf(L"Can not get path of current exe");
    }

    PWCHAR p = wcsrchr(path, '.');
    if (p == NULL) {
        fatalf(L"Can not get base path of current exe");
    }

	wcsncpy(p, new_suffix, path - p + ARRAYSIZE(path));

    HANDLE hFile = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        if (GetLastError() == ERROR_FILE_NOT_FOUND) {
            fatalf(L"Can not find config file");
        } else {
            fatalf(L"Can not open config file");
        }
    }

    static char config_buff[4096];
    if (!ReadFile(hFile, config_buff, ARRAYSIZE(config_buff) - 1, &n, NULL)) {
        fatalf(L"Can not read config file");
    }

    CloseHandle(hFile);

    if (n < ARRAYSIZE(config_buff)) {
        config_buff[n] = 0;
    }

    int len = MultiByteToWideChar(CP_UTF8, 0, config_buff, n, 0, 0);
    if (!len) {
        fatalf(L"Can not decode config file");
    }

    PWCHAR config_text = HeapAlloc(GetProcessHeap(), 0, (len + 1) * sizeof(WCHAR));
    if (config_text == NULL) {
        fatalf(L"HeapAlloc failed");
    }

    len = MultiByteToWideChar(CP_UTF8, 0, config_buff, n, config_text, len + 1);
    if (!len) {
        fatalf(L"Can not decode config file");
    }

    config_text[len] = 0;

    if (p = wcschr(config_text, '\r')) *p = 0;
    if (p = wcschr(config_text, '\n')) *p = 0;

    if (wcsncmp(config_text, L"search=", 7)) {
        fatalf(L"Invalid config file");
    }

    const int buf_count = 4096;
    PWCHAR path_expanded = HeapAlloc(GetProcessHeap(), 0, buf_count * sizeof(WCHAR));

    n = ExpandEnvironmentStringsW(config_text + 7, path_expanded, buf_count);
    if (n == 0) {
        fatalf(L"Can not determine search path");
    }

    HeapFree(GetProcessHeap(), 0, config_text);

    p = wcsrchr(path, '\\');
    if (p == NULL) p = path;
    PWCHAR p2 = wcsrchr(p, '.');
    if (p2 != NULL) {
        *p2 = 0;
    }

    wcsncat(path_expanded, p, buf_count);
    wcsncat(path_expanded, L".*", buf_count);

    WIN32_FIND_DATAW find;
    HANDLE hSearch = FindFirstFileW(path_expanded, &find);
    if (hSearch == INVALID_HANDLE_VALUE) {
        if (GetLastError() == ERROR_FILE_NOT_FOUND) {
            fatalf(L"target file not found: %s", path_expanded);
        } else {
            fatalf(L"Can not search in the path: %s\r\nerr: %d", path_expanded, GetLastError());
        }
    }

    do {
        if (find.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            continue;
        }

        p = wcsrchr(path_expanded, '\\');
        if (p == NULL) {
            fatalf(L"Invalid path: %s", path_expanded);
        }
        p[1] = 0;
        wcsncat(path_expanded, find.cFileName, buf_count);
    } while(FindNextFileW(hSearch, &find));

    FindClose(hSearch);

    SHELLEXECUTEINFOW shell = {
        .cbSize = sizeof(shell),
        .lpVerb = L"open",
        .lpFile = path_expanded,
        .nShow = SW_SHOW,
    };

    if (!ShellExecuteExW(&shell)) {
        fatalf(L"ShellExecuteExW failed: %d", GetLastError());
    }

    CloseHandle(shell.hProcess);

    HeapFree(GetProcessHeap(), 0, path_expanded);

    ExitProcess(0);
}