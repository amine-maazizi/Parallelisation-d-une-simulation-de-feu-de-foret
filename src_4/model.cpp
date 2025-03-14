#include <stdexcept>
#include <cmath>
#include <iostream>
#include "model.hpp"

namespace {
    double pseudo_random(std::size_t index, std::size_t time_step) {
        std::uint_fast32_t xi = std::uint_fast32_t(index * (time_step + 1));
        std::uint_fast32_t r = (48271 * xi) % 2147483647;
        return r / 2147483646.;
    }

    double log_factor(std::uint8_t value) {
        return std::log(1. + value) / std::log(256);
    }
}

Model::Model(double t_length, unsigned t_discretization, std::array<double,2> t_wind,
             LexicoIndices t_start_fire_position, int rank, int nbp, double t_max_wind)
    : m_length(t_length),
      m_distance(-1),
      m_geometry(t_discretization),
      m_wind(t_wind),
      m_wind_speed(std::sqrt(t_wind[0] * t_wind[0] + t_wind[1] * t_wind[1])),
      m_max_wind(t_max_wind),
      m_rank(rank),
      m_nbp(nbp) {
    if (t_discretization == 0) {
        throw std::range_error("Le nombre de cases par direction doit être plus grand que zéro.");
    }
    m_distance = m_length / double(m_geometry);

    // Calcul des indices locaux
    unsigned N = m_geometry;
    unsigned base_rows = N / m_nbp;
    unsigned extra_rows = N % m_nbp;
    m_local_rows = base_rows + (m_rank < extra_rows ? 1 : 0);
    m_first_row = m_rank * base_rows + std::min(static_cast<unsigned>(m_rank), extra_rows);
    m_last_row = m_first_row + m_local_rows - 1;

    // Allocation des cartes locales avec fantômes
    unsigned local_size = (m_local_rows + 2) * m_geometry;
    m_local_vegetation_map.resize(local_size, 255u);
    m_local_fire_map.resize(local_size, 0u);

    // Initialisation du foyer
    if (m_first_row <= t_start_fire_position.row && t_start_fire_position.row < m_first_row + m_local_rows) {
        unsigned local_row = t_start_fire_position.row - m_first_row + 1;
        std::size_t local_index = local_row * m_geometry + t_start_fire_position.column;
        m_local_fire_map[local_index] = 255u;
        m_fire_front[local_index] = 255u;
    }

    // Initialisation des paramètres 
    constexpr double alpha0 = 4.52790762e-01;
    constexpr double alpha1 = 9.58264437e-04;
    constexpr double alpha2 = 3.61499382e-05;

    if (m_wind_speed < t_max_wind)
        p1 = alpha0 + alpha1 * m_wind_speed + alpha2 * (m_wind_speed * m_wind_speed);
    else
        p1 = alpha0 + alpha1 * t_max_wind + alpha2 * (t_max_wind * t_max_wind);
    p2 = 0.3;

    if (m_wind[0] > 0) {
        alphaEastWest = std::abs(m_wind[0] / t_max_wind) + 1;
        alphaWestEast = 1. - std::abs(m_wind[0] / t_max_wind);
    } else {
        alphaWestEast = std::abs(m_wind[0] / t_max_wind) + 1;
        alphaEastWest = 1. - std::abs(m_wind[0] / t_max_wind);
    }

    if (m_wind[1] > 0) {
        alphaSouthNorth = std::abs(m_wind[1] / t_max_wind) + 1;
        alphaNorthSouth = 1. - std::abs(m_wind[1] / t_max_wind);
    } else {
        alphaNorthSouth = std::abs(m_wind[1] / t_max_wind) + 1;
        alphaSouthNorth = 1. - std::abs(m_wind[1] / t_max_wind);
    }
}

