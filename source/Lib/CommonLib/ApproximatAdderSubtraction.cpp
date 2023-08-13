#include "ApproximatAdderSubtraction.h"

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

long long int accurateTime    = 0;
long long int approximateTime = 0;


int charToInt(char c)
{
  return c - '0';
}

int XOR(int bitOne, int bitTwo)
{
  return bitOne ^ bitTwo;
}

int AND(int bitOne, int bitTwo)
{
  return bitOne & bitTwo;
}

int OR(int bitOne, int bitTwo)
{
  return bitOne | bitTwo;
}

string NOT(string binaryNumber)
{
  string result = "";
  for (int i = 0; i < binaryNumber.length(); i++)
  {
    if (binaryNumber[i] == '0')
      result = result + "1";
    else
      result = result + "0";
  }
  return result;
}

int NOT_int(int bit)
{
  if (bit == 0)
    return 1;
  else
    return 0;
}

char NOT_Bit(char bit)
{
  if (bit == '0')
    return '1';
  else
    return '0';
}

string decimalToBinary(int number ,int numberBits)
{
  string output = "";
  while (number > 0)
  {
    output = to_string(number % 2) + output;
    number = number / 2;
  }
  if (output == "") {
    output = "0";
  }
  if (output.length() > numberBits) {
    output = output.substr(output.length() - numberBits);
  }
  else if (output.length() < numberBits)
  {
    while (numberBits - output.length() != 0)
    {
      output = '0' + output;
    }
  }
  return output;
}

int binaryToDecimal(string binaryNumber)
{
  int result = 0;
  int length = static_cast<int>(binaryNumber.length());
  for(int i = length - 1; i >= 0; i--)
  {
    result += charToInt(binaryNumber[i]) * static_cast<int>(std::pow(2, length - i - 1));
  }
  return result;
}

adderOutput half_adder(int bitOne, int bitTwo)
{
  return { XOR(bitOne, bitTwo), AND(bitOne, bitTwo) };
}

adderOutput full_adder(int bitOne, int bitTwo, int carry)
{
  adderOutput halfOutput  = half_adder(bitOne, bitTwo);
  adderOutput finalOutput = half_adder(halfOutput.sum, carry);
  return { finalOutput.sum, OR(halfOutput.carry, finalOutput.carry) };
}

void balanceInputs(string &binaryOne, string &binaryTwo, int approximateCount)
{
  int len = static_cast<int>(binaryOne.length()) - static_cast<int> (binaryTwo.length());
  if (len > 0)
  {
    for (int i = 0; i < len; i++)
    {
      binaryTwo = "0" + binaryTwo;
    }
  }
  else if (len < 0)
  {
    for (int i = 0; i < len * -1; i++)
    {
      binaryOne = "0" + binaryOne;
    }
  }
  if (binaryOne.length() < approximateCount)
  {
    for (int i = 0; i < approximateCount - binaryOne.length(); i++)
    {
      binaryOne = "0" + binaryOne;
    }
  }
  if (binaryTwo.length() < approximateCount)
  {
    for (int i = 0; i < approximateCount - binaryTwo.length(); i++)
    {
      binaryTwo = "0" + binaryTwo;
    }
  }
}

binaryOutput accurate_binary_adder(string binaryOne, string binaryTwo, bool cOut)
{
  balanceInputs(binaryOne, binaryTwo, 0);
  binaryOutput finalResult = { "", "" };
  adderOutput  halfResult  = { 0, 0 };

  for (int index = static_cast<int>(binaryOne.length()) - 1; index >= 0; index--)
  {
    //auto start = high_resolution_clock::now();
    halfResult        = full_adder(charToInt(binaryOne[index]), charToInt(binaryTwo[index]), halfResult.carry);
    finalResult.carry = to_string(halfResult.carry) + finalResult.carry;
    finalResult.sum   = to_string(halfResult.sum) + finalResult.sum;
    if (index == 0 && cOut == true)
      finalResult.sum = to_string(halfResult.carry) + finalResult.sum;

    //auto stop     = high_resolution_clock::now();
    //auto duration = duration_cast<nanoseconds>(stop - start);
    //accurateTime  = accurateTime + duration.count();
  }
  return finalResult;
}

