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

#include "../working/LocalWorker.h"
#include "../comm/MpiComm.h"

#include <unistd.h>

LocalWorker::LocalWorker(int rank) : rank(rank)
{
    MpiComm::getInstance()->registerChildWorkingStrategy(this);
}

LocalWorker::~LocalWorker()
{
    for (size_t i = 0; i < slaves.size(); i++) {
      delete slaves[i];
   }
}

void LocalWorker::solve(const vector<int> &cube)
{
    for (auto slave : slaves) {
        slave->solve(cube);
    }
}

void LocalWorker::join(WorkingStrategy *strat, SatResult res,
                        const vector<int> &model)
{
    if (globalEnding) {
        return;
    }
    if (res != UNKNOWN) {
        MpiComm::getInstance()->reportResult(res, model);        
    }    
}

void LocalWorker::setInterrupt()
{
    for (auto slave : slaves) {
        slave->setInterrupt();
    }
}

void LocalWorker::unsetInterrupt() {
    for (auto slave : slaves) {
        slave->unsetInterrupt();
    }
}

void LocalWorker::waitInterrupt() {}