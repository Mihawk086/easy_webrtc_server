#include "PortManager.h"

#define MIN_PORT 10000
#define MAX_PORT 19999
#define SIZE 10000

PortManager::PortManager()
	:m_vecPorts(SIZE,false)
{
	
}

PortManager* PortManager::GetInstance()
{
	static PortManager manager;
	return &manager;
}

PortManager::~PortManager()
{
	
}

int16_t PortManager::GetPort()
{
	int16_t port = -1;
	std::lock_guard<std::mutex> lock(m_mutex);
	for (int i = m_nIndex; ; ) {
		if (!m_vecPorts[i]) {
			port = MIN_PORT + i;
			m_vecPorts[i] = true;
			m_nIndex = i;
			break;
		}
		if (i == SIZE - 1) {
			i = 0;
		}
		else {
			i++;
		}
		if (i == m_nIndex) {
			break;
		}
	}
	return port;
}

void PortManager::DelPort(int16_t nPort)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	if (nPort>= MIN_PORT && nPort <= MAX_PORT) {
		m_vecPorts[nPort] = false;
	}
}
