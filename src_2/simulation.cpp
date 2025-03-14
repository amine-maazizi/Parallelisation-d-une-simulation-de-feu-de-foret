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

void analyze_arg( int nargs, char* args[], ParamsType& params )
{
    if (nargs ==0) return;
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
        auto subkey = std::string(key,pos+11);
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
        std::string values =std::string(args[1]);
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
        std::string values =std::string(args[1]);
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

ParamsType parse_arguments( int nargs, char* args[] )
{
    if (nargs == 0) return {};
    if ( (std::string(args[0]) == "--help"s) || (std::string(args[0]) == "-h") )
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

    if ( (params.start.row >= params.discretization) || (params.start.column >= params.discretization) )
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

int main( int nargs, char* args[] )
{
    MPI_Comm commGlob;
    int nbp, rank;
    MPI_Init(&nargs, &args);
    MPI_Comm_dup(MPI_COMM_WORLD, &commGlob);
    MPI_Comm_size(commGlob, &nbp);
    MPI_Comm_rank(commGlob, &rank);

    auto params = parse_arguments(nargs-1, &args[1]);
    display_params(params);
    if (!check_params(params)) return EXIT_FAILURE;

    if (rank == 0) {
        auto displayer = Displayer::init_instance( params.discretization, params.discretization );
        SDL_Event event;
        MPI_Request req; 
        MPI_Status status;

        std::vector<std::uint8_t> vm_recv;
        std::vector<std::uint8_t> fm_recv;
        unsigned geometry;

        MPI_Irecv(&geometry, 1, MPI_UNSIGNED, 1, 100, commGlob, &req);
        MPI_Wait(&req, &status);
        vm_recv.resize(geometry * geometry);
        fm_recv.resize(geometry * geometry);

        bool running = true;
        while (running)
        {
            MPI_Request reqs[3];
            MPI_Irecv(vm_recv.data(), vm_recv.size(), MPI_UINT8_T, MPI_ANY_SOURCE, 101, commGlob, &reqs[0]);
            MPI_Irecv(fm_recv.data(), fm_recv.size(), MPI_UINT8_T, MPI_ANY_SOURCE, 102, commGlob, &reqs[1]);
            MPI_Irecv(&running, 1, MPI_CXX_BOOL, MPI_ANY_SOURCE, 103, commGlob, &reqs[2]);
            MPI_Waitall(3, reqs, MPI_STATUSES_IGNORE);

            displayer->update(vm_recv, fm_recv);
    
            if (SDL_PollEvent(&event) && event.type == SDL_QUIT)
                break;
            
            // std::this_thread::sleep_for(0.1s);
        }    
    }
        
    else {
        auto simu = Model( params.length, params.discretization, params.wind, params.start);
        SDL_Event event;
        bool running = true;
        unsigned geometry = simu.geometry();
        MPI_Send(&geometry, 1, MPI_UNSIGNED, 0, 100, commGlob);


        std::chrono::time_point<std::chrono::high_resolution_clock> start_iter;
        std::chrono::duration<double> total_time{0};
        int iteration_count = 0;

        while (running)
        {
            start_iter = std::chrono::high_resolution_clock::now();
            running = simu.update();
            
            if ((simu.time_step() & 31) == 0) {
                std::cout << "Time step " << simu.time_step() << "\n===============" << std::endl;
                if (iteration_count > 0 && rank == 1) {
                    double temps_moyen = total_time.count() / iteration_count;
                    iteration_count = 0;
                    std::chrono::duration<double>::zero();
                    std::cout << "Temps global moyen pris par iteration en temps: " << temps_moyen << " seconds" << std::endl;
                }
            }
            
            MPI_Send(simu.vegetal_map().data(), simu.vegetal_map().size(), MPI_UINT8_T, 0, 101, commGlob);
            MPI_Send(simu.fire_map().data(), simu.fire_map().size(), MPI_UINT8_T, 0, 102, commGlob);
            MPI_Send(&running, 1, MPI_CXX_BOOL, 0, 103, commGlob);

            // TODO: N'oublie pas de la supprimé => fausse les résultats !
            // std::this_thread::sleep_for(0.1s);

            auto end_iter = std::chrono::high_resolution_clock::now();
            total_time += end_iter - start_iter;
            iteration_count++;
        }
    }

    MPI_Finalize();
    return EXIT_SUCCESS;
}