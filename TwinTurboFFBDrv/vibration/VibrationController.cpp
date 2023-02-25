#include "VibrationController.h"
#include <algorithm>
#include <Hidsdi.h>

#undef DISABLE_INFINITE_VIBRATION

#define MAX_EFFECTS 5
#define MAXC(a, b) ((a) > (b) ? (a) : (b))

#ifdef _DEBUG
void LogMessage(const char* fmt, ...);
#endif

namespace vibration {
	bool quitVibrationThread[2] = { false };

	void SendHidCommand(HANDLE hHidDevice, const byte* buff, DWORD buffsz) {
		HidD_SetOutputReport(hHidDevice, (PVOID)buff, 5);
	}
	void SendVibrationForce(HANDLE hHidDevice, byte forceSmallMotor, byte forceBigMotor, DWORD dwID) {
		byte buffer1[] = {
			0x00, 0x01, 0x00, forceBigMotor, forceSmallMotor
		};
		buffer1[0] = (byte) dwID + 1;
		SendHidCommand(hHidDevice, buffer1, 5);
	}
	void SendVibrationStop(HANDLE hHidDevice, DWORD dwID) {
		byte GP_STOP_COMMAND[] = {
			0x00, 0x01, 0x00, 0x00, 0x00
		};
		GP_STOP_COMMAND[0] = (byte) dwID + 1;
		SendHidCommand(hHidDevice, GP_STOP_COMMAND, 5);
	}

	struct VibrationEff {
		DWORD dwEffectId;
		DWORD dwStartFrame;
		DWORD dwStopFrame;

		byte forceX;
		byte forceY;

		BOOL isActive;
		BOOL started;
	};

	HANDLE hHidDevice[2];
	VibrationEff VibEffects[MAX_EFFECTS][2];
	std::map<std::thread::id, DWORD> ThreadRef;

	std::vector<std::wstring> VibrationController::hidDevPath;
	std::mutex VibrationController::mtxSync;
	std::unique_ptr<std::thread, VibrationController::VibrationThreadDeleter> VibrationController::thrVibration[2];

	VibrationController::VibrationController()
	{
	}


	VibrationController::~VibrationController()
	{
	}

	char* VibrationController::EffectNameFromID(DWORD fxId) {
		char* effect_id_table[] = {
			"Constant force (DIEFT_CONSTANTFORCE)",
			"Ramp Force (DIEFT_RAMPFORCE)",
			"Periodic Force (DIEFT_PERIODIC)",
			"Conditional Force (DIEFT_CONDITION)",
			"Custom Force (DIEFT_CUSTOMFORCE)",
			"Unknown / Unsupported"
		};
		DWORD effect_type = DIEFT_GETTYPE(fxId);
		char* fxName;

		switch (effect_type) {
		case DIEFT_CONSTANTFORCE:
			fxName = effect_id_table[0];
			break;
		case DIEFT_RAMPFORCE:
			fxName = effect_id_table[1];
			break;
		case DIEFT_PERIODIC:
			fxName = effect_id_table[2];
			break;
		case DIEFT_CONDITION:
			fxName = effect_id_table[3];
			break;
		case DIEFT_CUSTOMFORCE:
			fxName = effect_id_table[4];
			break;
		default:
			fxName = effect_id_table[5];
		}

		return fxName;
	}

	void VibrationController::StartVibrationThread(DWORD dwID)
	{
		mtxSync.lock();
		if (thrVibration[dwID] == NULL) {
			quitVibrationThread[dwID] = false;

			for (int k = 0; k < MAX_EFFECTS; k++) {
				VibEffects[k][dwID].isActive = FALSE;
				VibEffects[k][dwID].dwEffectId = -1;
			}
			
			thrVibration[dwID].reset(new std::thread(VibrationController::VibrationThreadEntryPoint, dwID));
			
		}

		mtxSync.unlock();
	}

