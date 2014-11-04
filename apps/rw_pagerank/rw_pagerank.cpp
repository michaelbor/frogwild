#include <string>
#include <graphlab.hpp>
#include <math.h>


double prob_to_start_rw;
int num_of_rw_per_node_to_start;
int max_wait_iter;

struct vertex_data_type {

    unsigned int pagerank;

    void save(graphlab::oarchive& oarc) const {
        oarc << pagerank;
    }
    void load(graphlab::iarchive& iarc) {
        iarc >> pagerank;
    }
};


typedef graphlab::distributed_graph<vertex_data_type, graphlab::empty> graph_type;
void init_vertex(graph_type::vertex_type& vertex) { vertex.data().pagerank = 0; }
typedef int my_message_type;


class rw_pagerank_program : public graphlab::ivertex_program<graph_type, graphlab::empty,my_message_type>, public graphlab::IS_POD_TYPE {

    public:
        int received_msg;
        float frac_of_active_replicas;//this variable will by set by the engine
    
        void init(icontext_type& context, vertex_type& vertex, const my_message_type& msg){
            if(msg == -1)
                received_msg = (num_of_rw_per_node_to_start + (graphlab::random::rand01() < prob_to_start_rw)? 1 : 0);
            else
                received_msg = msg;
        }


        edge_dir_type gather_edges(icontext_type& context,
                const vertex_type& vertex) const {
            return graphlab::NO_EDGES;
        }


        void apply(icontext_type& context, vertex_type& vertex,
                const graphlab::empty& empty) {

            if(context.iteration() >= max_wait_iter && received_msg > 0){
                vertex.data().pagerank += received_msg;
                received_msg = 0;
            } 
        }


        edge_dir_type scatter_edges(icontext_type& context,
                const vertex_type& vertex) const {
            if (vertex.num_out_edges() >  0 && received_msg > 0){	
                return graphlab::OUT_EDGES;
            }
            else return graphlab::NO_EDGES;
        }


        void scatter(icontext_type& context, const vertex_type& vertex,
                edge_type& edge) const {

            int rw_to_send;
            float num_of_active_edges = frac_of_active_replicas*(float)vertex.num_out_edges();
            
            if(num_of_active_edges < 1){
                float tmp = (float)received_msg / num_of_active_edges;
                int to_send_int = (int)tmp;
                rw_to_send = to_send_int + ((graphlab::random::rand01() < (tmp-to_send_int))? 1 : 0);
                context.signal(edge.target(),rw_to_send);
            }
            else{
                float zero_prob = pow(1-1/num_of_active_edges,received_msg);
                if(graphlab::random::rand01() >= zero_prob){
                    float tmp = (float)received_msg / num_of_active_edges / (1-zero_prob);
                    int to_send_int = (int)tmp;
                    rw_to_send = to_send_int + ((graphlab::random::rand01() < (tmp-to_send_int))? 1 : 0);
                    context.signal(edge.target(),rw_to_send);
                }
            }

        }
}; 



class graph_writer {
    public:
        std::string save_vertex(graph_type::vertex_type v) {
            std::stringstream strm;
            strm << v.id() << "\t" <<v.data().pagerank <<"\n";
            return strm.str();
        }
        std::string save_edge(graph_type::edge_type e) { return ""; }
};




int main(int argc, char* argv[]) {

    graphlab::command_line_options clopts("RW PageRank algorithm.");
    graphlab::graphlab_options rw_opts; 
    std::string graph_file;

    clopts.attach_option("graph", graph_file, "The graph file.");
    
    int rw_num = 800000;
    clopts.attach_option("rwnum", rw_num,
            "number of random walks (not including additional walks created for resets)");

    max_wait_iter = 5;
    clopts.attach_option("maxwait", max_wait_iter,
            "max num of iterations to wait");


    float replica_activation_prob = 1;
    clopts.attach_option("replicap", replica_activation_prob,
            "fraction of active replicas for scatter");


    graphlab::mpi_tools::init(argc, argv);
    graphlab::distributed_control dc;
    graph_type graph(dc);
    global_logger().set_log_level(LOG_INFO);

    if(!clopts.parse(argc, argv)) {
        dc.cout() << "Error in parsing command line arguments." << std::endl;
        return EXIT_FAILURE;
    }

    if (graph_file.length() == 0) {
        dc.cout() << "No graph file given... exiting... " << std::endl;
        clopts.print_description();
        return 0;
    }


    rw_opts.engine_args.set_option("activation_prob",replica_activation_prob);
    rw_opts.engine_args.set_option("vertex_data_sync",false);
    
    max_wait_iter --;

    graph.load_format(graph_file, "tsv");
    graph.finalize();

    num_of_rw_per_node_to_start = rw_num/graph.num_vertices();

    prob_to_start_rw = ((double)rw_num - graph.num_vertices()*num_of_rw_per_node_to_start)/graph.num_vertices();

    graph.transform_vertices(init_vertex);

    graphlab::synchronous_engine<rw_pagerank_program> engine_rw(dc, graph, rw_opts);

    if(max_wait_iter > -1) 
        engine_rw.signal_all(-1);

    engine_rw.start();

    const double runtime = engine_rw.elapsed_seconds();
    dc.cout() << "Finished RW Pagerank in " << runtime
        << " seconds." << " total MB sent by master:  "<< dc.network_megabytes_sent() << std::endl;


    graph.save("output",
            graph_writer(),
            false, // set to true if each output file is to be gzipped
            true, // whether vertices are saved
            false); // whether edges are saved


    graphlab::mpi_tools::finalize();


    std::ofstream myfile;
    char fname[20];
    sprintf(fname,"mystats_%d.txt",dc.procid());
    myfile.open (fname);
    double total_compute_time = 0;
    for (size_t i = 0;i < engine_rw.per_thread_compute_time.size(); ++i) {
        total_compute_time += engine_rw.per_thread_compute_time[i];
    }
    myfile << total_compute_time << "\t"<< dc.network_bytes_sent() <<"\t"<<runtime <<"\n";

    myfile.close();

    return EXIT_SUCCESS;
}





