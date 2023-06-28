// cleand up version of https://linustechtips.com/topic/635593-how-do-i-get-cpu-mobo-ram-gpu-psu-info-with-c/

#include <iostream>
#include <stdint.h>
#include <string>
#include <cstring>

inline struct CPUInfo_t {
	//X86 CPU registers relevant to CPUID, in a struct to keep them together.
	struct X86Regs {
		uint32_t	eax;
		uint32_t	ebx;
		uint32_t	ecx;
		uint32_t	edx;
	};

	//Information returned by CPUID subfunction 1, in a struct to keep them together.
	struct	X86CPUInfo {
		uint32_t	Stepping;
		uint32_t	Model;
		uint32_t	Family;
		uint32_t	Type;
		uint32_t	ExtendedModel;
		uint32_t	ExtendedFamily;
	};

	/*The actual inline assembly CPUID function.
	This is roughly equivalent (simplified) to:

		mov eax, eax_in
		cpuid
		mov Regs_out->eax, eax
		mov Regs_out->ebx, ebx
		mov Regs_out->ecx, ecx
		mov Regs_out->edx, edx
	*/
	static inline void cpuid( uint32_t eax_in, X86Regs* Regs_out ) {
		asm volatile ( "cpuid"	:	"=a"( Regs_out->eax ),
						"=b"( Regs_out->ebx ),
						"=c"( Regs_out->ecx ),
						"=d"( Regs_out->edx )
					:	"a"( eax_in ) );
	}

	//Copies the given cpu register to given char buffer.
	void CopyRegToBuff ( uint32_t Reg, char* Buff ) {
		for ( int i = 0; i < 4; i++)	{
			Buff[ i ] = Reg & 0xFF;
			Reg >>= 8;
		}
	}

	//Calls CPUID subfunction 0 to retrieve highest supported subfunction in eax.
	uint32_t GetHighestFunction () {
		X86Regs	Regs;
		cpuid( 0, &Regs );
		return Regs.eax;
	}

	//Calls CPUID subfunction 0 to retrieve vendor ID string in ebx, edx, ecx.
	std::string GetVendorID () {
		X86Regs	Regs;
		cpuid( 0, &Regs );

		char VendorID[ 13 ];
		memset( VendorID, 0, sizeof(VendorID ) );
		CopyRegToBuff( Regs.ebx, VendorID );
		CopyRegToBuff( Regs.edx, VendorID + 4 );
		CopyRegToBuff( Regs.ecx, VendorID + 8 );

		return std::string(VendorID); 
	}

	//Calls CPUID subfunction 1 to retrieve CPUinfo, bitmasks are explained here:
	// https://en.wikipedia.org/wiki/CPUID#EAX.3D1:_Processor_Info_and_Feature_Bits
	void GetCPUInfo ( X86CPUInfo& CPUInfo ) {
		X86Regs Regs;
		cpuid( 1, &Regs );

		CPUInfo.Stepping = Regs.eax & 0x0F;
		CPUInfo.Model = ( Regs.eax & 0xF0 ) >> 4;
		CPUInfo.Family = ( Regs.eax & 0xF00 ) >> 8;
		CPUInfo.Type = ( Regs.eax & 0x3000 ) >> 12;
		CPUInfo.ExtendedModel = ( Regs.eax & 0xF0000 ) >> 16;
		CPUInfo.ExtendedFamily = ( Regs.eax & 0xFF00000 ) >> 20;
	}
} CPUInfo;