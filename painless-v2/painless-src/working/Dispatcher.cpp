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

#include "../working/Dispatcher.h"
#include "../comm/MpiComm.h"

#include <unistd.h>

Dispatcher::Dispatcher(int rank) : rank(rank)
{
    MpiComm::getInstance()->registerParentWorkingStrategy(this);
}

Dispatcher::~Dispatcher()
{}

void Dispatcher::solve(const vector<int> &cube)
{
    // send the cube to the receiver
    MpiComm::getInstance()->sendAssumption(cube, rank);
}

void Dispatcher::join(WorkingStrategy *strat, SatResult res,
                        const vector<int> &model)
{
    if (globalEnding) {
        return;
    }

    // the parent should not be a nullptr in this case
    parent->join(this, res, model);
}

void Dispatcher::setInterrupt()
{
    MpiComm::getInstance()->sendInterrupt(1, rank);
}

void Dispatcher::unsetInterrupt() {
    MpiComm::getInstance()->sendInterrupt(0, rank);
}

void Dispatcher::waitInterrupt() {}