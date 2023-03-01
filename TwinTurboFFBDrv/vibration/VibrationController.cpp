#include "VibrationController.h"
#include "dieffectattributes.h"
#include <algorithm>
#include <Hidsdi.h>

#undef DISABLE_INFINITE_VIBRATION

#define MAX_EFFECTS 5
#define MAXC(a, b) ((a) > (b) ? (a) : (b))

// interval between effect polling and sending HID commands.
// Increase this if the USB bus is being flooded by a game,
// potentially affecting input from the joystick itself
// (buttons, steering) and mouse.
#define POLL_DELAY 10

#define ACTUATOR_SMALL 0x01000002
#define ACTUATOR_BIG   0x01000102

#ifdef _DEBUG
void LogMessage(const char* fmt, ...);
#endif

namespace vibration {
	bool quitVibrationThread[2] = { false };
	bool paused[2] = { false };
	DWORD paused_at_frame[2] = { 0x00 };

	void SendHidCommand(HANDLE hHidDevice, const byte* buff, DWORD buffsz) {
		HidD_SetOutputReport(hHidDevice, (PVOID)buff, 5);
	}
	void SendVibrationForce(HANDLE hHidDevice, byte a1str, byte a2str, DWORD dwID) {
		byte buffer1[] = {
			0x00, 0x01, 0x00, a1str, a2str
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

		byte a1str;
		byte a2str;

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

#ifdef _DEBUG
	// Get the effect name from the controller's effect flags.
	// The controller-specific flags are defined in dieffectattributes.h and should match
	// what the controller has installed in OEMForceFeedback registry key.
	char* VibrationController::EffectNameFromCET(DWORD controllerEffectFlags) {
		char* fxName;
		switch (DIEFT_GETTYPE(controllerEffectFlags)) {
		case DIEFT_CONSTANTFORCE:
			fxName = "Constant force (DIEFT_CONSTANTFORCE)";
			break;
		case DIEFT_RAMPFORCE:
			fxName = "Ramp Force (DIEFT_RAMPFORCE)";
			break;
		case DIEFT_PERIODIC:
			fxName = "Periodic Force (DIEFT_PERIODIC)";
			break;
		case DIEFT_CONDITION:
			fxName = "Conditional Force (DIEFT_CONDITION)";
			break;
		case DIEFT_CUSTOMFORCE:
			fxName = "Custom Force (DIEFT_CUSTOMFORCE)";
			break;
		default:
			fxName = "Unknown / Unsupported";
		}

		return fxName;
	}

	// Get the effect name from its dwInternalEffectType flags.
	// They should resemble what's in OEMForceFeedback but don't
	// necessarily need, as they are used just for debugging purposes.
	char* effectNameFromIET(DWORD dwInternalEffectType) {
		char* fxName;

		switch (dwInternalEffectType) {
		case CEID_ConstantForce:
			fxName = "Constant Force";
			break;
        case CEID_RampForce   :
			fxName = "Ramp Force";
			break;
		case CEID_Square      :
			fxName = "Square";
			break;
		case CEID_Sine        :
			fxName = "Sine";
			break;
		case CEID_Triangle    :
			fxName = "Triangle";
			break;
		case CEID_SawtoothUp  :
			fxName = "Sawtooth Up";
			break;
		case CEID_SawtoothDown:
			fxName = "Swtooth Down";
			break;
		case CEID_Spring      :
			fxName = "Spring";
			break;
		case CEID_Damper      :
			fxName = "Damper";
			break;
		case CEID_Inertia     :
			fxName = "Inertia";
			break;
		case CEID_Friction    :
			fxName = "Friction";
			break;
		case CEID_CustomForce :
			fxName = "Custom Force";
			break;
		default:
			fxName = "<invalid>";
		}

		return fxName;
	}
#endif

	// Get controller-specific Effects Type flags from its dwInternalEffectType flags.
	// The controller-specific flags are defined in dieffectattributes.h and should match
	// what the controller has installed in OEMForceFeedback registry key.
	DWORD fxFlagsFromIET(DWORD dwInternalEffectType) {
		switch (dwInternalEffectType) {
		case CEID_ConstantForce:
			return CETYPE_ConstantForce;
		case CEID_RampForce:
			return CETYPE_RampForce;
		case CEID_Square:
			return CETYPE_Square;
		case CEID_Sine:
			return CETYPE_Sine;
		case CEID_Triangle:
			return CETYPE_Triangle;
		case CEID_SawtoothUp:
			return CETYPE_SawtoothUp;
		case CEID_SawtoothDown:
			return CETYPE_SawtoothDown;
		case CEID_Spring:
			return CETYPE_Spring;
		case CEID_Damper:
			return CETYPE_Damper;
		case CEID_Inertia:
			return CETYPE_Inertia;
		case CEID_Friction:
			return CETYPE_Friction;
		case CEID_CustomForce:
			return CETYPE_CustomForce;
		default:
			return 0x0;
		}
	}

	// Get controller-specific effects' capabilities from the Controller Effect flags.
	// The controller-specific flags are defined in dieffectattributes.h and should match
	// what the controller has installed in OEMForceFeedback registry key.
	DWORD fxCapabsFromCET(DWORD controllerEffectFlags) {
		switch (controllerEffectFlags) {
		case DIEFT_CONSTANTFORCE:
			return CESP_ConstantForce;
		case DIEFT_RAMPFORCE:
			return CESP_RampForce;
		case DIEFT_PERIODIC:
			return CESP_Periodic;
		case DIEFT_CONDITION:
			return CESP_Conditional;
		case DIEFT_CUSTOMFORCE:
			return CESP_CustomForce;
		default:
			return 0x0;
		}
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

		byte curra1str = 0;
		byte curra2str = 0;

		while (true) {
			mtxSync.lock();

			if (quitVibrationThread[dwID]) {
				mtxSync.unlock();
				break;
			} else if (paused[dwID]) {
				mtxSync.unlock();
				std::this_thread::sleep_for(std::chrono::milliseconds(POLL_DELAY));
				continue;
			}

			DWORD frame = GetTickCount();

			byte a1str = 0;
			byte a2str = 0;

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
							a1str = MAXC(a1str, VibEffects[k][dwID].a1str);
							a2str = MAXC(a2str, VibEffects[k][dwID].a2str);
						}
					}
					else {
						a1str = MAXC(a1str, VibEffects[k][dwID].a1str);
						a2str = MAXC(a2str, VibEffects[k][dwID].a2str);
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

						a1str = MAXC(a1str, VibEffects[k][dwID].a1str);
						a2str = MAXC(a2str, VibEffects[k][dwID].a2str);
					}
				}
			}

