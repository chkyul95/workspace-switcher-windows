#include <windows.h>
#include <shellapi.h>
#include <vector>
#include <string>
#include <thread>
#include <chrono>
#include <memory>
#include <functional>

class SystemAnimationManager {
public:
	SystemAnimationManager() {
		Backup();
	}
	
	~SystemAnimationManager() {
		Restore();
	}
	
	SystemAnimationManager(const SystemAnimationManager&) = delete;
	SystemAnimationManager& operator=(const SystemAnimationManager&) = delete;
	
	void SetEnabled(bool enable) {
		SystemParametersInfoW(SPI_SETCLIENTAREAANIMATION, 0, reinterpret_cast<PVOID>(static_cast<uintptr_t>(enable ? TRUE : FALSE)), SPIF_UPDATEINIFILE | SPIF_SENDCHANGE);
		
		ANIMATIONINFO ai{};
		ai.cbSize = sizeof(ANIMATIONINFO);
		ai.iMinAnimate = enable ? 1 : 0;
		
		SystemParametersInfoW(SPI_SETANIMATION, sizeof(ANIMATIONINFO), &ai, SPIF_UPDATEINIFILE | SPIF_SENDCHANGE);
		m_isModified = true;
	}
	
	void Restore() {
		if (!m_isModified) return;
		
		SystemParametersInfoW(SPI_SETCLIENTAREAANIMATION, 0, reinterpret_cast<PVOID>(static_cast<uintptr_t>(m_origClientAreaAnim)), SPIF_UPDATEINIFILE | SPIF_SENDCHANGE);
		SystemParametersInfoW(SPI_SETANIMATION, sizeof(ANIMATIONINFO), &m_origAnimInfo, SPIF_UPDATEINIFILE | SPIF_SENDCHANGE);
		m_isModified = false;
	}

private:
	void Backup() {
		SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &m_origClientAreaAnim, 0);
		m_origAnimInfo.cbSize = sizeof(ANIMATIONINFO);
		SystemParametersInfoW(SPI_GETANIMATION, sizeof(ANIMATIONINFO), &m_origAnimInfo, 0);
	}
	
	BOOL m_origClientAreaAnim{ TRUE };
	ANIMATIONINFO m_origAnimInfo{};
	bool m_isModified{ false };
};

class HotkeyManager {
public:
	explicit HotkeyManager(HWND hwnd = nullptr) : m_hwnd(hwnd) {}
	
	~HotkeyManager() {
		UnregisterAll();
	}
	
	void SetWindow(HWND hwnd) { m_hwnd = hwnd; }
	
	bool Register(int id, UINT fsModifiers, UINT vk) {
		if (!m_hwnd) return false;
		if (RegisterHotKey(m_hwnd, id, fsModifiers, vk)) {
			m_registeredIds.push_back(id);
			return true;
		}
		return false;
	}
	
	void UnregisterAll() {
		if (!m_hwnd) return;
		for (int id : m_registeredIds) {
			UnregisterHotKey(m_hwnd, id);
		}
		m_registeredIds.clear();
	}
	
private:
	HWND m_hwnd{ nullptr };
	std::vector<int> m_registeredIds;
};

class TrayIcon {
public:
	TrayIcon() = default;
	
	TrayIcon(HWND hwnd, UINT uID, UINT callbackMsg, std::wstring_view tip) {
		Init(hwnd, uID, callbackMsg, tip);
	}
	
	~TrayIcon() {
		Remove();
	}
	
	void Init(HWND hwnd, UINT uID, UINT callbackMsg, std::wstring_view tip) {
		ZeroMemory(&m_nid, sizeof(NOTIFYICONDATAW));
		m_nid.cbSize = sizeof(NOTIFYICONDATAW);
		m_nid.hWnd = hwnd;
		m_nid.uID = uID;
		m_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
		m_nid.uCallbackMessage = callbackMsg;
		m_nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);
		wcsncpy_s(m_nid.szTip, tip.data(), _TRUNCATE);
	}
	
	bool Add() {
		if (!m_isAdded && m_nid.cbSize != 0) {
			m_isAdded = Shell_NotifyIconW(NIM_ADD, &m_nid);
		}
		return m_isAdded;
	}
	
	bool Remove() {
		if (m_isAdded) {
			Shell_NotifyIconW(NIM_DELETE, &m_nid);
			m_isAdded = false;
			return true;
		}
		return false;
	}
	
	bool isAdded() const { return m_isAdded; }

private:
	NOTIFYICONDATAW m_nid{};
	bool m_isAdded{ false };
};