binaryOutput approximate_binary_adder(string binaryOne, string binaryTwo, bool cOut, int approximateCount)
{
  balanceInputs(binaryOne, binaryTwo, approximateCount);
  binaryOutput finalResult = { "", "" };
  int length               = static_cast<int>(binaryOne.length());
  approximateCount         = min(length, approximateCount);

  for (int index = length - 1; length - approximateCount <= index; index--)
  {
    //auto start = high_resolution_clock::now();
    finalResult.carry = "X" + finalResult.carry;
    finalResult.sum   = to_string(OR(charToInt(binaryOne[index]), charToInt(binaryTwo[index]))) + finalResult.sum;
    //auto stop         = high_resolution_clock::now();
    //auto duration     = duration_cast<nanoseconds>(stop - start);
    //approximateTime   = approximateTime + duration.count();
  }
  if (length > approximateCount) {
    adderOutput halfResult = { 0, AND(charToInt(binaryOne[length - approximateCount]),
                                      charToInt(binaryTwo[length - approximateCount])) };
    for (int index = length - approximateCount - 1; index >= 0; index--)
    {
      //auto start        = high_resolution_clock::now();
      halfResult        = full_adder(charToInt(binaryOne[index]), charToInt(binaryTwo[index]), halfResult.carry);
      finalResult.carry = to_string(halfResult.carry) + finalResult.carry;
      finalResult.sum   = to_string(halfResult.sum) + finalResult.sum;
      if (index == 0 && cOut == true)
        finalResult.sum = to_string(halfResult.carry) + finalResult.sum;

      //auto stop       = high_resolution_clock::now();
      //auto duration   = duration_cast<nanoseconds>(stop - start);
      //approximateTime = approximateTime + duration.count();
    }
  }
  return finalResult;
}

string complement(string binaryNumber, char type, int approximateCount)
{
  string result = NOT(binaryNumber);
  if (type == '2') {
    if (approximateCount > 0)
      return approximate_binary_adder(result, "1", false, approximateCount).sum;
    return accurate_binary_adder(result, "1", false).sum;
  }
  return result;
}


/// LOA ////////////////////////////////////////////////////

binaryOutput accurate_binary_subtraction(string binaryOne, string binaryTwo)
{
  balanceInputs(binaryOne, binaryTwo, 0);
  return accurate_binary_adder(binaryOne, complement(binaryTwo, '2', 0), false);
}

binaryOutput approximate_binary_subtraction(string binaryOne, string binaryTwo, int approximateCount)
{
  balanceInputs(binaryOne, binaryTwo, approximateCount);
  return approximate_binary_adder(binaryOne, complement(binaryTwo, '2', approximateCount), false, approximateCount);
}

int calculateSum(int numOne, int numTwo,int numberBits, int approximateCount)
{
  string       operation = "add";
  int          sign      = 1;
  binaryOutput result;
  if (abs(numOne) > abs(numTwo))
  {
    sign = numOne / abs(numOne);
  }
  else if (abs(numOne) < abs(numTwo))
  {
    sign = numTwo / abs(numTwo);
  }
  if (numOne * numTwo < 0)
  {
    operation = "subtraction";
  }
  string maxBinary = decimalToBinary(max(abs(numOne), abs(numTwo)), numberBits);
  string minBinary = decimalToBinary(min(abs(numOne), abs(numTwo)), numberBits);

  if (approximateCount > 0)
  {
    if (operation == "add")
      result = approximate_binary_adder(maxBinary, minBinary, true, approximateCount);
    else
      result = approximate_binary_subtraction(maxBinary, minBinary, approximateCount);
  }
  else
  {
    if (operation == "add")
      result = accurate_binary_adder(maxBinary, minBinary, true);
    else
      result = accurate_binary_subtraction(maxBinary, minBinary);
  }
  //cout << endl << numOne << " , " << numTwo << " , " << maxBinary << " , " << minBinary <<  " , " << " , " << operation << " , " << result.sum << " , " << sign * binaryToDecimal(result.sum);
  return sign * binaryToDecimal(result.sum);
}



