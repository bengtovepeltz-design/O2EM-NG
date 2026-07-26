#include "metadata_text_editor.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <algorithm>
#include <string>

namespace
{
    constexpr wchar_t WindowClassName[] = L"O2EMNG_MetadataTextEditor";
    constexpr int EditId = 1001;
    constexpr int SaveId = 1002;
    constexpr int CancelId = 1003;

    struct EditorState
    {
        HWND window = nullptr;
        HWND edit = nullptr;
        HWND owner = nullptr;
        HFONT font = nullptr;
        bool multiline = false;
        bool finished = false;
        bool accepted = false;
        std::wstring title;
        std::wstring value;
        WNDPROC originalEditProc = nullptr;
    };

    std::wstring Utf8ToWide(const std::string& text)
    {
        if (text.empty()) return {};
        const int count = MultiByteToWideChar(
            CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
        if (count <= 0) return {};
        std::wstring result(static_cast<std::size_t>(count), L'\0');
        MultiByteToWideChar(
            CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), count);
        return result;
    }

    std::string WideToUtf8(const std::wstring& text)
    {
        if (text.empty()) return {};
        const int count = WideCharToMultiByte(
            CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0,
            nullptr, nullptr);
        if (count <= 0) return {};
        std::string result(static_cast<std::size_t>(count), '\0');
        WideCharToMultiByte(
            CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), count,
            nullptr, nullptr);
        return result;
    }

    EditorState* StateFromWindow(HWND hwnd)
    {
        return reinterpret_cast<EditorState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    LRESULT CALLBACK EditSubclassProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
    {
        EditorState* state = reinterpret_cast<EditorState*>(
            GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (!state || !state->originalEditProc)
            return DefWindowProcW(hwnd, message, wParam, lParam);

        if (message == WM_KEYDOWN)
        {
            const bool controlDown = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
            if (controlDown && (wParam == 'A' || wParam == 'a'))
            {
                SendMessageW(hwnd, EM_SETSEL, 0, -1);
                return 0;
            }
            if (controlDown && (wParam == 'S' || wParam == 's'))
            {
                SendMessageW(state->window, WM_COMMAND, MAKEWPARAM(SaveId, BN_CLICKED), 0);
                return 0;
            }
            if (wParam == VK_ESCAPE)
            {
                SendMessageW(state->window, WM_COMMAND, MAKEWPARAM(CancelId, BN_CLICKED), 0);
                return 0;
            }
        }

        return CallWindowProcW(state->originalEditProc, hwnd, message, wParam, lParam);
    }

    void Finish(EditorState* state, bool accepted)
    {
        if (!state || state->finished) return;

        if (accepted)
        {
            const int length = GetWindowTextLengthW(state->edit);
            std::wstring text(static_cast<std::size_t>(length) + 1u, L'\0');
            if (length > 0)
                GetWindowTextW(state->edit, text.data(), length + 1);
            text.resize(static_cast<std::size_t>(length));
            state->value = std::move(text);
        }

        state->accepted = accepted;
        state->finished = true;
        DestroyWindow(state->window);
    }

    LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
    {
        EditorState* state = StateFromWindow(hwnd);

        if (message == WM_NCCREATE)
        {
            const auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
            state = static_cast<EditorState*>(create->lpCreateParams);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
            state->window = hwnd;
        }

        switch (message)
        {
        case WM_CREATE:
        {
            const DWORD editStyle = WS_CHILD | WS_VISIBLE | WS_TABSTOP |
                ES_LEFT | ES_AUTOVSCROLL |
                (state->multiline
                    ? (ES_MULTILINE | ES_WANTRETURN | WS_VSCROLL)
                    : ES_AUTOHSCROLL);

            state->edit = CreateWindowExW(
                WS_EX_CLIENTEDGE, L"EDIT", state->value.c_str(), editStyle,
                16, 16, 700, state->multiline ? 390 : 28,
                hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(EditId)), GetModuleHandleW(nullptr), nullptr);

            CreateWindowExW(
                0, L"BUTTON", L"Save", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
                500, state->multiline ? 420 : 58, 100, 30,
                hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(SaveId)), GetModuleHandleW(nullptr), nullptr);
            CreateWindowExW(
                0, L"BUTTON", L"Cancel", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                616, state->multiline ? 420 : 58, 100, 30,
                hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(CancelId)), GetModuleHandleW(nullptr), nullptr);

            state->font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
            SendMessageW(state->edit, WM_SETFONT, reinterpret_cast<WPARAM>(state->font), TRUE);
            for (int id : { SaveId, CancelId })
            {
                HWND control = GetDlgItem(hwnd, id);
                SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(state->font), TRUE);
            }

            SendMessageW(state->edit, EM_SETLIMITTEXT, 1024 * 1024, 0);
            SetWindowLongPtrW(state->edit, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
            state->originalEditProc = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(
                state->edit, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(EditSubclassProc)));
            SendMessageW(state->edit, EM_SETSEL, 0, 0);
            SetFocus(state->edit);
            return 0;
        }

        case WM_SIZE:
            if (state && state->edit)
            {
                RECT client{};
                GetClientRect(hwnd, &client);
                const int width = (std::max)(320, static_cast<int>(client.right - client.left));
                const int height = (std::max)(120, static_cast<int>(client.bottom - client.top));
                const int buttonY = height - 46;
                MoveWindow(state->edit, 16, 16, width - 32,
                    state->multiline ? buttonY - 28 : 28, TRUE);
                MoveWindow(GetDlgItem(hwnd, SaveId), width - 232, buttonY, 100, 30, TRUE);
                MoveWindow(GetDlgItem(hwnd, CancelId), width - 116, buttonY, 100, 30, TRUE);
            }
            return 0;

        case WM_COMMAND:
            if (LOWORD(wParam) == SaveId) { Finish(state, true); return 0; }
            if (LOWORD(wParam) == CancelId) { Finish(state, false); return 0; }
            break;

        case WM_CLOSE:
            Finish(state, false);
            return 0;
        }

        return DefWindowProcW(hwnd, message, wParam, lParam);
    }

    bool RegisterEditorClass()
    {
        static bool registered = false;
        if (registered) return true;

        WNDCLASSEXW windowClass{};
        windowClass.cbSize = sizeof(windowClass);
        windowClass.lpfnWndProc = WindowProc;
        windowClass.hInstance = GetModuleHandleW(nullptr);
        windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        windowClass.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
        windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
        windowClass.lpszClassName = WindowClassName;
        windowClass.hIconSm = windowClass.hIcon;

        registered = RegisterClassExW(&windowClass) != 0 ||
            GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
        return registered;
    }
}

