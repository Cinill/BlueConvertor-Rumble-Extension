#ifndef __DIEFFECTATTRIBUTES_H_INCLUDED__

#define __DIEFFECTATTRIBUTES_H_INCLUDED__

#include "dinput.h"

/*
 * Defines the controller's DIEFFECTATTRIBUTES that should go to
 * HKLM\System\CurrentControlSet\Control\MediaProperties\Joystick\OEM\VID_NNNN&PID_NNNN\OemForceFeedback\Effects\
 * And also guide in interpreting the effect it wants to play.
 * Documentation: 
 *
 * The values here were extracted and interpreted from the joystick original drivers/install.
 * The registry key will have 20 bytes, for example:
 * 01 01 00 00|01 86 00 00|ed 03 00 00|ed 03 00 00|30 00 00 00|(ConstantForce)
 * 0b 01 00 00|04 d8 00 00|6d 03 00 00|ed 03 00 00|30 00 00 00|(Friction)
 * dwEffectId |dwEffType  |dwStaticPar|dwDynamicPa|dwCoords   |
 *
 * Picking the Friction example:
 * - dwEffectId: 0b 01 00 00 (thus 0x0000010b DWORD)
 * - dwEffType: 04 d8 00 00 (thus 0x0000d804 DWORD)
 * - dwStaticParams: 6d 03 00 00 (thus 0x00006d03 DWORD)
 * - dwDynamicParams: ed 03 00 00 (thus 0x0000ed03 DWORD)
 * - dwCoords: 30 00 00 00 (thus 0x00000030 DWORD)
 */

// 1. dwEffectId

/*
 * Controller Effect IDs for this specific controller.
 * This should match its corresponding Attributes' first two bytes (reversed),
 * but it's probably safe to use 1..5 (DIEFT_CONSTANTFORCE..DIEFT_CUSTOMFORCE)
 * Registry key:
 * HKLM\System\CurrentControlSet\Control\MediaProperties\Joystick\OEM\VID_NNNN&PID_NNNN\OemForceFeedback\Effects\



dwEffectId  | dwEffType   | dwStaticPar | dwDynamicPar| dwCoords    |(EffectName)
00 00 00 00 | 01 86 00 00 | ed 03 00 00 | ed 03 00 00 | 30 00 00 00 | Constant
01 00 00 00 | 02 86 00 00 | ef 03 00 00 | ef 03 00 00 | 30 00 00 00 | Ramp Force
02 00 00 00 | 02 86 00 00 | ef 03 00 00 | ef 03 00 00 | 30 00 00 00 | Square Wave
03 00 00 00 | 03 86 00 00 | ef 03 00 00 | ef 03 00 00 | 30 00 00 00 | Sine Wave
04 00 00 00 | 03 86 00 00 | ef 03 00 00 | ef 03 00 00 | 30 00 00 00 | Triangle Wave
05 00 00 00 | 03 86 00 00 | ef 03 00 00 | ef 03 00 00 | 30 00 00 00 | Sawtooth Up Wave
06 00 00 00 | 03 86 00 00 | ef 03 00 00 | ef 03 00 00 | 30 00 00 00 | Sawtooth Down Wave
07 00 00 00 | 04 d8 00 00 | ef 03 00 00 | ef 03 00 00 | 30 00 00 00 | Spring
08 00 00 00 | 04 d8 00 00 | 6d 03 00 00 | 6d 03 00 00 | 30 00 00 00 | Damper
09 00 00 00 | 04 d8 00 00 | 6d 03 00 00 | 6d 03 00 00 | 30 00 00 00 | Inertia
0a 00 00 00 | 04 d8 00 00 | 6d 03 00 00 | 6d 03 00 00 | 30 00 00 00 | Friction
00 01 00 00 | 00 86 00 00 | ef 03 00 00 | ef 03 00 00 | 30 00 00 00 | CustomForce
*/


#define CEID_ConstantForce 0x00000000
#define CEID_RampForce     0x00000001
#define CEID_Square        0x00000002
#define CEID_Sine          0x00000003
#define CEID_Triangle      0x00000004
#define CEID_SawtoothUp    0x00000005
#define CEID_SawtoothDown  0x00000006
#define CEID_Spring        0x00000007
#define CEID_Damper        0x00000008
#define CEID_Inertia       0x00000009
#define CEID_Friction      0x0000000a
#define CEID_CustomForce   0x00000100

// 2. dwEffType