class DesktopUtils {
public:
	static void SwitchVirtualDesktop(bool goRight) {
		INPUT releaseAlt{};
		releaseAlt.type = INPUT_KEYBOARD;
		releaseAlt.ki.wVk = VK_MENU;
		releaseAlt.ki.dwFlags = KEYEVENTF_KEYUP;
		SendInput(1, &releaseAlt, sizeof(INPUT));
		
		INPUT inputs[6]{};
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
	
	static void TurnOffMonitorAsync(HWND hwnd) {
		std::thread([hwnd]() {
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			SendMessage(hwnd, WM_SYSCOMMAND, SC_MONITORPOWER, 2);
		}).detach();
	}
	
	static void ToggleForegroundTransparency() {
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
};

HFONT CreateSystemFont() {
	NONCLIENTMETRICSW ncm{};
	ncm.cbSize = sizeof(NONCLIENTMETRICSW);
	
	if (SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(NONCLIENTMETRICSW), &ncm, 0)) {
		return CreateFontIndirectW(&ncm.lfMessageFont);
	}
	
	return static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
}

class MainWindow {
public:
	static constexpr int IDC_CHK_ANIMATION = 2001;
	static constexpr UINT WM_TRAYICON = WM_USER + 1;
	static constexpr UINT IDM_TRAY_RESTORE = 3001;
	static constexpr UINT IDM_TRAY_EXIT = 3002;
	static constexpr UINT TRAY_ICON_UID = 100;
	
	enum HotkeyID {
		DesktopPrev = 1,
		DesktopNext = 2,
		MonitorOff = 3,
		ToggleAlpha = 4
	};
	
	MainWindow() = default;
	
	bool Create(HINSTANCE hInstance, int nCmdShow) {
		m_hInstance = hInstance;
		const wchar_t CLASS_NAME[] = L"StealthDesktopSwitchClass";
		
		WNDCLASSW wc{};
		wc.lpfnWndProc = StaticWndProc;
		wc.hInstance = hInstance;
		wc.lpszClassName = CLASS_NAME;
		wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
		wc.hCursor = LoadCursor(NULL, IDC_ARROW);
		
		if (!RegisterClassW(&wc)) return false;
		
		m_hwnd = CreateWindowExW(
		WS_EX_DLGMODALFRAME,
			CLASS_NAME, L"Stealth Desktop Switcher",
			WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
			CW_USEDEFAULT, CW_USEDEFAULT, 420, 180,
			NULL, NULL, hInstance, this
		);

		if (!m_hwnd) return false;

		ShowWindow(m_hwnd, nCmdShow);
		UpdateWindow(m_hwnd);
		return true;
	}

	int RunMessageLoop() {
		MSG msg{};
		BOOL bRet;
		while ((bRet = GetMessageW(&msg, NULL, 0, 0)) != 0) {
			if (bRet == -1) return 1;
			TranslateMessage(&msg);
			DispatchMessageW(&msg);
		}
		return static_cast<int>(msg.wParam);
	}

private:
	HINSTANCE m_hInstance{ nullptr };
	HWND m_hwnd{ nullptr };
	UINT m_wmTaskbarCreated{ 0 };
	HFONT m_hFont{ nullptr };
	
	SystemAnimationManager m_animManager;
	HotkeyManager m_hotkeyManager;
	TrayIcon m_trayIcon;
	
	static LRESULT CALLBACK StaticWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
		MainWindow* pThis = nullptr;

		if (msg == WM_NCCREATE) {
			auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
			pThis = reinterpret_cast<MainWindow*>(cs->lpCreateParams);
			SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
			pThis->m_hwnd = hwnd;
		} else {
			pThis = reinterpret_cast<MainWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
		}

		if (pThis) {
			LRESULT res = pThis->HandleMessage(msg, wParam, lParam);
			if (msg == WM_NCDESTROY) {
				SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
				pThis->m_hwnd = nullptr;
			}
			return res;
		}
		return DefWindowProcW(hwnd, msg, wParam, lParam);
	}
	
	LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
		if (msg == m_wmTaskbarCreated) {
			if (!IsWindowVisible(m_hwnd)) {
				m_trayIcon.Add();
			}
			return 0;
		}
		
