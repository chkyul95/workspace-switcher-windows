#include <windows.h>
#include <iostream>

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

int main() {
	
	SetSystemAnimations(false);
	
	if (!RegisterHotKey(NULL, 1, MOD_ALT | MOD_NOREPEAT, '1') || !RegisterHotKey(NULL, 2, MOD_ALT | MOD_NOREPEAT, '2')) {
		std::cerr << "Failed to register hot key!" << std::endl;
		return 1;
	}
	
	std::cout << "Virtual Desktop Switcher Running... (Alt + 1: Previous, Alt + 2: Next, Ctrl + C: Exit)" << std::endl;
	
	MSG msg = {};
	while (GetMessage(&msg, NULL, 0, 0)) {
		if (msg.message == WM_HOTKEY) {
			if (msg.wParam == 1) {
				SwitchVirtualDesktop(false);
			} else if (msg.wParam == 2) {
				SwitchVirtualDesktop(true);
			}
		}
	}
	
	UnregisterHotKey(NULL, 1);
	UnregisterHotKey(NULL, 2);
	return 0;
}
