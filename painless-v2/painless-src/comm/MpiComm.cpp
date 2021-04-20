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

#include "../comm/MpiComm.h"
#include "../utils/Parameters.h"
#include "../utils/Logger.h"

#include <unistd.h>
#include <vector>
#include <cstring>
#include <cassert>

#define REPORT_TAG 1    // worker report to master that it is starving
#define CUBE_TAG 2      // distribute a cube to a starving worker
#define CLAUSE_TAG 3    // clause sharing between workers
#define STOP_TAG 4      // stop solving
#define INTERRUPT_TAG 5 // set/unset interuption of solving

MpiComm *MpiComm::getInstance()
{
    static MpiComm ins;
    return &ins;
}

MpiComm::MpiComm()
{
    int litPerRound = Parameters::getIntParam("shr-lit", 1500);
    // add 500 (magic number) in order to store splitter '0'
    clauseBufLimit = litPerRound * 2;

    importBuf = (int *)malloc(sizeof(int) * clauseBufLimit);
    exportBuf = (int *)malloc(sizeof(int) * clauseBufLimit);
}

MpiComm::~MpiComm()
{
    free(importBuf);
    free(exportBuf);
}

void MpiComm::init(int rank, int size)
{
    this->rank = rank;
    this->size = size;
    int sharingGroupSize = Parameters::getIntParam("shr-group", 0);
    if (sharingGroupSize)
    {
        clsShrLowerRank = rank / sharingGroupSize * sharingGroupSize;
        clsShrUpperRank = rank / sharingGroupSize * sharingGroupSize + sharingGroupSize;
        clsShrUpperRank = clsShrUpperRank > size ? size : clsShrUpperRank;
    }
    else
    {
        clsShrLowerRank = 0;
        clsShrUpperRank = size;
    }
}

void MpiComm::addClausesToExternal(const vector<ClauseExchange *> &clauses)
{
    if (size == 1)
    {
        return;
    }

    for (auto cls : clauses)
    {
        if (externalFilter.registerClause(cls->lits, cls->size))
        {
            ClauseManager::increaseClause(cls, 1);
            clausesToExport.addClause(cls);
        }
    }
}

void MpiComm::getClausesFromExternal(vector<ClauseExchange *> &clauses)
{
    clausesToImport.getClauses(clauses);
}

void MpiComm::exportLearnedClauses()
{
    std::vector<ClauseExchange *> tmp;
    int selectCount;
    // Get shortest clauses from database
    int used = clausesToExport.giveSelection(tmp, Parameters::getIntParam("shr-lit", 1500), &selectCount);
    if (tmp.empty())
        return;

    int idx = 1;
    for (auto cls : tmp)
    {
        for (int i = 0; i < cls->size; i++)
        {
            exportBuf[idx++] = cls->lits[i];
        }
        exportBuf[idx++] = 0;
    }
    idx--;
    exportBuf[0] = idx;
    assert(idx < clauseBufLimit);
    for (int i = clsShrLowerRank; i < clsShrUpperRank; i++)
    {
        if (i == rank)
            continue;
        MPI_Bsend(exportBuf, clauseBufLimit, MPI_INT, i, CLAUSE_TAG, MPI_COMM_WORLD);
    }

    for (auto cls : tmp)
    {
        ClauseManager::releaseClause(cls);
    }
}

void MpiComm::cleanReceivingBuffer()
{
    int received;
    MPI_Status s;
    bool receiveDone = false;
    while (!receiveDone)
    {
        MPI_Iprobe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &received, &s);
        if (received)
        {
            int *garbage;
            int length;
            MPI_Get_count(&s, MPI_INT, &length);
            garbage = (int *)malloc(length * sizeof(int));
            MPI_Recv(garbage, length, MPI_INT, s.MPI_SOURCE, s.MPI_TAG, MPI_COMM_WORLD, &s);
            free(garbage);
        }
        else
        {
            receiveDone = true;
        }
    }
}

