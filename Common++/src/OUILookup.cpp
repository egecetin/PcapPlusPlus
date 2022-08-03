#include "OUILookup.h"
#include "Logger.h"

#include <fstream>
#include <string.h>

namespace pcpp
{

#define PCPP_SHORT_MACS 0
#define PCPP_LONG_MACS 1

int OUILookup::initOUIDatabase(const std::string &path)
{
	uint64_t ctrRead = 0;
	std::ifstream dataFile;

	// Open database
	if (path.empty())
		dataFile.open("PCPP_OUIDatabase.bin");
	else
		dataFile.open(path);

	if (!dataFile.is_open())
	{
		PCPP_LOG_ERROR(std::string("Can't open OUI database: ") + strerror(errno));
		return -1;
	}

	// Read file
	int condition = -1;
	uint16_t maskValue = 0;
	for (std::string line; std::getline(dataFile, line);)
	{
		if (!line.compare("PCPP_SHORT_MACS"))
		{
			condition = PCPP_SHORT_MACS;
			continue;
		}
		else if (!line.compare("PCPP_LONG_MACS"))
		{
			condition = PCPP_LONG_MACS;
			continue;
		}
		else if (condition == PCPP_LONG_MACS && !line.substr(0, 5).compare("MASK "))
		{
			maskValue = std::stoi(line.substr(5));
			OUIVendorListLong.push_back(std::make_pair(maskValue, std::unordered_map<std::string, std::string>()));
			continue;
		}

		switch (condition)
		{
		case PCPP_SHORT_MACS: {
			size_t pos = line.find_first_of(",");
			if (pos != std::string::npos)
			{
				OUIVendorListShort.insert(std::make_pair(line.substr(0, pos), line.substr(pos + 1)));
				++ctrRead;
			}
			break;
		}
		case PCPP_LONG_MACS: {
			size_t pos = line.find_first_of(",");
			if (pos != std::string::npos && !OUIVendorListLong.empty())
			{
				OUIVendorListLong.back().second.insert(std::make_pair(line.substr(0, pos), line.substr(pos + 1)));
				++ctrRead;
			}
			break;
		}
		default:
			break;
		}
	}

	PCPP_LOG_DEBUG(std::to_string(ctrRead) + " vendors read successfully");
	return ctrRead;
}

std::string OUILookup::getVendorName(const pcpp::MacAddress &addr)
{
	// First check long addresses
	for (const auto &entry : OUIVendorListLong)
	{
		// Get MAC address
		uint64_t bufferAddr;
		uint8_t buffArray[6];
		addr.copyTo(buffArray);

#if __BYTE_ORDER == __BIG_ENDIAN
		bufferAddr =
			(((uint64_t)(((uint8_t *)(buffArray))[5]) << 16) + ((uint64_t)(((uint8_t *)(buffArray))[4]) << 24) +
			 ((uint64_t)(((uint8_t *)(buffArray))[3]) << 32) + ((uint64_t)(((uint8_t *)(buffArray))[2]) << 40) +
			 ((uint64_t)(((uint8_t *)(buffArray))[1]) << 48) + ((uint64_t)(((uint8_t *)(buffArray))[0]) << 56));
#else
		bufferAddr =
			(((uint64_t)(((uint8_t *)(buffArray))[0]) << 0) + ((uint64_t)(((uint8_t *)(buffArray))[1]) << 8) +
			 ((uint64_t)(((uint8_t *)(buffArray))[2]) << 16) + ((uint64_t)(((uint8_t *)(buffArray))[3]) << 24) +
			 ((uint64_t)(((uint8_t *)(buffArray))[4]) << 32) + ((uint64_t)(((uint8_t *)(buffArray))[5]) << 40));
#endif

		// Align and mask
		uint64_t maskValue = htobe64(~((1 << (48 - entry.first)) - 1)) >> 16;
		bufferAddr = bufferAddr & maskValue;

#if __BYTE_ORDER == __BIG_ENDIAN
		buffArray[0] = (bufferAddr >> 56) & 0xFF;
		buffArray[1] = (bufferAddr >> 48) & 0xFF;
		buffArray[2] = (bufferAddr >> 40) & 0xFF;
		buffArray[3] = (bufferAddr >> 32) & 0xFF;
		buffArray[4] = (bufferAddr >> 24) & 0xFF;
		buffArray[5] = (bufferAddr >> 16) & 0xFF;
#else
		buffArray[5] = (bufferAddr >> 40) & 0xFF;
		buffArray[4] = (bufferAddr >> 32) & 0xFF;
		buffArray[3] = (bufferAddr >> 24) & 0xFF;
		buffArray[2] = (bufferAddr >> 16) & 0xFF;
		buffArray[1] = (bufferAddr >> 8) & 0xFF;
		buffArray[0] = (bufferAddr >> 0) & 0xFF;
#endif
		// Search
		std::string searchStr = MacAddress(buffArray).toString();
		auto itr = entry.second.find(searchStr);
		if (itr != entry.second.end())
			return itr->second;
	}

	// If not found search OUI list
	std::string searchStr = addr.toString().substr(0, 8);
	auto itr = OUIVendorListShort.find(searchStr);
	if (itr != OUIVendorListShort.end())
		return itr->second;
	return "Unknown";
}

} // namespace pcpp