//// Binary Adder 8 bit Version 1 /////////////////////////////////////


string approximate_binary_adder_8bit_type1(string binaryOne, string binaryTwo, bool cOut) {
  balanceInputs(binaryOne, binaryTwo, 0);
  //cout << binaryOne << endl << binaryTwo << endl;
  string finalOutput = to_string(OR(charToInt(binaryOne[7]), charToInt(binaryTwo[7])));
  finalOutput = binaryTwo[6] + finalOutput;
  finalOutput = NOT_Bit(binaryTwo[5]) + finalOutput;
  finalOutput = NOT_Bit(binaryTwo[4]) + finalOutput;
  finalOutput = binaryTwo[3] + finalOutput;
  finalOutput = to_string(XOR(charToInt(binaryOne[2]), charToInt(binaryTwo[2]))) + finalOutput;
  finalOutput = to_string(OR(charToInt(binaryOne[1]), charToInt(binaryTwo[1]))) + finalOutput;
  finalOutput = to_string(XOR(charToInt(binaryOne[0]), charToInt(binaryTwo[0]))) + finalOutput;
  if (cOut == true) {
    finalOutput = to_string(AND(charToInt(binaryOne[0]), charToInt(binaryTwo[0]))) + finalOutput;
  }
  return finalOutput;
}

string approximate_binary_subtraction_8bit_type1(string binaryOne, string binaryTwo)
{
  balanceInputs(binaryOne, binaryTwo, 0);
  return approximate_binary_adder_8bit_type1(binaryOne, complement(binaryTwo, '2', 0), false);
}

int approximate8BitSumType1(int numOne, int numTwo)
{
  string       operation = "add";
  int          sign = 1;
  string result;
  if (abs(numOne) > abs(numTwo))
  {
    sign = numOne / abs(numOne);
  }
  else if (abs(numOne) < abs(numTwo))
  {
    sign = numTwo / abs(numTwo);
  }
  if (numOne * numTwo < 0)
  {
    operation = "subtraction";
  }
  

  if (operation == "add")
    result = approximate_binary_adder_8bit_type1(decimalToBinary(abs(numOne), 8), decimalToBinary(abs(numTwo), 8), true);
  else {
    string maxBinary = decimalToBinary(max(abs(numOne), abs(numTwo)), 8);
    string minBinary = decimalToBinary(min(abs(numOne), abs(numTwo)), 8);
    result = approximate_binary_subtraction_8bit_type1(maxBinary, minBinary);
  }

  return sign * binaryToDecimal(result);
}



//// Binary Adder 8 bit Version 2 /////////////////////////////////////


string approximate_binary_adder_8bit_type2(string binaryOne, string binaryTwo, bool cOut) {
  balanceInputs(binaryOne, binaryTwo, 0);
  string finalOutput = "";
  finalOutput = binaryOne[7] + finalOutput;
  finalOutput = to_string(NOT_int(OR(charToInt(binaryTwo[7]), charToInt(binaryTwo[6])))) + finalOutput;
  finalOutput = to_string(XOR(charToInt(binaryOne[5]), charToInt(binaryTwo[5]))) + finalOutput;
  finalOutput = to_string(XOR(charToInt(binaryOne[4]), charToInt(binaryTwo[4]))) + finalOutput;
  finalOutput = binaryOne[3] + finalOutput;
  finalOutput = to_string(NOT_int(OR(NOT_int(OR(charToInt(binaryOne[3]), charToInt(binaryOne[2]))), charToInt(binaryTwo[2])))) + finalOutput;
  finalOutput = to_string(AND(charToInt(binaryOne[1]), charToInt(binaryTwo[1]))) + finalOutput;
  finalOutput = to_string(NOT_int(XOR(charToInt(binaryOne[0]), charToInt(binaryTwo[0])))) + finalOutput;
  if (cOut == true) {
    finalOutput = to_string(OR(charToInt(binaryOne[0]), charToInt(binaryTwo[0]))) + finalOutput;
  }
  return finalOutput;
}