	void VibrationController::VibrationThreadEntryPoint(DWORD dwID)
	{
		mtxSync.lock();
		
		ThreadRef.insert(std::make_pair(std::this_thread::get_id(), dwID));
		
		mtxSync.unlock();

		// Initialization
		hHidDevice[dwID] = CreateFile(
			hidDevPath[dwID].c_str(),
			GENERIC_WRITE | GENERIC_READ,
			FILE_SHARE_WRITE | FILE_SHARE_READ,
			NULL,
			OPEN_EXISTING,
			FILE_FLAG_OVERLAPPED,
			NULL);

		byte lastForceX = 0;
		byte lastForceY = 0;

		while (true) {
			mtxSync.lock();

			if (quitVibrationThread[dwID]) {
				mtxSync.unlock();
				break;
			}

			DWORD frame = GetTickCount();
			byte forceX = 0;
			byte forceY = 0;

			for (int k = 0; k < MAX_EFFECTS; k++) {
				if (!VibEffects[k][dwID].isActive)
					continue;

				if (VibEffects[k][dwID].started) {

					if (VibEffects[k][dwID].dwStopFrame != INFINITE) {

						if (VibEffects[k][dwID].dwStopFrame <= frame) {
#ifdef _DEBUG
							LogMessage("Wiping effect: slot:%i, dwId:0x%lx; Reached last frame.", k, dwID);
#endif
							VibEffects[k][dwID].isActive = FALSE;
						}
						else {
							forceX = MAXC(forceX, VibEffects[k][dwID].forceX);
							forceY = MAXC(forceY, VibEffects[k][dwID].forceY);
						}
					}
					else {
						forceX = MAXC(forceX, VibEffects[k][dwID].forceX);
						forceY = MAXC(forceY, VibEffects[k][dwID].forceY);
					}
				}
				else {
					if (VibEffects[k][dwID].dwStartFrame <= frame) {
						VibEffects[k][dwID].started = TRUE;
						
						if (VibEffects[k][dwID].dwStopFrame != INFINITE) {
							DWORD frmStart = VibEffects[k][dwID].dwStartFrame;
							DWORD frmStop = VibEffects[k][dwID].dwStopFrame;

							DWORD dt = frmStart <= frmStop ? frmStop - frmStart : frmStart + 100;

							VibEffects[k][dwID].dwStopFrame = frame + dt;
						}
#ifdef DISABLE_INFINITE_VIBRATION
						else {
							VibEffects[k][dwID].dwStopFrame = frame + 10000;
						}
#endif


						forceX = MAXC(forceX, VibEffects[k][dwID].forceX);
						forceY = MAXC(forceY, VibEffects[k][dwID].forceY);
					}
				}
			}

			if (forceX != lastForceX || forceY != lastForceY) {
#ifdef _DEBUG
				LogMessage("Sending rumble command: dwId:0x%lx, forceX:%u, forcey:%u", dwID, forceX, forceY);
#endif
				// Send the command
				if (forceX == 0 && forceY == 0)
					SendVibrationStop(hHidDevice[dwID], dwID);
				else
					SendVibrationForce(hHidDevice[dwID], forceX, forceY, dwID);

				lastForceX = forceX;
				lastForceY = forceY;
			}

			mtxSync.unlock();

			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}

		if (hHidDevice[dwID] != NULL) {
			SendVibrationStop(hHidDevice[dwID], dwID);
			CloseHandle(hHidDevice[dwID]);
		}
	}

	void VibrationController::SetHidDevicePath(LPWSTR path, DWORD dwID)
	{
		hidDevPath.push_back(path);
		Reset(dwID);
	}

