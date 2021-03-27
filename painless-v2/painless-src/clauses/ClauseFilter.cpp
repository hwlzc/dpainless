// Copyright (c) 2015 Tomas Balyo, Karlsruhe Institute of Technology
// This file is from HordeSAT (https://github.com/biotomas/hordesat)
/*
 * ClauseFilter.cpp
 *
 *  Created on: Aug 12, 2014
 *      Author: balyo
 */

#include "ClauseFilter.h"

#include <stdlib.h>
#include <stdio.h>

using namespace std;

#define NUM_PRIMES 12

static unsigned const int primes [] = {2038072819, 2038073287,	2038073761,	2038074317,
		2038072823,	2038073321,	2038073767,	2038074319,
		2038072847,	2038073341,	2038073789,	2038074329};

size_t ClauseFilter::commutativeHashFunction(int* cls, int size, int which) {
	size_t res = 0;
	// skip the first int (it is the glue)
	for (size_t j = 1; j < size; j++) {
		int lit = cls[j];
		res ^= lit * primes[abs((which * lit) % NUM_PRIMES)];
	}
	return res % NUM_BITS;
}

ClauseFilter::ClauseFilter() {
	s1 = new bitset<NUM_BITS>();
}

ClauseFilter::~ClauseFilter() {
	delete s1;
}

bool ClauseFilter::registerClause(int* cls, int size) {
	// unit clauses always get in
	if (size == 1) {
		if (unitSet.count(cls[0])) {
			return false;
		}
		unitSet.insert(cls[0]);
		return true;
	}

	size_t h1 = commutativeHashFunction(cls, size, 1);
	size_t h2 = commutativeHashFunction(cls, size, 2);
	size_t h3 = commutativeHashFunction(cls, size, 3);
	size_t h4 = commutativeHashFunction(cls, size, 4);

	if (s1->test(h1) && s1->test(h2) && s1->test(h3) && s1->test(h4)) {
		return false;
	} else {
		s1->set(h1, true);
		s1->set(h2, true);
		s1->set(h3, true);
		s1->set(h4, true);
		return true;
	}
}

void ClauseFilter::clear() {
	s1->reset();
}