string approximate_binary_subtraction_8bit_type2(string binaryOne, string binaryTwo)
{
  balanceInputs(binaryOne, binaryTwo, 0);
  return approximate_binary_adder_8bit_type2(binaryOne, complement(binaryTwo, '2', 0), false);
}

int approximate8BitSumType2(int numOne, int numTwo)
{
  string       operation = "add";
  int          sign = 1;
  string result;
  if (abs(numOne) > abs(numTwo))
  {
    sign = numOne / abs(numOne);
  }
  else if (abs(numOne) < abs(numTwo))
  {
    sign = numTwo / abs(numTwo);
  }
  if (numOne * numTwo < 0)
  {
    operation = "subtraction";
  }


  if (operation == "add")
    result = approximate_binary_adder_8bit_type2(decimalToBinary(abs(numOne), 8), decimalToBinary(abs(numTwo), 8), true);
  else {
    string maxBinary = decimalToBinary(max(abs(numOne), abs(numTwo)), 8);
    string minBinary = decimalToBinary(min(abs(numOne), abs(numTwo)), 8);
    result = approximate_binary_subtraction_8bit_type2(maxBinary, minBinary);
  }

  return sign * binaryToDecimal(result);
}



//// Binary Adder 8 bit Version 3 /////////////////////////////////////


string approximate_binary_adder_8bit_type3(string binaryOne, string binaryTwo, bool cOut) {
  balanceInputs(binaryOne, binaryTwo, 0);
  string finalOutput = "";
  finalOutput = NOT_Bit(binaryOne[7]) + finalOutput;
  finalOutput = to_string(NOT_int(XOR(charToInt(binaryOne[6]), charToInt(binaryTwo[6])))) + finalOutput;
  finalOutput = NOT_Bit(binaryOne[5]) + finalOutput;
  finalOutput = binaryTwo[4] + finalOutput;
  finalOutput = binaryOne[3] + finalOutput;
  finalOutput = to_string(NOT_int(OR(NOT_int(OR(charToInt(binaryOne[3]), charToInt(binaryOne[2]))), charToInt(binaryTwo[2])))) + finalOutput;
  finalOutput = to_string(AND(charToInt(binaryOne[1]), charToInt(binaryTwo[1]))) + finalOutput;
  finalOutput = to_string(NOT_int(XOR(charToInt(binaryOne[0]), charToInt(binaryTwo[0])))) + finalOutput;
  if (cOut == true) {
    finalOutput = to_string(OR(charToInt(binaryOne[0]), charToInt(binaryTwo[0]))) + finalOutput;
  }
  return finalOutput;
}

string approximate_binary_subtraction_8bit_type3(string binaryOne, string binaryTwo)
{
  balanceInputs(binaryOne, binaryTwo, 0);
  return approximate_binary_adder_8bit_type3(binaryOne, complement(binaryTwo, '2', 0), false);
}

