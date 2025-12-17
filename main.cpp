#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>

//labs
#include "monitor.h"
#include "connect4.h"

using namespace std;


int main(int argc, char* argv[]) {
	int Lab = 1; //Choose lab number
	switch (Lab) {
		case(1): Test1();
		case(2): Test2(argc, argv);
		case(3): Test1();
		case(4): Test1();
		default: cout << "Lab not found";
	}
		

}