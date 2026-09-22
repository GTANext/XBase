#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>

#include <string>
#include <vector>

namespace {

constexpr int IdModList = 1001;
constexpr int IdFileList = 1002;
constexpr int IdContent = 1003;
constexpr int IdRefresh = 1004;
constexpr int IdOpenDir = 1005;
constexpr int IdCopy = 1006;

HWND g_modList = nullptr;
HWND g_fileList = nullptr;
HWND g_content = nullptr;
std::wstring g_xbaseDir;
std::vector<std::wstring> g_mods;

std::wstring Utf8ToWide(const std::string& value) {
    if (value.empty()) return {};
    const int size = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), nullptr, 0);
    if (size <= 0) return {};
    std::wstring wide(static_cast<std::size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), wide.data(), size);
    return wide;
}

std::string WideToUtf8(const std::wstring& value) {
    if (value.empty()) return {};
    const int size = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    if (size <= 0) return {};
    std::string utf8(static_cast<std::size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), utf8.data(), size, nullptr, nullptr);
    return utf8;
}

std::wstring ExeDirectory() {
    wchar_t buffer[MAX_PATH] = {};
    const DWORD length = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
    if (length == 0) return {};
    std::wstring path(buffer, length);
    const std::size_t slash = path.find_last_of(L"\\/");
    return slash == std::wstring::npos ? path : path.substr(0, slash);
}

bool DirectoryExists(const std::wstring& path) {
    const DWORD attributes = GetFileAttributesW(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

bool FileExists(const std::wstring& path) {
    const DWORD attributes = GetFileAttributesW(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_DIRECTORY);
}

std::wstring Join(const std::wstring& left, const std::wstring& right) {
    if (left.empty()) return right;
    std::wstring result = left;
    if (result.back() != L'\\' && result.back() != L'/') result += L'\\';
    result += right;
    return result;
}

std::string ReadText(const std::wstring& path) {
    HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return {};

    LARGE_INTEGER size{};
    if (!GetFileSizeEx(file, &size) || size.QuadPart <= 0 || size.QuadPart > 16 * 1024 * 1024) {
        CloseHandle(file);
        return {};
    }

    std::string content(static_cast<std::size_t>(size.QuadPart), '\0');
    DWORD read = 0;
    const BOOL ok = ReadFile(file, content.data(), static_cast<DWORD>(content.size()), &read, nullptr);
    CloseHandle(file);
    if (!ok) return {};
    content.resize(read);

    // 去掉 UTF-8 BOM，编辑器显示更干净
    if (content.size() >= 3 && static_cast<unsigned char>(content[0]) == 0xEF
        && static_cast<unsigned char>(content[1]) == 0xBB
        && static_cast<unsigned char>(content[2]) == 0xBF) {
        content.erase(0, 3);
    }
    return content;
}

std::wstring ModDirectory(const std::wstring& mod) {
    if (mod.empty()) return g_xbaseDir;
    return Join(Join(g_xbaseDir, L"Mods"), mod);
}

void RefreshMods() {
    SendMessageW(g_modList, LB_RESETCONTENT, 0, 0);
    SendMessageW(g_fileList, LB_RESETCONTENT, 0, 0);
    SetWindowTextW(g_content, L"");
    g_mods.clear();

    SendMessageW(g_modList, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"XBase 自身"));
    g_mods.emplace_back();

    const std::wstring modsRoot = Join(g_xbaseDir, L"Mods");
    WIN32_FIND_DATAW data{};
    HANDLE find = FindFirstFileW(Join(modsRoot, L"*").c_str(), &data);
    if (find != INVALID_HANDLE_VALUE) {
        do {
            if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) continue;
            if (wcscmp(data.cFileName, L".") == 0 || wcscmp(data.cFileName, L"..") == 0) continue;
            SendMessageW(g_modList, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(data.cFileName));
            g_mods.emplace_back(data.cFileName);
        } while (FindNextFileW(find, &data));
        FindClose(find);
    }

    SendMessageW(g_modList, LB_SETCURSEL, 0, 0);
    SendMessageW(g_modList, WM_COMMAND, MAKEWPARAM(IdModList, LBN_SELCHANGE), 0);
}

void RefreshFiles() {
    SendMessageW(g_fileList, LB_RESETCONTENT, 0, 0);
    SetWindowTextW(g_content, L"");

    const int index = static_cast<int>(SendMessageW(g_modList, LB_GETCURSEL, 0, 0));
    if (index < 0 || index >= static_cast<int>(g_mods.size())) return;

    const std::wstring directory = ModDirectory(g_mods[index]);
    const wchar_t* candidates[] = {L"config.json", L"debug.log", L"hotkeys.json"};
    for (const wchar_t* name : candidates) {
        if (FileExists(Join(directory, name))) {
            SendMessageW(g_fileList, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(name));
        }
    }

    if (SendMessageW(g_fileList, LB_GETCOUNT, 0, 0) > 0) {
        SendMessageW(g_fileList, LB_SETCURSEL, 0, 0);
        SendMessageW(g_fileList, WM_COMMAND, MAKEWPARAM(IdFileList, LBN_SELCHANGE), 0);
    }
}