int approximate8BitSumType3(int numOne, int numTwo)
{
  string       operation = "add";
  int          sign = 1;
  string result;
  if (abs(numOne) > abs(numTwo))
  {
    sign = numOne / abs(numOne);
  }
  else if (abs(numOne) < abs(numTwo))
  {
    sign = numTwo / abs(numTwo);
  }
  if (numOne * numTwo < 0)
  {
    operation = "subtraction";
  }


  if (operation == "add")
    result = approximate_binary_adder_8bit_type3(decimalToBinary(abs(numOne), 8), decimalToBinary(abs(numTwo), 8), true);
  else {
    string maxBinary = decimalToBinary(max(abs(numOne), abs(numTwo)), 8);
    string minBinary = decimalToBinary(min(abs(numOne), abs(numTwo)), 8);
    result = approximate_binary_subtraction_8bit_type3(maxBinary, minBinary);
  }

  return sign * binaryToDecimal(result);
}




//// Binary Adder 8 bit Version 4 /////////////////////////////////////


string approximate_binary_adder_8bit_type4(string binaryOne, string binaryTwo, bool cOut) {
  balanceInputs(binaryOne, binaryTwo, 0);
  string finalOutput = "";
  finalOutput = binaryTwo[7] + finalOutput;
  finalOutput = binaryTwo[6] + finalOutput;
  finalOutput = to_string(NOT_int(AND(NOT_int(AND(charToInt(binaryOne[5]), charToInt(binaryTwo[5]))) , NOT_int(AND(charToInt(NOT_int(binaryOne[6])), charToInt(binaryOne[5])))))) + finalOutput;
  finalOutput = binaryOne[4] + finalOutput;
  finalOutput = binaryOne[3] + finalOutput;
  finalOutput = to_string(NOT_int(OR(NOT_int(OR(charToInt(binaryOne[3]), charToInt(binaryOne[2]))), charToInt(binaryTwo[2])))) + finalOutput;
  finalOutput = to_string(AND(charToInt(binaryOne[1]), charToInt(binaryTwo[1]))) + finalOutput;
  finalOutput = to_string(NOT_int(XOR(charToInt(binaryOne[0]), charToInt(binaryTwo[0])))) + finalOutput;
  if (cOut == true) {
    finalOutput = to_string(OR(charToInt(binaryOne[0]), charToInt(binaryTwo[0]))) + finalOutput;
  }
  return finalOutput;
}

string approximate_binary_subtraction_8bit_type4(string binaryOne, string binaryTwo)
{
  balanceInputs(binaryOne, binaryTwo, 0);
  return approximate_binary_adder_8bit_type4(binaryOne, complement(binaryTwo, '2', 0), false);
}

int approximate8BitSumType4(int numOne, int numTwo)
{
  string       operation = "add";
  int          sign = 1;
  string result;
  if (abs(numOne) > abs(numTwo))
  {
    sign = numOne / abs(numOne);
  }
  else if (abs(numOne) < abs(numTwo))
  {
    sign = numTwo / abs(numTwo);
  }
  if (numOne * numTwo < 0)
  {
    operation = "subtraction";
  }


  if (operation == "add")
    result = approximate_binary_adder_8bit_type4(decimalToBinary(abs(numOne), 8), decimalToBinary(abs(numTwo), 8), true);
  else {
    string maxBinary = decimalToBinary(max(abs(numOne), abs(numTwo)), 8);
    string minBinary = decimalToBinary(min(abs(numOne), abs(numTwo)), 8);
    result = approximate_binary_subtraction_8bit_type4(maxBinary, minBinary);
  }

  return sign * binaryToDecimal(result);
}







//// Binary Adder 8 bit Version 5 /////////////////////////////////////


string approximate_binary_adder_8bit_type5(string binaryOne, string binaryTwo, bool cOut) {
  balanceInputs(binaryOne, binaryTwo, 0);
  string finalOutput = "0";
  finalOutput = binaryOne[6] + finalOutput;
  finalOutput = to_string(OR(charToInt(NOT_Bit(binaryOne[5])), charToInt(binaryTwo[5]))) + finalOutput;
  finalOutput = binaryOne[4] + finalOutput;
  finalOutput = binaryOne[3] + finalOutput;
  finalOutput = to_string(NOT_int(OR(NOT_int(OR(charToInt(binaryOne[3]), charToInt(binaryOne[2]))), charToInt(binaryTwo[2])))) + finalOutput;
  finalOutput = to_string(AND(charToInt(binaryOne[1]), charToInt(binaryTwo[1]))) + finalOutput;
  finalOutput = to_string(NOT_int(XOR(charToInt(binaryOne[0]), charToInt(binaryTwo[0])))) + finalOutput;
  if (cOut == true) {
    finalOutput = to_string(OR(charToInt(binaryOne[0]), charToInt(binaryTwo[0]))) + finalOutput;
  }
  return finalOutput;
}

