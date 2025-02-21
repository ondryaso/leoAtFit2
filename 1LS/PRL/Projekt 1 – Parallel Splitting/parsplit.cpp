/**
 * @file parsplit.cpp
 * @brief An implementation of a parallel splitting algorithm over MPI.
 * @author Ondřej Ondryáš <xondry02@stud.fit.vut.cz>
 * @date 2023
 */

#include <iostream>
#include <mpi.h>
#include <vector>
#include <fstream>
#include <iterator>

/** When 1, prints the loaded numbers. */
#define PRINT_INPUT 0
/** When 1, exits if there are not between 8 and 64 numbers in the input file. */
#define CHECK_BOUNDS 0

using std::cout;
using std::vector;
using std::endl;

vector<uint8_t> load_data();

void print_vector(vector<uint8_t> const &v);

void gather_part(vector<uint8_t> const &data, const unsigned int *lengths, int offset);

void perform_split(vector<uint8_t> const &data, uint8_t median);

void print_eq(const unsigned int *lengths, uint8_t median);

void run_root(int nproc);

void run_node();

int main(int argc, char **argv) {
    MPI::Init(argc, argv);

    int rank = MPI::COMM_WORLD.Get_rank();
    int nproc = MPI::COMM_WORLD.Get_size();

    try {
        // the root process does special tasks like reading the input file
        if (rank == 0) {
            run_root(nproc);
        } else {
            run_node();
        }
    } catch (std::exception &e) {
        std::cerr << e.what() << std::endl;
        MPI::COMM_WORLD.Abort(1);
        MPI::Finalize();
        return 1;
    }

    MPI::Finalize();

    return 0;
}

/**
 * Runs the algorithm for the root process. Reads the input file and distributes it across the processes.
 * @param nproc The number of processes (that is, the size of the world communicator).
 */
void run_root(int nproc) {
    auto data = load_data();

#if PRINT_INPUT
    cout << "INPUT: ";
    print_vector(data);
    cout << endl;
#endif

    // each process gets equal part of the data
    int num_elements_per_p = static_cast<int>(data.size()) / nproc;
    int mod = static_cast<int>(data.size()) % nproc;

    // the assignment specifies that the number of elements will be divisible by the number of processes
    if (mod != 0) {
        throw std::runtime_error("number of elements isn't divisible by number of processes");
    }

    // the assignment specifies that there will always be 8 to 64 elements to split
#if CHECK_BOUNDS
    if (data.size() < 8 || data.size() > 64) {
        throw std::runtime_error("number of elements is not between 8 and 64");
    }
#endif

    uint8_t median = data[(data.size() - 1) / 2];
    // with the given spec, this could be array of uint8_t as num_elements_per_p would never be >255
    // however, sending extra 6 B costs nothing and why not support longer lists when we can
    int metadata[2] = {(int) median, num_elements_per_p};
    vector<uint8_t> data_chunk(num_elements_per_p);

    // broadcast the metadata (median and number of elements per process)
    MPI::COMM_WORLD.Bcast(metadata, 2, MPI::INT, 0);

    // send the chunks of the data
    MPI::COMM_WORLD.Scatter(data.data(), num_elements_per_p, MPI::UNSIGNED_CHAR, data_chunk.data(),
                            num_elements_per_p, MPI::UNSIGNED_CHAR, 0);

    // the root also handles its own chunk of data
    perform_split(data_chunk, median);
}

/**
 * Runs the algorithm for a non-root process.
 */
void run_node() {
    // receive median and number of elements
    int metadata[2];
    MPI::COMM_WORLD.Bcast(metadata, 2, MPI::INT, 0);

    uint8_t median = static_cast<uint8_t>(metadata[0]);
    int num_elements = metadata[1];

    // receive elements
    vector<uint8_t> data(num_elements);
    MPI::COMM_WORLD.Scatter(nullptr, 0, MPI::UNSIGNED_CHAR, data.data(),
                            num_elements, MPI::UNSIGNED_CHAR, 0);

    // commence the calculation
    perform_split(data, median);
}

/**
 * Splits a vector of numbers based on a median value to three vectors of numbers lower, equal and
 * greater than the median. Gathers the resulting lists lengths across all processes.
 * Calls gather_part() to gather the results in the root process and print it out.
 * @param data The vector of numbers.
 * @param median The median value to split by.
 */