/*
 * This stub code helped fill the table:

#define yx(type) DIEFT_GETTYPE(0x0000d804) == type ? 'x' : ' '
#define yy(mask) 0x0000d804 & mask ? 'x':' '
LogMessage("What does 0xd804 match:\n "
    "[%c] DIEFT_CONSTANTFORCE  [%c] DIEFT_RAMPFORCE       [%c] DIEFT_PERIODIC\n "
    "[%c] DIEFT_CONDITION      [%c] DIEFT_CUSTOMFORCE     [%c] DIEFT_HARDWARE\n "
    "[%c] DIEFT_FFATTACK       [%c] DIEFT_FFFADE          [%c] DIEFT_SATURATION\n "
    "[%c] DIEFT_POSNEGCOEFFIC. [%c] DIEFT_POSNEGSATURATI. [%c] DIEFT_DEADBAND\n "
    "[%c] DIEFT_STARTDELAY     [%u] DIEFT_GETTYPE(x)\n ",
    yx(DIEFT_CONSTANTFORCE), yx(DIEFT_RAMPFORCE), yx(DIEFT_PERIODIC),
    yx(DIEFT_CONDITION), yx(DIEFT_CUSTOMFORCE), yx(DIEFT_HARDWARE),
    yy(DIEFT_FFATTACK), yy(DIEFT_FFFADE), yy(DIEFT_SATURATION),
    yy(DIEFT_POSNEGCOEFFICIENTS), yy(DIEFT_POSNEGSATURATION), yy(DIEFT_DEADBAND),
    yy(DIEFT_STARTDELAY), DIEFT_GETTYPE(0x0000d804)
);

Results:

What does 0x8601 match:
[x] DIEFT_CONSTANTFORCE   [ ] DIEFT_RAMPFORCE       [ ] DIEFT_PERIODIC
[ ] DIEFT_CONDITION       [ ] DIEFT_CUSTOMFORCE     [ ] DIEFT_HARDWARE
[x] DIEFT_FFATTACK        [x] DIEFT_FFFADE          [ ] DIEFT_SATURATION
[ ] DIEFT_POSNEGCOEFFIC.  [ ] DIEFT_POSNEGSATURATI. [ ] DIEFT_DEADBAND
[x] DIEFT_STARTDELAY      [1] DIEFT_GETTYPE(x)

What does 0x8602 match:
[ ] DIEFT_CONSTANTFORCE   [x] DIEFT_RAMPFORCE       [ ] DIEFT_PERIODIC
[ ] DIEFT_CONDITION       [ ] DIEFT_CUSTOMFORCE     [ ] DIEFT_HARDWARE
[x] DIEFT_FFATTACK        [x] DIEFT_FFFADE          [ ] DIEFT_SATURATION
[ ] DIEFT_POSNEGCOEFFIC.  [ ] DIEFT_POSNEGSATURATI. [ ] DIEFT_DEADBAND
[x] DIEFT_STARTDELAY      [2] DIEFT_GETTYPE(x)

What does 0x8603 match:
[ ] DIEFT_CONSTANTFORCE   [ ] DIEFT_RAMPFORCE       [x] DIEFT_PERIODIC
[ ] DIEFT_CONDITION       [ ] DIEFT_CUSTOMFORCE     [ ] DIEFT_HARDWARE
[x] DIEFT_FFATTACK        [x] DIEFT_FFFADE          [ ] DIEFT_SATURATION
[ ] DIEFT_POSNEGCOEFFIC.  [ ] DIEFT_POSNEGSATURATI. [ ] DIEFT_DEADBAND
[x] DIEFT_STARTDELAY      [3] DIEFT_GETTYPE(x)

What does 0xd804 match:
[ ] DIEFT_CONSTANTFORCE   [ ] DIEFT_RAMPFORCE       [ ] DIEFT_PERIODIC
[x] DIEFT_CONDITION       [ ] DIEFT_CUSTOMFORCE     [ ] DIEFT_HARDWARE
[ ] DIEFT_FFATTACK        [ ] DIEFT_FFFADE          [x] DIEFT_SATURATION
[x] DIEFT_POSNEGCOEFFIC.  [ ] DIEFT_POSNEGSATURATI. [x] DIEFT_DEADBAND
[x] DIEFT_STARTDELAY      [4] DIEFT_GETTYPE(x)

What does 0x8600 match:
[ ] DIEFT_CONSTANTFORCE   [ ] DIEFT_RAMPFORCE       [ ] DIEFT_PERIODIC
[ ] DIEFT_CONDITION       [ ] DIEFT_CUSTOMFORCE     [ ] DIEFT_HARDWARE
[x] DIEFT_FFATTACK        [x] DIEFT_FFFADE          [ ] DIEFT_SATURATION
[ ] DIEFT_POSNEGCOEFFIC.  [ ] DIEFT_POSNEGSATURATI. [ ] DIEFT_DEADBAND
[x] DIEFT_STARTDELAY      [0] DIEFT_GETTYPE(x)


*/

