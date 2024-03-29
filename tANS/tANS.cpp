#include "stdafx.h"
#include <vector>
#include <queue>
#include <list>
#include <iostream>
#include "tANS.h"

#include <fstream>
#include <ctime>
using namespace std;




#define writeBit(x) { \
	if ( x) \
		*out|=bitpos;\
	bitpos>>=1;\
	if ( !bitpos){ \
		bitpos=0x80;\
		out++;\
		*out=0;\
	}\
}

#define readLastBit(x) { \
	bit = readbitpos & x;\
	bitpos <<=1;\
	if ( !bitpos == 0x80 ){\
		in++;\
		bitpos = 0x80;\
	}\
}


struct Tabeled_ANS
{
	unsigned short L; // start piont in Table
	unsigned int states; // max_state
	unsigned short alphabet; // alphabet size

	vector<unsigned char>	symbol; // Column --> Symbol
	vector<unsigned char>	symbol_column; // Symbol --> Coulmn
	vector<float>			probability; // Probability of Symbol
	vector<unsigned int>	n_probability; // normed probability (p*states)

	vector<unsigned int> encodeTable; // EncodeTable of Size states*alphabet
	vector<unsigned int> decodeTableSymbol; // Decode Table
	vector<unsigned int> decodeTableValue; // Decode Table

};


unsigned int getPrecisionBit(unsigned short alphabet) {
	unsigned int extraBits = 2;
	if (alphabet < 8)
		return 3 + extraBits;
	else if (alphabet < 16)
		return 4 + extraBits;
	else if (alphabet < 32)
		return 5 + extraBits;
	else if (alphabet < 64)
		return 6 + extraBits;
	else if (alphabet < 128)
		return 7 + extraBits;
	else
		return 8 + extraBits;
}
unsigned char bitsNeeded(unsigned int value) {

	if (value < 1) return 0;
	else if (value < 2)return 1;
	else if (value < 4)return 2;
	else if (value < 8)return 3;
	else if (value < 16)return 4;
	else if (value < 32)return 5;
	else if (value < 64)return 6;
	else if (value < 128)return 7;
	else if (value < 256)return 8;
	else if (value < 512)return 9;
	else if (value < 1024)return 10;
	else if (value < 2048)return 11;
	else return 12;

}


void countChar(const unsigned char * in, unsigned int length, unsigned int * table) {
	const unsigned char * input = in;
	const unsigned char * const ending = in + length;
	int a = 0;

	do
	{
		a = (int)table[*input];
		a++;
		table[*input] = (unsigned int)a;
		input++;

	} while (input < ending);

}

unsigned char bestSymbol(Tabeled_ANS * tans, unsigned int * rows, unsigned int value) {
	float val = 0;
	float best = FLT_MAX;
	unsigned char result = 0;

	for (unsigned short i = 0; i < tans->alphabet; i++)
	{
		if (rows[i] < tans->n_probability[i]) {
			val = ((float)(rows[i] + tans->n_probability[i])) / tans->probability[i];
			float b = abs(val - value);
			if (b < best) {
				best = b;
				result = i;
			}
		}
	}
	return result;
}


unsigned int storeProb(unsigned char * out, Tabeled_ANS * tans) {

	*out = (unsigned char)(tans->alphabet - 1);
	out++;

	for (unsigned short i = 0; i < tans->alphabet; i++)
	{
		*out = tans->symbol[i];
		out++;
		*(float*)out = (float)tans->probability[i];
		out += 4;
	}

	return (tans->alphabet * 5) + 1;
}

void readProb(const unsigned char * in, Tabeled_ANS * tans) {
	tans->alphabet = *in + 1;
	in++;

	tans->symbol.resize(tans->alphabet);
	tans->symbol_column.resize(256);
	tans->probability.resize(tans->alphabet);
	tans->n_probability.resize(tans->alphabet);

	unsigned int precisionBits = getPrecisionBit(tans->alphabet);
	tans->L = (1 << precisionBits);


	unsigned int range = tans->L - tans->alphabet - 2;
	for (unsigned short i = 0; i < tans->alphabet; i++)
	{
		tans->symbol[i] = *in;
		tans->symbol_column[*in] = i;
		in++;
		tans->probability[i] = *(float*)in;
		tans->n_probability[i] = 1 + floor(tans->probability[i] * range);
		in += 4;
	}
}

void getProb(const unsigned char * in, const unsigned int length, Tabeled_ANS * tans) {
	// Count chars
	unsigned int table[256] = { 0 };
	countChar(in, length, table);

	// Sort Chars by Count
	vector<pair<unsigned int, unsigned int>> que;
	unsigned int count = 0;
	for (unsigned int i = 0; i < 256; i++) {
		unsigned int value = table[i];
		if (value != 0) {
			pair<unsigned int, unsigned int> p(value, i);
			que.push_back(p);
			count++;
		}
	}
	sort(que.begin(), que.end());

	// calc max value
	unsigned int precisionBits = getPrecisionBit(count);
	tans->alphabet = count;
	tans->L = (1 << precisionBits);

	// init alloc arrays
	tans->symbol.resize(tans->alphabet);
	tans->symbol_column.resize(256);
	tans->probability.resize(tans->alphabet);
	tans->n_probability.resize(tans->alphabet);



	// write prob to struct
	unsigned int i = 0;
	unsigned int range = tans->L - tans->alphabet - 2;
	while (!que.empty())
	{
		pair<unsigned int, unsigned int> p = que.back();
		que.pop_back();
		tans->symbol[i] = p.second;
		tans->symbol_column[p.second] = i;
		tans->probability[i] = ((float)p.first / (float)length);
		tans->n_probability[i] = 1 + floor(tans->probability[i] * range);
		i++;
	}


}

