#include <iostream>
#include <random>
#include <unistd.h>
#include <unordered_set>
#include <chrono>
#include <thread>
#include "md5.h"
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

long long int integrity(void* message, uint64_t dist) {
	long long int mod1 = 122242314, mod2 = 9814121221;
	long long int cnt1 = 0, cnt2 = 0;
	for (uint64_t i = 0; i < dist; i++) {
		cnt1 = (cnt1 + *(uint8_t *)(message+i))%mod1;
		cnt2 = (cnt2 + *(uint8_t *)(message+i))%mod2;
	}
	return cnt1+cnt2;
}

void* GetRandomData(uint64_t bytes) {
	void* message = malloc(bytes);
	srand(55109);
	for (int i = 0; i < bytes; i++) {
		*(int *)(message+i) = rand()%255;
	}
	return message;
}

std::unordered_set<int> pickSet(int N, int k) {
  std::unordered_set<int> elems;
	std::mt19937 gen;
  for (int r = N - k; r < N; ++r) {
    int v = std::uniform_int_distribution<>(1, r)(gen);
    if (!elems.insert(v).second) {
      elems.insert(r);
    }   
  }
  return elems;
}

void BenchmarkWirehair(uint64_t bytes, uint32_t chunksize, float redundancy, int encoderruns, float packetlosses[], int pklosscnt) {
	std::cout << "---------------------- BENCHMARK ------------------------ " << std::endl;
	std::cout << "Benchmark Messagelen (KB): " << bytes/1024 << " with chunksize (KB): " << chunksize/1024 << std::endl;
	std::cout << "Encoder Runs: " << encoderruns << std::endl;
	std::cout << "Packet losses per encode: ";
	for (int i = 0; i < pklosscnt; i++)
		std::cout << packetlosses[i]*100 << "% ";
	std::cout << std::endl;
	
	void* message = GetRandomData(bytes);
	void* encodeBuffer = malloc((uint64_t)bytes*redundancy + 200);
	void* decodeBuffer = malloc((uint64_t)bytes);
	long long int message_integrity = integrity(message, bytes);
	std::cout << "Message integrity prior to run: " << message_integrity << std::endl;

	auto encoder_object_creation_start = std::chrono::high_resolution_clock::now();
	WirehairCodec encoder = wirehair_encoder_create(nullptr, message, bytes, chunksize);
	if (!encoder) {
		std::cout << "Error! Cannot create wirehair encoder object" <<std::endl;
		free(message);
		free(encodeBuffer);
		free(decodeBuffer);
		return;
	}
	auto encoder_object_creation_end = std::chrono::high_resolution_clock::now();

	int total_generate = (int)((redundancy * bytes) / chunksize);

	std::cout << "Redundancy selected: " << redundancy << std::endl;
	std::cout << "No. of pure message chunks: " << bytes/chunksize << std::endl; 
	std::cout << "No. of encoded chunks to generate: " << total_generate << std::endl;

	for (int i = 0; i < encoderruns; i++) {
		std::cout << ">> EncoderRun " << i << std::endl;
		uint32_t offset = 0, tempoffset = 0;
		std::vector<uint32_t> encChunkSizes;

		auto encoder_start = std::chrono::high_resolution_clock::now();
		for (int j = 0; j < total_generate; j++) {
			WirehairResult encodeResult = wirehair_encode(encoder, j, encodeBuffer+offset, chunksize, &tempoffset);
			if (encodeResult != Wirehair_Success) {
				std::cout << "Wirehair encoding has failed!" <<std::endl;
				exit(1);
			}
			offset += chunksize;
			//std::cout << (tempoffset==chunksize?"= ":"< ") ;
			encChunkSizes.push_back(tempoffset);
		}
		//std::cout << std::endl;
		
		auto encoder_end = std::chrono::high_resolution_clock::now();

		std::cout << "Encode timelines: ";
		std::cout << "excl obj creation: " << std::chrono::duration_cast<std::chrono::microseconds>(encoder_end - encoder_start).count() << " microsecs, ";
		std::cout << "incl obj creation: " << std::chrono::duration_cast<std::chrono::microseconds>(encoder_end - encoder_start).count() 
																				+ std::chrono::duration_cast<std::chrono::microseconds>(encoder_object_creation_end - encoder_object_creation_start).count() << " microsecs" << std::endl;
		for (int j = 0; j < pklosscnt; j++) {
			std::cout << ">>>> DecoderRun " << j << " for " << packetlosses[j]*100 << "% packet loss; ";
			std::cout << (int)(total_generate*(1.0 - packetlosses[j])) << " of " <<total_generate << " recieved";
			std::unordered_set<int> chunkset = pickSet(total_generate, (int)total_generate*(1.0 - packetlosses[j]));
		
			auto decoder_object_creation_start = std::chrono::high_resolution_clock::now();
			
			WirehairCodec decoder = wirehair_decoder_create(nullptr, bytes, chunksize);
			if (!decoder) {
				std::cout << "Error! Cannot create wirehair decoder object" << std::endl;
				free(message);
				free(encodeBuffer);
				free(decodeBuffer);
				wirehair_free(encoder);
				return;
			}
			auto decoder_object_creation_end = std::chrono::high_resolution_clock::now();

			std::unordered_set<int>::iterator it = chunkset.begin();
			int numChunksNeeded = 0;
			WirehairResult decodeResult = Wirehair_NeedMore;
			
			do {
				decodeResult = wirehair_decode(decoder, *it, encodeBuffer+(*it * chunksize), encChunkSizes[*it]);
				it++;
			} while(decodeResult == Wirehair_NeedMore);
			
			if (decodeResult != Wirehair_Success) {
				std::cout << "Wirehair decoding was unsuccessful" << std::endl;
				continue;
			}
			auto decoder_population_end = std::chrono::high_resolution_clock::now();
			
			decodeResult = wirehair_recover(decoder, decodeBuffer, bytes);

			if (decodeResult != Wirehair_Success) {
				std::cout << "Wirehair recovery was unsuccessful" << std::endl;
				continue;
			}
			auto decoder_retrieval = std::chrono::high_resolution_clock::now();

			long long int recoveryIntegrity = integrity(decodeBuffer, bytes);
			if (recoveryIntegrity == message_integrity)
				std::cout << " Message recovery integrity verified!" << std::endl;
			else
				std::cout << " Message recovery integrity verification failed!" << std::endl;	
		
			std::cout << "     Decode timelines: ";
			std::cout << "end to end: " << std::chrono::duration_cast<std::chrono::microseconds>(decoder_retrieval - decoder_object_creation_start).count() << " microsecs; ";
			std::cout << "dec object creation: " << std::chrono::duration_cast<std::chrono::microseconds>(decoder_object_creation_end - decoder_object_creation_start).count() << " microsecs; ";
			std::cout << "dec populate: " << std::chrono::duration_cast<std::chrono::microseconds>(decoder_population_end - decoder_object_creation_end).count() << " microsecs; ";
			std::cout << "dec buffer retrieval: " << std::chrono::duration_cast<std::chrono::microseconds>(decoder_retrieval - decoder_population_end).count() << " microsecs" << std::endl;
			
		}
	}

	free(message);
	std::cout << "--------------------------------------------------------- " << std::endl;
}

int main() {
	std::cout << "WIREHAIR Erasure Channel Benchmarks for Marlin Protocol" << std::endl;
	
	// Wirehair initialisation
	if (wirehair_init() != Wirehair_Success) {
		std::cout << "Wirehair init unsuccessful" << std::endl;
		exit(1);
	}

  float packetlosses[] = {0.02, 0.08, 0.14, 0.25, 0.35, 0.40};

	std::cout << "RUNNING GENERAL PERFORMANCE BENCHMARKS" << std::endl;
	BenchmarkWirehair(50*1024*1024, 2*1024*1024, 1.6, 2, packetlosses, 5);	

	std::cout << "RUNNING BITCOING AND ETHEREUM SIMULATIONS FOR MARLIN" << std::endl;
	BenchmarkWirehair(30*1024, 1*1024, 1.8, 3, packetlosses, 5);
	BenchmarkWirehair(1024*1024, 1024, 1.6, 2, packetlosses, 5);

	return 0;
}
