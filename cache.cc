#include "cache.h"
#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <string.h>
#include <iomanip>
#include <math.h>

using namespace std;

unsigned cacheSize;
unsigned cacheAss;
unsigned cacheLine;
write_policy_t hitPol;
write_policy_t missPol;
unsigned hitTime;
unsigned missPen;
unsigned addressWid;
int block_bits;
int index_bits;
int tag_bits;
aCache theCache;
int clockAge;
int set_entries;

//statistics variables

int numWrites;
int numReads;
int numRDHits;
int numWRHits;
int numEvic;
int numRDMisses;
int numWRMisses;
int numMemWR;
int numMemAcc;

cache::cache(unsigned size,
			 unsigned associativity,
			 unsigned line_size,
			 write_policy_t wr_hit_policy,
			 write_policy_t wr_miss_policy,
			 unsigned hit_time,
			 unsigned miss_penalty,
			 unsigned address_width)
{ //assigning inputs to global variables
	cacheSize = size;
	cacheAss = associativity;
	cacheLine = line_size;
	hitPol = wr_hit_policy;
	missPol = wr_miss_policy;
	hitTime = hit_time;
	missPen = miss_penalty;
	addressWid = address_width;
	clockAge = 0;
	//computing block and set sizes
	int num_of_entries = size / line_size;
	set_entries = num_of_entries / cacheAss;
	theCache.blocks = new aBlock[cacheAss];

	for (int i = 0; i < cacheAss; i++)
	{
		theCache.blocks[i].entries = new cache_entry[set_entries];
		for (int j = 0; j < set_entries; j++)
		{
			theCache.blocks[i].entries[j].tag = 0;
			theCache.blocks[i].entries[j].dirty_bit = false;
		}
	}

	numWrites = 0;
	numReads = 0;
	numRDHits = 0;
	numWRHits = 0;
	numEvic = 0;
	numRDMisses = 0;
	numWRMisses = 0;
	numMemWR = 0;
	numMemAcc = 0;

	//address break-down
	block_bits = log2(cacheLine);
	index_bits = log2(cacheSize / (cacheLine * cacheAss));
	tag_bits = addressWid - block_bits - index_bits;
}

void cache::print_configuration()
{
	cout << "CACHE CONFIGURATION\n";
	cout << "size = " << cacheSize / 1024 << " KB\n";
	cout << "associativity = " << cacheAss << "-way\n";
	cout << "cache line size = " << cacheLine << " B\n";
	if (hitPol == WRITE_BACK)
	{
		cout << "write hit policy = write-back\n";
	}
	else
	{
		cout << "write hit policy = write-through\n";
	}
	if (missPol == WRITE_ALLOCATE)
	{
		cout << "write miss policy = write-allocate\n";
	}
	else
	{
		cout << "write miss policy = no-write-allocate\n";
	}
	cout << "cache hit time = " << hitTime << " CLK\n";
	cout << "cache miss penalty = " << missPen << " CLK\n";
	cout << "memory address width = " << addressWid << " bits\n";
}

cache::~cache()
{
	delete[] theCache.blocks;
	clockAge = 0;
	numWrites = 0;
	numReads = 0;
	numRDHits = 0;
	numWRHits = 0;
	numEvic = 0;
	numRDMisses = 0;
	numWRMisses = 0;
	numMemWR = 0;
	numMemAcc = 0;
}

void cache::load_trace(const char *filename)
{
	stream.open(filename);
}

