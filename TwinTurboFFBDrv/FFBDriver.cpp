
#include "FFBDriver.h"
#include "vibration/VibrationController.h"

#ifdef _DEBUG
#include <sys/stat.h>
#define LOGPATH "c:\\cygwin\\var\\log\\"
#define LOGFILE "ffbdriver.log"
#endif

void LogMessage(const char* fmt, ...) {
	va_list args;
#ifdef _DEBUG
	SYSTEMTIME st;
	GetSystemTime(&st);
	struct stat statdres = { 0 }, statfres = { 0 };

	if (stat(LOGPATH LOGFILE, &statfres)) {
		if (!(stat(LOGPATH, &statdres))) return; // No log dir at all.
		else if (!(statdres.st_mode & S_IWRITE & S_IFDIR)) return; // log dir not writable or not a dir
	} else if (!(statfres.st_mode & S_IWRITE)) return; // File exists but not writable.

	FILE* log_file = fopen(LOGPATH LOGFILE, "a");

	// [XXXX-XX-XX XX:XX:XX.XXX UTC: ] => 29 chars
	fprintf_s(log_file, "%04d-%02d-%02d %02d:%02d:%02d.%03d UTC: ",
		st.wYear, st.wMonth, st.wDay,
		st.wHour, st.wMinute, st.wSecond,
		st.wMilliseconds);

	va_start(args, fmt);
	vfprintf_s(log_file, fmt, args);
	va_end(args);
	fwrite("\n", 1, 1, log_file);

	fclose(log_file);
#endif
}

FFBDriver::FFBDriver()
{
}

FFBDriver::~FFBDriver()
{
	vibration::VibrationController::Reset(0);
	vibration::VibrationController::Reset(1);
}


STDMETHODIMP FFBDriver::QueryInterface(REFIID riid, LPVOID *ppv)
{
	*ppv = NULL;
	if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_IDirectInputEffectDriver))
	{
		*ppv = (IDirectInputEffectDriver *)this;
		_AddRef();
		return S_OK;
	}
#ifdef _DEBUG
	LogMessage("Query interface called.");
#endif
	return E_NOINTERFACE;
}


HRESULT STDMETHODCALLTYPE FFBDriver::DeviceID(
		DWORD             dwDIVer,
		DWORD             dwExternalID,
		DWORD             fBegin,
		DWORD             dwInternalId,
		LPVOID            lpInfo) 
{
	LPDIHIDFFINITINFO lpDIHIDInitInfo = (LPDIHIDFFINITINFO)lpInfo;

#ifdef _DEBUG
	LogMessage("DeviceID\n\tdwDIVer=0x%04x\n\tdwExternalID=0x%04x\n\tfBegin=0x%04x\n\tdwInternalId=0x%04x",
		dwDIVer, dwExternalID, fBegin, dwInternalId);
#endif

	vibration::VibrationController::SetHidDevicePath(lpDIHIDInitInfo->pwszDeviceInterface, dwExternalID);

	return S_OK;
}

HRESULT STDMETHODCALLTYPE FFBDriver::GetVersions(LPDIDRIVERVERSIONS lpVersions) {
#ifdef _DEBUG
	LogMessage("GetVersions");
#endif

	lpVersions->dwFFDriverVersion = 0x100;
	lpVersions->dwFirmwareRevision = 0x100;
	lpVersions->dwHardwareRevision = 0x100;

	return S_OK;
}
HRESULT STDMETHODCALLTYPE FFBDriver::Escape(THIS_ DWORD, DWORD, LPDIEFFESCAPE) {
#ifdef _DEBUG
	LogMessage("Escape!");
#endif
	return S_OK;
}
HRESULT STDMETHODCALLTYPE FFBDriver::SetGain(
	DWORD dwID,
	DWORD dwGain) 
{
#ifdef _DEBUG
	LogMessage("SetGain\n\tdwID=0x%04x\n\tdwGain=0x%04x",
		dwID, dwGain);
#endif

	/*
	DWORD newGain = (dwGain / 38);
	if (newGain > 0xfe)
		currentGain = 0xfe;
	else
		currentGain = (byte)newGain;
	*/

	return DIERR_UNSUPPORTED;
}

