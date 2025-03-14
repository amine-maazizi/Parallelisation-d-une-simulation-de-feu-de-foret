#include <string>
#include <cstdlib>
#include <cassert>
#include <iostream>
#include <thread>
#include <chrono>
#include <mpi.h>

#include "model.hpp"
#include "display.hpp"

using namespace std::string_literals;
using namespace std::chrono_literals;

struct ParamsType
{
    double length{1.};
    unsigned discretization{20u};
    std::array<double,2> wind{0.,0.};
    Model::LexicoIndices start{10u,10u};
};

void analyze_arg(int nargs, char* args[], ParamsType& params)
{
    if (nargs == 0) return;
    std::string key(args[0]);
    if (key == "-l"s)
    {
        if (nargs < 2)
        {
            std::cerr << "Manque une valeur pour la longueur du terrain !" << std::endl;
            exit(EXIT_FAILURE);
        }
        params.length = std::stoul(args[1]);
        analyze_arg(nargs-2, &args[2], params);
        return;
    }
    auto pos = key.find("--longueur=");
    if (pos < key.size())
    {
        auto subkey = std::string(key, pos+11);
        params.length = std::stoul(subkey);
        analyze_arg(nargs-1, &args[1], params);
        return;
    }

    if (key == "-n"s)
    {
        if (nargs < 2)
        {
            std::cerr << "Manque une valeur pour le nombre de cases par direction pour la discrétisation du terrain !" << std::endl;
            exit(EXIT_FAILURE);
        }
        params.discretization = std::stoul(args[1]);
        analyze_arg(nargs-2, &args[2], params);
        return;
    }
    pos = key.find("--number_of_cases=");
    if (pos < key.size())
    {
        auto subkey = std::string(key, pos+18);
        params.discretization = std::stoul(subkey);
        analyze_arg(nargs-1, &args[1], params);
        return;
    }

    if (key == "-w"s)
    {
        if (nargs < 2)
        {
            std::cerr << "Manque une paire de valeurs pour la direction du vent !" << std::endl;
            exit(EXIT_FAILURE);
        }
        std::string values = std::string(args[1]);
        params.wind[0] = std::stod(values);
        auto pos = values.find(",");
        if (pos == values.size())
        {
            std::cerr << "Doit fournir deux valeurs séparées par une virgule pour définir la vitesse" << std::endl;
            exit(EXIT_FAILURE);
        }
        auto second_value = std::string(values, pos+1);
        params.wind[1] = std::stod(second_value);
        analyze_arg(nargs-2, &args[2], params);
        return;
    }
    pos = key.find("--wind=");
    if (pos < key.size())
    {
        auto subkey = std::string(key, pos+7);
        params.wind[0] = std::stoul(subkey);
        auto pos = subkey.find(",");
        if (pos == subkey.size())
        {
            std::cerr << "Doit fournir deux valeurs séparées par une virgule pour définir la vitesse" << std::endl;
            exit(EXIT_FAILURE);
        }
        auto second_value = std::string(subkey, pos+1);
        params.wind[1] = std::stod(second_value);
        analyze_arg(nargs-1, &args[1], params);
        return;
    }

    if (key == "-s"s)
    {
        if (nargs < 2)
        {
            std::cerr << "Manque une paire de valeurs pour la position du foyer initial !" << std::endl;
            exit(EXIT_FAILURE);
        }
        std::string values = std::string(args[1]);
        params.start.column = std::stod(values);
        auto pos = values.find(",");
        if (pos == values.size())
        {
            std::cerr << "Doit fournir deux valeurs séparées par une virgule pour définir la position du foyer initial" << std::endl;
            exit(EXIT_FAILURE);
        }
        auto second_value = std::string(values, pos+1);
        params.start.row = std::stod(second_value);
        analyze_arg(nargs-2, &args[2], params);
        return;
    }
    pos = key.find("--start=");
    if (pos < key.size())
    {
        auto subkey = std::string(key, pos+8);
        params.start.column = std::stoul(subkey);
        auto pos = subkey.find(",");
        if (pos == subkey.size())
        {
            std::cerr << "Doit fournir deux valeurs séparées par une virgule pour définir la vitesse" << std::endl;
            exit(EXIT_FAILURE);
        }
        auto second_value = std::string(subkey, pos+1);
        params.start.row = std::stod(second_value);
        analyze_arg(nargs-1, &args[1], params);
        return;
    }
}

ParamsType parse_arguments(int nargs, char* args[])
{
    if (nargs == 0) return {};
    if ((std::string(args[0]) == "--help"s) || (std::string(args[0]) == "-h"))
    {
        std::cout << 
R"RAW(Usage : simulation [option(s)]
  Lance la simulation d'incendie en prenant en compte les [option(s)].
  Les options sont :
    -l, --longueur=LONGUEUR     Définit la taille LONGUEUR (réel en km) du carré représentant la carte de la végétation.
    -n, --number_of_cases=N     Nombre n de cases par direction pour la discrétisation
    -w, --wind=VX,VY            Définit le vecteur vitesse du vent (pas de vent par défaut).
    -s, --start=COL,ROW         Définit les indices I,J de la case où commence l'incendie (milieu de la carte par défaut)
)RAW";
        exit(EXIT_SUCCESS);
    }
    ParamsType params;
    analyze_arg(nargs, args, params);
    return params;
}

