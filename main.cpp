#include <windows.h>
#include <shellapi.h>

constexpr int IDC_CHK_ANIMATION = 2001;
constexpr UINT WM_TRAYICON       = WM_USER + 1;
constexpr UINT IDM_TRAY_RESTORE  = 3001;
constexpr UINT IDM_TRAY_EXIT     = 3002;
constexpr UINT TRAY_ICON_UID     = 100;

constexpr int HOTKEY_DESKTOP_PREV = 1;
constexpr int HOTKEY_DESKTOP_NEXT = 2;
constexpr int HOTKEY_MONITOR_OFF = 3;
constexpr int HOTKEY_TOGGLE_ALPHA = 4;

static BOOL g_origClientAreaAnim = TRUE;
static ANIMATIONINFO g_origAnimInfo = {};
static NOTIFYICONDATAW g_nid = {};
static UINT g_wmTaskbarCreated = 0;

void BackupSystemAnimations() {
	SystemParametersInfoW(
		SPI_GETCLIENTAREAANIMATION,
		0,
		&g_origClientAreaAnim,
		0
	);
	
	g_origAnimInfo.cbSize = sizeof(ANIMATIONINFO);
	SystemParametersInfoW(
		SPI_GETANIMATION,
		sizeof(ANIMATIONINFO),
		&g_origAnimInfo,
		0
	);
}

void SetSystemAnimations(bool enable) {
	SystemParametersInfoW(
		SPI_SETCLIENTAREAANIMATION,
		0,
		(PVOID)(uintptr_t)(enable ? TRUE : FALSE),
		SPIF_UPDATEINIFILE | SPIF_SENDCHANGE
	);
	
	ANIMATIONINFO ai = {};
	ai.cbSize = sizeof(ANIMATIONINFO);
	ai.iMinAnimate = enable ? 1 : 0;
	
	SystemParametersInfoW(
		SPI_SETANIMATION,
		sizeof(ANIMATIONINFO),
		&ai,
		SPIF_UPDATEINIFILE | SPIF_SENDCHANGE
	);
}

void RestoreSystemAnimations() {
	SystemParametersInfoW(
		SPI_SETCLIENTAREAANIMATION,
		0,
		(PVOID)(uintptr_t)g_origClientAreaAnim,
		SPIF_UPDATEINIFILE | SPIF_SENDCHANGE
	);
	
	SystemParametersInfoW(
		SPI_SETANIMATION,
		sizeof(ANIMATIONINFO),
		&g_origAnimInfo,
		SPIF_UPDATEINIFILE | SPIF_SENDCHANGE
	);
}

void AddTrayIcon(HWND hwnd) {
	ZeroMemory(&g_nid, sizeof(NOTIFYICONDATAW));
	g_nid.cbSize = sizeof(NOTIFYICONDATAW);
	g_nid.hWnd = hwnd;
	g_nid.uID = TRAY_ICON_UID;
	g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
	g_nid.uCallbackMessage = WM_TRAYICON;
	g_nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	lstrcpyW(g_nid.szTip, L"Stealth Desktop Switcher");
	
	Shell_NotifyIconW(NIM_ADD, &g_nid);
}

void RemoveTrayIcon() {
	if (g_nid.cbSize != 0) {
		Shell_NotifyIconW(NIM_DELETE, &g_nid);
		g_nid.cbSize = 0;
	}
}

void ShowTrayContextMenu(HWND hwnd) {
	POINT pt;
	GetCursorPos(&pt);
	
	HMENU hMenu = CreatePopupMenu();
	if (hMenu) {
		InsertMenuW(hMenu, -1, MF_BYPOSITION | MF_STRING, IDM_TRAY_RESTORE, L"Restore Window");
		InsertMenuW(hMenu, -1, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);
		InsertMenuW(hMenu, -1, MF_BYPOSITION | MF_STRING, IDM_TRAY_EXIT, L"Exit");
		
		SetForegroundWindow(hwnd);
		TrackPopupMenu(hMenu, TPM_RIGHTBUTTON | TPM_BOTTOMALIGN | TPM_LEFTALIGN, pt.x, pt.y, 0, hwnd, NULL);
		PostMessageW(hwnd, WM_NULL, 0, 0);
		
		DestroyMenu(hMenu);
	}
}

void RestoreWindowFromTray(HWND hwnd) {
	ShowWindow(hwnd, SW_RESTORE);
	SetForegroundWindow(hwnd);
	RemoveTrayIcon();
}

void MinimizeWindowToTray(HWND hwnd) {
	ShowWindow(hwnd, SW_HIDE);
	AddTrayIcon(hwnd);
}

