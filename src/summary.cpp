#include <ctime>
#include <iostream>
#include <lo2s/summary.hpp>
#include <lo2s/util.hpp>
namespace lo2s
{
double Summary::start_cpu_time = 0;
double Summary::start_wall_time = 0;
long Summary::wakeups = 0;
Summary::Summary()
{
}

void Summary::start()
{
    std::pair<double, double> times = get_process_times();
    Summary::start_wall_time = times.first;
    Summary::start_cpu_time = times.second;
    Summary::wakeups = 0;
}

void Summary::finalize_and_print()
{
    std::pair<double, double> times = get_process_times();
    double wall_time = times.first - Summary::start_wall_time;
    double cpu_time = times.second - Summary::start_cpu_time;
    std::cout << "EXECUTION SUMMARY\n";
    std::cout << wall_time << "s Wall Time " << cpu_time << "s CPU time\n";
}
}