HRESULT STDMETHODCALLTYPE FFBDriver::SendForceFeedbackCommand(
	DWORD dwID,
	DWORD dwCommand) 
{
#ifdef _DEBUG
	LogMessage("SendForceFeedbackCommand\n\tdwID=0x%04x\n\tdwCommand=0x%04x",
		dwID, dwCommand);
	LogMessage("Command match table:\n"
		" [%c] Reset                [%c] Stop All Effects      [%c] Pause\n"
		" [%c] Continue (resume)    [%c] Set Actuators on      [%c] Set Actuators off",
#define yn(cmd) dwCommand & cmd ? 'x' : ' '
		yn(DISFFC_RESET), yn(DISFFC_STOPALL), yn(DISFFC_PAUSE),
		yn(DISFFC_CONTINUE), yn(DISFFC_SETACTUATORSON), yn(DISFFC_SETACTUATORSOFF));
#undef yn
#endif

	switch (dwCommand) {
	case DISFFC_RESET:
		vibration::VibrationController::Reset(dwID);
		break;

	case DISFFC_STOPALL:
		vibration::VibrationController::DequeueAllEffects(dwID);
		break;

	case DISFFC_PAUSE:
		vibration::VibrationController::Pause(dwID);
		break;

	case DISFFC_CONTINUE:
		vibration::VibrationController::Resume(dwID);
		break;

	case DISFFC_SETACTUATORSON:
	case DISFFC_SETACTUATORSOFF:
		break;
	}
	
	return S_OK;
}

HRESULT STDMETHODCALLTYPE FFBDriver::GetForceFeedbackState(THIS_ DWORD, LPDIDEVICESTATE) {
#ifdef _DEBUG
	LogMessage("GetForceFeedbackState!");
#endif
	return DIERR_UNSUPPORTED;
}

HRESULT STDMETHODCALLTYPE FFBDriver::DownloadEffect(
	DWORD       dwDeviceID,
	DWORD       dwInternalEffectType,
	LPDWORD     lpdwDnloadID,
	LPCDIEFFECT lpEffect,
	DWORD       dwFlags) 
{
#ifdef _DEBUG
	LogMessage("DownloadEffect: dwID:0x%04x, dwEffectID:0x%04x, dwFlags:0x%04x, peff->dwDuration:%4lu, gain:%4lu",
		dwDeviceID, dwInternalEffectType, dwFlags, lpEffect->dwDuration, lpEffect->dwGain);
	LogMessage("Modification flags (dwFlags):\n"
		" [%c] duration             [%c] sample period         [%c] gain\n"
		" [%c] trigger button       [%c] trig.btn.repeat intvl [%c] axes\n"
		" [%c] direction            [%c] envelope              [%c] type-specific params\n"
		" [%c] force restart        [%c] deny restart          [%c] don't download",
#define yn(mask) dwFlags & mask ? 'x' : ' '
		yn(DIEP_DURATION), yn(DIEP_SAMPLEPERIOD), yn(DIEP_GAIN), yn(DIEP_TRIGGERBUTTON), yn(DIEP_TRIGGERREPEATINTERVAL),
		yn(DIEP_AXES), yn(DIEP_DIRECTION), yn(DIEP_ENVELOPE), yn(DIEP_TYPESPECIFICPARAMS), yn(DIEP_START),
		yn(DIEP_NORESTART), yn(DIEP_NODOWNLOAD));
#undef yn
#endif

	return vibration::VibrationController::EnqueueEffect(dwDeviceID, dwInternalEffectType, lpdwDnloadID, lpEffect, dwFlags);
}

HRESULT STDMETHODCALLTYPE FFBDriver::DestroyEffect(DWORD, DWORD) {
#ifdef _DEBUG
	LogMessage("DestroyEffect!");
#endif
	return DIERR_UNSUPPORTED;
}
HRESULT STDMETHODCALLTYPE FFBDriver::StartEffect(DWORD, DWORD, DWORD, DWORD) {
#ifdef _DEBUG
	LogMessage("StartEffect!");
#endif
	return DIERR_UNSUPPORTED;
}
HRESULT STDMETHODCALLTYPE FFBDriver::StopEffect(DWORD dwID, DWORD dwEffect) {
#ifdef _DEBUG
	LogMessage("StopEffect!");
#endif
	return DIERR_UNSUPPORTED;
}
HRESULT STDMETHODCALLTYPE FFBDriver::GetEffectStatus(DWORD, DWORD, LPDWORD) {
#ifdef _DEBUG
	LogMessage("GetEffectStatus!");
#endif
	return DIERR_UNSUPPORTED;
}