			if (a1str != curra1str || a2str != curra2str) {
#ifdef _DEBUG
				LogMessage("Sending rumble command: dwId:0x%lx, small actuator strength: %u, big actuator strength: %u",
					dwID, (a1str * 100.0f) / 254.0f, (a2str * 100.0f) / 254.0f);
#endif
				// Send the command
				if (a1str == 0 && a2str == 0) {
					SendVibrationStop(hHidDevice[dwID], dwID);
				// the effect will only change if the maximum between the
				// two actuators has changed. No need to flood the HID bus
				// for this change.
				} else if (max(a1str, a2str) != max(curra1str, curra2str)) {
					SendVibrationForce(hHidDevice[dwID], a1str, a2str, dwID);
				}

				curra1str = a1str;
				curra2str = a2str;
			}

			mtxSync.unlock();

			std::this_thread::sleep_for(std::chrono::milliseconds(POLL_DELAY));
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

	size_t getEffectSlot(DWORD dwID, DWORD dwEffectID) {
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

		return idx;
	}

	HRESULT VibrationController::EnqueueEffect(DWORD dwDeviceID, DWORD dwInternalEffectType, LPDWORD lpdwDnloadID, LPCDIEFFECT lpEffect, DWORD dwFlags)
	{
		LPDIENVELOPE envelope;

		DWORD fxDeviceFlags = fxFlagsFromIET(dwInternalEffectType);
		byte fxType = DIEFT_GETTYPE(fxDeviceFlags);
		DWORD fxCapabs = fxCapabsFromCET(fxType);

		if (fxDeviceFlags == 0x00) {
#ifdef _DEBUG
			LogMessage("Start effect: no effect with the provided id exists. device: %lu, effect: %s (%s, id:0x%04lx)",
				dwDeviceID, effectNameFromIET(dwInternalEffectType), EffectNameFromCET(fxType), dwInternalEffectType);
#endif
			return DIERR_INCOMPLETEEFFECT;
		}

		DWORD specificParamsSize = lpEffect->cbTypeSpecificParams;

#ifdef _DEBUG
		LogMessage("Start Effect:\n - %s (%s, flags: 0x%04lx)\n"
			"   dwId=0x%lx, duration:%lums, specific params size: %lub\n"
			"   gain: %lu, axis count: %lu",
			effectNameFromIET(dwInternalEffectType), EffectNameFromCET(fxType), fxDeviceFlags,
			dwDeviceID, lpEffect->dwDuration / 1000, specificParamsSize, lpEffect->dwGain, lpEffect->cAxes);

		/* This is only useful while building dieffectattributes.h
		LogMessage("Effect information: %s (%s, flags: 0x%04lx)\n"
			" [%c] Hardware Effect      [%c] Force Feedback Attack [%c] Force Feedback Fade\n"
			" [%c] Saturation           [%c] Pos/neg Coefficients  [%c] Pos/neg Saturation",
			effectNameFromDeviceID(dwInternalEffectType), EffectNameFromID(fxType), fxDeviceFlags,
#define fxf(mask) (fxDeviceFlags & mask) != 0 ? 'x' : ' '
			fxf(DIEFT_HARDWARE), fxf(DIEFT_FFATTACK), fxf(DIEFT_FFFADE),
			fxf(DIEFT_SATURATION), fxf(DIEFT_POSNEGCOEFFICIENTS), fxf(DIEFT_POSNEGSATURATION));
#undef fxf
		LogMessage("Effect support table (flags: 0x%04lx):\n"
			" [%c] duration             [%c] sample period         [%c] gain\n"
			" [%c] trigger button       [%c] trig.btn.repeat intvl [%c] axes\n"
			" [%c] direction            [%c] envelope              [%c] type-specific params\n"
			" [%c] force restart        [%c] deny restart          [%c] don't download",
			fxCapabs,
#define fxc(mask) (fxCapabs & mask) != 0 ? 'x' : ' '
			fxc(DIEP_DURATION), fxc(DIEP_SAMPLEPERIOD), fxc(DIEP_GAIN),
			fxc(DIEP_TRIGGERBUTTON), fxc(DIEP_TRIGGERREPEATINTERVAL), fxc(DIEP_AXES),
			fxc(DIEP_DIRECTION), fxc(DIEP_ENVELOPE), fxc(DIEP_TYPESPECIFICPARAMS),
			fxc(DIEP_START), fxc(DIEP_NORESTART), fxc(DIEP_NODOWNLOAD));
#undef fxc */
		LogMessage("Effect flags (0x%04lx):\n"
			" Object referenced: (%c) By IDs     (%c) By Offset\n"
			" Coordinate system: (%c) Cartesian  (%c) Polar      (%c) Spherical",
			lpEffect->dwFlags,
#define fxe(mask) lpEffect->dwFlags & mask ? 'o' : ' '
			fxe(DIEFF_OBJECTIDS), fxe(DIEFF_OBJECTOFFSETS),
			fxe(DIEFF_CARTESIAN), fxe(DIEFF_POLAR), fxe(DIEFF_SPHERICAL));
#endif

		bool smallActuator = false;
		bool bigActuator = false;

		if (lpEffect->cAxes > 0) {
			bool hasAxisRefs = lpEffect->dwFlags & (DIEFF_OBJECTIDS | DIEFF_OBJECTOFFSETS);
			bool axisRefsByOffset = lpEffect->dwFlags & DIEFF_OBJECTOFFSETS;

			// CECOORDS below ensures coordinate systems not supported by the device
			// (like Spherical) are not accounted for as valid "has direction".
			bool hasDirection = (lpEffect->dwFlags & CECOORDS ) & (DIEFF_CARTESIAN | DIEFF_POLAR | DIEFF_SPHERICAL);
			bool cartesianCoords = (lpEffect->dwFlags & CECOORDS ) & DIEFF_CARTESIAN;
			size_t lastAxisIdx = lpEffect->cAxes - 1;
			for (size_t i = 0; i < lpEffect->cAxes; i++)
			{
				if (!hasAxisRefs) {
					switch (lpEffect->rgdwAxes[i]) {
					case ACTUATOR_SMALL:
						smallActuator = true;
						break;
					case ACTUATOR_BIG:
						bigActuator = true;
						break;
#ifdef _DEBUG
					default:
						LogMessage("  Actuator reference didn't match any supported actuator. Provided: 0x%08lx, small: 0x%08lx, big: 0x%08lx",
							lpEffect->rgdwAxes[i], ACTUATOR_SMALL, ACTUATOR_BIG);
#endif
					};
#ifdef _DEBUG
					// This will vary from 0x01000002 to 0x01000102 when x360ce wants to send values  to the small (00) and big (01) motors.
					LogMessage("  Ref for actuator #%u: %s: 0x%08lx", i + 1, axisRefsByOffset ? "offset" : "index", lpEffect->rgdwAxes[i]);
				} else {
					// This will vary from 0x01000002 to 0x01000102 when x360ce wants to send values  to the small (00) and big (01) motors.
					LogMessage("  Ref for axis #%u: %s: 0x%08lx", i + 1, axisRefsByOffset ? "offset" : "index", lpEffect->rgdwAxes[i]);
#endif
				}

				// If we have direction and not in cartesian coordinates, the value in the last position is not used.
				//if (hasDirection && (cartesianCoords || i < lastAxisIdx)) {
#if _DEBUG
				if ((cartesianCoords || i < lastAxisIdx)) {
					LogMessage("  Dir for axis #%u: %li %s", i+1, lpEffect->rglDirection[i],
						cartesianCoords ? "(cartesian)" : (lpEffect->dwFlags & DIEFF_POLAR ? "/100deg (polar)" : "/100deg (spherical)"));
				}
#endif
			}
		}

		DWORD magnitude = 0x0;
		if (specificParamsSize == 0) {
			// No specific effects. Go on.
		} else if (fxType == DIEFT_CONSTANTFORCE && specificParamsSize == sizeof(DICONSTANTFORCE)) {
			LPDICONSTANTFORCE constantForceParams = (LPDICONSTANTFORCE)lpEffect->lpvTypeSpecificParams;
			if (lpEffect->lpEnvelope != nullptr) {
				if (sizeof(*lpEffect->lpEnvelope) != sizeof(DIENVELOPE)) {
#ifdef _DEBUG
					LogMessage("Provided envelope to Constant Force effect does not match envelope size. Provided:%lu, DIENVELOPE:%lu",
						sizeof(*lpEffect->lpEnvelope), sizeof(DIENVELOPE));
#endif
					return DIERR_INVALIDPARAM;
				}
				envelope = lpEffect->lpEnvelope;

				magnitude = max(0, min(DI_FFNOMINALMAX, envelope->dwAttackLevel));
				// dwAttackLevel: initial force (0-10k)
				// dwAttackTime: time between initial force and lMagnitude (microsseconds)
				// dwFadeLevel: force after effect ends (0-10k)
				// dwFadeTime: time between lMagnitude and FadeLevel (microsseconds)
#ifdef _DEBUG
				LogMessage("Ignored envelope for Constant Force. attackLevel:%lu, attackTime:%lu, fadeLevel:%lu, fadeTime:%lu, magnitude:%lu",
					envelope->dwAttackLevel, envelope->dwAttackTime, envelope->dwFadeLevel, envelope->dwFadeTime, magnitude);
#endif
			} else {
				// Valid values are -10,000 .. 10,000 (DI_FFNOMINALMAX)
				if (constantForceParams->lMagnitude < 0) constantForceParams->lMagnitude = constantForceParams->lMagnitude * (-1);
				magnitude = max(0, min(DI_FFNOMINALMAX, constantForceParams->lMagnitude));
#ifdef _DEBUG
				LogMessage("Constant Force with magnitude specific parameter. magnitude: %lu, compressed to 0-10k: %lu",
					constantForceParams->lMagnitude, magnitude);
#endif
			}
#ifdef _DEBUG
			LogMessage("Constant force effect handled. byteMagnitude:%lu, diMagnitude:%lu",
				 magnitude, constantForceParams->lMagnitude);
#endif
		} else if (fxType == DIEFT_RAMPFORCE && specificParamsSize == sizeof(DIRAMPFORCE)) {
			if (lpEffect->lpEnvelope != nullptr) {
#ifdef _DEBUG
				LogMessage("Ramp force can't take an envelope: envelope size:%lu",
					sizeof(lpEffect->lpEnvelope));
#endif
				return DIERR_INVALIDPARAM;
			}
			LPDIRAMPFORCE rampForceParams = (LPDIRAMPFORCE)lpEffect->lpvTypeSpecificParams;
#ifdef _DEBUG
			LogMessage("Ramp force effects not supported.");
#endif
			return ERROR_NOT_SUPPORTED;
		} else if (fxType == DIEFT_PERIODIC && specificParamsSize == sizeof(DIPERIODIC)) {
			LPDIPERIODIC periodicForceParams = (LPDIPERIODIC)lpEffect->lpvTypeSpecificParams;
			if (lpEffect->lpEnvelope != nullptr) {
				if (sizeof(*lpEffect->lpEnvelope) != sizeof(DIENVELOPE)) {
					return DIERR_INVALIDPARAM;
				}
				envelope = lpEffect->lpEnvelope;

				magnitude = max(0, min(DI_FFNOMINALMAX, envelope->dwAttackLevel));
				// dwAttackLevel: initial force (0-10k)
				// dwAttackTime: time between initial force and IOffset (microsseconds)
				// dwFadeLevel: force after effect ends (0-10k)
				// dwFadeTime: time between lMagnitude and FadeLevel (microsseconds)
				// IOffset will mark the midpoint force of the attack (baseline).
				// dwMagnitude is the maximum difference between the current attack level and the actual force, each peak (+ and -)
				//  attained every dwPeriod/2.
				// Wonder how to represent this in a single puny vibration motor. :P
#ifdef _DEBUG
				LogMessage("Ignored envelope for Constant Force: attackLevel=%lu attackTime=%lu fadeLevel=%lu fadeTime:%lu, magnitude:%lu",
					envelope->dwAttackLevel, envelope->dwAttackTime, envelope->dwFadeLevel, envelope->dwFadeTime, magnitude);
#endif
			} else {
				// Valid values are 0 .. 10,000 (DI_FFNOMINALMAX), translated into 0 .. 254
				magnitude = max(0, min(DI_FFNOMINALMAX, periodicForceParams->dwMagnitude));
			}

			// After determining the behavior of dwMagnitude, save the initial phase (dwPhase), IOffset (reference/base force for the effect)
			// IOffset will always be the baseline of the attack, or simply the center of the dwMagnitude oscillation
			// dwPeriod, the time for a full cycle between IOffset+dwMagnitude > IOffset > IOffset-dwMagnitude > IOffset
			// dwPhase being the initial position the oscillator would start at.
#ifdef _DEBUG
			LogMessage("Periodic effects not supported. magnitude:%lu", periodicForceParams->dwMagnitude);
#endif
			return ERROR_NOT_SUPPORTED;
		} else if (fxType == DIEFT_CUSTOMFORCE && specificParamsSize == sizeof(DICUSTOMFORCE)) {
			if (lpEffect->lpEnvelope != nullptr) {
#ifdef _DEBUG
				LogMessage("Custom force can't take an envelope. envelope size:%lu",
					sizeof(lpEffect->lpEnvelope));
#endif
				return DIERR_INVALIDPARAM;
			}
			LPDICUSTOMFORCE customForceParams = (LPDICUSTOMFORCE)lpEffect->lpvTypeSpecificParams;
#ifdef _DEBUG
			LogMessage("Custom force effects not supported.");
#endif
			return ERROR_NOT_SUPPORTED;
		} else if (fxType == DIEFT_CONDITION && specificParamsSize == sizeof(DICONDITION)) {
			if (lpEffect->lpEnvelope != nullptr) {
#ifdef _DEBUG
				LogMessage("Condition can't take an envelope. envelope size:%lu",
					sizeof(lpEffect->lpEnvelope));
#endif
				return DIERR_INVALIDPARAM;
			}
			LPDICONDITION conditionParamList = (LPDICONDITION)lpEffect->lpvTypeSpecificParams;
#ifdef _DEBUG
			LogMessage("Condition effects not supported.");
#endif
			return ERROR_NOT_SUPPORTED;
		} else {
#ifdef _DEBUG
			LogMessage("Specific parameters size doesn't match any supported structure. size:%lu",
				specificParamsSize);
			LogMessage("Known parameter sizes: DICONSTANTFORCE:%lu, DIRAMPFORCE:%lu, DIPERIODIC:%lu, DICUSTOMFORCE:%lu, DICONDITION:%lu",
				sizeof(DICONSTANTFORCE), sizeof(DIRAMPFORCE), sizeof(DIPERIODIC), sizeof(DICUSTOMFORCE), sizeof(DICONDITION));
#endif
			return ERROR_NOT_SUPPORTED;
		}

		DWORD now = GetTickCount();

		byte str = 0;
		if (magnitude != 0 && lpEffect->dwGain > 0) {
			if (lpEffect->dwGain != 10000) {
				magnitude *= max(0, min(DI_FFNOMINALMAX, lpEffect->dwGain)) / 10000;
			}

			// DI_FFNOMINALMAX - 254 => str = (magnitude * 254) / DI_FFNOMINALMAX
			//       magnitude - str
			str = static_cast<byte>(magnitude * (254.0f / DI_FFNOMINALMAX)); // group by the constants for optimization
		}

		mtxSync.lock();
		int idx = getEffectSlot(dwDeviceID, dwInternalEffectType);
		if (idx < 0) {
#ifdef _DEBUG
			LogMessage("Start Effect: no free slots found to allocate effect. device: %lu, effect: %s (%s, id:0x%04lx), flags: %04lx",
				dwDeviceID, effectNameFromIET(dwInternalEffectType), EffectNameFromCET(fxType), dwInternalEffectType, fxDeviceFlags);
#endif
			mtxSync.unlock();
			return DIERR_DEVICEFULL;
		}

		VibrationEff *fx = &VibEffects[idx][dwDeviceID];
#ifdef _DEBUG
		// 254 - 100
		// mgn - x
		// x = 100 * mgn / 254
		if (lpEffect->dwDuration == INFINITE)
			LogMessage("Applying effect at slot #%u. Strength: %.2f%%, stop: never, small actuator: %s, big actuator: %s",
				idx, (str * 100.0f) / 254.0f, smallActuator ? "yes" : "no", bigActuator ? "yes" : "no");
		else
			LogMessage("Applying effect at slot #%u. Strength: %.2f%%, stop: %lu, small actuator: %s, big actuator: %s",
				idx, (str * 100.0f) / 254.0f, lpEffect->dwDuration,
				smallActuator ? "yes" : "no", bigActuator ? "yes" : "no");
#endif
		// Leave the value alone if we're not changing the strength value for an actuator
		// but the effect is already active.
		if (smallActuator) fx->a1str = str;
		else if (!fx->isActive) fx->a1str = 0x00;
		if (bigActuator) fx->a2str = str;
		else if (!fx->isActive) fx->a2str = 0x00;

		fx->dwEffectId = dwInternalEffectType;
		fx->dwStartFrame = now + (lpEffect->dwStartDelay / 1000);
		fx->dwStopFrame =
			lpEffect->dwDuration == INFINITE ? INFINITE :
			fx->dwStartFrame + (lpEffect->dwDuration / 1000);

		fx->isActive = TRUE;
		fx->started = FALSE;

		mtxSync.unlock();
#ifdef _DEBUG
		LogMessage("Effect has been queued for execution.");
#endif
		StartVibrationThread(dwDeviceID);

		return S_OK;
	}

