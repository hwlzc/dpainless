// -----------------------------------------------------------------------------
// Copyright (C) 2017  Ludovic LE FRIOUX
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

#include "painless.h"

#include "clauses/ClauseManager.h"
#include "sharing/HordeSatSharing.h"
#include "sharing/Sharer.h"
#include "sharing/SimpleSharing.h"
#include "sharing/DistributionSharing.h"
#include "solvers/SolverFactory.h"
#include "utils/Logger.h"
#include "utils/Parameters.h"
#include "utils/SatUtils.h"
#include "utils/System.h"
#include "working/CubeAndConquer.h"
#include "working/DivideAndConquer.h"
#include "working/Portfolio.h"
#include "working/Dispatcher.h"
#include "working/LocalWorker.h"
#include "working/SequentialWorker.h"
#include "comm/MpiComm.h"

#include <mpi.h>
#include <unistd.h>

using namespace std;


// -------------------------------------------
// Declaration of global variables
// -------------------------------------------
atomic<bool> globalEnding(false);

Sharer ** sharers = NULL;

int nSharers = 0;

WorkingStrategy * working = NULL;

SatResult finalResult = UNKNOWN;

vector<int> finalModel;


// -------------------------------------------
// Main du framework
// -------------------------------------------
int main(int argc, char ** argv)
{
   Parameters::init(argc, argv);
   
   if (Parameters::getFilename() == NULL ||
       Parameters::isSet("h"))
   {
      printf("USAGE: %s [parameters] input.cnf\n", argv[0]);
      printf("Parameters:\n");
      printf("\t-solver=type\t\t glucose, lingeling, minisat, maple, or" \
             " combo\n");
      printf("\t-lbd-limit=<INT>\t lbd limit of exported clauses\n");
      printf("\t-d=0...7\t\t diversification 0=none, 1=sparse, 2=dense," \
             " 3=random, 4=native, 5=1&4, 6=sparse-random, 7=6&4," \
             " default is 0.\n");
      printf("\t-c=<INT>\t\t number of cpus, default is 4.\n");
      printf("\t-wkr-strat=1...6\t 1=portfolio, 2=cube and conquer," \
             " 4=divide and conquer, 6=distributed portfolio, default is portfolio\n");
      printf("\t-shr-strat=1...5\t 1=alltoall, 2=hordesat sharing," \
             " 5=distributed hordesat sharing, default is 0\n");
      printf("\t-shr-group=<INT>\t number of processes current process" \
             " will share clauses to, default is 0 (share to all)\n");
      printf("\t-shr-sleep=<INT>\t time in usecond a sharer sleep each" \
             " round, default 500000 (0.5s)\n");
      printf("\t-shr-lit=<INT>\t\t number of literals shared per round by the" \
             " hordesat strategy, default is 1500\n");
      printf("\t-no-model\t\t won't print the model if the problem is SAT\n");
      printf("\t-t=<INT>\t\t timeout in second, default is no limit\n");
      printf("\t-split-heur=1...3\t for D&C: splitting heuristic," \
             " 1=VSIDS, 2=flips, 3=propagation rate, default is 1\n");
      printf("\t-copy-mode=1...2\t for D&C: copy mode for solvers when " \
            "splitting work, 1=reuse the old solver, 2=clone solver and " \
            "delete old solver, default is 1\n");

      return 0;
   }

   int mpiSize, mpiRank;
   MPI_Init(&argc, &argv);
   MPI_Comm_size(MPI_COMM_WORLD, &mpiSize);
   MPI_Comm_rank(MPI_COMM_WORLD, &mpiRank);
   MpiComm::getInstance()->init(mpiRank, mpiSize);

   if (mpiRank == 0) {
      Parameters::printParams();
   }   

   setVerbosityLevel(Parameters::getIntParam("v", 0));

   int cpus = Parameters::getIntParam("c", 4);

   srand(time(NULL));

   // Buffer for sent MPI messages
   int bufSize = sizeof(int) * 100 * 1000 * 1000;
   int* mpiBuf = (int*) malloc(bufSize);
   MPI_Buffer_attach(mpiBuf, bufSize);

   // Create solvers
   vector<SolverInterface *> solvers;
   
   const string solverType = Parameters::getParam("solver");
   const int wkrStrat = Parameters::getIntParam("wkr-strat", 1);

   int nSolvers = cpus;
   if (wkrStrat == 2 || wkrStrat == 5 || (wkrStrat == 4 &&
       Parameters::getIntParam("copy-mode", 1) == 2)) {
       nSolvers = 1;
   } else if (Parameters::getIntParam("wkr-strat", 1) == 3) {
      nSolvers /= 3;
   }

   if (solverType == "glucose") {
      SolverFactory::createGlucoseSolvers(nSolvers, mpiRank, solvers);
   } else if (solverType == "lingeling") {
      SolverFactory::createLingelingSolvers(nSolvers, mpiRank, solvers);
   } else if (solverType == "maple") {
      SolverFactory::createMapleSolvers(nSolvers, mpiRank, solvers);
   } else if (solverType == "combo") {
      SolverFactory::createComboSolvers(nSolvers, mpiRank, solvers);
   } else {
      // MiniSat is the default choice
      SolverFactory::createMiniSatSolvers(nSolvers, mpiRank, solvers);
   }

   // Diversifycation
   int diversification = Parameters::getIntParam("d", 0);

   switch (diversification) {
      case 1 :
         SolverFactory::sparseDiversification(solvers, mpiRank, mpiSize);
         break;

      case 2 :
         SolverFactory::binValueDiversification(solvers);
         break;

      case 3 :
         SolverFactory::randomDiversification(solvers, 2015);
         break;

      case 4 :
         SolverFactory::nativeDiversification(solvers, mpiRank);
         break;

      case 5 :
         SolverFactory::sparseDiversification(solvers, mpiRank, mpiSize);
         SolverFactory::nativeDiversification(solvers, mpiRank);
         break;

      case 6 :
         SolverFactory::sparseRandomDiversification(solvers, mpiRank, mpiSize);
         break;

      case 7 :
         SolverFactory::sparseRandomDiversification(solvers, mpiRank, mpiSize);
         SolverFactory::nativeDiversification(solvers, mpiRank);
         break;

      case 0 :
         break;
   }

   if (Parameters::getIntParam("wkr-strat", 1) == 3) {
      solvers.push_back(SolverFactory::createLingelingSolver());
   }

   vector<SolverInterface *> from;
   // Start sharing threads
   switch(Parameters::getIntParam("shr-strat", 0)) {
      case 1 :
         nSharers   = 1;
         sharers    = new Sharer*[nSharers];
         sharers[0] = new Sharer(0, new SimpleSharing(), solvers, solvers);
         break;
      case 2 :
         nSharers = cpus;
         sharers  = new Sharer*[nSharers];

         for (size_t i = 0; i < nSharers; i++) {
            from.clear();
            from.push_back(solvers[i]);
            sharers[i] = new Sharer(i, new HordeSatSharing(), from,
                                    solvers);
         }
         break;
      case 3 :
         nSharers = 1;
         sharers  = new Sharer*[nSharers];
         sharers[0] = new Sharer(0, new HordeSatSharing(), solvers, solvers);
         break;

      case 4:
         nSharers = 2;
         sharers  = new Sharer*[nSharers];

         for (size_t i=0; i < nSolvers; i++) {
            from.push_back(solvers[i]);
         }
         sharers[0] = new Sharer(0, new SimpleSharing(), from, solvers);

         from.clear();
         from.push_back(solvers[nSolvers]);
         sharers[1] = new Sharer(1, new HordeSatSharing(), from, solvers);
         break;

      case 5:
         nSharers = 1;
         sharers = new Sharer*[nSharers];
         sharers[0] = new Sharer(0, new DistributionSharing(), solvers, solvers);
         break;

      case 0 :
         break;
   }

   WorkingStrategy * childPF, *childCC;
   WorkingStrategy * localWorker;
   int maxMemorySolvers; double memoryUsed;
   // Working strategy creation
   switch(Parameters::getIntParam("wkr-strat", 1)) {
      case 1 :
         working = new Portfolio();
         for (size_t i = 0; i < cpus; i++) {
            working->addSlave(new SequentialWorker(solvers[i]));
         }
         break;

      case 2 :
         working = new CubeAndConquer(cpus);
         working->addSlave(new SequentialWorker(solvers[0]));
         break;

      case 3 :
         working = new Portfolio();

         childPF = new Portfolio();
         for (size_t i = 0; i < nSolvers; i++) {
            childPF->addSlave(new SequentialWorker(solvers[i]));
         }
         working->addSlave(childPF);

         childCC = new CubeAndConquer(cpus - nSolvers);
         childCC->addSlave(new SequentialWorker(solvers[nSolvers]));
         working->addSlave(childCC);
         break;

      case 4 :
         working = new DivideAndConquer();

         if(Parameters::getIntParam("copy-mode",1) == 2) {
            working->addSlave(new SequentialWorker(solvers[0]));
            for(size_t i = 1; i < cpus; i++) {
	            working->addSlave(new SequentialWorker(NULL));
            }
         } else {
            for(size_t i = 0; i < cpus; i++) {
	            working->addSlave(new SequentialWorker(solvers[i]));
            }
         }
         break;
      
      // case 5 is used in the original painless for unknown reasons
      case 6:
         if (mpiRank == 0) {
            working = new Portfolio();
            for (int i = 0; i < mpiSize; i++) {
               working->addSlave(new Dispatcher(i));
            }
         }

         localWorker = new LocalWorker(mpiRank);
         for (size_t i = 0; i < cpus; i++) {
            localWorker->addSlave(new SequentialWorker(solvers[i]));
         }
         break;

      case 0 :
         break;
   }

   // Init the management of clauses
   ClauseManager::initClauseManager();

   // Start a thread for the communicator
   std::thread commThread([&] {
      while (globalEnding == false) {
         sleep(1);
         
         // Handle incoming MPI messages
         MpiComm::getInstance()->receiveIncomingMsg();
         
         // Inter-process clause sharing
         if (globalEnding == false)
            MpiComm::getInstance()->exportLearnedClauses();
      }
   });

   // Launch working
   vector<int> cube;
   if (working) {
      working->solve(cube);
   }

   // Wait until end or timeout
   int timeout   = Parameters::getIntParam("t", -1);
   int maxMemory = Parameters::getIntParam("max-memory", -1) * 1024 * 1024;

   while(globalEnding == false) {
      sleep(1);

      if (maxMemory > 0 && getMemoryUsed() > maxMemory) {
         cout << "c Memory used is going too large!!!!" << endl;
      }

      if (timeout > 0 && getRelativeTime() >= timeout) {
         globalEnding = true;
         working->setInterrupt();
      }   
   }

   // Broadcast the result to all peers
   if (mpiRank == 0) {
      MpiComm::getInstance()->updateWorkingStatus();
   }

   // Get the consumed time
   double consumedTime = getRelativeTime();

   if (!mpiRank) {
      // Print solver stats
      //SolverFactory::printStats(solvers);

      // Print the result and the model if SAT
      if (finalResult == SAT) {
         printf("s SATISFIABLE\n");

         if (Parameters::isSet("no-model") == false) {
            printModel(finalModel);
         }
      } else if (finalResult == UNSAT) {
         printf("s UNSATISFIABLE\n");
      } else {
         printf("s UNKNOWN\n");
      }
      printf("t consumed time: %f\n", consumedTime);
   }

   MPI_Barrier(MPI_COMM_WORLD);

   // Delete sharers
   for (int i=0; i < nSharers; i++) {
      delete sharers[i];
   }
   delete sharers;

   // Delete working strategy
   delete working;

   // Delete shared clauses
   ClauseManager::joinClauseManager();

   commThread.join();
   
   // Clean MPI buffer and finalize MPI
   MpiComm::getInstance()->cleanReceivingBuffer();
   MPI_Finalize();

   // MPI will show warnings if non-zero value is returned
   return 0;
}
