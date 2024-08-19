#include <lo2s/perf/pmu-events.hpp>

#include <lo2s/perf/event_provider.hpp>
namespace lo2s
{
namespace perf
{
namespace pmu_events
{
static const struct pmu_events_table* get_table()
{
#ifdef __x86_64__
    return find_core_events_table("x86", get_cpuid_str(NULL));
#else
    return NULL;
#endif
}

std::optional<EventDescription> read_event(const std::string& ev_desc)
{
    const auto* tbl = get_table();

    for (uint32_t i = 0; i < tbl->num_pmus; i++)
    {
        struct pmu_event pmu;
        decompress_event(tbl->pmus[i].pmu_name.offset, &pmu);
        std::filesystem::path pmu_path = std::filesystem::path("/sys/devices") / pmu.name;
        if (!std::filesystem::exists(pmu_path))
        {
            // Use the generic cpu pmu as a fallback
            pmu_path = "/sys/devices/cpu";
        }
        // read PMU type id
        std::underlying_type<perf_type_id>::type type;
        std::ifstream type_stream(pmu_path / "type");
        type_stream >> type;
        if (!type_stream)
        {
            return std::nullopt;
        }

        for (uint32_t j = 0; j < tbl->pmus[i].num_entries; j++)
        {
            struct pmu_event pmu_ev;
            decompress_event(tbl->pmus[i].entries[j].offset, &pmu_ev);

            if (ev_desc == pmu_ev.name)
            {
                EventDescription ev =
                    EventDescription(ev_desc, static_cast<perf_type_id>(type), 0, 0);

                try
                {
                    parse_event_configuration(ev, pmu_path, pmu_ev.event);
                }
                catch (...)
                {
                    return std::nullopt;
                }

                if (!event_is_openable(ev))
                {
                    return std::nullopt;
                }
                return ev;
            }
        }
    }

    return std::nullopt;
}

std::vector<EventDescription> get_events()
{
    std::vector<EventDescription> res;
    const auto* tbl = get_table();

    for (uint32_t i = 0; i < tbl->num_pmus; i++)
    {
        struct pmu_event pmu;
        decompress_event(tbl->pmus[i].pmu_name.offset, &pmu);

        std::filesystem::path pmu_path = std::filesystem::path("/sys/devices") / pmu.name;
        if (!std::filesystem::exists(pmu_path))
        {
            // Use the generic cpu pmu as a fallback
            pmu_path = "/sys/devices/cpu";
        }
        // read PMU type id
        std::underlying_type<perf_type_id>::type type;
        std::ifstream type_stream(pmu_path / "type");
        type_stream >> type;
        if (!type_stream)
        {
            continue;
        }
        for (uint32_t j = 0; j < tbl->pmus[i].num_entries; j++)
        {
            struct pmu_event pmu_ev;
            decompress_event(tbl->pmus[i].entries[j].offset, &pmu_ev);

            EventDescription ev =
                EventDescription(pmu_ev.name, static_cast<perf_type_id>(type), 0, 0);

            try
            {
                parse_event_configuration(ev, pmu_path, pmu_ev.event);
            }
            catch (...)
            {
                continue;
            }

            if (!event_is_openable(ev))
            {
                continue;
            }
            res.emplace_back(ev);
        }
    }

    return res;
}
} // namespace pmu_events
} // namespace perf
} // namespace lo2s