		switch (msg) {
			case WM_CREATE:
				return OnCreate();

			case WM_SYSCOMMAND:
				if ((wParam & 0xFFF0) == SC_MINIMIZE) {
					MinimizeToTray();
					return 0;
				}
				break;
			
			case WM_TRAYICON:
				return OnTrayIconMessage(lParam);
			
			case WM_COMMAND:
				return OnCommand(LOWORD(wParam));
			
			case WM_HOTKEY:
				return OnHotkey(static_cast<int>(wParam));
			
			case WM_DESTROY:
				if (m_hFont) {
					DeleteObject(m_hFont);
					m_hFont = nullptr;
				}
				PostQuitMessage(0);
				return 0;
		}
		return DefWindowProcW(m_hwnd, msg, wParam, lParam);
	}
	
	LRESULT OnCreate() {
		m_wmTaskbarCreated = RegisterWindowMessageW(L"TaskbarCreated");
		m_trayIcon.Init(m_hwnd, TRAY_ICON_UID, WM_TRAYICON, L"Stealth Desktop Switcher");
		m_hotkeyManager.SetWindow(m_hwnd);
		
		m_hFont = CreateSystemFont();
		
		HWND hCheck = CreateWindowW(
			L"BUTTON", L"Turn off screen switching animation (Recommended)",
			WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,
			20, 20, 360, 24, m_hwnd, reinterpret_cast<HMENU>(IDC_CHK_ANIMATION), NULL, NULL
		);
		if (!hCheck) return -1;
		SendMessageW(hCheck, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFont), TRUE);
		CheckDlgButton(m_hwnd, IDC_CHK_ANIMATION, BST_CHECKED);
		
		HWND hStatic = CreateWindowW(
			L"STATIC",
			L"- Alt + 1: Previous desktop\n"
			L"- Alt + 2: Next desktop\n"
			L"- Alt + `: Turn off monitor to sleep mode\n"
			L"- Alt + Q: Toggle window transparency",
			WS_VISIBLE | WS_CHILD | SS_LEFT,
			20, 60, 360, 70, m_hwnd, NULL, NULL, NULL
		);
		if (!hStatic) return -1;
		SendMessageW(hStatic, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFont), TRUE);
		
		m_animManager.SetEnabled(false);
		
		if (!m_hotkeyManager.Register(HotkeyID::DesktopPrev, MOD_ALT | MOD_NOREPEAT, '1') ||
			!m_hotkeyManager.Register(HotkeyID::DesktopNext, MOD_ALT | MOD_NOREPEAT, '2') ||
			!m_hotkeyManager.Register(HotkeyID::MonitorOff,  MOD_ALT | MOD_NOREPEAT, VK_OEM_3) ||
			!m_hotkeyManager.Register(HotkeyID::ToggleAlpha, MOD_ALT | MOD_NOREPEAT, 'Q')) {
			MessageBoxW(m_hwnd, L"Failed to register hotkeys!", L"Error", MB_ICONERROR);
			return -1;
		}
		
		return 0;
	}
	
	LRESULT OnTrayIconMessage(LPARAM lParam) {
		if (lParam == WM_RBUTTONUP) {
			ShowTrayContextMenu();
		} else if (lParam == WM_LBUTTONUP || lParam == WM_LBUTTONDBLCLK) {
			RestoreFromTray();
		}
		return 0;
	}
	
	LRESULT OnCommand(int id) {
		if (id == IDC_CHK_ANIMATION) {
			bool checked = (IsDlgButtonChecked(m_hwnd, IDC_CHK_ANIMATION) == BST_CHECKED);
			if (checked) {
				m_animManager.SetEnabled(false);
			} else {
				m_animManager.Restore();
			}
		} else if (id == IDM_TRAY_RESTORE) {
			RestoreFromTray();
		} else if (id == IDM_TRAY_EXIT) {
			DestroyWindow(m_hwnd);
		}
		return 0;
	}
	
	LRESULT OnHotkey(int hotkeyId) {
		switch (hotkeyId) {
			case HotkeyID::DesktopPrev: DesktopUtils::SwitchVirtualDesktop(false); break;
			case HotkeyID::DesktopNext: DesktopUtils::SwitchVirtualDesktop(true); break;
			case HotkeyID::MonitorOff:  DesktopUtils::TurnOffMonitorAsync(m_hwnd); break;
			case HotkeyID::ToggleAlpha: DesktopUtils::ToggleForegroundTransparency(); break;
		}
		return 0;
	}
	
	void MinimizeToTray() {
		ShowWindow(m_hwnd, SW_HIDE);
		m_trayIcon.Add();
	}
	
	void RestoreFromTray() {
		ShowWindow(m_hwnd, SW_RESTORE);
		SetForegroundWindow(m_hwnd);
		m_trayIcon.Remove();
	}
	
	void ShowTrayContextMenu() {
		POINT pt;
		GetCursorPos(&pt);
		
		HMENU hMenu = CreatePopupMenu();
		if (hMenu) {
			InsertMenuW(hMenu, -1, MF_BYPOSITION | MF_STRING, IDM_TRAY_RESTORE, L"Restore Window");
			InsertMenuW(hMenu, -1, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);
			InsertMenuW(hMenu, -1, MF_BYPOSITION | MF_STRING, IDM_TRAY_EXIT, L"Exit");
			
			SetForegroundWindow(m_hwnd);
			TrackPopupMenu(hMenu, TPM_RIGHTBUTTON | TPM_BOTTOMALIGN | TPM_LEFTALIGN, pt.x, pt.y, 0, m_hwnd, NULL);
			PostMessageW(m_hwnd, WM_NULL, 0, 0);
			
			DestroyMenu(hMenu);
		}
	}
};


int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow) {
	MainWindow app;
	if (!app.Create(hInstance, nCmdShow)) {
		MessageBoxW(NULL, L"Failed to create window!", L"Error", MB_ICONERROR);
		return 1;
	}
	
	return app.RunMessageLoop();
}

