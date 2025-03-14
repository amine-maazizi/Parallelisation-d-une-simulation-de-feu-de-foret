#pragma once
#include <cstdint>
#include <array>
#include <vector>
#include <unordered_map>
#include <mpi.h>

class Model {
public:
    struct LexicoIndices {
        unsigned row, column;
    };

    // rank et nbp pour la parallélisation
    Model(double t_length, unsigned t_discretization, std::array<double,2> t_wind,
          LexicoIndices t_start_fire_position, int rank, int nbp, double t_max_wind = 60.);

    Model(Model const&) = delete;
    Model(Model&&) = delete;
    ~Model() = default;

    Model& operator=(Model const&) = delete;
    Model& operator=(Model&&) = delete;

    bool update();

    unsigned geometry() const { return m_geometry; }
    // cartes locales
    std::vector<std::uint8_t> const& vegetal_map() const { return m_local_vegetation_map; }
    std::vector<std::uint8_t> const& fire_map() const { return m_local_fire_map; }
    std::size_t time_step() const { return m_time_step; }

    unsigned m_local_rows;

private:
    std::size_t get_index_from_lexicographic_indices(LexicoIndices t_lexico_indices) const;
    LexicoIndices get_lexicographic_from_index(std::size_t t_global_index) const;

    double m_length;
    double m_distance;
    std::size_t m_time_step = 0;
    unsigned m_geometry;
    std::array<double,2> m_wind{0.,0.};
    double m_wind_speed;
    double m_max_wind;

    // cartes locales avec fantômes
    std::vector<std::uint8_t> m_local_vegetation_map, m_local_fire_map;
    int m_rank, m_nbp;
    unsigned m_first_row, m_last_row;

    double p1{0.}, p2{0.};
    double alphaEastWest, alphaWestEast, alphaSouthNorth, alphaNorthSouth;
    std::unordered_map<std::size_t, std::uint8_t> m_fire_front;
};