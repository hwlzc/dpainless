// Copyright (c) 2015 Tomas Balyo, Karlsruhe Institute of Technology
// This file is from HordeSAT (https://github.com/biotomas/hordesat)
/*
 * ClauseFilter.h
 *
 *  Created on: Aug 12, 2014
 *      Author: balyo
 */

#ifndef CLAUSEFILTER_H_
#define CLAUSEFILTER_H_

#include <vector>
#include <bitset>
#include <unordered_set>

using namespace std;

//#define NUM_BITS 268435399 // 32MB
#define NUM_BITS 26843543 // 3,2MB

class ClauseFilter {
public:
	ClauseFilter();
	virtual ~ClauseFilter();
	/**
	 * Return false if the given clause has already been registered
	 * otherwise add it to the filter and return true.
	 */
	bool registerClause(int* cls, int size);
	/**
	 * Clear the filter, i.e., return to its initial state.
	 */
	void clear();

private:
	bitset<NUM_BITS>* s1;
	unordered_set<int> unitSet;
	size_t hashFunction(int* cls, int size);
	size_t commutativeHashFunction(int* cls, int size, int which);
	size_t unitHashFunction(int var);

};

#endif /* CLAUSEFILTER_H_ */