void SwitchVirtualDesktop(bool goRight) {
	INPUT releaseAlt = {};
	releaseAlt.type = INPUT_KEYBOARD;
	releaseAlt.ki.wVk = VK_MENU;
	releaseAlt.ki.dwFlags = KEYEVENTF_KEYUP;
	SendInput(1, &releaseAlt, sizeof(INPUT));
	
	INPUT inputs[6] = {};
	WORD keyArrow = goRight ? VK_RIGHT : VK_LEFT;
	
	inputs[0].type = INPUT_KEYBOARD;
	inputs[0].ki.wVk = VK_CONTROL;
	
	inputs[1].type = INPUT_KEYBOARD;
	inputs[1].ki.wVk = VK_LWIN;
	
	inputs[2].type = INPUT_KEYBOARD;
	inputs[2].ki.wVk = keyArrow;
	
	inputs[3].type = INPUT_KEYBOARD;
	inputs[3].ki.wVk = keyArrow;
	inputs[3].ki.dwFlags = KEYEVENTF_KEYUP;
	
	inputs[4].type = INPUT_KEYBOARD;
	inputs[4].ki.wVk = VK_LWIN;
	inputs[4].ki.dwFlags = KEYEVENTF_KEYUP;
	
	inputs[5].type = INPUT_KEYBOARD;
	inputs[5].ki.wVk = VK_CONTROL;
	inputs[5].ki.dwFlags = KEYEVENTF_KEYUP;
	
	SendInput(6, inputs, sizeof(INPUT));
}

DWORD WINAPI MonitorOffThread(LPVOID lpParam) {
	HWND hwnd = (HWND)lpParam;
	Sleep(100);
	SendMessage(hwnd, WM_SYSCOMMAND, SC_MONITORPOWER, 2);
	return 0;
}

void TurnOffMonitor(HWND hwnd) {
	CloseHandle(CreateThread(NULL, 0, MonitorOffThread, hwnd, 0, NULL));
}