	HRESULT VibrationController::StartEffect(DWORD dwEffectID, LPCDIEFFECT peff, DWORD dwID)
	{
		LPDIENVELOPE envelope;

		mtxSync.lock();

		int idx = -1;
		// Reusing the same idx if effect was already created
		for (int k = 0; k < MAX_EFFECTS; k++) {
			if (VibEffects[k][dwID].dwEffectId == dwEffectID) {
				idx = k;
				break;
			}
		}

		// Find a non-active idx
		if (idx < 0) {
			for (int k = 0; k < MAX_EFFECTS; k++) {
				if (!VibEffects[k][dwID].isActive || k == MAX_EFFECTS - 1) {
					idx = k;
					break;
				}
			}
		}
#ifdef _DEBUG
		LogMessage("Start Effect: slot=%i dwId=0x%lx effect=%s (typeId: %lu; flags:0x%04lx), duration:%lu",
			idx, dwID, EffectNameFromID(dwEffectID), DIEFT_GETTYPE(dwEffectID), dwEffectID, peff->dwDuration);
#endif

		// Calculating intensity
		byte forceX = 0x0;
		byte forceY = 0x0;

		byte magnitude = 0x0;

		DWORD specificParamsSize = peff->cbTypeSpecificParams;

		byte fxType = DIEFT_GETTYPE(dwEffectID);

		if (specificParamsSize == 0) {
			// No specific effects. Go on.
		} else if (fxType == DIEFT_CONSTANTFORCE && specificParamsSize == sizeof(DICONSTANTFORCE)) {
			LPDICONSTANTFORCE constantForceParams = (LPDICONSTANTFORCE)peff->lpvTypeSpecificParams;
			if (peff->lpEnvelope != nullptr) {
				if (sizeof(*peff->lpEnvelope) != sizeof(DIENVELOPE)) {
#ifdef _DEBUG
					LogMessage("Provided envelope to Constant Force effect does not match envelope size. Provided:%lu, DIENVELOPE:%lu",
						sizeof(*peff->lpEnvelope), sizeof(DIENVELOPE));
#endif
					mtxSync.unlock();
					return DIERR_INVALIDPARAM;
				}
				envelope = peff->lpEnvelope;

				magnitude = static_cast<byte>((max(0, min(DI_FFNOMINALMAX, envelope->dwAttackLevel)) * 254) / DI_FFNOMINALMAX);
				// dwAttackLevel: initial force (0-10k)
				// dwAttackTime: time between initial force and lMagnitude (microsseconds)
				// dwFadeLevel: force after effect ends (0-10k)
				// dwFadeTime: time between lMagnitude and FadeLevel (microsseconds)
#ifdef _DEBUG
				LogMessage("Ignored envelope for Constant Force. attackLevel:%lu, attackTime:%lu, fadeLevel:%lu, fadeTime:%lu, magnitude:%u",
					envelope->dwAttackLevel, envelope->dwAttackTime, envelope->dwFadeLevel, envelope->dwFadeTime, magnitude);
#endif
			} else {
				// Valid values are -10,000 .. 10,000 (DI_FFNOMINALMAX), translated into 0 .. 254
				magnitude = static_cast<byte>((max(0, min(DI_FFNOMINALMAX, constantForceParams->lMagnitude)) * 254) / DI_FFNOMINALMAX); // FIXME
#ifdef _DEBUG
				LogMessage("Constant Force with magnitude specific parameter. magnitude:%lu, compressed to byte:%02X",
					constantForceParams->lMagnitude, magnitude);
#endif
			}
#ifdef _DEBUG
			LogMessage("Constant force effect handled. byteMagnitude:%u, diMagnitude:%lu",
				 magnitude, constantForceParams->lMagnitude);
#endif
		} else if (fxType == DIEFT_RAMPFORCE && specificParamsSize == sizeof(DIRAMPFORCE)) {
			if (peff->lpEnvelope != nullptr) {
#ifdef _DEBUG
				LogMessage("Ramp force can't take an envelope: envelope size:%lu",
					sizeof(peff->lpEnvelope));
#endif
				mtxSync.unlock();
				return DIERR_INVALIDPARAM;
			}
			LPDIRAMPFORCE rampForceParams = (LPDIRAMPFORCE)peff->lpvTypeSpecificParams;
#ifdef _DEBUG
			LogMessage("Ramp force effects not supported.");
#endif
			mtxSync.unlock();
			return ERROR_NOT_SUPPORTED;
		} else if (fxType == DIEFT_PERIODIC && specificParamsSize == sizeof(DIPERIODIC)) {
			LPDIPERIODIC periodicForceParams = (LPDIPERIODIC)peff->lpvTypeSpecificParams;
			if (peff->lpEnvelope != nullptr) {
				if (sizeof(*peff->lpEnvelope) != sizeof(DIENVELOPE)) {
					mtxSync.unlock();
					return DIERR_INVALIDPARAM;
				}
				envelope = peff->lpEnvelope;

				magnitude = static_cast<byte>((max(0, min(DI_FFNOMINALMAX, envelope->dwAttackLevel)) * 254) / DI_FFNOMINALMAX);
				// dwAttackLevel: initial force (0-10k)
				// dwAttackTime: time between initial force and IOffset (microsseconds)
				// dwFadeLevel: force after effect ends (0-10k)
				// dwFadeTime: time between lMagnitude and FadeLevel (microsseconds)
				// IOffset will mark the midpoint force of the attack (baseline).
				// dwMagnitude is the maximum difference between the current attack level and the actual force, each peak (+ and -)
				//  attained every dwPeriod/2.
				// Wonder how to represent this in a single puny vibration motor. :P
#ifdef _DEBUG
				LogMessage("Ignored envelope for Constant Force: attackLevel=%lu attackTime=%lu fadeLevel=%lu fadeTime:%lu, magnitude:%u",
					envelope->dwAttackLevel, envelope->dwAttackTime, envelope->dwFadeLevel, envelope->dwFadeTime, magnitude);
#endif
			} else {
				// Valid values are 0 .. 10,000 (DI_FFNOMINALMAX), translated into 0 .. 254
				magnitude = static_cast<byte>((max(0, min(DI_FFNOMINALMAX, periodicForceParams->dwMagnitude)) * 254) / DI_FFNOMINALMAX);
			}

			// After determining the behavior of dwMagnitude, save the initial phase (dwPhase), IOffset (reference/base force for the effect)
			// IOffset will always be the baseline of the attack, or simply the center of the dwMagnitude oscillation
			// dwPeriod, the time for a full cycle between IOffset+dwMagnitude > IOffset > IOffset-dwMagnitude > IOffset
			// dwPhase being the initial position the oscillator would start at.
#ifdef _DEBUG
			LogMessage("Periodic effects not supported. magnitude:%lu", periodicForceParams->dwMagnitude);
#endif
			mtxSync.unlock();
			return ERROR_NOT_SUPPORTED;
		} else if (fxType == DIEFT_CUSTOMFORCE && specificParamsSize == sizeof(DICUSTOMFORCE)) {
			if (peff->lpEnvelope != nullptr) {
#ifdef _DEBUG
				LogMessage("Custom force can't take an envelope. envelope size:%lu",
					sizeof(peff->lpEnvelope));
#endif
				mtxSync.unlock();
				return DIERR_INVALIDPARAM;
			}
			LPDICUSTOMFORCE customForceParams = (LPDICUSTOMFORCE)peff->lpvTypeSpecificParams;
#ifdef _DEBUG
			LogMessage("Custom force effects not supported.");
#endif
			mtxSync.unlock();
			return ERROR_NOT_SUPPORTED;
		} else if (fxType == DIEFT_CONDITION && specificParamsSize == sizeof(DICONDITION)) {
			if (peff->lpEnvelope != nullptr) {
#ifdef _DEBUG
				LogMessage("Condition can't take an envelope. envelope size:%lu",
					sizeof(peff->lpEnvelope));
#endif
				mtxSync.unlock();
				return DIERR_INVALIDPARAM;
			}
			LPDICONDITION conditionParamList = (LPDICONDITION)peff->lpvTypeSpecificParams;
#ifdef _DEBUG
			LogMessage("Condition effects not supported.");
#endif
			mtxSync.unlock();
			return ERROR_NOT_SUPPORTED;
		} else {
#ifdef _DEBUG
			LogMessage("Specific parameters size doesn't match any supported structure. size:%lu",
				specificParamsSize);
			LogMessage("Known parameter sizes: DICONSTANTFORCE:%lu, DIRAMPFORCE:%lu, DIPERIODIC:%lu, DICUSTOMFORCE:%lu, DICONDITION:%lu",
				sizeof(DICONSTANTFORCE), sizeof(DIRAMPFORCE), sizeof(DIPERIODIC), sizeof(DICUSTOMFORCE), sizeof(DICONDITION));
#endif
			mtxSync.unlock();
			return ERROR_NOT_SUPPORTED;
		}

		if (peff->cAxes == 1) {
			// If direction is negative, then it is a forceX
			// Otherwise it is a forceY
			LONG direction = peff->rglDirection[0];
			static byte lastForceX = 0;
			static byte lastForceY = 0;

			forceX = lastForceX;
			forceY = lastForceY;

			if (direction == -1) {
				//forceX = lastForceX = (byte)(round((((double)peff->dwGain) / 10000.0) * 254.0));
				forceX = lastForceX = magnitude;
			}
			else if (direction == 1) {
				//forceY = lastForceY = (byte)(round((((double)peff->dwGain) / 10000.0) * 254.0));
				forceY = lastForceY = magnitude;
			}
#ifdef _DEBUG
			LogMessage("One-axis-effect. direction:%l, forceX:%u, forceY:%u", direction, forceX, forceY);
#endif
		}
		else {
			if (peff->cAxes >= 1) {
				LONG fx = peff->rglDirection[0];
				//if (fx <= 1) fx = peff->dwGain;

				if (fx > 0)
					forceX = forceY = magnitude;
				else
					forceX = forceY = 0;
			}

			if (peff->cAxes >= 2) {
				LONG fy = peff->rglDirection[1];
				//if (fy <= 1) fy = peff->dwGain;

				if (fy > 0)
					forceY = magnitude;
				else
					forceY = 0;
			}
#ifdef _DEBUG
			if (peff->cAxes > 2)
				LogMessage("Multi-axes-effect. Only axes 1 and 2 considered. Axis count:%lu, forceX:%u, forceY:%u", peff->cAxes, forceX, forceY);
			else
				LogMessage("Two-axes-effect. forceX:%u, forceY:%u", forceX, forceY);
#endif
		}


		DWORD frame = GetTickCount();

#ifdef _DEBUG
		LogMessage("Effect has been queued for execution.");
#endif

		VibEffects[idx][dwID].forceX = forceX;
		VibEffects[idx][dwID].forceY = forceY;

		VibEffects[idx][dwID].dwEffectId = dwEffectID;
		VibEffects[idx][dwID].dwStartFrame = frame + (peff->dwStartDelay / 1000);
		VibEffects[idx][dwID].dwStopFrame =
			peff->dwDuration == INFINITE ? INFINITE : 
			VibEffects[idx][dwID].dwStartFrame + (peff->dwDuration / 1000);
		VibEffects[idx][dwID].isActive = TRUE;
		VibEffects[idx][dwID].started = FALSE;

		mtxSync.unlock();
		StartVibrationThread(dwID);

		return S_OK;
	}

