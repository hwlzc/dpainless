// -----------------------------------------------------------------------------
// Copyright (C) 2021
//
// This file is part of PaInleSS.
//
// PaInleSS is free software: you can redistribute it and/or modify it under the
// terms of the GNU General Public License as published by the Free Software
// Foundation, either version 3 of the License, or (at your option) any later
// version.
//
// This program is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
// details.
//
// You should have received a copy of the GNU General Public License along with
// this program.  If not, see <http://www.gnu.org/licenses/>.
// -----------------------------------------------------------------------------

#pragma once

#include "../working/WorkingStrategy.h"

#include <thread>
#include <memory>

using namespace std;

extern atomic<bool> globalEnding;

// Receive msg from dispatcher and send to WorkingStrateies in Slave
// Report result & model to dispatcher
class LocalWorker : public WorkingStrategy
{
public:
    LocalWorker(int rank);

    ~LocalWorker();

    void solve(const vector<int> &cube);

    void join(WorkingStrategy *strat, SatResult res,
              const vector<int> &model);

    void setInterrupt();

    void unsetInterrupt();

    void waitInterrupt();

    int getDivisionVariable() { return -1; }

    void setPhase(int var, bool value) {}

    void bumpVariableActivity(int var, int times)
    {
    }

protected:
    atomic<bool> strategyEnding;
    int rank;
};