void ToggleForegroundTransparency() {
	HWND fgHwnd = GetForegroundWindow();
	if (!fgHwnd || fgHwnd == GetDesktopWindow() || fgHwnd == GetShellWindow()) return;
	
	LONG_PTR exStyle = GetWindowLongPtrW(fgHwnd, GWL_EXSTYLE);
	BYTE alpha = 255;
	DWORD flags = 0;
	
	if ((exStyle & WS_EX_LAYERED) && GetLayeredWindowAttributes(fgHwnd, NULL, &alpha, &flags)) {
		if (alpha < 255) {
			SetLayeredWindowAttributes(fgHwnd, 0, 255, LWA_ALPHA);
			SetWindowLongPtrW(fgHwnd, GWL_EXSTYLE, exStyle & ~WS_EX_LAYERED);
			return;
		}
	}
	
	SetWindowLongPtrW(fgHwnd, GWL_EXSTYLE, exStyle | WS_EX_LAYERED);
	SetLayeredWindowAttributes(fgHwnd, 0, 50, LWA_ALPHA);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	if (msg == g_wmTaskbarCreated) {
		if (!IsWindowVisible(hwnd)) {
			AddTrayIcon(hwnd);
		}
		return 0;
	}
	
	switch (msg) {
		case WM_CREATE: {
			BackupSystemAnimations();
			
			g_wmTaskbarCreated = RegisterWindowMessageW(L"TaskbarCreated");
			
			HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
			
			HWND hCheck = CreateWindowW(
				L"BUTTON", L"Turn off screen switching animation (Recommended)",
				WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,
				20, 20, 360, 24, hwnd, (HMENU)IDC_CHK_ANIMATION, NULL, NULL
			);
			if (!hCheck) return -1;
			SendMessageW(hCheck, WM_SETFONT, (WPARAM)hFont, TRUE);
			CheckDlgButton(hwnd, IDC_CHK_ANIMATION, BST_CHECKED);
			
			HWND hStatic = CreateWindowW(
				L"STATIC",
				L"- Alt + 1: Previous desktop\n"
				L"- Alt + 2: Next desktop\n"
				L"- Alt + `: Turn off monitor to sleep mode\n"
				L"- Alt + Q: Toggle window transparency",
				WS_VISIBLE | WS_CHILD | SS_LEFT,
				20, 60, 360, 70, hwnd, NULL, NULL, NULL
			);
			if (!hStatic) return -1;
			SendMessageW(hStatic, WM_SETFONT, (WPARAM)hFont, TRUE);
			
			SetSystemAnimations(false);
			
			if (!RegisterHotKey(hwnd, HOTKEY_DESKTOP_PREV, MOD_ALT | MOD_NOREPEAT, '1')) {
				MessageBoxW(hwnd, L"Failed to register Alt+1 hotkey!", L"Error", MB_ICONERROR);
				return -1;
			}
			if (!RegisterHotKey(hwnd, HOTKEY_DESKTOP_NEXT, MOD_ALT | MOD_NOREPEAT, '2')) {
				MessageBoxW(hwnd, L"Failed to register Alt+2 hotkey!", L"Error", MB_ICONERROR);
				UnregisterHotKey(hwnd, HOTKEY_DESKTOP_PREV);
				return -1;
			}
			if (!RegisterHotKey(hwnd, HOTKEY_MONITOR_OFF, MOD_ALT | MOD_NOREPEAT, VK_OEM_3)) {
				MessageBoxW(hwnd, L"Failed to register Alt+` hotkey!", L"Error", MB_ICONERROR);
				UnregisterHotKey(hwnd, HOTKEY_DESKTOP_PREV);
				UnregisterHotKey(hwnd, HOTKEY_DESKTOP_NEXT);
				return -1;
			}
			
			if (!RegisterHotKey(hwnd, HOTKEY_TOGGLE_ALPHA, MOD_ALT | MOD_NOREPEAT, 'Q')) {
				MessageBox(hwnd, L"Failed to register Alt+Q hotkey!", L"Error", MB_ICONERROR);
				UnregisterHotKey(hwnd, HOTKEY_DESKTOP_PREV);
				UnregisterHotKey(hwnd, HOTKEY_DESKTOP_NEXT);
				UnregisterHotKey(hwnd, HOTKEY_MONITOR_OFF);
				return -1;
			}
			
			return 0;
		}
		
		case WM_SYSCOMMAND: {
			if ((wParam & 0xFFF0) == SC_MINIMIZE) {
				MinimizeWindowToTray(hwnd);
				return 0;
			}
			break;
		}
		
		case WM_TRAYICON: {
			if (lParam == WM_RBUTTONUP) {
				ShowTrayContextMenu(hwnd);
			} else if (lParam == WM_LBUTTONUP || lParam == WM_LBUTTONDBLCLK) {
				RestoreWindowFromTray(hwnd);
			}
			return 0;
		}
		
		case WM_COMMAND: {
			int id = LOWORD(wParam);
			if (id == IDC_CHK_ANIMATION) {
				bool checked = (IsDlgButtonChecked(hwnd, IDC_CHK_ANIMATION) == BST_CHECKED);
				if (checked) {
					SetSystemAnimations(false);
				} else {
					RestoreSystemAnimations();
				}
			} else if (id == IDM_TRAY_RESTORE) {
				RestoreWindowFromTray(hwnd);
			} else if (id == IDM_TRAY_EXIT) {
				DestroyWindow(hwnd);
			}
			return 0;
		}
		case WM_HOTKEY: {
			switch (wParam) {
				case HOTKEY_DESKTOP_PREV: SwitchVirtualDesktop(false); break;
				case HOTKEY_DESKTOP_NEXT: SwitchVirtualDesktop(true); break;
				case HOTKEY_MONITOR_OFF: TurnOffMonitor(hwnd); break;
				case HOTKEY_TOGGLE_ALPHA: ToggleForegroundTransparency(); break;
			}
			return 0;
		}
		case WM_DESTROY:
			RestoreSystemAnimations();
			
			UnregisterHotKey(hwnd, HOTKEY_DESKTOP_PREV);
			UnregisterHotKey(hwnd, HOTKEY_DESKTOP_NEXT);
			UnregisterHotKey(hwnd, HOTKEY_MONITOR_OFF);
			UnregisterHotKey(hwnd, HOTKEY_TOGGLE_ALPHA);
			PostQuitMessage(0);
			return 0;
	}
	return DefWindowProcW(hwnd, msg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow) {
	const wchar_t CLASS_NAME[] = L"StealthDesktopSwitchClass";
	
	WNDCLASSW wc = {};
	wc.lpfnWndProc = WndProc;
	wc.hInstance = hInstance;
	wc.lpszClassName = CLASS_NAME;
	wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	
	if (!RegisterClassW(&wc)) {
		MessageBoxW(NULL, L"Failed to register window class!", L"Error", MB_ICONERROR);
		return 1;
	}
	
	HWND hwnd = CreateWindowExW(
		WS_EX_DLGMODALFRAME,
		CLASS_NAME, L"Stealth Desktop Switcher",
		WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
		CW_USEDEFAULT, CW_USEDEFAULT, 420, 180,
		NULL, NULL, hInstance, NULL
	);
	
	if (!hwnd) {
		MessageBoxW(NULL, L"Failed to create window!", L"Error", MB_ICONERROR);
		return 1;
	}
	
	ShowWindow(hwnd, nCmdShow);
	UpdateWindow(hwnd);
	
	MSG msg = {};
	BOOL bRet;
	while ((bRet = GetMessageW(&msg, NULL, 0, 0)) != 0) {
		if (bRet == -1) {
			return 1;
		}
		TranslateMessage(&msg);
		DispatchMessageW(&msg);
	}
	return 0;
}