void perform_split(vector<uint8_t> const &data, uint8_t median) {
    vector<uint8_t> l, g;
    unsigned int e = 0; // number of encounters of the median

    for (auto const &x: data) {
        if (x == median) {
            e++;
        } else if (x > median) {
            g.push_back(x);
        } else {
            l.push_back(x);
        }
    }

    int rank = MPI::COMM_WORLD.Get_rank();
    int nproc = MPI::COMM_WORLD.Get_size();

    // the L/E/G lengths are gathered together
    unsigned int lengths[3]{(unsigned int) l.size(), e, (unsigned int) g.size()};
    // so the gather_recv_buf is filled with a sequence of P0_L P0_E P0_G P1_L P1_E P1_G P2_L etc.
    unsigned int *gather_recv_buf = nullptr;

    if (rank == 0) {
        gather_recv_buf = new unsigned int[3 * nproc];
    }

    try {
        MPI::COMM_WORLD.Gather(lengths, 3, MPI::UNSIGNED, gather_recv_buf,
                               3, MPI::UNSIGNED, 0);

        // gather and print L-values
        gather_part(l, gather_recv_buf, 0);

        // E-values don't have to be sent over in an array as they are always... the same value
        // it suffices to sum the already gathered E counts stored in the gather_recv_buf
        if (rank == 0) {
            print_eq(gather_recv_buf, median);
        }

        // gather and print G-values
        gather_part(g, gather_recv_buf, 2);
    } catch (std::exception &exception) {
        delete[] gather_recv_buf;
        throw;
    }

    delete[] gather_recv_buf;
}

/**
 * Gathers and prints a part of the split list, that is, one of the resulting sublists L/G.
 * The E sublist is generated using print_eq() instead as it doesn't require sending over arrays of the same value.
 *
 * The root process gathers the results from the others. Values in @p lengths specify the expected
 * lengths of received data from each process.
 *
 * The other processes just send their data to the Gatherv routine.
 * @param data The data to gather.
 * @param lengths An array of result lengths from each process. It contains uint triplets:
 * { P1_L, P1_E, P1_G, P2_L, P2_E, etc. }.
 * @param offset An offset that specifies which component of the triplets stored in @p lengths is used.
 */
void gather_part(vector<uint8_t> const &data, const unsigned int *lengths, int offset) {
    int rank = MPI::COMM_WORLD.Get_rank();

    if (rank == 0) {
        if (lengths == nullptr)
            throw std::runtime_error("invalid parameter");

        // calculate displacements (offsets to the result array) for the results from each process
        // displacements are basically prefix sums of the lengths reported by the processes

        int nproc = MPI::COMM_WORLD.Get_size();
        int total_length;
        int displacements[nproc];
        int recv_counts[nproc];

        displacements[0] = 0;
        recv_counts[0] = (int) lengths[offset];
        total_length = recv_counts[0];

        for (int i = 1; i < nproc; i++) {
            recv_counts[i] = (int) lengths[offset + i * 3];
            total_length += recv_counts[i];
            displacements[i] = displacements[i - 1] + (int) lengths[offset + (i - 1) * 3];
        }

        vector<uint8_t> res(total_length);
        MPI::COMM_WORLD.Gatherv(data.data(), data.size(), MPI::UNSIGNED_CHAR, res.data(),
                                recv_counts, displacements, MPI::UNSIGNED_CHAR, 0);

        char id = offset == 0 ? 'L' : (offset == 1 ? 'E' : 'G');
        cout << id << ": ";
        print_vector(res);
        cout << endl;
    } else {
        MPI::COMM_WORLD.Gatherv(data.data(), data.size(), MPI::UNSIGNED_CHAR, nullptr,
                                nullptr, nullptr, MPI::UNSIGNED_CHAR, 0);

    }
}

/**
 * Prints the E sublist, that is, the median value repeated N times where N is a sum of the numbers of encounters
 * of the median value reported by each process.
 *
 * @param lengths An array of result lengths from each process. It contains uint triplets:
 * { P1_L, P1_E, P1_G, P2_L, P2_E, etc. }.
 * @param median The value to print.
 */
void print_eq(const unsigned int *lengths, uint8_t median) {
    cout << "E: [";
    int nproc = MPI::COMM_WORLD.Get_size();

    unsigned int total = 0;
    for (int p = 0; p < nproc; p++) {
        total += lengths[1 + p * 3];
    }
    for (unsigned int l = 0; l < total; l++) {
        cout << (int) median;
        if (l != total - 1) {
            cout << ",";
        }
    }

    cout << ']' << endl;
}

/**
 * Loads the input file called "numbers" from the working directory.
 * @return A vector of loaded byte values.
 * @throws std::runtime_error The file cannot be opened.
 */
vector<uint8_t> load_data() {
    std::ifstream fs("numbers", std::ios::binary | std::ios::in | std::ios::ate);
    if (!fs.is_open()) {
        throw std::runtime_error("unable to open the input file");
    }

    // the file has been opened with the 'seek to end after opening file'
    // so the current position should equal the file size
    unsigned size = fs.tellg();

    std::vector<uint8_t> data_cont;
    data_cont.reserve(size);

    fs.seekg(0, std::ios::beg);
    data_cont.insert(data_cont.begin(), std::istreambuf_iterator<char>(fs),
                     std::istreambuf_iterator<char>());

    fs.close();
    return data_cont;
}

/**
 * Prints a vector of unsigned byte values (in their numeric representations).
 * @param v The vector to print.
 */
void print_vector(vector<uint8_t> const &v) {
    cout << '[';
    auto size = v.size();
    for (int i = 0; i < size; i++) {
        cout << (int) v[i];
        if (i != size - 1) {
            cout << ",";
        }
    }
    cout << ']';
}