void createTable(Tabeled_ANS * tans) {
	unsigned int prob_sum = 0;
	for (unsigned short i = 0; i < tans->alphabet; i++)
	{
		prob_sum += tans->n_probability[i];
	}
	int diff = tans->L - prob_sum;
	tans->n_probability[0] += (diff - 2);

	// init encodeTable
	unsigned int table_size = (tans->n_probability[0] + 2) * tans->alphabet;
	tans->encodeTable.resize(table_size);

	// the current row to write the value per symbol
	unsigned int * read_row = new unsigned int[tans->alphabet];
	for (unsigned i = 0; i < tans->alphabet; i++)
	{
		read_row[i] = 0;
	}

	// greedy - maybe replace
	unsigned int L_2 = tans->L << 1;
	for (unsigned int i = tans->L; i < L_2 - 1; i++)
	{
		unsigned char symb = bestSymbol(tans, read_row, i);
		tans->encodeTable[read_row[symb] * tans->alphabet + symb] = i;
		read_row[symb]++;

	}
	tans->n_probability[0]++;
	tans->encodeTable[(tans->n_probability[0])*tans->alphabet] = L_2 - 1;


}

void createDecodeTable(Tabeled_ANS * tans) {
	tans->decodeTableSymbol.resize(2 * tans->L);
	tans->decodeTableValue.resize(2 * tans->L);

	if (tans->n_probability[0] != (2 * tans->L) - 1)
		tans->n_probability[0]++;

	for (unsigned short s = 0; s < tans->alphabet; s++) {
		for (unsigned int i = 0; i < tans->n_probability[s]; i++)
		{
			unsigned int value = tans->encodeTable[i*(tans->alphabet) + s];
			tans->decodeTableSymbol[value] = tans->symbol[s];
			tans->decodeTableValue[value] = i + tans->n_probability[s];

		}
	}
}


//-------------------------------------------------------------------------------------------------

DllExport unsigned int Encode(const unsigned char * __restrict in, unsigned char * __restrict out, const unsigned int length)
{
	Tabeled_ANS tableANS;
	unsigned char bitpos = 0b10000000;
	unsigned int readbitpos = 1;
	bool bit = 0;

	const unsigned char * begin_in = in;
	unsigned char * begin = out;
	out += 5;

	getProb(in, length, &tableANS);
	createTable(&tableANS);
	createDecodeTable(&tableANS);

	out += storeProb(out, &tableANS);

	unsigned int precisionBits = getPrecisionBit(tableANS.alphabet);
	unsigned char input_value = *in;
	unsigned int current_state = (tableANS.L << 1) - 1;
	unsigned int target_column = tableANS.symbol_column[input_value];
	unsigned int target_column_height = (2 * tableANS.n_probability[target_column]) - 1;
	in++;

	*out = 0;
	for (unsigned int i = 0; i < length; i++)
	{
		while (current_state > target_column_height) {
			bit = readbitpos & current_state;
			writeBit(bit);
			current_state >>= 1;
		}

		current_state = tableANS.encodeTable[(current_state - tableANS.n_probability[target_column])* tableANS.alphabet + target_column];
		input_value = *in;
		target_column = tableANS.symbol_column[input_value];
		target_column_height = (2 * tableANS.n_probability[target_column]) - 1;
		in++;

	}

	for (unsigned char i = 1; i < precisionBits + 2; i++)
	{
		bit = readbitpos & current_state;
		readbitpos <<= 1;
		writeBit(bit);
	}

	unsigned int bytes = out - begin;
	*begin = bitpos;
	begin++;
	*(unsigned int*)begin = (bytes);

	return bytes + 1;

}

DllExport void Decode(const unsigned char * __restrict in, unsigned char * __restrict out, const unsigned int length)
{
	Tabeled_ANS tableANS;
	unsigned char bitpos = 0b10000000;
	unsigned char readbitpos = *in;
	readbitpos <<= 1;


	bool bit = 0;
	const unsigned char * begin = in;
	in++;
	unsigned int bytes = *(unsigned int*)in;
	in += 4;

	readProb(in, &tableANS);

	createTable(&tableANS);
	//printTable(&tableANS);
	createDecodeTable(&tableANS);



	in = begin + bytes;
	if (readbitpos == 0) {
		readbitpos = 1;
		in--;
	}

	unsigned int value = 0;
	unsigned char bitcount = getPrecisionBit(tableANS.alphabet) + 1;
	for (unsigned char i = 1; i < bitcount + 1; i++)
	{
		bit = *in & readbitpos;
		value |= (bit << (bitcount - i));
		readbitpos <<= 1;
		if (readbitpos == 0) {
			readbitpos = 1;
			in--;
		}

	}

	unsigned char * begin_out = out;
	out += length - 1;


	*out = tableANS.decodeTableSymbol[value];
	out--;
	value = tableANS.decodeTableValue[value];

	


	while (out >= begin_out) {
		unsigned char bits = bitsNeeded(value);
		unsigned char diff = bitcount - bits;
		value <<= (diff);

		for (unsigned char i = 1; i < diff + 1; i++)
		{
			bit = *in & readbitpos;
			value |= (bit << (diff - i));
			readbitpos <<= 1;
			if (readbitpos == 0) {
				readbitpos = 1;
				in--;
			}
		}

		*out = tableANS.decodeTableSymbol[value];

		out--;
		value = tableANS.decodeTableValue[value];;

	}

}