bool check_params(ParamsType& params)
{
    bool flag = true;
    if (params.length <= 0)
    {
        std::cerr << "[ERREUR FATALE] La longueur du terrain doit être positive et non nulle !" << std::endl;
        flag = false;
    }

    if (params.discretization <= 0)
    {
        std::cerr << "[ERREUR FATALE] Le nombre de cellules par direction doit être positive et non nulle !" << std::endl;
        flag = false;
    }

    if ((params.start.row >= params.discretization) || (params.start.column >= params.discretization))
    {
        std::cerr << "[ERREUR FATALE] Mauvais indices pour la position initiale du foyer" << std::endl;
        flag = false;
    }
    
    return flag;
}

void display_params(ParamsType const& params)
{
    std::cout << "Parametres définis pour la simulation : \n"
              << "\tTaille du terrain : " << params.length << std::endl 
              << "\tNombre de cellules par direction : " << params.discretization << std::endl 
              << "\tVecteur vitesse : [" << params.wind[0] << ", " << params.wind[1] << "]" << std::endl
              << "\tPosition initiale du foyer (col, ligne) : " << params.start.column << ", " << params.start.row << std::endl;
}

int main(int nargs, char* args[]) {
    MPI_Init(&nargs, &args);
    int rank, nbp;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &nbp);

    auto params = parse_arguments(nargs-1, &args[1]);
    display_params(params);
    if (!check_params(params)) return EXIT_FAILURE;

    unsigned geometry = params.discretization;

    Model simu(params.length, geometry, params.wind, params.start, rank, nbp);

    std::chrono::time_point<std::chrono::high_resolution_clock> start_global, end_global;
    double total_time_global = 0.0;
    std::chrono::duration<double> total_time_iter{0};
    int iteration_count = 0;

    start_global = std::chrono::high_resolution_clock::now();

    std::vector<int> recvcounts, displs;
    std::vector<unsigned> local_rows(nbp), first_rows(nbp);
    std::vector<std::uint8_t> vm_recv, fm_recv;
    std::shared_ptr<Displayer> displayer;  
    if (rank == 0) {
        displayer = Displayer::init_instance(geometry, geometry);
        unsigned N = geometry;
        unsigned base_rows = N / nbp;
        unsigned extra_rows = N % nbp;
        unsigned offset = 0;
        for (int p = 0; p < nbp; p++) {
            local_rows[p] = base_rows + (static_cast<unsigned>(p) < extra_rows ? 1 : 0);
            first_rows[p] = offset;
            offset += local_rows[p];
        }
        recvcounts.resize(nbp);
        displs.resize(nbp);
        for (int p = 0; p < nbp; p++) {
            recvcounts[p] = local_rows[p] * geometry;
            displs[p] = first_rows[p] * geometry;
        }
        vm_recv.resize(geometry * geometry);
        fm_recv.resize(geometry * geometry);
    }

    bool local_running = true, global_running = true;
    while (global_running) {
        auto start_iter = std::chrono::high_resolution_clock::now();
        local_running = simu.update();

        unsigned local_rows_p = simu.m_local_rows;
        std::vector<std::uint8_t> local_vm(simu.vegetal_map().begin() + geometry,
                                          simu.vegetal_map().begin() + (local_rows_p + 1) * geometry);
        std::vector<std::uint8_t> local_fm(simu.fire_map().begin() + geometry,
                                          simu.fire_map().begin() + (local_rows_p + 1) * geometry);

        if (rank == 0) {
            MPI_Gatherv(local_vm.data(), local_vm.size(), MPI_UINT8_T,
                       vm_recv.data(), recvcounts.data(), displs.data(), MPI_UINT8_T,
                       0, MPI_COMM_WORLD);
            MPI_Gatherv(local_fm.data(), local_fm.size(), MPI_UINT8_T,
                       fm_recv.data(), recvcounts.data(), displs.data(), MPI_UINT8_T,
                       0, MPI_COMM_WORLD);
            displayer->update(vm_recv, fm_recv);
        } else {
            MPI_Gatherv(local_vm.data(), local_vm.size(), MPI_UINT8_T,
                       nullptr, nullptr, nullptr, MPI_UINT8_T,
                       0, MPI_COMM_WORLD);
            MPI_Gatherv(local_fm.data(), local_fm.size(), MPI_UINT8_T,
                       nullptr, nullptr, nullptr, MPI_UINT8_T,
                       0, MPI_COMM_WORLD);
        }

        MPI_Allreduce(&local_running, &global_running, 1, MPI_CXX_BOOL, MPI_LOR, MPI_COMM_WORLD);

        auto end_iter = std::chrono::high_resolution_clock::now();
        total_time_iter += end_iter - start_iter;
        iteration_count++;
    }

    end_global = std::chrono::high_resolution_clock::now();
    total_time_global = std::chrono::duration<double>(end_global - start_global).count();

    if (rank == 0) {
        double avg_iter_time = total_time_iter.count() / iteration_count;
        std::cout << "Temps global (rang " << rank << ") : " << total_time_global << " secondes\n";
        std::cout << "Temps moyen par itération : " << avg_iter_time << " secondes\n";
    }

    MPI_Finalize();
    return EXIT_SUCCESS;
}