#pragma once

#include "initguid.h"
#include "stdafx.h"

#include "Registrar.h"
#include "ComBase.h"

// {D46464FB-0810-0001-A675-AB5A481BAC79}
_declspec(selectany) GUID CLSID_FFBDriver = 
    { 0xD46464FB, 0x0810, 0x0001, { 0xA6, 0x75, 0xAB, 0x5A, 0x48, 0x1B, 0xAC, 0x79 } };

//IID_IDirectInputEffectDriver

class FFBDriver
	: public CComBase<>, public InterfaceImpl<IDirectInputEffectDriver>
{
public:
	FFBDriver();
	~FFBDriver();

	STDMETHODIMP FFBDriver::QueryInterface(REFIID riid, LPVOID *ppv);

	HRESULT STDMETHODCALLTYPE DeviceID(DWORD, DWORD, DWORD, DWORD, LPVOID);
	HRESULT STDMETHODCALLTYPE GetVersions(LPDIDRIVERVERSIONS);
	HRESULT STDMETHODCALLTYPE Escape(DWORD, DWORD, LPDIEFFESCAPE);
	HRESULT STDMETHODCALLTYPE SetGain(DWORD, DWORD);
	HRESULT STDMETHODCALLTYPE SendForceFeedbackCommand(DWORD, DWORD);
	HRESULT STDMETHODCALLTYPE GetForceFeedbackState(DWORD, LPDIDEVICESTATE);
	HRESULT STDMETHODCALLTYPE DownloadEffect(DWORD, DWORD, LPDWORD, LPCDIEFFECT, DWORD);
	HRESULT STDMETHODCALLTYPE DestroyEffect(DWORD, DWORD);
	HRESULT STDMETHODCALLTYPE StartEffect(DWORD, DWORD, DWORD, DWORD);
	HRESULT STDMETHODCALLTYPE StopEffect(DWORD, DWORD);
	HRESULT STDMETHODCALLTYPE GetEffectStatus(DWORD, DWORD, LPDWORD);
};

