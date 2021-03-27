# dpainless

Distributed PArallel INstantiabLE Sat Solver

## Building

``` shell
cd painless-v2
make -j4
```

## Usage

``` shell
# stand-alone parallel
painless -d=1 -c=4 -wkr-strat=6 -lbd-limit=2 -solver=maple -shr-strat=5 test.cnf

# distributed parallel
mpirun -np $num_processes painless -d=5 -c=4 -wkr-strat=6 -lbd-limit=2 -solver=maple -shr-strat=5 -shr-group=10 test.cnf
```