string approximate_binary_subtraction_8bit_type5(string binaryOne, string binaryTwo)
{
  balanceInputs(binaryOne, binaryTwo, 0);
  return approximate_binary_adder_8bit_type5(binaryOne, complement(binaryTwo, '2', 0), false);
}

int approximate8BitSumType5(int numOne, int numTwo)
{
  string       operation = "add";
  int          sign = 1;
  string result;
  if (abs(numOne) > abs(numTwo))
  {
    sign = numOne / abs(numOne);
  }
  else if (abs(numOne) < abs(numTwo))
  {
    sign = numTwo / abs(numTwo);
  }
  if (numOne * numTwo < 0)
  {
    operation = "subtraction";
  }


  if (operation == "add")
    result = approximate_binary_adder_8bit_type5(decimalToBinary(abs(numOne), 8), decimalToBinary(abs(numTwo), 8), true);
  else {
    string maxBinary = decimalToBinary(max(abs(numOne), abs(numTwo)), 8);
    string minBinary = decimalToBinary(min(abs(numOne), abs(numTwo)), 8);
    result = approximate_binary_subtraction_8bit_type5(maxBinary, minBinary);
  }

  return sign * binaryToDecimal(result);
}



//// Binary Adder 8 bit Version 45_35_7 /////////////////////////////////////


string approximate_binary_adder_8bit_M45_P35_A7(string binaryOne, string binaryTwo, bool cOut) {
  balanceInputs(binaryOne, binaryTwo, 0);
  string finalOutput = "";
  finalOutput = binaryOne[7] + finalOutput;
  finalOutput = to_string(NOT_int(AND(charToInt(binaryOne[6]), charToInt(binaryTwo[6])))) + finalOutput;
  finalOutput = binaryTwo[5] + finalOutput;
  finalOutput = "0" + finalOutput;
  finalOutput = binaryOne[3] + finalOutput;
  finalOutput = to_string(NOT_int(OR(NOT_int(OR(charToInt(binaryOne[3]), charToInt(binaryOne[2]))), charToInt(binaryTwo[2])))) + finalOutput;
  finalOutput = to_string(AND(charToInt(binaryOne[1]), charToInt(binaryTwo[1]))) + finalOutput;
  finalOutput = to_string(NOT_int(XOR(charToInt(binaryOne[0]), charToInt(binaryTwo[0])))) + finalOutput;
  if (cOut == true) {
    finalOutput = to_string(OR(charToInt(binaryOne[0]), charToInt(binaryTwo[0]))) + finalOutput;
  }
  return finalOutput;
}

string approximate_binary_subtraction_8bit_M45_P35_A7(string binaryOne, string binaryTwo)
{
  balanceInputs(binaryOne, binaryTwo, 0);
  return approximate_binary_adder_8bit_M45_P35_A7(binaryOne, complement(binaryTwo, '2', 0), false);
}

