#include "PortManager.h"

#define MIN_PORT 40000
#define MAX_PORT 49999
#define SIZE 10000;

PortManager::PortManager()
	:m_vecPorts(SIZE,false)
{
	
}

PortManager* PortManager::GetInstance()
{
	return nullptr;
}

PortManager::~PortManager()
{
	static PortManager manager;
	return &manager;
}

int16_t PortManager::GetPort()
{
	int16_t port = 0;
	std::lock_guard<std::mutex> lock(m_mutex);
	for (int i = m_nIndex; i < SIZE; ) {
		if (!m_vecPorts[i]) {
			port = MIN_PORT + 1;
			break;
		}

	}
}
