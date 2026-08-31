#pragma once
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>

class TestSuite
{
public:
    explicit TestSuite(std::string name) : m_name(std::move(name)) {}
    void Check(bool condition, const char* message)
    {
        ++m_checks;
        if (!condition) throw std::runtime_error(message);
    }
    void Near(double actual, double expected, const char* message, double tolerance = 1e-8)
    {
        Check(std::isfinite(actual) && std::abs(actual - expected) <= tolerance, message);
    }
    template<class Function> void Run(const char* name, Function function)
    {
        ++m_scenarios;
        try { function(); }
        catch (const std::exception& error)
        {
            ++m_failed;
            std::cerr << "FAIL " << m_name << '/' << name << ": " << error.what() << '\n';
        }
    }
    int Finish() const
    {
        std::cout << m_name << ": " << m_scenarios << " scenarios, " << m_checks
            << " checks, " << m_failed << " failures.\n";
        return m_failed == 0 ? 0 : 1;
    }
private:
    std::string m_name;
    int m_scenarios = 0;
    std::size_t m_checks = 0;
    int m_failed = 0;
};