void cache::run(unsigned num_entries)
{
	unsigned first_access = number_memory_accesses;
	string line;
	unsigned line_nr = 0;

	while (getline(stream, line))
	{
		clockAge++;
		bool hitFlag = false;
		//cout << clockAge << endl;
		line_nr++;
		char *str = const_cast<char *>(line.c_str());

		// tokenize the instruction
		char *op = strtok(str, " ");
		char *addr = strtok(NULL, " ");
		unsigned long long address = strtoull(addr, NULL, 16);
		//cout << address << endl;
		string theAddress = long2binary(address);

		theAddress.erase(0, 64 - addressWid);
		//cout << theAddress << endl;
		string tag_string = theAddress.substr(0, tag_bits);
		string index_string = theAddress.substr(tag_bits, index_bits);
		unsigned long long index_num = binStr2long(index_string);
		unsigned long long tag_num = binStr2long(tag_string);

		//cout << index_num << endl;
		//cout << hex << tag_num << endl;

		if (op[0] == 'w')
		{
			numWrites++;
		}
		else if (op[0] == 'r')
		{
			numReads++;
		}

		int hitBlock = UNDEFINED;

		for (int i = 0; i < cacheAss; i++)
		{
			if (tag_num == theCache.blocks[i].entries[index_num].tag)
			{
				hitFlag = true;
				hitBlock = i;
				//cout << "HIT!!" << endl;
				goto CHECKED;
			}
		}

	CHECKED:;

		if (hitFlag == true)
		{
			if (op[0] == 'w')
			{
				if (hitPol == WRITE_THROUGH)
				{
					numMemWR++;
				}
				else
				{ //this is if the write hit policy is write back
					theCache.blocks[hitBlock].entries[index_num].dirty_bit = true;
				}
				numWRHits++;
			}
			else
			{
				numRDHits++;
			}
			theCache.blocks[hitBlock].entries[index_num].age = clockAge; //updating the age after the hit
		}
		else if (hitFlag == false)
		{
			if (hitPol == WRITE_THROUGH && op[0] == 'w')
			{
				numMemWR++;
			}
			//cout << "MISS" << endl;
			if (op[0] == 'r')
			{
				numRDMisses++;
			}
			if (missPol == NO_WRITE_ALLOCATE && op[0] == 'w')
			{
				numWRMisses++;
				goto NO_ALOCATE_SKIP;
			}

			int vacantBlock = UNDEFINED;

			for (int i = 0; i < cacheAss; i++)
			{
				if (theCache.blocks[i].entries[index_num].tag == 0)
				{
					//cout << "VACANT SPOT!!" << endl;
					vacantBlock = i;
					break;
				}
			}

			if (vacantBlock != UNDEFINED) //
			{
				//cout << "WRITING over spot " << vacantBlock << endl;
				theCache.blocks[vacantBlock].entries[index_num].tag = tag_num;
				theCache.blocks[vacantBlock].entries[index_num].age = clockAge;
				if (op[0] == 'w' && missPol == WRITE_ALLOCATE)
				{
					theCache.blocks[vacantBlock].entries[index_num].dirty_bit = true;
					numWRMisses++;
				}
			}
			else
			{

				//cout << "ENTERING EVICTION!!" << endl;
				numEvic++;
				int minAge = 2147483647;
				int blockEvicted;
				for (int i = 0; i < cacheAss; i++)
				{
					if (theCache.blocks[i].entries[index_num].age < minAge)
					{
						minAge = theCache.blocks[i].entries[index_num].age;
						blockEvicted = i;
					}
				}
				if (theCache.blocks[blockEvicted].entries[index_num].dirty_bit == 1)
				{
					if (hitPol == WRITE_BACK)
					{
						numMemWR++;
					}
				}

				theCache.blocks[blockEvicted].entries[index_num].tag = tag_num;
				theCache.blocks[blockEvicted].entries[index_num].age = clockAge;

				if (op[0] == 'w')
				{
					theCache.blocks[blockEvicted].entries[index_num].dirty_bit = true;
					numWRMisses++;
				}
				else if (op[0] == 'r')
				{
					theCache.blocks[blockEvicted].entries[index_num].dirty_bit = false;
				}
			}
		}

	NO_ALOCATE_SKIP:;

		/* 
		edit here:
		insert the code to process read and write operations
	   	using the read() and write() functions below

	*/
		number_memory_accesses++;
		if (num_entries != 0 && (number_memory_accesses - first_access) == num_entries)
			break;
	}
}

void cache::print_statistics()
{
	float AMAT = float(hitTime) + float(missPen) * ((float(numRDMisses) + float(numWRMisses)) / (float(numReads) + float(numWrites)));
	std::cout << std::dec << "STATISTICS"
			  << endl;
	cout << "memory accesses = " << clockAge << endl;
	cout << "read = " << numReads << endl;
	cout << "read misses = " << numRDMisses << endl;
	cout << "write = " << numWrites << endl;
	cout << "write misses = " << numWRMisses << endl;
	cout << "evictions = " << numEvic << endl;
	cout << "memory writes = " << numMemWR << endl;
	cout << "average memory access time = " << setprecision(4) << fixed << AMAT << endl;
}

access_type_t cache::read(address_t address)
{
	/* edit here */
	return MISS;
}

access_type_t cache::write(address_t address)
{
	/* edit here */
	return MISS;
}

