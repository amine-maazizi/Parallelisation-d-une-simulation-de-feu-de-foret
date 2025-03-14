#include "model.hpp"

namespace
{
    double pseudo_random(std::size_t index, std::size_t time_step)
    {
        std::uint_fast32_t xi = std::uint_fast32_t(index * (time_step + 1));
        std::uint_fast32_t r = (48271 * xi) % 2147483647;
        return r / 2147483646.;
    }

    double log_factor(std::uint8_t value)
    {
        return std::log(1. + value) / std::log(256);
    }
}

Model::Model(double t_length, unsigned t_discretization, std::array<double, 2> t_wind,
             LexicoIndices t_start_fire_position, double t_max_wind)
    : m_length(t_length),
      m_distance(-1),
      m_geometry(t_discretization),
      m_wind(t_wind),
      m_wind_speed(std::sqrt(t_wind[0] * t_wind[0] + t_wind[1] * t_wind[1])),
      m_max_wind(t_max_wind),
      m_vegetation_map(t_discretization * t_discretization, 255u),
      m_fire_map(t_discretization * t_discretization, 0u)
{
    if (t_discretization == 0)
    {
        throw std::range_error("Le nombre de cases par direction doit être plus grand que zéro.");
    }
    m_distance = m_length / double(m_geometry);
    auto index = get_index_from_lexicographic_indices(t_start_fire_position);
    m_fire_map[index] = 255u;
    m_fire_front[index] = 255u;

    constexpr double alpha0 = 4.52790762e-01;
    constexpr double alpha1 = 9.58264437e-04;
    constexpr double alpha2 = 3.61499382e-05;

    if (m_wind_speed < t_max_wind)
        p1 = alpha0 + alpha1 * m_wind_speed + alpha2 * (m_wind_speed * m_wind_speed);
    else
        p1 = alpha0 + alpha1 * t_max_wind + alpha2 * (t_max_wind * t_max_wind);
    p2 = 0.3;

    if (m_wind[0] > 0)
    {
        alphaEastWest = std::abs(m_wind[0] / t_max_wind) + 1;
        alphaWestEast = 1. - std::abs(m_wind[0] / t_max_wind);
    }
    else
    {
        alphaWestEast = std::abs(m_wind[0] / t_max_wind) + 1;
        alphaEastWest = 1. - std::abs(m_wind[0] / t_max_wind);
    }

    if (m_wind[1] > 0)
    {
        alphaSouthNorth = std::abs(m_wind[1] / t_max_wind) + 1;
        alphaNorthSouth = 1. - std::abs(m_wind[1] / t_max_wind);
    }
    else
    {
        alphaNorthSouth = std::abs(m_wind[1] / t_max_wind) + 1;
        alphaSouthNorth = 1. - std::abs(m_wind[1] / t_max_wind);
    }
}
// --------------------------------------------------------------------------------------------------------------------
bool Model::update()
{
    std::vector<std::size_t> fire_keys;
    for (const auto& pair : m_fire_front) {
        fire_keys.push_back(pair.first);
    }

    auto next_front = m_fire_front;

    #pragma omp parallel for  
    for (size_t i = 0; i < fire_keys.size(); ++i)
    {
        std::size_t f = fire_keys[i];
        LexicoIndices coord = get_lexicographic_from_index(f);
        double power = log_factor(m_fire_front[f]);

        if (coord.row < m_geometry - 1)
        {
            double tirage = pseudo_random(f + m_time_step, m_time_step);
            double green_power = m_vegetation_map[f + m_geometry];
            double correction = power * log_factor(green_power);
            if (tirage < alphaSouthNorth * p1 * correction)
            {
                #pragma omp critical
                {
                    m_fire_map[f + m_geometry] = 255;
                    next_front[f + m_geometry] = 255;
                }
            }
        }

        if (coord.row > 0)
        {
            double tirage = pseudo_random(f * 13427 + m_time_step, m_time_step);
            double green_power = m_vegetation_map[f - m_geometry];
            double correction = power * log_factor(green_power);
            if (tirage < alphaNorthSouth * p1 * correction)
            {
                #pragma omp critical
                {
                    m_fire_map[f - m_geometry] = 255;
                    next_front[f - m_geometry] = 255;
                }
            }
        }

        if (coord.column < m_geometry - 1)
        {
            double tirage = pseudo_random(f * 13427 * 13427 + m_time_step, m_time_step);
            double green_power = m_vegetation_map[f + 1];
            double correction = power * log_factor(green_power);
            if (tirage < alphaEastWest * p1 * correction)
            {
                #pragma omp critical
                {
                    m_fire_map[f + 1] = 255;
                    next_front[f + 1] = 255;
                }
            }
        }

        if (coord.column > 0)
        {
            double tirage = pseudo_random(f * 13427 * 13427 * 13427 + m_time_step, m_time_step);
            double green_power = m_vegetation_map[f - 1];
            double correction = power * log_factor(green_power);
            if (tirage < alphaWestEast * p1 * correction)
            {
                #pragma omp critical
                {
                    m_fire_map[f - 1] = 255;
                    next_front[f - 1] = 255;
                }
            }
        }

        if (m_fire_front[f] == 255)
        {
            double tirage = pseudo_random(f * 52513 + m_time_step, m_time_step);
            if (tirage < p2)
            {
                #pragma omp critical
                {
                    m_fire_map[f] >>= 1;
                    next_front[f] >>= 1;
                }
            }
        }
        else
        {
            #pragma omp critical
            {
                m_fire_map[f] >>= 1;
                next_front[f] >>= 1;
                if (next_front[f] == 0)
                {
                    next_front.erase(f);
                }
            }
        }
    }

    m_fire_front = next_front;
    for (auto f : m_fire_front)
    {
        if (m_vegetation_map[f.first] > 0)
            m_vegetation_map[f.first] -= 1;
    }
    m_time_step += 1;

    // Ajout du calcul SHA-1 pour m_fire_map et m_vegetation_map
    std::stringstream buffer;
    // Parcours de m_fire_map (std::vector<uint8_t>, ordre des indices)
    for (const auto& val : m_fire_map) {
        buffer << static_cast<int>(val); // Convertit uint8_t en int pour éviter problèmes d’affichage
    }
    // Parcours de m_vegetation_map (std::vector<uint8_t>, ordre des indices)
    for (const auto& val : m_vegetation_map) {
        buffer << static_cast<int>(val); // Idem
    }
    std::string data = buffer.str();

    // Calcul du hash SHA-1
    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1(reinterpret_cast<const unsigned char*>(data.c_str()), data.size(), hash);

    // Conversion en hexadécimal pour affichage
    std::stringstream ss;
    for (int i = 0; i < SHA_DIGEST_LENGTH; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    }
    std::cout << "SHA-1 à t=" << m_time_step << ": " << ss.str() << std::endl;

    return !m_fire_front.empty();
}
// ====================================================================================================================
std::size_t Model::get_index_from_lexicographic_indices(LexicoIndices t_lexico_indices) const
{
    return t_lexico_indices.row * this->geometry() + t_lexico_indices.column;
}
// --------------------------------------------------------------------------------------------------------------------
auto Model::get_lexicographic_from_index(std::size_t t_global_index) const -> LexicoIndices
{
    LexicoIndices ind_coords;
    ind_coords.row = t_global_index / this->geometry();
    ind_coords.column = t_global_index % this->geometry();
    return ind_coords;
}