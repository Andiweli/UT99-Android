#pragma once

#include "Engine.h"
#include "Texture.h"
#include "Mesh.h"
#include "Sound.h"

template<class T>
class FSimpleArray
{
public:
	FSimpleArray() : Data(NULL), ArrayNum(0), Capacity(0) {}
	~FSimpleArray() { if (Data) delete[] Data; }

	void Add(const T& Item)
	{
		if (ArrayNum >= Capacity)
		{
			Capacity = Capacity ? Capacity * 2 : 16;
			T* NewData = new T[Capacity];
			for (INT i = 0; i < ArrayNum; i++)
				NewData[i] = Data[i];
			if (Data) delete[] Data;
			Data = NewData;
		}
		Data[ArrayNum++] = Item;
	}

	UBOOL RemoveItem(const T& Item)
	{
		for (INT i = 0; i < ArrayNum; i++)
		{
			if (Data[i] == Item)
			{
				// Shift remaining items
				for (INT j = i; j < ArrayNum - 1; j++)
				{
					Data[j] = Data[j + 1];
				}
				ArrayNum--;
				return true;
			}
		}
		return false;
	}

	T& operator()(INT Index) { return Data[Index]; }
	const T& operator()(INT Index) const { return Data[Index]; }
	INT Num() const { return ArrayNum; }
	void Empty() { ArrayNum = 0; }

private:
	T* Data;
	INT ArrayNum;
	INT Capacity;
};

class FDCUtil
{
public:

	void InitEngine();
	void Main();
	void HandleError( const char* Exception );
	void ExitEngine();

private:
	void LoadPackages( const char* Dir );
	void ParsePackageArg( const char* Arg, const char* Glob );
	UBOOL ConvertTexturePkg( const FString& PkgPath, UPackage* Pkg );
	UBOOL ConvertSoundPkg( const FString& PkgPath, UPackage* Pkg );
	UBOOL ConvertMusicPkg( const FString& PkgPath, UPackage* Pkg );
	UBOOL ConvertMeshPkg( const FString& PkgPath, UPackage* Pkg );
	UBOOL ConvertMapPkg( const FString& PkgPath, UPackage* Pkg );
	void CommitChanges();
	void CommitChanges( const FSimpleArray<FString>& ChangedNames, const FSimpleArray<UPackage*>& ChangedPtrs );

private:
	UEngine* Engine = nullptr;
	FSimpleArray<FString> LoadedPackageNames;
	FSimpleArray<UPackage*> LoadedPackagePtrs;
	FSimpleArray<FString> ChangedPackageNames;
	FSimpleArray<UPackage*> ChangedPackagePtrs;
	TArray<FGuid> PackageGuids;
	TMapBase<UPackage*, QWORD> PackageSizeBefore;
	FSimpleArray<UPalette*> UnrefPalettes;
	DWORD TotalPrevSize = 0;
	DWORD TotalNewSize = 0;
};