	// !!! StopEffect !!! Reverse all references !!!
	void VibrationController::DequeueEffect(DWORD dwDeviceID, DWORD dwInternalEffectType)
	{
#if _DEBUG
		LogMessage("Effect stop requested for dwEffectID:%lu (%s, %u), dwID:%lu",
			dwInternalEffectType, EffectNameFromCET(dwInternalEffectType),
			DIEFT_GETTYPE(fxFlagsFromIET(dwInternalEffectType)), dwDeviceID);
#endif
		mtxSync.lock();
		for (int k = 0; k < MAX_EFFECTS; k++) {
			if (VibEffects[k][dwDeviceID].dwEffectId != dwInternalEffectType)
				continue;

			VibEffects[k][dwDeviceID].dwStopFrame = 0;
		}
		
		mtxSync.unlock();
	}

	void VibrationController::DequeueAllEffects(DWORD dwDeviceID)
	{
#if _DEBUG
		LogMessage("Stop all effects requested for dwID:%lu", dwDeviceID);
#endif
		mtxSync.lock();
		for (int k = 0; k < MAX_EFFECTS; k++) {
			VibEffects[k][dwDeviceID].dwStopFrame = 0;
		}
		mtxSync.unlock();

		Reset(dwDeviceID);
	}

	void VibrationController::Pause(DWORD dwDeviceID)
	{
		if (!paused[dwDeviceID]) {
			paused_at_frame[dwDeviceID] = GetTickCount();
			paused[dwDeviceID] = true;
#if _DEBUG
			LogMessage("Pause all effects requested for dwID:%lu. Reference frame: %lu", dwDeviceID, paused_at_frame[dwDeviceID]);
#endif
		} else {
			// If we re-pause and refresh the current frame, we would lose a reliable reference to update
			// frame count left for running effects when we resume execution.
#if _DEBUG
			LogMessage("Pause all effects re-requested for dwID:%lu (already paused). Reference frame: %lu", dwDeviceID, paused_at_frame[dwDeviceID]);
#endif
		}
	}