// Effects with 86 in their 'major' byte in registry's OEMForceFeedback attributes
#define CETYPE_CAPABS DIEFT_FFATTACK | DIEFT_FFFADE | DIEFT_STARTDELAY

// Effects with d8 in their 'major' byte in registry's OEMForceFeedback attributes
#define CETYPE_CONDITION_CAPABS DIEFT_SATURATION | DIEFT_POSNEGCOEFFICIENTS | DIEFT_DEADBAND | DIEFT_STARTDELAY

#define CETYPE_ConstantForce DIEFT_CONSTANTFORCE | CETYPE_CAPABS  // 86 01
#define CETYPE_RampForce     DIEFT_RAMPFORCE | CETYPE_CAPABS     // 86 02
#define CETYPE_Periodic      DIEFT_PERIODIC | CETYPE_CAPABS      // 86 03, Square, Sine, Triangle, Sawtooth
#define CETYPE_Square        CETYPE_Periodic
#define CETYPE_Sine          CETYPE_Periodic
#define CETYPE_Triangle      CETYPE_Periodic
#define CETYPE_SawtoothUp    CETYPE_Periodic
#define CETYPE_SawtoothDown  CETYPE_Periodic
#define CETYPE_Conditional   DIEFT_CONDITION | CETYPE_CONDITION_CAPABS // d8 04, Spring, Damper, Inertia, Friction
#define CETYPE_Spring        CETYPE_Conditional
#define CETYPE_Damper        CETYPE_Conditional
#define CETYPE_Inertia       CETYPE_Conditional
#define CETYPE_Friction      CETYPE_Conditional
#define CETYPE_CustomForce   DIEFT_CUSTOMFORCE | CETYPE_CAPABS   // 86 05

// 3. dwStaticParams

/*

Stub code:
#define yy(mask) 0x000003ef & mask ? 'x':' '
LogMessage("What does 0x03ef for Spring, Damper, Inertia, Firction match:\n "
    "[%c] DIEP_DURATION        [%c] DIEP_SAMPLEPERIOD     [%c] DIEP_GAIN\n "
    "[%c] DIEP_TRIGGERBUTTON   [%c] DIEP_TRIGGERREPEATIN. [%c] DIEP_AXES\n "
    "[%c] DIEP_DIRECTION       [%c] DIEP_ENVELOPE         [%c] DIEP_TYPESPECIFICPA.\n "
    "[%c] DIEP_STARTDELAY      [%c] DIEP_ALLPARAMS_DX5    [%c] DIEP_ALLPARAMS\n "
    "[%c] DIEP_START           [%c] DIEP_NORESTART        [%c] DIEP_NODOWNLOAD\n ",
    yy(DIEP_DURATION     ), yy(DIEP_SAMPLEPERIOD   ), yy(DIEP_GAIN),
    yy(DIEP_TRIGGERBUTTON), yy(DIEP_TRIGGERREPEATINTERVAL), yy(DIEP_AXES),
    yy(DIEP_DIRECTION    ), yy(DIEP_ENVELOPE       ), yy(DIEP_TYPESPECIFICPARAMS),
    yy(DIEP_STARTDELAY   ), yy(DIEP_ALLPARAMS_DX5  ), yy(DIEP_ALLPARAMS),
    yy(DIEP_START        ), yy(DIEP_NORESTART      ), yy(DIEP_NODOWNLOAD)
);

: 03 ed (ConstantForce)
What does 0x03ed for ConstantForce match:
 [x] DIEP_DURATION        [ ] DIEP_SAMPLEPERIOD     [x] DIEP_GAIN
 [x] DIEP_TRIGGERBUTTON   [ ] DIEP_TRIGGERREPEATIN. [x] DIEP_AXES
 [x] DIEP_DIRECTION       [x] DIEP_ENVELOPE         [x] DIEP_TYPESPECIFICPA.
 [x] DIEP_STARTDELAY      [x] DIEP_ALLPARAMS_DX5    [x] DIEP_ALLPARAMS
 [ ] DIEP_START           [ ] DIEP_NORESTART        [ ] DIEP_NODOWNLOAD

: 03 ef (Ramp, Square, Sine, Triangle, SawtoothUp, SawtoothDown, Spring, CustomForce)
What does 0x03ef for Ramp, Square, Sine, Triangle, SawtoothUp, SawtoothDown, Spring, CustomForce match:
 [x] DIEP_DURATION        [x] DIEP_SAMPLEPERIOD     [x] DIEP_GAIN
 [x] DIEP_TRIGGERBUTTON   [ ] DIEP_TRIGGERREPEATIN. [x] DIEP_AXES
 [x] DIEP_DIRECTION       [x] DIEP_ENVELOPE         [x] DIEP_TYPESPECIFICPA.
 [x] DIEP_STARTDELAY      [x] DIEP_ALLPARAMS_DX5    [x] DIEP_ALLPARAMS
 [ ] DIEP_START           [ ] DIEP_NORESTART        [ ] DIEP_NODOWNLOAD

: 03 6d (Damper, Inertia, Firction)
What does  0x036d for Spring, Damper, Inertia, Firction match:
 [x] DIEP_DURATION        [ ] DIEP_SAMPLEPERIOD     [x] DIEP_GAIN
 [x] DIEP_TRIGGERBUTTON   [ ] DIEP_TRIGGERREPEATIN. [x] DIEP_AXES
 [x] DIEP_DIRECTION       [ ] DIEP_ENVELOPE         [x] DIEP_TYPESPECIFICPA.
 [x] DIEP_STARTDELAY      [x] DIEP_ALLPARAMS_DX5    [x] DIEP_ALLPARAMS
 [ ] DIEP_START           [ ] DIEP_NORESTART        [ ] DIEP_NODOWNLOAD

*/