int approximate8BitSumM45P35A7(int numOne, int numTwo)
{
  string       operation = "add";
  int          sign = 1;
  string result;
  if (abs(numOne) > abs(numTwo))
  {
    sign = numOne / abs(numOne);
  }
  else if (abs(numOne) < abs(numTwo))
  {
    sign = numTwo / abs(numTwo);
  }
  if (numOne * numTwo < 0)
  {
    operation = "subtraction";
  }


  if (operation == "add")
    result = approximate_binary_adder_8bit_M45_P35_A7(decimalToBinary(abs(numOne), 8), decimalToBinary(abs(numTwo), 8), true);
  else {
    string maxBinary = decimalToBinary(max(abs(numOne), abs(numTwo)), 8);
    string minBinary = decimalToBinary(min(abs(numOne), abs(numTwo)), 8);
    result = approximate_binary_subtraction_8bit_M45_P35_A7(maxBinary, minBinary);
  }

  return sign * binaryToDecimal(result);
}



//// Binary Adder 8 bit Version 45_30_6 /////////////////////////////////////


string approximate_binary_adder_8bit_M45_P30_A6(string binaryOne, string binaryTwo, bool cOut) {
  balanceInputs(binaryOne, binaryTwo, 0);
  string finalOutput = "0";
  finalOutput = binaryOne[6] + finalOutput;
  finalOutput = to_string(NOT_int(OR(NOT_int(AND(charToInt(binaryOne[6]), charToInt(binaryTwo[6]))), charToInt(binaryTwo[5])))) + finalOutput;
  finalOutput = binaryOne[4] + finalOutput;
  finalOutput = to_string(XOR(charToInt(binaryOne[3]), charToInt(binaryTwo[3]))) + finalOutput;
  finalOutput = "00" + finalOutput;
  finalOutput = to_string(NOT_int(XOR(charToInt(binaryOne[0]), charToInt(binaryTwo[0])))) + finalOutput;
  if (cOut == true) {
    finalOutput = to_string(OR(charToInt(binaryOne[0]), charToInt(binaryTwo[0]))) + finalOutput;
  }
  return finalOutput;
}

string approximate_binary_subtraction_8bit_M45_P30_A6(string binaryOne, string binaryTwo)
{
  balanceInputs(binaryOne, binaryTwo, 0);
  return approximate_binary_adder_8bit_M45_P30_A6(binaryOne, complement(binaryTwo, '2', 0), false);
}

int approximate8BitSumM45P30A6(int numOne, int numTwo)
{
  string       operation = "add";
  int          sign = 1;
  string result;
  if (abs(numOne) > abs(numTwo))
  {
    sign = numOne / abs(numOne);
  }
  else if (abs(numOne) < abs(numTwo))
  {
    sign = numTwo / abs(numTwo);
  }
  if (numOne * numTwo < 0)
  {
    operation = "subtraction";
  }


  if (operation == "add")
    result = approximate_binary_adder_8bit_M45_P30_A6(decimalToBinary(abs(numOne), 8), decimalToBinary(abs(numTwo), 8), true);
  else {
    string maxBinary = decimalToBinary(max(abs(numOne), abs(numTwo)), 8);
    string minBinary = decimalToBinary(min(abs(numOne), abs(numTwo)), 8);
    result = approximate_binary_subtraction_8bit_M45_P30_A6(maxBinary, minBinary);
  }

  return sign * binaryToDecimal(result);
}




//// Binary Adder 8 bit Version 60_25_6 /////////////////////////////////////


string approximate_binary_adder_8bit_M60_P25_A6(string binaryOne, string binaryTwo, bool cOut) {
  balanceInputs(binaryOne, binaryTwo, 0);
  string finalOutput = "";
  finalOutput = binaryTwo[7] + finalOutput;
  finalOutput = to_string(NOT_int(OR(charToInt(binaryOne[6]), charToInt(binaryTwo[6])))) + finalOutput;
  finalOutput = binaryOne[5] + finalOutput;
  finalOutput = "0" + finalOutput;
  finalOutput = binaryTwo[3] + finalOutput;
  finalOutput = to_string(NOT_int(AND(NOT_int(OR(charToInt(binaryOne[3]), charToInt(binaryTwo[3]))), charToInt(binaryTwo[2])))) + finalOutput;
  finalOutput = binaryOne[1] + finalOutput;
  finalOutput = to_string(XOR(charToInt(binaryOne[0]), charToInt(binaryTwo[0]))) + finalOutput;
  if (cOut == true) {
    finalOutput = to_string(AND(charToInt(binaryOne[0]), charToInt(binaryTwo[0]))) + finalOutput;
  }
  return finalOutput;
}

