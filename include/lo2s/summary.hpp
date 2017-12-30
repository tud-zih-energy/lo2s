#include <ctime>
namespace lo2s
{
class Summary
{
public:
    static void start();
    static void finalize_and_print();
    static void record_wakeups(int wakeups);

private:
    Summary();
    static double start_cpu_time;
    static double start_wall_time;
    static long wakeups;
};
}