// All static parameter types defines these
#define CESP_Common DIEP_DURATION | DIEP_GAIN | DIEP_TRIGGERBUTTON | DIEP_AXES | DIEP_DIRECTION | DIEP_TYPESPECIFICPARAMS | DIEP_STARTDELAY | DIEP_ALLPARAMS_DX5 | DIEP_ALLPARAMS

#define CESP_ConstantForce CESP_Common | DIEP_ENVELOPE

// The Ramp, Periodic and Custom forrce effects have all Constant Force ones -plus- DIEP_SAMPLEPERIOD
#define CESP_RampForce CESP_ConstantForce | DIEP_SAMPLEPERIOD
//#define CESP_Square CESP_RampForce
//#define CESP_Sine CESP_RampForce
//#define CESP_Triangle CESP_RampForce
//#define CESP_SawtoothUp CESP_RampForce
//#define CESP_SawtoothDown CESP_RampForce
//#define CESP_Spring CESP_RampForce
#define CESP_CustomForce CESP_RampForce

// The Damper, Inertia and Firction forrce effects have all Common Force
//#define CESP_Damper CESP_Common
//#define CESP_Inertia CESP_Common
//#define CESP_Friction CESP_Common

// Strange
#define CESP_Periodic CESP_RampForce
#define CESP_Conditional CESP_Common

// 4. dwDynamicParams
// All effects have the same value from dwStaticParams, thus

#define CEDP_Common CESP_ConstantForce

// The Cobstant force

#define CEDP_ConstantForce  CESP_ConstantForce 

// The Ramp, Periodic and Custom force effects have all Constant Force ones -plus- DIEP_SAMPLEPERIOD
#define CEDP_RampForce   CESP_RampForce 
//#define CEDP_Square   CESP_Square 
//#define CEDP_Sine   CESP_Sine 
//#define CEDP_Triangle   CESP_Triangle 
//#define CEDP_SawtoothUp  CESP_SawtoothUp 
//#define CEDP_SawtoothDown   CESP_SawtoothDown 
//#define CEDP_Spring   CESP_Spring 
#define CEDP_CustomForce   CESP_CustomForce 

// The Damper, Inertia and Friction force effects have all Common Force
//#define CEDP_Damper   CESP_Damper 
//#define CEDP_Inertia   CESP_Inertia 
//#define CEDP_Friction   CESP_Friction 

// Strange
#define CEDP_Periodic   CESP_Periodic 
#define CEDP_Conditional   CESP_Conditional 




// 5. dwCoords
// All effects have 0x30 (0x00000030), which means DIEFF_SPHERICAL is not supported.

#define CECOORDS DIEFF_CARTESIAN | DIEFF_POLAR
#endif