string approximate_binary_subtraction_8bit_M60_P25_A6(string binaryOne, string binaryTwo)
{
  balanceInputs(binaryOne, binaryTwo, 0);
  return approximate_binary_adder_8bit_M60_P25_A6(binaryOne, complement(binaryTwo, '2', 0), false);
}

int approximate8BitSumM60P25A6(int numOne, int numTwo)
{
  string       operation = "add";
  int          sign = 1;
  string result;
  if (abs(numOne) > abs(numTwo))
  {
    sign = numOne / abs(numOne);
  }
  else if (abs(numOne) < abs(numTwo))
  {
    sign = numTwo / abs(numTwo);
  }
  if (numOne * numTwo < 0)
  {
    operation = "subtraction";
  }


  if (operation == "add")
    result = approximate_binary_adder_8bit_M60_P25_A6(decimalToBinary(abs(numOne), 8), decimalToBinary(abs(numTwo), 8), true);
  else {
    string maxBinary = decimalToBinary(max(abs(numOne), abs(numTwo)), 8);
    string minBinary = decimalToBinary(min(abs(numOne), abs(numTwo)), 8);
    result = approximate_binary_subtraction_8bit_M60_P25_A6(maxBinary, minBinary);
  }

  return sign * binaryToDecimal(result);
}





//// Binary Adder 8 bit Version 95_10_5 /////////////////////////////////////


string approximate_binary_adder_8bit_M95_P10_A5(string binaryOne, string binaryTwo, bool cOut)
{
  balanceInputs(binaryOne, binaryTwo, 0);
  string finalOutput = "";
  finalOutput        = binaryOne[7] + finalOutput;
  finalOutput        = "0" + finalOutput;
  finalOutput        = NOT_Bit(binaryOne[5]) + finalOutput;
  finalOutput        = binaryOne[4] + finalOutput;
  finalOutput        = NOT_Bit(binaryOne[3]) + finalOutput;
  finalOutput        = "0" + finalOutput;
  finalOutput        = binaryOne[1] + finalOutput;
  finalOutput        = NOT_Bit(binaryOne[0]) + finalOutput;
  if (cOut == true)
  {
    finalOutput = binaryOne[0] + finalOutput;
  }
  return finalOutput;
}

string approximate_binary_subtraction_8bit_M95_P10_A5(string binaryOne, string binaryTwo)
{
  balanceInputs(binaryOne, binaryTwo, 0);
  return approximate_binary_adder_8bit_M95_P10_A5(binaryOne, complement(binaryTwo, '2', 0), false);
}

int approximate8BitSumM95P10A5(int numOne, int numTwo)
{
  string       operation = "add";
  int          sign = 1;
  string result;
  if (abs(numOne) > abs(numTwo))
  {
    sign = numOne / abs(numOne);
  }
  else if (abs(numOne) < abs(numTwo))
  {
    sign = numTwo / abs(numTwo);
  }
  if (numOne * numTwo < 0)
  {
    operation = "subtraction";
  }


  if (operation == "add")
    result = approximate_binary_adder_8bit_M95_P10_A5(decimalToBinary(abs(numOne), 8), decimalToBinary(abs(numTwo), 8), true);
  else {
    string maxBinary = decimalToBinary(max(abs(numOne), abs(numTwo)), 8);
    string minBinary = decimalToBinary(min(abs(numOne), abs(numTwo)), 8);
    result = approximate_binary_subtraction_8bit_M95_P10_A5(maxBinary, minBinary);
  }

  return sign * binaryToDecimal(result);
}