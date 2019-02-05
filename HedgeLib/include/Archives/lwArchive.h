#ifndef HLW_ARCHIVE_H_INCLUDED
#define HLW_ARCHIVE_H_INCLUDED
#include "archive.h"
#include "IO/dataSignature.h"
#include "IO/offsets.h"
#include "IO/nodes.h"
#include "IO/BINA.h"
#include "IO/file.h"
#include <memory>
#include <string_view>
#include <cstdint>
#include <array>
#include <vector>
#include <filesystem>

namespace HedgeLib::Archives
{
	static constexpr HedgeLib::IO::DataSignature PACxSignature = "PACx";
	static constexpr std::array<char, 3> LWPACxVersion = { '2', '0', '1' };

	#define CREATE_PACxHeader() HedgeLib::IO::BINA::DBINAV2Header(\
		LWPACxVersion, HedgeLib::IO::BINA::LittleEndianFlag, PACxSignature);

    struct DPACxDataNode
    {
		HedgeLib::IO::BINA::DBINAV2NodeHeader Header = HedgeLib::IO::BINA::DATASignature;
        std::uint32_t FileDataSize = 0;
        std::uint32_t ExtensionTableSize = 0;
        std::uint32_t ProxyTableSize = 0;
        std::uint32_t StringTableSize = 0;
        std::uint32_t OffsetTableSize = 0;
        std::uint8_t Unknown1 = 1;

		ENDIAN_SWAP(FileDataSize, ExtensionTableSize,
			ProxyTableSize, StringTableSize, OffsetTableSize);

		static constexpr std::uintptr_t SizeOffset =
			HedgeLib::IO::BINA::DBINAV2NodeHeader::SizeOffset;

		static constexpr long Origin = -16;

		constexpr std::uint32_t Size() const noexcept
		{
			return Header.Size();
		}

		template<template<typename> class OffsetType>
		void FixOffsets(const bool swapEndianness = false)
		{
			if (swapEndianness)
				EndianSwap();

			if (!OffsetTableSize)
				return;

			std::uintptr_t ptr = reinterpret_cast<std::uintptr_t>(this);
			HedgeLib::IO::BINA::FixOffsets<OffsetType>(
				reinterpret_cast<std::uint8_t*>(ptr + Header.Size()),
				OffsetTableSize, static_cast<std::uintptr_t>(
				static_cast<std::intptr_t>(ptr) + Origin), swapEndianness);
		}

		inline void FinishWrite(const HedgeLib::IO::File& file,
			long strTablePos, long offTablePos,
			long startPos, long endPos) const
		{
			// Fix Node Size
			startPos += sizeof(HedgeLib::IO::BINA::DBINAV2Header);
			std::uint32_t nodeSize = static_cast<std::uint32_t>(
				endPos - startPos);

			file.Seek(startPos + Header.SizeOffset);
			file.Write(&nodeSize, sizeof(nodeSize), 1);

			// Fix File Data Size
			file.Write(&FileDataSize, sizeof(FileDataSize), 1);

			// Fix Extension Table Size
			file.Write(&ExtensionTableSize, sizeof(ExtensionTableSize), 1);

			// Fix Proxy Table Size
			file.Write(&ProxyTableSize, sizeof(ProxyTableSize), 1);

			// Fix String Table Size
			std::uint32_t strTableSize = static_cast<std::uint32_t>(
				offTablePos - strTablePos);

			file.Write(&strTableSize, sizeof(strTableSize), 1);

			// Fix Offset Table Size
			std::uint32_t offTableSize = static_cast<std::uint32_t>(
				endPos - offTablePos);

			file.Write(&offTableSize, sizeof(offTableSize), 1);
		}
    };

	struct DPACProxyEntry
	{
		HedgeLib::IO::StringOffset32 Extension;
		HedgeLib::IO::StringOffset32 Name;
		std::uint32_t Index;
		
		ENDIAN_SWAP(Index);
	};

	struct DPACProxyEntryTable
	{
		HedgeLib::IO::ArrOffset32<DPACProxyEntry> ProxyEntries;
		ENDIAN_SWAP(ProxyEntries);
	};

	template<template<typename> class OffsetType>
	struct DPACSplitEntry
	{
		OffsetType<char> Name;
		ENDIAN_SWAP(Name);
	};

	template<template<typename> class OffsetType>
	struct DPACSplitsEntryTable
	{
		OffsetType<DPACSplitEntry<OffsetType>> Splits;
		std::uint32_t SplitsCount;

		ENDIAN_SWAP(Splits, SplitsCount);
	};

    enum DataFlags : std::uint8_t
    {
        DATA_FLAGS_NONE = 0,
        DATA_FLAGS_NO_DATA = 0x80
    };

    struct DPACDataEntry
    {
        std::uint32_t DataSize;
        std::uint32_t Unknown1 = 0;
        std::uint32_t Unknown2 = 0;
        DataFlags Flags = DATA_FLAGS_NONE;

		inline const std::uint8_t* GetDataPtr() const
		{
			return reinterpret_cast<const std::uint8_t*>(this + 1);
		}

		ENDIAN_SWAP(DataSize, Unknown1, Unknown2);
    };

	template<template<typename> class OffsetType, typename DataType>
	struct DPACxNode
	{
		OffsetType<char> Name;
		OffsetType<DataType> Data;

		ENDIAN_SWAP(Name, Data);
	};

	template<typename DataType>
	struct DPACxNodeTree
	{
		HedgeLib::IO::ArrOffset32<DPACxNode
			<HedgeLib::IO::DataOffset32, DataType>> Nodes;

		ENDIAN_SWAP(Nodes);
	};

	struct DLWArchive
	{
		DPACxDataNode Header;
		DPACxNodeTree<DPACxNodeTree<DPACDataEntry>> TypesTree;

		ENDIAN_SWAP(TypesTree);
	};

	class LWArchive : public Archive
	{
		HedgeLib::IO::NodePointer<DLWArchive> d = nullptr;
		void GenerateDLWArchive();

	public:
		static constexpr std::string_view Extension = ".pac";

		LWArchive() = default;
		~LWArchive()
		{
			std::default_delete<LWArchive>();
		}

		void Read(const HedgeLib::IO::File& file) override;
		void Write(const HedgeLib::IO::File& file) override;

		std::vector<std::filesystem::path> GetSplitList(
			const std::filesystem::path filePath) override;

		void Extract(const std::filesystem::path dir) override;
	};
}
#endif