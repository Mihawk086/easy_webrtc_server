#pragma once

#include <unordered_map>
#include <vector>
#include <mutex>

class PortManager
{
public:
	PortManager* GetInstance();
	~PortManager();
	int16_t GetPort();
private:
	PortManager();
	std::unordered_map<int16_t, bool> m_mapPorts;
	std::vector<bool> m_vecPorts;
	std::mutex m_mutex;

};