void cache::print_tag_array()
{
	if (hitPol == WRITE_THROUGH)
	{
		std::cout << std::dec << "TAG ARRAY" << endl;
		for (int i = 0; i < cacheAss; i++)
		{
			cout << "BLOCKS " << i << endl;
			cout << setfill(' ') << setw(7) << "index" << setw(6) << setw(4 + tag_bits / 4)
				 << "tag" << endl;
			for (int j = 0; j < set_entries; j++)
			{
				if (theCache.blocks[i].entries[j].tag != 0)
				{
					cout << setfill(' ') << setw(7) << dec << j << setw(6) << setw(tag_bits / 4) << "0x"
						 << hex << theCache.blocks[i].entries[j].tag << endl;
				}
			}
		}
	}
	else if (hitPol == WRITE_BACK)
	{
		std::cout << std::dec << "TAG ARRAY" << endl;
		for (int i = 0; i < cacheAss; i++)
		{
			cout << "BLOCKS " << i << endl;
			cout << setfill(' ') << setw(7) << "index" << setw(6) << "dirty" << setw(4 + tag_bits / 4)
				 << "tag" << endl;
			for (int j = 0; j < set_entries; j++)
			{
				if (theCache.blocks[i].entries[j].tag != 0)
				{
					cout << setfill(' ') << setw(7) << dec << j << setw(6) << theCache.blocks[i].entries[j].dirty_bit << setw(tag_bits / 4)
						 << "0x" << hex << theCache.blocks[i].entries[j].tag << endl;
				}
			}
		}
	}
}

unsigned cache::evict(unsigned index)
{
	/* edit here */
	return 0;
}

unsigned long long cache::binStr2long(string inStr)
{
	unsigned long long theReturn = 0;
	int len = inStr.size();
	for (int i = 0; i < len; i++)
	{
		theReturn += (inStr[len - i - 1] - 48) * pow(2, i);
	}
	return theReturn;
}

char *cache::long2binary(unsigned long long k)
{
	static char c[65];
	c[0] = '\0';

	unsigned long long val;
	for (val = 1ULL << (sizeof(unsigned long long) * 8 - 1); val > 0; val >>= 1)
	{
		strcat(c, ((k & val) == val) ? "1" : "0");
	}
	return c;
}

string cache::bin2hex(string sBinary)
{
	string rest("0x"), tmp, chr = "";
	int len = sBinary.length() / 4;
	chr = chr.substr(0, len);
	sBinary = chr + sBinary;
	for (int i = 0; i < sBinary.length(); i += 4)
	{
		tmp = sBinary.substr(i, 4);
		if (!tmp.compare("0000"))
		{
			rest = rest + "0";
		}
		else if (!tmp.compare("0001"))
		{
			rest = rest + "1";
		}
		else if (!tmp.compare("0010"))
		{
			rest = rest + "2";
		}
		else if (!tmp.compare("0011"))
		{
			rest = rest + "3";
		}
		else if (!tmp.compare("0100"))
		{
			rest = rest + "4";
		}
		else if (!tmp.compare("0101"))
		{
			rest = rest + "5";
		}
		else if (!tmp.compare("0110"))
		{
			rest = rest + "6";
		}
		else if (!tmp.compare("0111"))
		{
			rest = rest + "7";
		}
		else if (!tmp.compare("1000"))
		{
			rest = rest + "8";
		}
		else if (!tmp.compare("1001"))
		{
			rest = rest + "9";
		}
		else if (!tmp.compare("1010"))
		{
			rest = rest + "a";
		}
		else if (!tmp.compare("1011"))
		{
			rest = rest + "b";
		}
		else if (!tmp.compare("1100"))
		{
			rest = rest + "c";
		}
		else if (!tmp.compare("1101"))
		{
			rest = rest + "d";
		}
		else if (!tmp.compare("1110"))
		{
			rest = rest + "e";
		}
		else if (!tmp.compare("1111"))
		{
			rest = rest + "f";
		}
		else
		{
			continue;
		}
	}
	return rest;
}

string cache::hexstr2bin(string sHex)
{
	string sReturn = "";
	for (int i = 0; i < sHex.length(); ++i)
	{
		switch (sHex[i])
		{
		case '0':
			sReturn.append("0000");
			break;
		case '1':
			sReturn.append("0001");
			break;
		case '2':
			sReturn.append("0010");
			break;
		case '3':
			sReturn.append("0011");
			break;
		case '4':
			sReturn.append("0100");
			break;
		case '5':
			sReturn.append("0101");
			break;
		case '6':
			sReturn.append("0110");
			break;
		case '7':
			sReturn.append("0111");
			break;
		case '8':
			sReturn.append("1000");
			break;
		case '9':
			sReturn.append("1001");
			break;
		case 'A':
			sReturn.append("1010");
			break;
		case 'B':
			sReturn.append("1011");
			break;
		case 'C':
			sReturn.append("1100");
			break;
		case 'D':
			sReturn.append("1101");
			break;
		case 'E':
			sReturn.append("1110");
			break;
		case 'F':
			sReturn.append("1111");
			break;
		}
	}
	return sReturn;
}
