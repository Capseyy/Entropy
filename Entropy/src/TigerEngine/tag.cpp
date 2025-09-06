#include "tag.h"
#include "TigerEngine/package.h"
#include "TigerEngine/globaldata.h"
#include "TigerEngine/helpers.h"


int TagHash::getPkgId() {
	uint16_t pkgID = floor((hash - 0x80800000) / 8192);
	return pkgID;
}

int TagHash::getEntryID() {
	uint32_t entryId = hash & 0x1FFF;
	return entryId;
}

unsigned char* TagHash::getData() {
	if (hash == 0 or hash == 4294967295) {
		size = 0;
		data = nullptr;
		return nullptr;
	}
	int pkgId = getPkgId();
	int entryId = getEntryID();
	auto& map = GlobalData::getMap();
	auto it = map.find(pkgId);
	if (it == map.end()) {
		printf("Invalid package ID: %d found for taghash %d\n ", pkgId, hash);
		size = 0;
		data = nullptr;
		return nullptr;
	}
	Package* pkg = &it->second;
	//printf("Extracting tag %08X\n", hash);
	auto ReturnObject = pkg->ExtractEntry(entryId);
	success = ReturnObject.success;
	size = pkg->Entries[entryId].file_size;
	reference = pkg->Entries[entryId].reference;
	data = ReturnObject.data;
	return data;

}

void TagHash::print_buffer() {
	for (size_t i = 0; i < size; ++i) {
		printf("%02X", data[i]); // print each byte as two-digit hex
	}
	printf("\n");
}

void WideHash::print() {
	std::ostream& os = std::cout;
	os << "WideHash {\n"
		<< "  Unk0: " << wideHashData.Unk0 << "\n"
		<< "  Unk4: " << wideHashData.Unk4 << "\n"
		<< "  Hash64: 0x" << std::hex << std::setw(16) << std::setfill('0')
		<< wideHashData.Hash64 << std::dec << "\n"
		<< "  size: " << size << "\n"
		<< "  reference: " << reference << "\n"
		<< "  success: " << std::boolalpha << success << "\n";

	if (data && size > 0) {
		os << "  data: ";
		for (std::size_t i = 0; i < size; ++i) {
			os << std::hex << std::setw(2) << std::setfill('0')
				<< static_cast<unsigned>(data[i]) << " ";
		}
		os << std::dec << "\n";
	}
	else {
		os << "  data: null\n";
	}
	os << "}\n";
}
unsigned char* WideHash::getData() {
	using clock = std::chrono::steady_clock; // monotonic => good for timing
	const auto& h64 = GlobalData::getH64();
	auto it = h64.find(wideHashData.Hash64);

	
	if (it == h64.end()) {
		printf("H64 lookup failed for 0x%016 \n", wideHashData.Hash64);
		return nullptr;
	}
	TagHash toTagHash = it->second;
	toTagHash.getData();
	printf("Resolving WideHash 0x%016" PRIx64 " to TagHash %08X\n", wideHashData.Hash64, toTagHash.hash);
	size = toTagHash.size;
	reference = toTagHash.reference;
	success = toTagHash.success;
	data = toTagHash.data;
	tagHash32 = toTagHash.hash;
	return toTagHash.data;

}
