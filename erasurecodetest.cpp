#include <iostream>
#include "wirehair/wirehair.h"

int main() {
	std::cout << "Hello" << std::endl;
	if (wirehair_init() == Wirehair_Success) {
		std::cout << "Wirehair success" << std::endl;
	}
	return 0;
}
