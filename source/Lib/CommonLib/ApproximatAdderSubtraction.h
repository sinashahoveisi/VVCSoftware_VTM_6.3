#ifndef APPROXIMATADDERSUBTRACTION_H   // To make sure you don't declare the function more than once by including the header multiple times.
#define APPROXIMATADDERSUBTRACTION_H


#include <iostream>
#include<algorithm>
#include <string>
#include <cmath>
#include <chrono>

using std::chrono::duration;
using std::chrono::duration_cast;
using std::chrono::high_resolution_clock;
using std::chrono::nanoseconds;

using namespace std;

extern long long int accurateTime;
extern long long int approximateTime;

struct adderOutput
{
  int sum;
  int carry;
};

struct binaryOutput
{
  string sum;
  string carry;
};

int XOR(int bitOne, int bitTwo);
int AND(int bitOne, int bitTwo);
int OR(int bitOne, int bitTwo);
int NOT_int(int bit);
char NOT_Bit(char bit);
string NOT(string binaryNumber);
string decimalToBinary(int number,int numberBits);
int    binaryToDecimal(string binaryNumber);
adderOutput half_adder(int bitOne, int bitTwo);

adderOutput full_adder(int bitOne, int bitTwo, int carry);
void        balanceInputs(string &binaryOne, string &binaryTwo, int approximateCount);
binaryOutput accurate_binary_adder(string binaryOne, string binaryTwo, bool cOut);
binaryOutput approximate_binary_adder(string binaryOne, string binaryTwo, bool cOut, int approximateCount);
string       complement(string binaryNumber, char type);
binaryOutput accurate_binary_subtraction(string binaryOne, string binaryTwo);
binaryOutput approximate_binary_subtraction(string binaryOne, string binaryTwo, int approximateCount);
extern int   calculateSum(int numOne, int numTwo, int numberBits, int approximateCount);

string approximate_binary_adder_8bit_type1(string binaryOne, string binaryTwo, bool cOut);
string approximate_binary_subtraction_8bit_type1(string binaryOne, string binaryTwo);
extern int approximate8BitSumType1(int numOne, int numTwo);

string     approximate_binary_adder_8bit_type2(string binaryOne, string binaryTwo, bool cOut);
string     approximate_binary_subtraction_8bit_type2(string binaryOne, string binaryTwo);
extern int approximate8BitSumType2(int numOne, int numTwo);

string     approximate_binary_adder_8bit_type3(string binaryOne, string binaryTwo, bool cOut);
string     approximate_binary_subtraction_8bit_type3(string binaryOne, string binaryTwo);
extern int approximate8BitSumType3(int numOne, int numTwo);

string     approximate_binary_adder_8bit_type4(string binaryOne, string binaryTwo, bool cOut);
string     approximate_binary_subtraction_8bit_type4(string binaryOne, string binaryTwo);
extern int approximate8BitSumType4(int numOne, int numTwo);

string     approximate_binary_adder_8bit_type5(string binaryOne, string binaryTwo, bool cOut);
string     approximate_binary_subtraction_8bit_type5(string binaryOne, string binaryTwo);
extern int approximate8BitSumType5(int numOne, int numTwo);

string     approximate_binary_adder_8bit_M45_P35_A7(string binaryOne, string binaryTwo, bool cOut);
string     approximate_binary_subtraction_8bit_M45_P35_A7(string binaryOne, string binaryTwo);
extern int approximate8BitSumM45P35A7(int numOne, int numTwo);

string     approximate_binary_adder_8bit_M45_P30_A6(string binaryOne, string binaryTwo, bool cOut);
string     approximate_binary_subtraction_8bit_M45_P30_A6(string binaryOne, string binaryTwo);
extern int approximate8BitSumM45P30A6(int numOne, int numTwo);

string     approximate_binary_adder_8bit_M60_P25_A6(string binaryOne, string binaryTwo, bool cOut);
string     approximate_binary_subtraction_8bit_M60_P25_A6(string binaryOne, string binaryTwo);
extern int approximate8BitSumM60P25A6(int numOne, int numTwo);

string     approximate_binary_adder_8bit_M95_P10_A5(string binaryOne, string binaryTwo, bool cOut);
string     approximate_binary_subtraction_8bit_M95_P10_A5(string binaryOne, string binaryTwo);
extern int approximate8BitSumM95P10A5(int numOne, int numTwo);


#endif