bool Model::update() {
    // Échange des cellules fantômes
    if (m_rank > 0) {
        MPI_Sendrecv(&m_local_fire_map[m_geometry], m_geometry, MPI_UINT8_T, m_rank - 1, 0,
                     &m_local_fire_map[0], m_geometry, MPI_UINT8_T, m_rank - 1, 0,
                     MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    }
    if (m_rank < m_nbp - 1) {
        MPI_Sendrecv(&m_local_fire_map[m_local_rows * m_geometry], m_geometry, MPI_UINT8_T, m_rank + 1, 0,
                     &m_local_fire_map[(m_local_rows + 1) * m_geometry], m_geometry, MPI_UINT8_T, m_rank + 1, 0,
                     MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    }

    std::vector<std::size_t> fire_keys;
    for (const auto& pair : m_fire_front) {
        fire_keys.push_back(pair.first);
    }
    auto next_front = m_fire_front;

    for (size_t i = 0; i < fire_keys.size(); ++i) {
        std::size_t f = fire_keys[i];
        unsigned local_row = f / m_geometry;
        unsigned column = f % m_geometry;

        // Voisin du haut
        if (local_row > 1 || (local_row == 1 && m_rank > 0)) {
            std::size_t neighbor_f = f - m_geometry;
            double tirage = pseudo_random(f + m_time_step, m_time_step);
            double green_power = m_local_vegetation_map[neighbor_f];
            double correction = log_factor(m_fire_front[f]) * log_factor(green_power);
            if (tirage < alphaSouthNorth * p1 * correction) {
                m_local_fire_map[neighbor_f] = 255;
                next_front[neighbor_f] = 255;
            }
        }

        // Voisin du bas
        if (local_row < m_local_rows || (local_row == m_local_rows && m_rank < m_nbp - 1)) {
            std::size_t neighbor_f = f + m_geometry;
            double tirage = pseudo_random(f * 13427 + m_time_step, m_time_step);
            double green_power = m_local_vegetation_map[neighbor_f];
            double correction = log_factor(m_fire_front[f]) * log_factor(green_power);
            if (tirage < alphaNorthSouth * p1 * correction) {
                m_local_fire_map[neighbor_f] = 255;
                next_front[neighbor_f] = 255;
            }
        }

        // Voisins à droite et à gauche (pas de frontière horizontale)
        if (column < m_geometry - 1) {
            std::size_t neighbor_f = f + 1;
            double tirage = pseudo_random(f * 13427 * 13427 + m_time_step, m_time_step);
            double green_power = m_local_vegetation_map[neighbor_f];
            double correction = log_factor(m_fire_front[f]) * log_factor(green_power);
            if (tirage < alphaEastWest * p1 * correction) {
                m_local_fire_map[neighbor_f] = 255;
                next_front[neighbor_f] = 255;
            }
        }
        if (column > 0) {
            std::size_t neighbor_f = f - 1;
            double tirage = pseudo_random(f * 13427 * 13427 * 13427 + m_time_step, m_time_step);
            double green_power = m_local_vegetation_map[neighbor_f];
            double correction = log_factor(m_fire_front[f]) * log_factor(green_power);
            if (tirage < alphaWestEast * p1 * correction) {
                m_local_fire_map[neighbor_f] = 255;
                next_front[neighbor_f] = 255;
            }
        }

        // Mise à jour du feu
        if (m_fire_front[f] == 255) {
            double tirage = pseudo_random(f * 52513 + m_time_step, m_time_step);
            if (tirage < p2) {
                m_local_fire_map[f] >>= 1;
                next_front[f] >>= 1;
            }
        } else {
            m_local_fire_map[f] >>= 1;
            next_front[f] >>= 1;
            if (next_front[f] == 0) {
                next_front.erase(f);
            }
        }
    }

    m_fire_front = next_front;
    for (auto& f : m_fire_front) {
        if (m_local_vegetation_map[f.first] > 0) {
            m_local_vegetation_map[f.first] -= 1;
        }
    }
    m_time_step += 1;
    return !m_fire_front.empty();
}

std::size_t Model::get_index_from_lexicographic_indices(LexicoIndices t_lexico_indices) const {
    return t_lexico_indices.row * this->geometry() + t_lexico_indices.column;
}

Model::LexicoIndices Model::get_lexicographic_from_index(std::size_t t_global_index) const {
    LexicoIndices ind_coords;
    ind_coords.row = t_global_index / this->geometry();
    ind_coords.column = t_global_index % this->geometry();
    return ind_coords;
}