SatResult MpiComm::receiveIncomingMsg()
{
    int received;
    MPI_Status s;
    bool receiveDone = false;
    while (!receiveDone)
    {
        MPI_Iprobe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &received, &s);
        if (received)
        {
            int tag = s.MPI_TAG;
            if (globalEnding) tag = -1;
            switch (tag)
            {
            case REPORT_TAG:
            {
                assert(rank == 0);
                int length;
                MPI_Get_count(&s, MPI_INT, &length);
                // FIXME: free the result
                int* result = (int*) malloc(sizeof(int) * length);
                MPI_Recv(result, length, MPI_INT, s.MPI_SOURCE, s.MPI_TAG, MPI_COMM_WORLD, &s);
                log(1, "root receives result %d from rank %d\n", result[0], s.MPI_SOURCE);
                if (res && s.MPI_SOURCE != 0)
                {
                    break;
                }
                res = SatResult(result[0]);
                if (length > 1)
                {
                    model.clear();
                    for (int i = 1; i < length; i++)
                    {
                        model.push_back(result[i]);
                    }
                }
                this->parentStrategy[s.MPI_SOURCE]->join(parentStrategy[s.MPI_SOURCE], res, model);
                break;
            }
            case CUBE_TAG:
            {
                int length;
                MPI_Get_count(&s, MPI_INT, &length);
                int branchBuf[length];
                MPI_Recv(branchBuf, length, MPI_INT, s.MPI_SOURCE, s.MPI_TAG, MPI_COMM_WORLD, &s);
                vector<int> cube;
                if (length > 1)
                {
                    for (int i = 1; i < branchBuf[0] + 1; i++)
                    {
                        cube.push_back(branchBuf[i]);
                    }
                }

                log(1, "Rank %d receives solve with cube size %d\n", rank, (int)cube.size());
                this->childStrategy->solve(cube);
                break;
            }
            case CLAUSE_TAG:
            {
                MPI_Recv(importBuf, clauseBufLimit, MPI_INT, s.MPI_SOURCE, s.MPI_TAG, MPI_COMM_WORLD, &s);
                handleReceivedClauses(importBuf, clauseBufLimit);
                break;
            }
            // TODO: should this update globalEnding?
            case STOP_TAG:
            {
                int ret;
                MPI_Recv(&ret, 1, MPI_INT, s.MPI_SOURCE, s.MPI_TAG, MPI_COMM_WORLD, &s);
                assert(s.MPI_SOURCE == 0);
                log(1, "%d receives result %d from root\n", this->rank, ret);
                res = SatResult(ret);
                globalEnding = true;
                break;
            }
            case INTERRUPT_TAG:
            {
                int ret;
                MPI_Recv(&ret, 1, MPI_INT, s.MPI_SOURCE, s.MPI_TAG, MPI_COMM_WORLD, &s);
                assert(s.MPI_SOURCE == 0);
                if (ret)
                {
                    log(1, "%d receives setInterrupt from root\n", this->rank);
                    this->childStrategy->setInterrupt();
                }
                else
                {
                    log(1, "%d receives unsetInterrupt from root\n", this->rank);
                    this->childStrategy->unsetInterrupt();
                }
                break;
            }
            default:
                log(1, "garbage is received from rank %d with tag %d\n", rank, s.MPI_TAG);
                int *garbage;
                int length;
                MPI_Get_count(&s, MPI_INT, &length);
                garbage = (int *)malloc(length * sizeof(int));
                MPI_Recv(garbage, length, MPI_INT, s.MPI_SOURCE, s.MPI_TAG, MPI_COMM_WORLD, &s);
                free(garbage);
                break;
            }
        }
        else
        {
            receiveDone = true;
        }
    }
    return res;
}

void MpiComm::handleReceivedClauses(int *clsBuf, int size)
{
    int length = clsBuf[0];
    assert(length < size);
    vector<int> tmp;
    for (int i = 1; i <= length; i++)
    {
        if (clsBuf[i])
        {
            tmp.push_back(clsBuf[i]);
        }
        else
        {
            int size = tmp.size();
            if (!externalFilter.registerClause(tmp.data(), size))
            {
                tmp.clear();
                continue;
            }
            ClauseExchange *cls = ClauseManager::allocClause(size);
            cls->size = size;
            cls->from = -1; // from external
            std::copy(tmp.begin(), tmp.end(), cls->lits);
            clausesToImport.addClause(cls);
            tmp.clear();
        }
    }
}

void MpiComm::updateWorkingStatus()
{
    assert(rank == 0);

    // Bcast stop message
    log(1, "Root broadcasts result %d to the all ranks\n", res);
    MPI_Request requests[size - 1];
    for (int i = 1; i < size; i++)
    {
        MPI_Isend(&res, 1, MPI_INT, i, STOP_TAG, MPI_COMM_WORLD, &requests[i - 1]);
    }
    MPI_Waitall(size - 1, requests, MPI_STATUSES_IGNORE);
}

void MpiComm::reportResult(SatResult currRes, const vector<int> &model)
{
    if (res)
        return;
    assert(currRes != UNKNOWN);
    res = currRes;
    // Report the result to rank 0
    int length = model.size();
    // FIXME: free the buffer
    int*  modelBuf = (int *) malloc(sizeof(int) * (length + 1));
    std::copy(model.begin(), model.end(), modelBuf + 1);
    modelBuf[0] = currRes;
    log(1, "Rank %d is the winner, reports result % d to the root\n ", rank, currRes);
    MPI_Send(modelBuf, length + 1, MPI_INT, 0, REPORT_TAG, MPI_COMM_WORLD);
    free(modelBuf);
}

void MpiComm::sendAssumption(const vector<int> &assumption, int targetRank)
{
    // cube buffer: [cube length] [... cube ...]
    int length = assumption.size();
    // FIXME: no free here
    int* buf = (int*) malloc(sizeof(int) * (length + 1));
    std::copy(assumption.begin(), assumption.end(), buf + 1);
    buf[0] = length;
    // FIXME
    MPI_Request req;
    MPI_Ibsend(&buf, length + 1, MPI_INT, targetRank, CUBE_TAG, MPI_COMM_WORLD, &req);
}

void MpiComm::sendInterrupt(int interrupt, int targetRank)
{
    // 0 for unset interrupt, 1 for interrupt
    // FIXME
    static int val;
    val = interrupt;
    MPI_Request req;
    MPI_Isend(&val, 1, MPI_INT, targetRank, INTERRUPT_TAG, MPI_COMM_WORLD, &req);
}