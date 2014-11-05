Approximation PageRank for GraphLab.

=====================================

This repository contains all the GraphLab code with the following two changes:

1) The synchronous engine was slightly changed to support: (i) optional vertex data synchronization, (ii) synchronization of a replica with a probability defined by user, and (iii) user program gets the actual fraction of replicas that were synchronized ("activated").

2) Approximation PageRank application: "rw_pagerank". The algorithm is based on random walks.

=====================================

Execution example of the rw_pagerank application:

mpiexec -n 20 -hostfile ~/machines rw_pagerank --graph grap_file --rwnum 800000 --maxwait 4 --replicap 0.7

This command executes the rw_pagerank application on the cluster of 20 machines. The directed graph should be provided in the graph_file in the "tsv" format. The application is launched with 800000 initial random walks, 4 iterations, and the replica activation probability 0.7.

======================================

The new synchronous engine may be used by any other application in the following way:

1) Define the graphlab options object:

graphlab::graphlab_options rw_opts;


2) Set the optional "vertex_data_sync" parameter using:

rw_opts.engine_args.set_option("vertex_data_sync",false); 

I.e., here we ask from the synchronous engine not to synchronize vertex data.


3) Set the optional "activation_prob" parameter using:

rw_opts.engine_args.set_option("activation_prob",replica_activation_prob);


4) If "activation_prob" was set (to a value less than 1), add the following parameter to the vertex program:

public:
        float frac_of_active_replicas;

This variable will be set by the synchronous engine and will represent the actual fraction of replicas that were activated for the "scatter" in the current iteration.

=========================================




