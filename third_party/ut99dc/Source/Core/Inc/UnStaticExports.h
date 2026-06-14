/*=============================================================================
	UnStaticExports.h: package export lookup for static builds.
=============================================================================*/

#ifdef UNREAL_STATIC

struct FPackageExport;

extern CORE_API FPackageExport* GExportsTable;

struct FPackageExport
{
	const char* Name;
	void* Address;
	FPackageExport* Next = nullptr;
	FPackageExport( const char* InName, void* InAddress ) : Name(InName), Address(InAddress)
	{
		FPackageExport* NextExport = GExportsTable;
		GExportsTable = this;
		Next = NextExport;
	}
};

#define STATIC_EXPORT( sexp, ssym ) \
	extern "C" { GCC_USED FPackageExport GStaticExport##sexp( #sexp, (void*)&ssym ); }

CORE_API void* appGetStaticExport( const char* Name );

#else

#define STATIC_EXPORT( sexp, ssym )

#endif