	void VibrationController::Resume(DWORD dwDeviceID)
	{
		VibrationEff fx;
		DWORD frame, deltaf;

		if (paused[dwDeviceID]) {
			frame = GetTickCount();
			deltaf = frame - paused_at_frame[dwDeviceID];
#if _DEBUG
			LogMessage("Resuming effects for dwID:%lu. Was paused at frame %lu. Paused for %lu frames.",
				dwDeviceID, paused_at_frame[dwDeviceID], deltaf);
#endif

			mtxSync.lock();
			for (int k = 0; k < MAX_EFFECTS; k++) {
				fx = VibEffects[k][dwDeviceID];
				if (fx.dwStopFrame != INFINITE && fx.dwStopFrame < paused_at_frame[dwDeviceID]) {
#ifdef _DEBUG
					LogMessage("Effect in slot #%i: forwarding stop frame from %lu to %lu.",
						k, fx.dwStopFrame, fx.dwStopFrame + deltaf);
#endif
					fx.dwStopFrame = fx.dwStopFrame + deltaf;
#ifdef _DEBUG
				} else {
					LogMessage("Effect in slot #%i: %s", fx.dwStopFrame == INFINITE ? "is infinite" : "already reached last frame before pause");
#endif
				}
			}
			mtxSync.unlock();
#if _DEBUG
		} else {
			LogMessage("Effects for dwID:%lu are not paused. Can't resume non-paused effects.", dwDeviceID);
#endif
		}
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
