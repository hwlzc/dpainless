#!/bin/bash
/usr/sbin/sshd -D &

PATH="$PATH:/opt/openmpi/bin/"
BASENAME="${0##*/}"
log () {
  echo "${BASENAME} - ${1}"
}
HOST_FILE_PATH="/tmp/hostfile"
#aws s3 cp $S3_INPUT $SCRATCH_DIR
#tar -xvf $SCRATCH_DIR/*.tar.gz -C $SCRATCH_DIR

sleep 2
echo main node: ${AWS_BATCH_JOB_MAIN_NODE_INDEX}
echo this node: ${AWS_BATCH_JOB_NODE_INDEX}
echo Downloading problem from S3: ${COMP_S3_PROBLEM_PATH}
log "=======cat ~/.aws/credentials"
cat ~/.aws/credentials
log "=======cat ~/.aws/credentials"
if [[ "${COMP_S3_PROBLEM_PATH}" == *".xz" ]];
then
  aws s3 cp s3://${S3_BKT}/${COMP_S3_PROBLEM_PATH} test.cnf.xz
  unxz test.cnf.xz
elif [[ "${COMP_S3_PROBLEM_PATH}" == *".bz2" ]];
then
  aws s3 cp s3://${S3_BKT}/${COMP_S3_PROBLEM_PATH} test.cnf.bz2
  bzip2 -d test.cnf.bz2
else
  aws s3 ls s3://${S3_BKT}/${COMP_S3_PROBLEM_PATH}
  aws s3 cp s3://${S3_BKT}/${COMP_S3_PROBLEM_PATH} test.cnf
fi

# Set child by default switch to main if on main node container
NODE_TYPE="child"
if [ "${AWS_BATCH_JOB_MAIN_NODE_INDEX}" == "${AWS_BATCH_JOB_NODE_INDEX}" ]; then
  log "Running synchronize as the main node"
  NODE_TYPE="main"
fi

# wait for all nodes to report
wait_for_nodes () {
  log "Running as master node"

  touch $HOST_FILE_PATH
  ip=$(/sbin/ip -o -4 addr list eth0 | awk '{print $4}' | cut -d/ -f1)

  availablecores=$(nproc)
  log "master details -> $ip:$availablecores"
  log "main IP: $ip"
#  echo "$ip slots=$availablecores" >> $HOST_FILE_PATH
  echo "$ip availablecores=$availablecores" >> $HOST_FILE_PATH
  lines=$(ls -dq /tmp/hostfile* | wc -l)
  log "AWS_BATCH_JOB_NUM_NODES=$AWS_BATCH_JOB_NUM_NODES"
  index=1
  while [ "${AWS_BATCH_JOB_NUM_NODES}" -gt "${lines}" ]
  do
    cat $HOST_FILE_PATH
    lines=$(ls -dq /tmp/hostfile* | wc -l)

    log "====index=$index  $lines out of $AWS_BATCH_JOB_NUM_NODES nodes joined, check again in 1 second"
    sleep 1
    ((index++))
#    lines=$(sort $HOST_FILE_PATH|uniq|wc -l)
  done

  log "=======out of while begin to supervised-scripts/make_combined_hostfile"
  # All of the hosts report their IP and number of processors. Combine all these
  # into one file with the following script:
  supervised-scripts/make_combined_hostfile.py ${ip}
  log "=======cat combined_hostfile"
  cat combined_hostfile

  cat combined_hostfile |awk '{print$1}' >temp;cat temp >combined_hostfile;rm -rf temp
  log "=======final cat combined_hostfile"
  cat combined_hostfile
  node_num=$(cat combined_hostfile|wc -l)
  np=$((node_num*4))
  log "======run cmd= mpirun --mca btl_tcp_if_include eth0 --allow-run-as-root  --hostfile combined_hostfile --map-by node -np ${np} /painless-v2/painless  -t=1000 -d=5 -c=5 -wkr-strat=6 -lbd-limit=3 -solver=maple -shr-strat=5 -shr-group=10 test.cnf "
  # REPLACE THE FOLLOWING LINE WITH YOUR PARTICULAR SOLVER
  time mpirun --mca btl_tcp_if_include eth0 --allow-run-as-root  --hostfile combined_hostfile --map-by node -np ${np} /painless-v2/painless  -t=1000 -d=5 -c=4 -wkr-strat=6 -lbd-limit=2 -solver=maple -shr-strat=5 -shr-group=10 test.cnf
}

# Fetch and run a script
report_to_master () {
  # get own ip and num cpus
  #
  ip=$(/sbin/ip -o -4 addr list eth0 | awk '{print $4}' | cut -d/ -f1)


  availablecores=$(nproc)

  log "I am a child node -> $ip:$availablecores, reporting to the master node -> ${AWS_BATCH_JOB_MAIN_NODE_PRIVATE_IPV4_ADDRESS}"

#  echo "$ip slots=$availablecores" >> $HOST_FILE_PATH${AWS_BATCH_JOB_NODE_INDEX}
  echo "$ip availablecores=$availablecores" >> $HOST_FILE_PATH${AWS_BATCH_JOB_NODE_INDEX}
  ping -c 3 ${AWS_BATCH_JOB_MAIN_NODE_PRIVATE_IPV4_ADDRESS}
  
  sleep 5
  log "ls -l /tmp cat $HOST_FILE_PATH${AWS_BATCH_JOB_NODE_INDEX} =================================begin"
  ls -l /tmp
  cat $HOST_FILE_PATH${AWS_BATCH_JOB_NODE_INDEX}
  log "ls -l /tmp cat $HOST_FILE_PATH${AWS_BATCH_JOB_NODE_INDEX} =================================end"
  log "scp cmd=scp $HOST_FILE_PATH${AWS_BATCH_JOB_NODE_INDEX} ${AWS_BATCH_JOB_MAIN_NODE_PRIVATE_IPV4_ADDRESS}:$HOST_FILE_PATH${AWS_BATCH_JOB_NODE_INDEX}"
  until scp $HOST_FILE_PATH${AWS_BATCH_JOB_NODE_INDEX} ${AWS_BATCH_JOB_MAIN_NODE_PRIVATE_IPV4_ADDRESS}:$HOST_FILE_PATH${AWS_BATCH_JOB_NODE_INDEX}
  do
    echo "Sleeping 5 seconds and trying again"
    sleep 5
  done

  
  log "done! goodbye"
  ps -ef | grep sshd
  tail -f /dev/null
}
##
#
# Main - dispatch user request to appropriate function
log "NODE_TYPE=$NODE_TYPE"
case $NODE_TYPE in
  main)
    wait_for_nodes "${@}"
    ;;

  child)
    report_to_master "${@}"
    ;;

  *)
    log $NODE_TYPE
    usage "Could not determine node type. Expected (main/child)"
    ;;
esac