void ShowSelectedFile() {
    const int modIndex = static_cast<int>(SendMessageW(g_modList, LB_GETCURSEL, 0, 0));
    const int fileIndex = static_cast<int>(SendMessageW(g_fileList, LB_GETCURSEL, 0, 0));
    if (modIndex < 0 || modIndex >= static_cast<int>(g_mods.size()) || fileIndex < 0) return;

    wchar_t name[260] = {};
    SendMessageW(g_fileList, LB_GETTEXT, fileIndex, reinterpret_cast<LPARAM>(name));
    const std::wstring path = Join(ModDirectory(g_mods[modIndex]), name);
    const std::string content = ReadText(path);
    if (content.empty()) {
        SetWindowTextW(g_content, L"(空文件或读取失败)");
        return;
    }
    SetWindowTextW(g_content, Utf8ToWide(content).c_str());
}

void OpenCurrentDirectory() {
    const int modIndex = static_cast<int>(SendMessageW(g_modList, LB_GETCURSEL, 0, 0));
    const std::wstring directory = modIndex >= 0 && modIndex < static_cast<int>(g_mods.size())
        ? ModDirectory(g_mods[modIndex])
        : g_xbaseDir;
    ShellExecuteW(nullptr, L"open", directory.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

void CopyContent() {
    const int length = GetWindowTextLengthW(g_content);
    if (length <= 0) return;
    std::wstring text(static_cast<std::size_t>(length) + 1, L'\0');
    GetWindowTextW(g_content, &text[0], length + 1);
    text.resize(static_cast<std::size_t>(length));

    if (!OpenClipboard(g_content)) return;
    EmptyClipboard();
    const std::size_t bytes = (text.size() + 1) * sizeof(wchar_t);
    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (memory) {
        void* target = GlobalLock(memory);
        if (target) {
            memcpy(target, text.c_str(), bytes);
            GlobalUnlock(memory);
            SetClipboardData(CF_UNICODETEXT, memory);
        }
    }
    CloseClipboard();
}

LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE: {
        const DWORD listStyle = WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | LBS_NOTIFY;
        g_modList = CreateWindowExW(0, L"LISTBOX", L"", listStyle, 12, 12, 190, 260, window,
                                    reinterpret_cast<HMENU>(IdModList), nullptr, nullptr);
        g_fileList = CreateWindowExW(0, L"LISTBOX", L"", listStyle, 214, 12, 170, 120, window,
                                     reinterpret_cast<HMENU>(IdFileList), nullptr, nullptr);
        g_content = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                                    WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
                                    214, 140, 470, 132, window, reinterpret_cast<HMENU>(IdContent), nullptr, nullptr);
        CreateWindowExW(0, L"BUTTON", L"刷新", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 394, 12, 90, 26, window,
                        reinterpret_cast<HMENU>(IdRefresh), nullptr, nullptr);
        CreateWindowExW(0, L"BUTTON", L"打开目录", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 394, 44, 90, 26, window,
                        reinterpret_cast<HMENU>(IdOpenDir), nullptr, nullptr);
        CreateWindowExW(0, L"BUTTON", L"复制内容", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 394, 76, 90, 26, window,
                        reinterpret_cast<HMENU>(IdCopy), nullptr, nullptr);

        HFONT font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
        for (HWND control : {g_modList, g_fileList, g_content}) {
            SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
        }

        RefreshMods();
        return 0;
    }
    case WM_SIZE: {
        const int width = LOWORD(lParam);
        const int height = HIWORD(lParam);
        if (width > 320 && height > 200) {
            MoveWindow(g_modList, 12, 12, 190, height - 24, TRUE);
            MoveWindow(g_content, 214, 140, width - 214 - 12, height - 152, TRUE);
        }
        return 0;
    }
    case WM_COMMAND: {
        const int id = LOWORD(wParam);
        const int code = HIWORD(wParam);
        if (id == IdModList && code == LBN_SELCHANGE) {
            RefreshFiles();
        } else if (id == IdFileList && code == LBN_SELCHANGE) {
            ShowSelectedFile();
        } else if (id == IdRefresh) {
            RefreshMods();
        } else if (id == IdOpenDir) {
            OpenCurrentDirectory();
        } else if (id == IdCopy) {
            CopyContent();
        }
        return 0;
    }
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(window, message, wParam, lParam);
    }
}

} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand) {
    g_xbaseDir = ExeDirectory();
    if (g_xbaseDir.empty()) return 1;
    // 允许放在游戏根目录或 XBase 目录内运行
    if (_wcsicmp(g_xbaseDir.substr(g_xbaseDir.find_last_of(L"\\/") + 1).c_str(), L"XBase") != 0) {
        g_xbaseDir = Join(g_xbaseDir, L"XBase");
    }

    const std::wstring className = L"XBaseViewerWindow";
    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.lpfnWndProc = &WindowProc;
    windowClass.hInstance = instance;
    windowClass.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
    windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
    windowClass.lpszClassName = className.c_str();
    RegisterClassExW(&windowClass);

    HWND window = CreateWindowExW(
        0, className.c_str(), L"XBase 日志与配置查看器",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 720, 420,
        nullptr, nullptr, instance, nullptr);
    if (!window) return 1;

    ShowWindow(window, showCommand);
    UpdateWindow(window);

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0)) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return 0;
}