	void VibrationController::StopEffect(DWORD dwEffectID, DWORD dwID)
	{
#if _DEBUG
		LogMessage("Effect stop requested for dwEffectID:%lu (%s, %u), dwID:%lu",
			dwEffectID, EffectNameFromID(dwEffectID), DIEFT_GETTYPE(dwEffectID), dwID);
#endif
		mtxSync.lock();
		for (int k = 0; k < MAX_EFFECTS; k++) {
			if (VibEffects[k][dwID].dwEffectId != dwEffectID)
				continue;

			VibEffects[k][dwID].dwStopFrame = 0;
		}
		
		mtxSync.unlock();
	}

	void VibrationController::StopAllEffects(DWORD dwID)
	{
#if _DEBUG
		LogMessage("Stop all effects requested for dwID:%lu", dwID);
#endif
		mtxSync.lock();
		for (int k = 0; k < MAX_EFFECTS; k++) {
			VibEffects[k][dwID].dwStopFrame = 0;
		}
		mtxSync.unlock();

		Reset(dwID);
	}

	void VibrationController::Reset(DWORD dwID, std::thread* t)
	{	
		if (t == NULL)
		{		
#if _DEBUG
			LogMessage("Thread reset requested for dwID:%lu, all threads", dwID);
#endif
			if (thrVibration[dwID] == NULL)
				return;

			quitVibrationThread[dwID] = true;
			thrVibration[dwID]->join();
			thrVibration[dwID].reset(NULL);

			for (auto& ref : ThreadRef)
			{
				if (ref.second = dwID)
				{
					ThreadRef.erase(ref.first);
					break; 
				}
			}
		}
		else {
#if _DEBUG
			LogMessage("Thread reset requested for dwID:%lu, thread#:%u",
				dwID, t->get_id());
#endif
			DWORD ldwID = ThreadRef[t->get_id()];
			
			if (thrVibration[ldwID] == NULL)
				return;
			
			ThreadRef.erase(t->get_id());
			quitVibrationThread[ldwID] = true;
			thrVibration[ldwID]->join();
			thrVibration[ldwID].reset(NULL);	
		}
	}

}
