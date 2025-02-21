/**
 * @file parkmeans.cc
 * @brief A parallel implementation of the k-means algorithm.
 * @author Ondřej Ondryáš <xondry02@stud.fit.vut.cz>
 * @date 2023-04-22
 */

#include <mpi.h>
#include <iostream>
#include <vector>
#include <fstream>
#include <iterator>
#include <random>

#define USE_SIGNED_VALUES 1
#if USE_SIGNED_VALUES
using val_t = int8_t;
#define VAL_MPI_TYPE MPI::SIGNED_CHAR
#else
using val_t = uint8_t;
#define VAL_MPI_TYPE MPI::UNSIGNED_CHAR
#endif

constexpr int CLUSTERS = 4;

using std::cout;
using std::vector;
using std::endl;
using MPI::COMM_WORLD;

/**
 * Performs the k-means algorithm.
 */
void run();

/**
 * Prints the results.
 * @param [in] data An array of the input data (of length @p n).
 * @param [in] cluster_means An array of final cluster means (of length CLUSTERS).
 * @param [in] cluster_indices An array which contains an index of the target cluster for each value
 *                             in the input data (of length @p n).
 * @param n The number of input values.
 */
void print_results(const val_t *data, const float *cluster_means, const int *cluster_indices, int n);

/**
 * Loads the input file called "numbers" from the working directory.
 * @param [in] data An array to fill with the loaded values.
 * @param n The requested number of values.
 * @throws std::runtime_error The file cannot be opened or there are less than @p n values in the file.
 */
void load_data(val_t *data, int n);

/**
 * Generates a random boolean value with uniform distribution.
 * Not reentrant.
 */
bool random_bool();

int main(int argc, char **argv) {
    MPI::Init(argc, argv);

    try {
        run();
    } catch (std::exception &e) {
        std::cerr << e.what() << std::endl;
        MPI::COMM_WORLD.Abort(1);
        MPI::Finalize();
        return 1;
    }

    MPI::Finalize();

    return 0;
}

void run() {
    int rank = COMM_WORLD.Get_rank();
    int n = COMM_WORLD.Get_size();

    // root loads data
    val_t data[n];
    if (rank == 0) {
        load_data(data, n);
    }

    // root broadcasts, everyone gets data
    COMM_WORLD.Bcast(data, n, VAL_MPI_TYPE, 0);

    float means[CLUSTERS];
    for (int i = 0; i < CLUSTERS; i++) {
        means[i] = data[i];
    }

    // another possible solution:
    // val_t recv_val;
    // COMM_WORLD.Scatter(data, 1, VAL_MPI_TYPE, &recv_val, 1, VAL_MPI_TYPE, 0);
    // COMM_WORLD.Bcast(means, CLUSTERS, VAL_MPI_TYPE, 0);
    // auto val = (float) recv_val;

    float val = data[rank];
    bool converged = false;
    int min_index = 0;
    float sums[CLUSTERS], sums_red[CLUSTERS];
    int counts[CLUSTERS], counts_red[CLUSTERS];

    while (!converged) {
        // find the cluster to put the value in this process into
        // each process creates an array 'sums' with their value in the position of the corresponding cluster
        // and zeros in all th others; it also creates an array 'counts' with a 1 in that position
        std::fill_n(sums, CLUSTERS, 0.0f);
        std::fill_n(counts, CLUSTERS, 0);

        min_index = 0;
        float min_dist = std::abs(val - means[0]);
        for (int j = 1; j < CLUSTERS; j++) {
            float dist = std::abs(val - means[j]);
            if (dist < min_dist || (dist == min_dist && random_bool())) {
                min_dist = dist;
                min_index = j;
            }
        }

        sums[min_index] += val;
        counts[min_index] = 1;

        // the items in the arrays now get summed over all the processes
        // we get two arrays with sums of values and numbers of values for each cluster (=> each new mean)
        COMM_WORLD.Reduce(sums, sums_red, CLUSTERS, MPI::FLOAT, MPI_SUM, 0);
        COMM_WORLD.Reduce(counts, counts_red, CLUSTERS, MPI::INT, MPI_SUM, 0);

        // compute new means in the root
        if (rank == 0) {
            converged = true;
            for (int i = 0; i < CLUSTERS; i++) {
                if (counts_red[i] > 0) {
                    float new_mean = sums_red[i] / (float) counts_red[i];

                    float diff = std::abs(new_mean - means[i]);
                    if (diff > 1e-6) {
                        converged = false;
                    }

                    means[i] = new_mean;
                }
            }
        }

        // distribute the new means and information about convergence from the root
        COMM_WORLD.Bcast(&converged, 1, MPI::BOOL, 0);
        if (!converged)
            COMM_WORLD.Bcast(means, CLUSTERS, MPI::FLOAT, 0);
    }

    // the last value of min_index in each process contains the last cluster assigned to each value
    // collect the resulting cluster indices for each value
    int min_indices[n];
    COMM_WORLD.Gather(&min_index, 1, MPI::INT, min_indices, 1, MPI::INT, 0);

    // the root process prints out the results
    if (rank == 0) {
        print_results(data, means, min_indices, n);
    }
}

void load_data(val_t *data, int n) {
    std::ifstream fs("numbers", std::ios::binary | std::ios::in);
    if (!fs.is_open()) {
        throw std::runtime_error("unable to open the input file");
    }

    fs.read((char *) data, n);
    if (fs.eof()) {
        throw std::runtime_error("not enough numbers in file");
    }

    fs.close();
}

void print_results(const val_t *data, const float *cluster_means, const int *cluster_indices, int n) {
    val_t tmp[n];
    int n_items_in_cluster;

    for (int i = 0; i < CLUSTERS; i++) {
        cout << '[' << cluster_means[i] << "] ";

        n_items_in_cluster = 0;
        for (int j = 0; j < n; j++) {
            if (cluster_indices[j] == i) {
                tmp[n_items_in_cluster++] = data[j];
            }
        }

        for (int j = 0; j < n_items_in_cluster; j++) {
            cout << (int) tmp[j];
            if (j != (n_items_in_cluster - 1))
                cout << ", ";
        }

        cout << endl;
    }
}

bool random_bool() {
    static std::mt19937 rng(std::random_device{}());
    static std::uniform_int_distribution<int> dist(0, 1);
    return dist(rng) == 1;
}


