/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2010  PCSX2 Dev Team
 *
 *  PCSX2 is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU Lesser General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  PCSX2 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with PCSX2.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

enum x86VendorType {
    x86Vendor_Intel = 0,
    x86Vendor_AMD = 1,
    x86Vendor_Unknown = 2
};

// --------------------------------------------------------------------------------------
//  x86capabilities
// --------------------------------------------------------------------------------------
class x86capabilities
{
public:
    bool isIdentified;

public:
    x86VendorType VendorID;

    uint FamilyID; // Processor Family
    uint Model;    // Processor Model
    uint TypeID;   // Processor Type
    uint StepID;   // Stepping ID

    u32 Flags;   // Feature Flags
    u32 Flags2;  // More Feature Flags
    u32 EFlags;  // Extended Feature Flags
    u32 EFlags2; // Extended Feature Flags pg2
    u32 SEFlag;  // Structured Extended Feature Flags Enumeration

    char VendorName[16]; // Vendor/Creator ID
    char FamilyName[50]; // the original cpu name

    // ----------------------------------------------------------------------------
    //   x86 CPU Capabilities Section (all boolean flags!)
    // ----------------------------------------------------------------------------

    union
    {
        struct
        {
            u32 hasFloatingPointUnit : 1;
            u32 hasVirtual8086ModeEnhancements : 1;
            u32 hasDebuggingExtensions : 1;
            u32 hasPageSizeExtensions : 1;
            u32 hasTimeStampCounter : 1;
            u32 hasModelSpecificRegisters : 1;
            u32 hasPhysicalAddressExtension : 1;
            u32 hasCOMPXCHG8BInstruction : 1;
            u32 hasAdvancedProgrammableInterruptController : 1;
            u32 hasSEPFastSystemCall : 1;
            u32 hasMemoryTypeRangeRegisters : 1;
            u32 hasPTEGlobalFlag : 1;
            u32 hasMachineCheckArchitecture : 1;
            u32 hasConditionalMoveAndCompareInstructions : 1;
            u32 hasFGPageAttributeTable : 1;
            u32 has36bitPageSizeExtension : 1;
            u32 hasProcessorSerialNumber : 1;
            u32 hasCFLUSHInstruction : 1;
            u32 hasDebugStore : 1;
            u32 hasACPIThermalMonitorAndClockControl : 1;
            u32 hasFastStreamingSIMDExtensionsSaveRestore : 1;
            u32 hasStreamingSIMDExtensions : 1;
            u32 hasStreamingSIMD2Extensions : 1;
            u32 hasSelfSnoop : 1;

            u32 hasThermalMonitor : 1;
            u32 hasIntel64BitArchitecture : 1;
            u32 hasStreamingSIMD3Extensions : 1;
            u32 hasSupplementalStreamingSIMD3Extensions : 1;
            u32 hasStreamingSIMD4Extensions : 1;
            u32 hasStreamingSIMD4Extensions2 : 1;
            u32 hasAVX : 1;
            u32 hasAVX2 : 1;
            u32 hasBMI1 : 1;
            u32 hasBMI2 : 1;
            u32 hasFMA : 1;

            // AMD-specific CPU Features
            u32 hasAMD64BitArchitecture : 1;
            u32 hasStreamingSIMD4ExtensionsA : 1;
        };

        u64 AllCapabilities;
    };

public:
    x86capabilities();

    void Identify();

    void SIMD_EstablishMXCSRmask();
};

enum SSE_RoundMode {
    SSE_RoundMode_FIRST = 0,
    SSEround_Nearest = 0,
    SSEround_NegInf,
    SSEround_PosInf,
    SSEround_Chop,
    SSE_RoundMode_COUNT
};

ImplementEnumOperators(SSE_RoundMode);

// Predeclaration for xIndirect32
namespace x86Emitter {
	template <typename T> class xIndirect;
	typedef xIndirect<u32> xIndirect32;
}

// --------------------------------------------------------------------------------------
//  SSE_MXCSR  -  Control/Status Register (bitfield)
// --------------------------------------------------------------------------------------
// Bits 0-5 are exception flags; used only if SSE exceptions have been enabled.
//   Bits in this field are "sticky" and, once an exception has occured, must be manually
//   cleared using LDMXCSR or FXRSTOR.
//
// Bits 7-12 are the masks for disabling the exceptions in bits 0-5.  Cleared bits allow
//   exceptions, set bits mask exceptions from being raised.
//
union SSE_MXCSR
{
    u32 bitmask;
    struct
    {
        u32
            InvalidOpFlag : 1,
            DenormalFlag : 1,
            DivideByZeroFlag : 1,
            OverflowFlag : 1,
            UnderflowFlag : 1,
            PrecisionFlag : 1,

            // This bit is supported only on SSE2 or better CPUs.  Setting it to 1 on
            // SSE1 cpus will result in an invalid instruction exception when executing
            // LDMXSCR.
            DenormalsAreZero : 1,

            InvalidOpMask : 1,
            DenormalMask : 1,
            DivideByZeroMask : 1,
            OverflowMask : 1,
            UnderflowMask : 1,
            PrecisionMask : 1,

            RoundingControl : 2,
            FlushToZero : 1;
    };

    SSE_RoundMode GetRoundMode() const;
    SSE_MXCSR &SetRoundMode(SSE_RoundMode mode);
    SSE_MXCSR &ClearExceptionFlags();
    SSE_MXCSR &EnableExceptions();
    SSE_MXCSR &DisableExceptions();

    SSE_MXCSR &ApplyReserveMask();

    bool operator==(const SSE_MXCSR &right) const
    {
        return bitmask == right.bitmask;
    }

    bool operator!=(const SSE_MXCSR &right) const
    {
        return bitmask != right.bitmask;
    }

    operator x86Emitter::xIndirect32() const;
};

extern SSE_MXCSR MXCSR_Mask;


extern __aligned16 x86capabilities x86caps;
