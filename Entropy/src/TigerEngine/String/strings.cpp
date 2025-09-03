#include <unordered_map>
#include <string>
#include "string.h"
#include "TigerEngine/tag.h"
#include <execution>
using namespace bin;


std::string ToAsciiString(const std::string& input) {
    std::string output;
    output.reserve(input.size());

    for (unsigned char c : input) {
       
        output.push_back(c);
        
    }
    return output;
}


std::unordered_map<int, std::string> GenerateStringMap() {
    auto hashes = GetAllTagsFromReference(0x808099ef);
    std::vector<std::unordered_map<int, std::string>> parts(hashes.size());
    std::transform(std::execution::par, hashes.begin(), hashes.end(), parts.begin(),
        [](auto h) {
            return ProcessStringContainer(static_cast<int>(h));
        });

    std::size_t total = 0;
    for (auto& m : parts) total += m.size();

    std::unordered_map<int, std::string> result;
    result.reserve(total);
    for (auto& m : parts) {
        for (auto& kv : m) {
            result.emplace(std::move(kv));
        }
    }
    std::ofstream outFile("output.txt");
    if (!outFile) {
        std::cerr << "Error opening file!" << std::endl;
    }
    else {
        for (const auto& pair : result) {
            outFile << std::hex << std::setw(8) << std::setfill('0')
                << pair.first
                << " - " << std::dec << pair.second << "\n";
        }
        outFile.close();
        std::cout << "Data written to output.txt" << std::endl;
    }
    return result;
}

std::unordered_map<int, std::string> ProcessStringContainer(int Hash) {
	TagHash tag(static_cast<int>(Hash));
    if (tag.success == false) {
        printf("Failed to retrieve tag data for hash: %08X\n", Hash);
        return {};
	}
    //printf("Starting String Parse for hash: %08X\n", Hash);
	auto buffer = tag.data;
    SStringContainer sc = bin::parse<SStringContainer>(tag.data, tag.size);
    SStringBank sb = bin::parse<SStringBank>(sc.Language.data, sc.Language.size);
	std::unordered_map<int, std::string> StringMap = {};
	int stringIndex = 0;
    for (const auto& meta : sb.StringMetadata) {
		std::string asciiStr;
        for (int i = 0; i < meta.partCount; i++) {
            unsigned char part_buffer[0x20];
			memcpy(part_buffer, sc.Language.data + (meta.partOffset.offset+(i*0x20)), 0x20);
			SStringPart part = bin::parse<SStringPart>(part_buffer,0x20);
            auto strOffset = part.StringOffset.offset;
            auto strLength = part.ByteLength;
            std::string str(reinterpret_cast<const char*>(sc.Language.data + strOffset+ (meta.partOffset.offset + (i * 0x20))), strLength);
            asciiStr+=ToAsciiString(str);
		}
        int StringHash = sc.HashTable[stringIndex].hash;
        StringMap[StringHash] = asciiStr;
        stringIndex++;
    }
	return StringMap;
}