bool OpenMetadataTextEditor(
    const char* fieldLabel,
    bool multiline,
    std::string& value)
{
    if (!RegisterEditorClass()) return false;

    EditorState state;
    state.owner = GetActiveWindow();
    state.multiline = multiline;
    state.title = L"O2EM-NG - Edit ";
    state.title += Utf8ToWide(fieldLabel ? fieldLabel : "Metadata");
    state.value = Utf8ToWide(value);

    const int width = multiline ? 760 : 620;
    const int height = multiline ? 500 : 135;
    RECT work{};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
    const int x = work.left + ((work.right - work.left) - width) / 2;
    const int y = work.top + ((work.bottom - work.top) - height) / 2;

    if (state.owner) EnableWindow(state.owner, FALSE);

    HWND window = CreateWindowExW(
        WS_EX_DLGMODALFRAME,
        WindowClassName,
        state.title.c_str(),
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME,
        x, y, width, height,
        state.owner, nullptr, GetModuleHandleW(nullptr), &state);

    if (!window)
    {
        if (state.owner) EnableWindow(state.owner, TRUE);
        return false;
    }

    ShowWindow(window, SW_SHOW);
    UpdateWindow(window);

    MSG message{};
    while (!state.finished && GetMessageW(&message, nullptr, 0, 0) > 0)
    {
        if (!IsDialogMessageW(window, &message))
        {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }

    if (state.owner)
    {
        EnableWindow(state.owner, TRUE);
        SetForegroundWindow(state.owner);
    }

    if (state.accepted)
        value = WideToUtf8(state.value);
    return state.accepted;
}
