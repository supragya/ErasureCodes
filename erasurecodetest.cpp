#include <iostream>
#include <unistd.h>
#include <chrono>
#include <thread>
#include "wirehair/wirehair.h"

void randomizeData(void* message, int bytes) {
	int step = 1314;
	int ctr = 0;
	uint8_t *messageint = (uint8_t*)message;
	while (ctr < bytes) {
		messageint[ctr] = 231;
		ctr += step;
	}
}

int main() {
	std::cout << "Wirehair tests for erasure code" << std::endl;
	
	// Wirehair initialisation
	if (wirehair_init() == Wirehair_Success) {
		std::cout << "Wirehair init success" << std::endl;
	} else {
		std::cout << "Wirehair init unsuccessful" << std::endl;
		exit(1);
	}

	int bytes = 512 * 1024 * 1024 * sizeof(uint8_t); // 512 MB
	int blockBytes = 50 * 1024; // 50 KB

	std::cout << "Value of N (2 <= N <= 64000 expected): " 
					<< float(bytes + blockBytes - 1) / blockBytes << std::endl;	
	
	void* message = malloc(bytes);
	randomizeData(message, bytes);
	std::this_thread::sleep_for(std::chrono::milliseconds(1000));
	free(message);

	return 0;
}
