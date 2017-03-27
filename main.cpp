#include <iostream>
#include "lib\threadpool.h"

#include <bitset>
#include <algorithm>
#include <iterator>
#include <random>
#include <functional>

int mod(int a, int b)
{
	if (b < 0)
		return mod(a, -b);
	int ret = a % b;
	if (ret < 0)
		ret += b;
	return ret;
}

int modTimes(int a, int b, int modulus) {
	return mod(a*b, modulus);
}

const int numberIntSize = sizeof(int)*CHAR_BIT;
int firstBitFromLeft(std::bitset<numberIntSize> binary) { 
	for (auto i = binary.size()-1; i != -1; i--) {
		if (binary[i] == true) return static_cast<int>(i);
	}
	return -1; 
}

int firstBitFromRight(std::bitset<numberIntSize> binary) {
	for (int i = 0; i != binary.size(); i++) {
		if (binary[i] == true) return static_cast<int>(i);
	}
	return -1;
}

bool MillerRabin(int power) {
	auto die = std::bind(std::uniform_int_distribution<>{1, power-1}, std::default_random_engine{});
	int x = die();
	std::bitset < numberIntSize > binary(power-1);

	int r = firstBitFromRight(binary);

	int temp = 1;
	int i = firstBitFromLeft(binary);

	for (; i != -1; i--) {
		if (binary[i] == true) {
			temp = modTimes( modTimes(x, temp, power), temp, power);
			if (i == r && (temp == 1 || temp == mod(-1, power) ) ) return true;
		}
		else {
			temp = modTimes(temp, temp, power);
			if (r > i && temp == mod(-1, power) ) return true;
		}
	}
	return false;
}

bool isLikelyPrime(int workitem) {
	bool passed = true;
	int i = 20;
	while (i--) {
		passed = (passed && MillerRabin(workitem));
	}
	return passed;
}

void ProcessPrime(Threadpool<int>& pool, int workitem)
{
	if (workitem < 300)
	{
		int t = workitem + (2 * pool.GetThreadCount());
		pool.EnqueueWorkItem(std::move(t));
		if (isLikelyPrime(workitem)) {
			pool.EnqueueResult(std::move(workitem));
		}
	}
}

void ProcessResult(Threadpool<int>& pool, int result)
{
	std::cout << result << std::endl;
}

Threadpool<int> tp(std::thread::hardware_concurrency(), ProcessPrime, ProcessResult);


int main(char* argv, int argc)
{
	for (int i = 0, j=101; i < tp.GetThreadCount(); i++, j+=2)
	{
		int t = j;
		tp.EnqueueWorkItem(std::move(t));
	}

	std::this_thread::sleep_for(std::chrono::seconds(10));
	tp.Stop();
	std::this_thread::sleep_for(std::chrono::seconds(60));
	

	return 0;
}