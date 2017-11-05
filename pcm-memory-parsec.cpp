
// written by Hosein Mohammadi Makrani
//            George Mason University



/*!     
\implementation of a simple performance counter monitoring utility
*/
#define HACK_TO_REMOVE_DUPLICATE_ERROR
#include <iostream>
#ifdef _MSC_VER
#pragma warning(disable : 4996) // for sprintf
#include <windows.h>
#include "../PCM_Win/windriver.h"
#else
#include <unistd.h>
//#include <signal.h>   // for atexit()
#include <sys/time.h> // for gettimeofday()
#endif
#include <math.h>
#include <iomanip>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <cstring>
#include <sstream>
#include <assert.h>
#include <bitset>
#include "cpucounters.h"
#include "utils.h"

#define SIZE (10000000)
#define PCM_DELAY_DEFAULT 1.0 // in seconds
#define PCM_DELAY_MIN 0.015 // 15 milliseconds is practical on most modern CPUs
#define PCM_CALIBRATION_INTERVAL 50 // calibrate clock only every 50th iteration
#define MAX_CORES 4096

/*
// Block Size
#define N64g 68719476736
#define N36g 38654705664
#define N32g 34359738368
#define N28g 30064771072
#define N20g 21474836480
#define N16g 17179869184
#define N14g 15032385536
#define N8g 8589934592
#define N4g 4294967296
#define N2g 2147483648
#define N1g 1073741824
#define N512m 536870912
#define N128m 134217728
#define N16m 16777216
*/

//Programmable iMC counter
#define READ 0
#define WRITE 1
#define READ_RANK_A 0
#define WRITE_RANK_A 1
#define READ_RANK_B 2
#define WRITE_RANK_B 3
#define PARTIAL 2
#define DEFAULT_DISPLAY_COLUMNS 2

using namespace std;

template <class IntType>
double float_format(IntType n)
{
    return double(n) / 1024 / 1024;
}

std::string temp_format(int32 t)
{
    char buffer[1024];
    if (t == PCM_INVALID_THERMAL_HEADROOM)
        return "N/A";

    sprintf(buffer, "%2d", t);
    return buffer;
}

std::string l3cache_occ_format(uint64 o)
{
    char buffer[1024];
    if (o == PCM_INVALID_QOS_MONITORING_DATA)
        return "N/A";

    sprintf(buffer, "%6d", (uint32) o);
    return buffer;
}
/*
void print_help(const string prog_name)
{
    cerr << endl << " Usage: " << endl << " " << prog_name
        << " --help | [delay] [options] [-- external_program [external_program_options]]" << endl;
    cerr << "   <delay>                           => time interval to sample performance counters." << endl;
    cerr << "                                        If not specified, or 0, with external program given" << endl;
    cerr << "                                        will read counters only after external program finishes" << endl;
    cerr << " Supported <options> are: " << endl;
    cerr << "  -h    | --help      | /h           => print this help and exit" << endl;
#ifdef _MSC_VER
    cerr << "  --uninstallDriver   | --installDriver=> (un)install driver" << endl;
#endif
    cerr << "  -r    | --reset     | /reset       => reset PMU configuration (at your own risk)" << endl;
    cerr << "  -nc   | --nocores   | /nc          => hide core related output" << endl;
    cerr << "  -yc   | --yescores  | /yc          => enable specific cores to output" << endl;
    cerr << "  -ns   | --nosockets | /ns          => hide socket related output" << endl;
    cerr << "  -nsys | --nosystem  | /nsys        => hide system related output" << endl;
    cerr << "  -m    | --multiple-instances | /m  => allow multiple PCM instances running in parallel" << endl;
    cerr << "  -csv[=file.csv] | /csv[=file.csv]  => output compact CSV format to screen or" << endl
        << "                                        to a file, in case filename is provided" << endl;
    cerr << "  -i[=number] | /i[=number]          => allow to determine number of iterations" << endl;
    cerr << " Examples:" << endl;
    cerr << "  " << prog_name << " 1 -nc -ns          => print counters every second without core and socket output" << endl;
    cerr << "  " << prog_name << " 1 -i=10            => print counters every second 10 times and exit" << endl;
    cerr << "  " << prog_name << " 0.5 -csv=test.log  => twice a second save counter values to test.log in CSV format" << endl;
    cerr << "  " << prog_name << " /csv 5 2>/dev/null => one sampe every 5 seconds, and discard all diagnostic output" << endl;
    cerr << endl;
}
*/

void print_output(PCM * m,
    const std::vector<CoreCounterState> & cstates1,
    const std::vector<CoreCounterState> & cstates2,
    const std::vector<SocketCounterState> & sktstate1,
    const std::vector<SocketCounterState> & sktstate2,
    const std::bitset<MAX_CORES> & ycores,
    const SystemCounterState& sstate1,
    const SystemCounterState& sstate2,
    const int cpu_model,
    const bool show_core_output,
    const bool show_partial_core_output,
    const bool show_socket_output,
    const bool show_system_output
    )
{
    cout << "\n";
    cout << " EXEC  : instructions per nominal CPU cycle" << "\n";
    cout << " IPC   : instructions per CPU cycle" << "\n";
    cout << " FREQ  : relation to nominal CPU frequency='unhalted clock ticks'/'invariant timer ticks' (includes Intel Turbo Boost)" << "\n";
    if (cpu_model != PCM::ATOM) cout << " AFREQ : relation to nominal CPU frequency while in active state (not in power-saving C state)='unhalted clock ticks'/'invariant timer ticks while in C0-state'  (includes Intel Turbo Boost)" << "\n";
    if (cpu_model != PCM::ATOM) cout << " L3MISS: L3 cache misses " << "\n";
    if (cpu_model == PCM::ATOM)
        cout << " L2MISS: L2 cache misses " << "\n";
    else
        cout << " L2MISS: L2 cache misses (including other core's L2 cache *hits*) " << "\n";
    if (cpu_model != PCM::ATOM) cout << " L3HIT : L3 cache hit ratio (0.00-1.00)" << "\n";
    cout << " L2HIT : L2 cache hit ratio (0.00-1.00)" << "\n";
    if (cpu_model != PCM::ATOM) cout << " L3MPI : number of L3 cache misses per instruction\n";
    if (cpu_model != PCM::ATOM) cout << " L2MPI : number of L2 cache misses per instruction\n";
    if (cpu_model != PCM::ATOM) cout << " READ  : bytes read from memory controller (in GBytes)" << "\n";
    if (cpu_model != PCM::ATOM) cout << " WRITE : bytes written to memory controller (in GBytes)" << "\n";
    if (m->memoryIOTrafficMetricAvailable()) cout << " IO    : bytes read/written due to IO requests to memory controller (in GBytes); this may be an over estimate due to same-cache-line partial requests" << "\n";
    if (m->L3CacheOccupancyMetricAvailable()) cout << " L3OCC : L3 occupancy (in KBytes)" << "\n";
    if (m->CoreLocalMemoryBWMetricAvailable()) cout << " LMB   : L3 cache external bandwidth satisfied by local memory (in MBytes)" << "\n";
    if (m->CoreRemoteMemoryBWMetricAvailable()) cout << " RMB   : L3 cache external bandwidth satisfied by remote memory (in MBytes)" << "\n";
    cout << " TEMP  : Temperature reading in 1 degree Celsius relative to the TjMax temperature (thermal headroom): 0 corresponds to the max temperature" << "\n";
    cout << " energy: Energy in Joules" << "\n";
    cout << "\n";
    cout << "\n";
    const char * longDiv = "---------------------------------------------------------------------------------------------------------------\n";
    cout.precision(2);
    cout << std::fixed;
    if (cpu_model == PCM::ATOM)
        cout << " Core (SKT) | EXEC | IPC  | FREQ | L2MISS | L2HIT | TEMP" << "\n" << "\n";
    else
    {
			cout << " Core (SKT) | EXEC | IPC  | FREQ  | AFREQ | L3MISS | L2MISS | L3HIT | L2HIT | L3MPI | L2MPI |";

			if (m->L3CacheOccupancyMetricAvailable())
					cout << "  L3OCC |";
			if (m->CoreLocalMemoryBWMetricAvailable())
					cout << "   LMB  |";
			if (m->CoreRemoteMemoryBWMetricAvailable())
				cout << "   RMB  |";

            cout << " TEMP" << "\n" << "\n";
    }


    if (show_core_output)
    {
        for (uint32 i = 0; i < m->getNumCores(); ++i)
        {
            if (m->isCoreOnline(i) == false || (show_partial_core_output && ycores.test(i) == false))
                continue;

            if (cpu_model != PCM::ATOM)
            {
                cout << " " << setw(3) << i << "   " << setw(2) << m->getSocketId(i) <<
                    "     " << getExecUsage(cstates1[i], cstates2[i]) <<
                    "   " << getIPC(cstates1[i], cstates2[i]) <<
                    "   " << getRelativeFrequency(cstates1[i], cstates2[i]) <<
                    "    " << getActiveRelativeFrequency(cstates1[i], cstates2[i]) <<
                    "    " << unit_format(getL3CacheMisses(cstates1[i], cstates2[i])) <<
                    "   " << unit_format(getL2CacheMisses(cstates1[i], cstates2[i])) <<
                    "    " << getL3CacheHitRatio(cstates1[i], cstates2[i]) <<
                    "    " << getL2CacheHitRatio(cstates1[i], cstates2[i]) <<
                    "    " << double(getL3CacheMisses(cstates1[i], cstates2[i])) / getInstructionsRetired(cstates1[i], cstates2[i]) <<
                    "    " << double(getL2CacheMisses(cstates1[i], cstates2[i])) / getInstructionsRetired(cstates1[i], cstates2[i]) ;
                if (m->L3CacheOccupancyMetricAvailable())
                    cout << "   " << setw(6) << l3cache_occ_format(getL3CacheOccupancy(cstates2[i])) ;
				if (m->CoreLocalMemoryBWMetricAvailable())
					cout << "   " << setw(6) << getLocalMemoryBW(cstates1[i], cstates2[i]);
				if (m->CoreRemoteMemoryBWMetricAvailable())
                	cout << "   " << setw(6) << getRemoteMemoryBW(cstates1[i], cstates2[i]) ;
                cout << "     " << temp_format(cstates2[i].getThermalHeadroom()) <<
                    "\n";
            }
            else
                cout << " " << setw(3) << i << "   " << setw(2) << m->getSocketId(i) <<
                "     " << getExecUsage(cstates1[i], cstates2[i]) <<
                "   " << getIPC(cstates1[i], cstates2[i]) <<
                "   " << getRelativeFrequency(cstates1[i], cstates2[i]) <<
                "   " << unit_format(getL2CacheMisses(cstates1[i], cstates2[i])) <<
                "    " << getL2CacheHitRatio(cstates1[i], cstates2[i]) <<
                "     " << temp_format(cstates2[i].getThermalHeadroom()) <<
                "\n";
        }
    }
    if (show_socket_output)
    {
        if (!(m->getNumSockets() == 1 && cpu_model == PCM::ATOM))
        {
            cout << longDiv;
            for (uint32 i = 0; i < m->getNumSockets(); ++i)
            {
                cout << " SKT   " << setw(2) << i <<
                    "     " << getExecUsage(sktstate1[i], sktstate2[i]) <<
                    "   " << getIPC(sktstate1[i], sktstate2[i]) <<
                    "   " << getRelativeFrequency(sktstate1[i], sktstate2[i]) <<
                    "    " << getActiveRelativeFrequency(sktstate1[i], sktstate2[i]) <<
                    "    " << unit_format(getL3CacheMisses(sktstate1[i], sktstate2[i])) <<
                    "   " << unit_format(getL2CacheMisses(sktstate1[i], sktstate2[i])) <<
                    "    " << getL3CacheHitRatio(sktstate1[i], sktstate2[i]) <<
                    "    " << getL2CacheHitRatio(sktstate1[i], sktstate2[i]) <<
                    "    " << double(getL3CacheMisses(sktstate1[i], sktstate2[i])) / getInstructionsRetired(sktstate1[i], sktstate2[i]) <<
                    "    " << double(getL2CacheMisses(sktstate1[i], sktstate2[i])) / getInstructionsRetired(sktstate1[i], sktstate2[i]);
                if (m->L3CacheOccupancyMetricAvailable())
                    cout << "   " << setw(6) << l3cache_occ_format(getL3CacheOccupancy(sktstate2[i])) ;
				if (m->CoreLocalMemoryBWMetricAvailable())
					cout << "   " << setw(6) << getLocalMemoryBW(sktstate1[i], sktstate2[i]);
				if (m->CoreRemoteMemoryBWMetricAvailable())
					cout << "   " << setw(6) << getRemoteMemoryBW(sktstate1[i], sktstate2[i]);

                cout << "     " << temp_format(sktstate2[i].getThermalHeadroom()) << "\n";
            }
        }
    }
    cout << longDiv;

    if (show_system_output)
    {
        if (cpu_model != PCM::ATOM)
        {
            cout << " TOTAL  *     " << getExecUsage(sstate1, sstate2) <<
                "   " << getIPC(sstate1, sstate2) <<
                "   " << getRelativeFrequency(sstate1, sstate2) <<
                "    " << getActiveRelativeFrequency(sstate1, sstate2) <<
                "    " << unit_format(getL3CacheMisses(sstate1, sstate2)) <<
                "   " << unit_format(getL2CacheMisses(sstate1, sstate2)) <<
                "    " << getL3CacheHitRatio(sstate1, sstate2) <<
                "    " << getL2CacheHitRatio(sstate1, sstate2) <<
                "    " << double(getL3CacheMisses(sstate1, sstate2)) / getInstructionsRetired(sstate1, sstate2) <<
                "    " << double(getL2CacheMisses(sstate1, sstate2)) / getInstructionsRetired(sstate1, sstate2);
            if (m->L3CacheOccupancyMetricAvailable())
                cout << "    " << " N/A ";
			if (m->CoreLocalMemoryBWMetricAvailable())
				cout << "   " << " N/A ";
			if (m->CoreRemoteMemoryBWMetricAvailable())
				cout << "   " << " N/A ";

            cout << "     N/A\n";
        }
        else
            cout << " TOTAL  *     " << getExecUsage(sstate1, sstate2) <<
            "   " << getIPC(sstate1, sstate2) <<
            "   " << getRelativeFrequency(sstate1, sstate2) <<
            "   " << unit_format(getL2CacheMisses(sstate1, sstate2)) <<
            "    " << getL2CacheHitRatio(sstate1, sstate2) <<
            "     N/A\n";
    }

    if (show_system_output)
    {
        cout << "\n" << " Instructions retired: " << unit_format(getInstructionsRetired(sstate1, sstate2)) << " ; Active cycles: " << unit_format(getCycles(sstate1, sstate2)) << " ; Time (TSC): " << unit_format(getInvariantTSC(cstates1[0], cstates2[0])) << "ticks ; C0 (active,non-halted) core residency: " << (getCoreCStateResidency(0, sstate1, sstate2)*100.) << " %\n";
        cout << "\n";
        for (int s = 1; s <= PCM::MAX_C_STATE; ++s)
        if (m->isCoreCStateResidencySupported(s))
            std::cout << " C" << s << " core residency: " << (getCoreCStateResidency(s, sstate1, sstate2)*100.) << " %;";
        cout << "\n";
        for (int s = 0; s <= PCM::MAX_C_STATE; ++s)
        if (m->isPackageCStateResidencySupported(s))
            std::cout << " C" << s << " package residency: " << (getPackageCStateResidency(s, sstate1, sstate2)*100.) << " %;";
        cout << "\n";
        if (m->getNumCores() == m->getNumOnlineCores())
        {
            cout << "\n" << " PHYSICAL CORE IPC                 : " << getCoreIPC(sstate1, sstate2) << " => corresponds to " << 100. * (getCoreIPC(sstate1, sstate2) / double(m->getMaxIPC())) << " % utilization for cores in active state";
            cout << "\n" << " Instructions per nominal CPU cycle: " << getTotalExecUsage(sstate1, sstate2) << " => corresponds to " << 100. * (getTotalExecUsage(sstate1, sstate2) / double(m->getMaxIPC())) << " % core utilization over time interval" << "\n";
        }
    }

    if (show_socket_output)
    {
        if (m->getNumSockets() > 1 && m->incomingQPITrafficMetricsAvailable()) // QPI info only for multi socket systems
        {
            cout << "\n" << "Intel(r) QPI data traffic estimation in bytes (data traffic coming to CPU/socket through QPI links):" << "\n" << "\n";


            const uint32 qpiLinks = (uint32)m->getQPILinksPerSocket();

            cout << "              ";
            for (uint32 i = 0; i < qpiLinks; ++i)
                cout << " QPI" << i << "    ";

            if (m->qpiUtilizationMetricsAvailable())
            {
                cout << "| ";
                for (uint32 i = 0; i < qpiLinks; ++i)
                    cout << " QPI" << i << "  ";
            }

            cout << "\n" << longDiv;


            for (uint32 i = 0; i < m->getNumSockets(); ++i)
            {
                cout << " SKT   " << setw(2) << i << "     ";
                for (uint32 l = 0; l < qpiLinks; ++l)
                    cout << unit_format(getIncomingQPILinkBytes(i, l, sstate1, sstate2)) << "   ";

                if (m->qpiUtilizationMetricsAvailable())
                {
                    cout << "|  ";
                    for (uint32 l = 0; l < qpiLinks; ++l)
                        cout << setw(3) << std::dec << int(100. * getIncomingQPILinkUtilization(i, l, sstate1, sstate2)) << "%   ";
                }

                cout << "\n";
            }
        }
    }

    if (show_system_output)
    {
        cout << longDiv;

        if (m->getNumSockets() > 1 && m->incomingQPITrafficMetricsAvailable()) // QPI info only for multi socket systems
            cout << "Total QPI incoming data traffic: " << unit_format(getAllIncomingQPILinkBytes(sstate1, sstate2)) << "     QPI data traffic/Memory controller traffic: " << getQPItoMCTrafficRatio(sstate1, sstate2) << "\n";
    }

    if (show_socket_output)
    {
        if (m->getNumSockets() > 1 && (m->outgoingQPITrafficMetricsAvailable())) // QPI info only for multi socket systems
        {
            cout << "\n" << "Intel(r) QPI traffic estimation in bytes (data and non-data traffic outgoing from CPU/socket through QPI links):" << "\n" << "\n";


            const uint32 qpiLinks = (uint32)m->getQPILinksPerSocket();

            cout << "              ";
            for (uint32 i = 0; i < qpiLinks; ++i)
                cout << " QPI" << i << "    ";


            cout << "| ";
            for (uint32 i = 0; i < qpiLinks; ++i)
                cout << " QPI" << i << "  ";


            cout << "\n" << longDiv;


            for (uint32 i = 0; i < m->getNumSockets(); ++i)
            {
                cout << " SKT   " << setw(2) << i << "     ";
                for (uint32 l = 0; l < qpiLinks; ++l)
                    cout << unit_format(getOutgoingQPILinkBytes(i, l, sstate1, sstate2)) << "   ";

                cout << "|  ";
                for (uint32 l = 0; l < qpiLinks; ++l)
                    cout << setw(3) << std::dec << int(100. * getOutgoingQPILinkUtilization(i, l, sstate1, sstate2)) << "%   ";

                cout << "\n";
            }

            cout << longDiv;
            cout << "Total QPI outgoing data and non-data traffic: " << unit_format(getAllOutgoingQPILinkBytes(sstate1, sstate2)) << "\n";
        }
    }
    if (show_socket_output)
    {
        cout << "\n";
        cout << "          |";
        if (m->memoryTrafficMetricsAvailable())
            cout << "  READ |  WRITE |";
        if (m->memoryIOTrafficMetricAvailable())
            cout << "    IO  |";
        if (m->packageEnergyMetricsAvailable())
            cout << " CPU energy |";
        if (m->dramEnergyMetricsAvailable())
            cout << " DIMM energy";
        cout << "\n";
        cout << longDiv;
        for (uint32 i = 0; i < m->getNumSockets(); ++i)
        {
                cout << " SKT  " << setw(2) << i;
                if (m->memoryTrafficMetricsAvailable())
                    cout << "    " << setw(5) << getBytesReadFromMC(sktstate1[i], sktstate2[i]) / double(1024ULL * 1024ULL * 1024ULL) <<
                            "    " << setw(5) << getBytesWrittenToMC(sktstate1[i], sktstate2[i]) / double(1024ULL * 1024ULL * 1024ULL);
                if (m->memoryIOTrafficMetricAvailable())
                    cout << "    " << setw(5) << getIORequestBytesFromMC(sktstate1[i], sktstate2[i]) / double(1024ULL * 1024ULL * 1024ULL);
                cout << "     ";
                if(m->packageEnergyMetricsAvailable()) {
                  cout << setw(6) << getConsumedJoules(sktstate1[i], sktstate2[i]);
                }
                cout << "     ";
                if(m->dramEnergyMetricsAvailable()) {
                  cout << setw(6) << getDRAMConsumedJoules(sktstate1[i], sktstate2[i]);
                }
                cout << "\n";
        }
        cout << longDiv;
        if (m->getNumSockets() > 1) {
            cout << "       *";
            if (m->memoryTrafficMetricsAvailable())
                cout << "    " << setw(5) << getBytesReadFromMC(sstate1, sstate2) / double(1024ULL * 1024ULL * 1024ULL) <<
                        "    " << setw(5) << getBytesWrittenToMC(sstate1, sstate2) / double(1024ULL * 1024ULL * 1024ULL);
            if (m->memoryIOTrafficMetricAvailable())
                cout << "    " << setw(5) << getIORequestBytesFromMC(sstate1, sstate2) / double(1024ULL * 1024ULL * 1024ULL);
            cout << "     ";
            if (m->packageEnergyMetricsAvailable()) {
                cout << setw(6) << getConsumedJoules(sstate1, sstate2);
            }
            cout << "     ";
            if (m->dramEnergyMetricsAvailable()) {
                cout << setw(6) << getDRAMConsumedJoules(sstate1, sstate2);
            }
            cout << "\n";
        }
    }

}
/*
void print_csv_header(PCM * m,
    const int cpu_model,
    const bool show_core_output,
    const bool show_socket_output,
    const bool show_system_output
    )
{
    // print first header line
    cout << "System;;";
    if (show_system_output)
    {
        if (cpu_model != PCM::ATOM)
            {
				cout << ";;;;;;;;;;;;";
				if (m->L3CacheOccupancyMetricAvailable())
            		cout << ";";
				if (m->CoreLocalMemoryBWMetricAvailable())
					cout << ";";
				if (m->CoreLocalMemoryBWMetricAvailable())
					cout << ";";
            		
            }
        else
            cout << ";;;;;";


        cout << ";;;;;;;";
        if (m->getNumSockets() > 1) // QPI info only for multi socket systems
            cout << ";;";
        if (m->qpiUtilizationMetricsAvailable())
            cout << ";";
        cout << "System Core C-States";
        for (int s = 0; s <= PCM::MAX_C_STATE; ++s)
        if (m->isCoreCStateResidencySupported(s))
            cout << ";";
        cout << "System Pack C-States";
        for (int s = 0; s <= PCM::MAX_C_STATE; ++s)
        if (m->isPackageCStateResidencySupported(s))
            cout << ";";
        if (m->packageEnergyMetricsAvailable())
            cout << ";";
        if (m->dramEnergyMetricsAvailable())
            cout << ";";
    }


    if (show_socket_output)
    {
        for (uint32 i = 0; i < m->getNumSockets(); ++i)
        {
				if (cpu_model == PCM::ATOM)
						cout << "Socket" << i << ";;;;;;;";
				else
				{
					if (m->L3CacheOccupancyMetricAvailable())
						cout << "Socket" <<  i << ";;;;;;;;;;;;;;";
					else
						cout << "Socket" <<  i << ";;;;;;;;;;;;;";
				}
             }

        if (m->getNumSockets() > 1) // QPI info only for multi socket systems
        {
            const uint32 qpiLinks = (uint32)m->getQPILinksPerSocket();

            for (uint32 s = 0; s < m->getNumSockets(); ++s)
            {
                cout << "SKT" << s << "dataIn";
                for (uint32 i = 0; i < qpiLinks; ++i)
                    cout << ";";
                if (m->qpiUtilizationMetricsAvailable())
                {
                    cout << "SKT" << s << "dataIn (percent)";
                    for (uint32 i = 0; i < qpiLinks; ++i)
                        cout << ";";
                }
            }
        }

        if (m->getNumSockets() > 1 && (m->outgoingQPITrafficMetricsAvailable())) // QPI info only for multi socket systems
        {
            const uint32 qpiLinks = (uint32)m->getQPILinksPerSocket();

            for (uint32 s = 0; s < m->getNumSockets(); ++s)
            {
                cout << "SKT" << s << "trafficOut";
                for (uint32 i = 0; i < qpiLinks; ++i)
                    cout << ";";
                cout << "SKT" << s << "trafficOut (percent)";
                for (uint32 i = 0; i < qpiLinks; ++i)
                    cout << ";";
            }
        }


        for (uint32 i = 0; i < m->getNumSockets(); ++i)
        {
            cout << "SKT" << i << " Core C-State";
            for (int s = 0; s <= PCM::MAX_C_STATE; ++s)
            if (m->isCoreCStateResidencySupported(s))
                cout << ";";
            cout << "SKT" << i << " Package C-State";
            for (int s = 0; s <= PCM::MAX_C_STATE; ++s)
            if (m->isPackageCStateResidencySupported(s))
                cout << ";";
        }

        if (m->packageEnergyMetricsAvailable())
        {
            cout << "Proc Energy (Joules)";
            for (uint32 i = 0; i < m->getNumSockets(); ++i)
                cout << ";";
        }
        if (m->dramEnergyMetricsAvailable())
        {
            cout << "DRAM Energy (Joules)";
            for (uint32 i = 0; i < m->getNumSockets(); ++i)
                cout << ";";
        }
    }

    if (show_core_output)
    {
        for (uint32 i = 0; i < m->getNumCores(); ++i)
        {
			if (cpu_model == PCM::ATOM)
				cout << "Core" << i << " (Socket" << setw(2) << m->getSocketId(i) << ");;;;;";
			else
				cout << "Core" << i << " (Socket" << setw(2) << m->getSocketId(i) << ");;;;;;;;;;;";

            for (int s = 0; s <= PCM::MAX_C_STATE; ++s)
            if (m->isCoreCStateResidencySupported(s))
                cout << ";";
        }
    }

    // print second header line
    cout << "\nDate;Time;";
    if (show_system_output)
    {
        if (cpu_model != PCM::ATOM)
        {
		cout << "EXEC;IPC;FREQ;AFREQ;L3MISS;L2MISS;L3HIT;L2HIT;L3MPI;L2MPI;";
            	if (m->L3CacheOccupancyMetricAvailable())
                    cout << "L3OCC;";
        	if (m->CoreLocalMemoryBWMetricAvailable())
            	cout << "LMB";
			if (m->CoreRemoteMemoryBWMetricAvailable())
				cout << "RMB;";

		cout << "READ;WRITE;";
	}
        else
        {
            cout << "EXEC;IPC;FREQ;L2MISS;L2HIT;";
        }


        cout << "INST;ACYC;TIME(ticks);PhysIPC;PhysIPC%;INSTnom;INSTnom%;";
        if (m->getNumSockets() > 1) // QPI info only for multi socket systems
            cout << "TotalQPIin;QPItoMC;";
        if (m->outgoingQPITrafficMetricsAvailable())
            cout << "TotalQPIout;";

        for (int s = 0; s <= PCM::MAX_C_STATE; ++s)
        if (m->isCoreCStateResidencySupported(s))
            cout << "C" << s << "res%;";

        for (int s = 0; s <= PCM::MAX_C_STATE; ++s)
        if (m->isPackageCStateResidencySupported(s))
            cout << "C" << s << "res%;";

        if (m->packageEnergyMetricsAvailable())
            cout << "Proc Energy (Joules);";
        if (m->dramEnergyMetricsAvailable())
            cout << "DRAM Energy (Joules);";
    }


    if (show_socket_output)
    {
        for (uint32 i = 0; i < m->getNumSockets(); ++i)
        {
				if (cpu_model == PCM::ATOM)
						cout << "EXEC;IPC;FREQ;L2MISS;L2HIT;TEMP;";
				else
				{
					
					cout << "EXEC;IPC;FREQ;AFREQ;L3MISS;L2MISS;L3HIT;L2HIT;L3MPI;L2MPI;";
					if (m->L3CacheOccupancyMetricAvailable())
						cout << "L3OCC;";
					if (m->CoreLocalMemoryBWMetricAvailable())
						cout << "LMB";
					if (m->CoreRemoteMemoryBWMetricAvailable())
						cout << "RMB;";
					cout << "READ;WRITE;TEMP;";
				}
            }

        if (m->getNumSockets() > 1) // QPI info only for multi socket systems
        {
            const uint32 qpiLinks = (uint32)m->getQPILinksPerSocket();

            for (uint32 s = 0; s < m->getNumSockets(); ++s)
            {
                for (uint32 i = 0; i < qpiLinks; ++i)
                    cout << "QPI" << i << ";";

                if (m->qpiUtilizationMetricsAvailable())
                for (uint32 i = 0; i < qpiLinks; ++i)
                    cout << "QPI" << i << ";";
            }

        }

        if (m->getNumSockets() > 1 && (m->outgoingQPITrafficMetricsAvailable())) // QPI info only for multi socket systems
        {
            const uint32 qpiLinks = (uint32)m->getQPILinksPerSocket();
            for (uint32 s = 0; s < m->getNumSockets(); ++s)
            {
                for (uint32 i = 0; i < qpiLinks; ++i)
                    cout << "QPI" << i << ";";
                for (uint32 i = 0; i < qpiLinks; ++i)
                    cout << "QPI" << i << ";";
            }

        }


        for (uint32 i = 0; i < m->getNumSockets(); ++i)
        {
            for (int s = 0; s <= PCM::MAX_C_STATE; ++s)
            if (m->isCoreCStateResidencySupported(s))
                cout << "C" << s << "res%;";

            for (int s = 0; s <= PCM::MAX_C_STATE; ++s)
            if (m->isPackageCStateResidencySupported(s))
                cout << "C" << s << "res%;";
        }

        if (m->packageEnergyMetricsAvailable())
        {
            for (uint32 i = 0; i < m->getNumSockets(); ++i)
                cout << "SKT" << i << ";";
        }
        if (m->dramEnergyMetricsAvailable())
        {
            for (uint32 i = 0; i < m->getNumSockets(); ++i)
                cout << "SKT" << i << ";";
        }

    }

    if (show_core_output)
    {
        for (uint32 i = 0; i < m->getNumCores(); ++i)
        {
            if (cpu_model == PCM::ATOM)
                cout << "EXEC;IPC;FREQ;L2MISS;L2HIT;";
            else
				{
					cout << "EXEC;IPC;FREQ;AFREQ;L3MISS;L2MISS;L3HIT;L2HIT;L3MPI;L2MPI;";
					if (m->L3CacheOccupancyMetricAvailable())
						cout << "L3OCC;";
					if (m->CoreLocalMemoryBWMetricAvailable())
						cout << "LMB";
					if (m->CoreRemoteMemoryBWMetricAvailable())
						cout << "RMB;";
					cout << "READ;WRITE;TEMP;";
				}


            for (int s = 0; s <= PCM::MAX_C_STATE; ++s)
            if (m->isCoreCStateResidencySupported(s))
                cout << "C" << s << "res%;";

            cout << "TEMP;";

        }

    }

}
*/
/*
void print_csv(PCM * m,
    const std::vector<CoreCounterState> & cstates1,
    const std::vector<CoreCounterState> & cstates2,
    const std::vector<SocketCounterState> & sktstate1,
    const std::vector<SocketCounterState> & sktstate2,
    const SystemCounterState& sstate1,
    const SystemCounterState& sstate2,
    const int cpu_model,
    const bool show_core_output,
    const bool show_socket_output,
    const bool show_system_output
    )
{
#ifndef _MSC_VER
    struct timeval timestamp;
    gettimeofday(&timestamp, NULL);
#endif
    time_t t = time(NULL);
    tm *tt = localtime(&t);
    char old_fill = cout.fill('0');
    cout.precision(3);
    cout << endl << setw(4) << 1900 + tt->tm_year << '-' << setw(2) << 1 + tt->tm_mon << '-'
        << setw(2) << tt->tm_mday << ';' << setw(2) << tt->tm_hour << ':'
        << setw(2) << tt->tm_min << ':' << setw(2) << tt->tm_sec
#ifdef _MSC_VER
        << ';';
#else
        << "." << setw(3) << ceil(timestamp.tv_usec / 1000) << ';';
#endif
    cout.fill(old_fill);

    if (show_system_output)
    {
        if (cpu_model != PCM::ATOM)
        {
            cout << getExecUsage(sstate1, sstate2) <<
                ';' << getIPC(sstate1, sstate2) <<
                ';' << getRelativeFrequency(sstate1, sstate2) <<
                ';' << getActiveRelativeFrequency(sstate1, sstate2) <<
                ';' << float_format(getL3CacheMisses(sstate1, sstate2)) <<
                ';' << float_format(getL2CacheMisses(sstate1, sstate2)) <<
                ';' << getL3CacheHitRatio(sstate1, sstate2) <<
                ';' << getL2CacheHitRatio(sstate1, sstate2) <<
                ';' << double(getL3CacheMisses(sstate1, sstate2)) / getInstructionsRetired(sstate1, sstate2) <<
                ';' << double(getL2CacheMisses(sstate1, sstate2)) / getInstructionsRetired(sstate1, sstate2) << ";";
            if (m->L3CacheOccupancyMetricAvailable())
                cout << "N/A;";
	    if(m->CoreLocalMemoryBWMetricAvailable())
            	cout << "N/A";
		if (m->CoreRemoteMemoryBWMetricAvailable())
			cout << "N/A;";
            if (!(m->memoryTrafficMetricsAvailable()))
                cout << "N/A;N/A;";
            else
                cout << getBytesReadFromMC(sstate1, sstate2) / double(1024ULL * 1024ULL * 1024ULL) <<
                ';' << getBytesWrittenToMC(sstate1, sstate2) / double(1024ULL * 1024ULL * 1024ULL) << ';';
        }
        else
            cout << getExecUsage(sstate1, sstate2) <<
            ';' << getIPC(sstate1, sstate2) <<
            ';' << getRelativeFrequency(sstate1, sstate2) <<
            ';' << float_format(getL2CacheMisses(sstate1, sstate2)) <<
            ';' << getL2CacheHitRatio(sstate1, sstate2) <<
            ';';

        cout << float_format(getInstructionsRetired(sstate1, sstate2)) << ";"
            << float_format(getCycles(sstate1, sstate2)) << ";"
            << float_format(getInvariantTSC(cstates1[0], cstates2[0])) << ";"
            << getCoreIPC(sstate1, sstate2) << ";"
            << 100. * (getCoreIPC(sstate1, sstate2) / double(m->getMaxIPC())) << ";"
            << getTotalExecUsage(sstate1, sstate2) << ";"
            << 100. * (getTotalExecUsage(sstate1, sstate2) / double(m->getMaxIPC())) << ";";

        if (m->getNumSockets() > 1) // QPI info only for multi socket systems
            cout << float_format(getAllIncomingQPILinkBytes(sstate1, sstate2)) << ";"
            << getQPItoMCTrafficRatio(sstate1, sstate2) << ";";
        if (m->outgoingQPITrafficMetricsAvailable())
            cout << float_format(getAllOutgoingQPILinkBytes(sstate1, sstate2)) << ";";

        for (int s = 0; s <= PCM::MAX_C_STATE; ++s)
        if (m->isCoreCStateResidencySupported(s))
            cout << getCoreCStateResidency(s, sstate1, sstate2) * 100 << ";";

        for (int s = 0; s <= PCM::MAX_C_STATE; ++s)
        if (m->isPackageCStateResidencySupported(s))
            cout << getPackageCStateResidency(s, sstate1, sstate2) * 100 << ";";

        if (m->packageEnergyMetricsAvailable())
            cout << getConsumedJoules(sstate1, sstate2) << ";";
        if (m->dramEnergyMetricsAvailable())
            cout << getDRAMConsumedJoules(sstate1, sstate2) << ";";

    }

    if (show_socket_output)
    {
        {
            for (uint32 i = 0; i < m->getNumSockets(); ++i)
            {
                if (cpu_model == PCM::ATOM)
                    cout << getExecUsage(sktstate1[i], sktstate2[i]) <<
                    ';' << getIPC(sktstate1[i], sktstate2[i]) <<
                    ';' << getRelativeFrequency(sktstate1[i], sktstate2[i]) <<
                    ';' << float_format(getL2CacheMisses(sktstate1[i], sktstate2[i])) <<
                    ';' << getL2CacheHitRatio(sktstate1[i], sktstate2[i]);
                else
                    cout << getExecUsage(sktstate1[i], sktstate2[i]) <<
                    ';' << getIPC(sktstate1[i], sktstate2[i]) <<
                    ';' << getRelativeFrequency(sktstate1[i], sktstate2[i]) <<
                    ';' << getActiveRelativeFrequency(sktstate1[i], sktstate2[i]) <<
                    ';' << float_format(getL3CacheMisses(sktstate1[i], sktstate2[i])) <<
                    ';' << float_format(getL2CacheMisses(sktstate1[i], sktstate2[i])) <<
                    ';' << getL3CacheHitRatio(sktstate1[i], sktstate2[i]) <<
                    ';' << getL2CacheHitRatio(sktstate1[i], sktstate2[i]) <<
                    ';' << double(getL3CacheMisses(sktstate1[i], sktstate2[i])) / getInstructionsRetired(sktstate1[i], sktstate2[i]) <<
                    ';' << double(getL2CacheMisses(sktstate1[i], sktstate2[i])) / getInstructionsRetired(sktstate1[i], sktstate2[i]) ;
                if (m->L3CacheOccupancyMetricAvailable())
                    cout << ';' << l3cache_occ_format(getL3CacheOccupancy(sktstate2[i]));
				if (m->CoreLocalMemoryBWMetricAvailable())
					cout << ';' << getLocalMemoryBW(sktstate1[i], sktstate2[i]);
				if (m->CoreRemoteMemoryBWMetricAvailable())
                	cout << ';' << getRemoteMemoryBW(sktstate1[i], sktstate2[i]) ;
		if (!(m->memoryTrafficMetricsAvailable()))
                    cout << ";N/A;N/A";
                else
                    cout << ';' << getBytesReadFromMC(sktstate1[i], sktstate2[i]) / double(1024ULL * 1024ULL * 1024ULL) <<
                    ';' << getBytesWrittenToMC(sktstate1[i], sktstate2[i]) / double(1024ULL * 1024ULL * 1024ULL);
                cout << ';' << temp_format(sktstate2[i].getThermalHeadroom()) << ';';
            }
        }

        if (m->getNumSockets() > 1) // QPI info only for multi socket systems
        {
            const uint32 qpiLinks = (uint32)m->getQPILinksPerSocket();
            for (uint32 i = 0; i < m->getNumSockets(); ++i)
            {
                for (uint32 l = 0; l < qpiLinks; ++l)
                    cout << float_format(getIncomingQPILinkBytes(i, l, sstate1, sstate2)) << ";";

                if (m->qpiUtilizationMetricsAvailable())
                {
                    for (uint32 l = 0; l < qpiLinks; ++l)
                        cout << setw(3) << std::dec << int(100. * getIncomingQPILinkUtilization(i, l, sstate1, sstate2)) << "%;";
                }
            }
        }

        if (m->getNumSockets() > 1 && (m->outgoingQPITrafficMetricsAvailable())) // QPI info only for multi socket systems
        {
            const uint32 qpiLinks = (uint32)m->getQPILinksPerSocket();
            for (uint32 i = 0; i < m->getNumSockets(); ++i)
            {
                for (uint32 l = 0; l < qpiLinks; ++l)
                    cout << float_format(getOutgoingQPILinkBytes(i, l, sstate1, sstate2)) << ";";

                for (uint32 l = 0; l < qpiLinks; ++l)
                    cout << setw(3) << std::dec << int(100. * getOutgoingQPILinkUtilization(i, l, sstate1, sstate2)) << "%;";
            }
        }


        for (uint32 i = 0; i < m->getNumSockets(); ++i)
        {
            for (int s = 0; s <= PCM::MAX_C_STATE; ++s)
            if (m->isCoreCStateResidencySupported(s))
                cout << getCoreCStateResidency(s, sktstate1[i], sktstate2[i]) * 100 << ";";

            for (int s = 0; s <= PCM::MAX_C_STATE; ++s)
            if (m->isPackageCStateResidencySupported(s))
                cout << getPackageCStateResidency(s, sktstate1[i], sktstate2[i]) * 100 << ";";
        }

        if (m->packageEnergyMetricsAvailable())
        {
            for (uint32 i = 0; i < m->getNumSockets(); ++i)
                cout << getConsumedJoules(sktstate1[i], sktstate2[i]) << ";";
        }
        if (m->dramEnergyMetricsAvailable())
        {
            for (uint32 i = 0; i < m->getNumSockets(); ++i)
                cout << getDRAMConsumedJoules(sktstate1[i], sktstate2[i]) << " ;";
        }
    }

    if (show_core_output)
    {
        for (uint32 i = 0; i < m->getNumCores(); ++i)
        {
            if (cpu_model != PCM::ATOM)
                {
                    cout << getExecUsage(cstates1[i], cstates2[i]) <<
                    ';' << getIPC(cstates1[i], cstates2[i]) <<
                    ';' << getRelativeFrequency(cstates1[i], cstates2[i]) <<
                    ';' << getActiveRelativeFrequency(cstates1[i], cstates2[i]) <<
                    ';' << float_format(getL3CacheMisses(cstates1[i], cstates2[i])) <<
                    ';' << float_format(getL2CacheMisses(cstates1[i], cstates2[i])) <<
                    ';' << getL3CacheHitRatio(cstates1[i], cstates2[i]) <<
                    ';' << getL2CacheHitRatio(cstates1[i], cstates2[i]) <<
                    ';' << double(getL3CacheMisses(cstates1[i], cstates2[i])) / getInstructionsRetired(cstates1[i], cstates2[i]) <<
                    ';' << double(getL2CacheMisses(cstates1[i], cstates2[i])) / getInstructionsRetired(cstates1[i], cstates2[i]);
                    if (m->L3CacheOccupancyMetricAvailable())
                        cout << ';' << l3cache_occ_format(getL3CacheOccupancy(cstates2[i])) ;
					if (m->CoreLocalMemoryBWMetricAvailable())
						cout << ';' << getLocalMemoryBW(cstates1[i], cstates2[i]);
					if (m->CoreRemoteMemoryBWMetricAvailable())
						cout <<	';' << getRemoteMemoryBW(cstates1[i], cstates2[i]) ;
		    cout << ';';
                }
            else
                cout << getExecUsage(cstates1[i], cstates2[i]) <<
                ';' << getIPC(cstates1[i], cstates2[i]) <<
                ';' << getRelativeFrequency(cstates1[i], cstates2[i]) <<
                ';' << float_format(getL2CacheMisses(cstates1[i], cstates2[i])) <<
                ';' << getL2CacheHitRatio(cstates1[i], cstates2[i]) <<
                ';' << temp_format(cstates2[i].getThermalHeadroom()) <<
                ';';

            for (int s = 0; s <= PCM::MAX_C_STATE; ++s)
            if (m->isCoreCStateResidencySupported(s))
                cout << getCoreCStateResidency(s, cstates1[i], cstates2[i]) * 100 << ";";

            cout << temp_format(cstates2[i].getThermalHeadroom()) << ';';
        }
    }

}
*/

///////////////////////////////////////////////////////////////////////////////////// pcm-power
int getFirstRank(int imc_profile)
{
    return imc_profile*2;
}
int getSecondRank(int imc_profile)
{
    return (imc_profile*2)+1;
}
///////////////////////////////////////////////////////////////////////////////////// pcm-power
double getCKEOffResidency(uint32 channel, uint32 rank, const ServerUncorePowerState & before, const ServerUncorePowerState & after)
{
    return double(getMCCounter(channel,(rank&1)?2:0,before,after))/double(getDRAMClocks(channel,before,after));
}
///////////////////////////////////////////////////////////////////////////////////// pcm-power
int64 getCKEOffAverageCycles(uint32 channel, uint32 rank, const ServerUncorePowerState & before, const ServerUncorePowerState & after)
{
    uint64 div = getMCCounter(channel,(rank&1)?3:1,before,after);
    if(div)
      return getMCCounter(channel,(rank&1)?2:0,before,after)/div;
    
    return -1;
}
///////////////////////////////////////////////////////////////////////////////////// pcm-power
int64 getCyclesPerTransition(uint32 channel, uint32 rank, const ServerUncorePowerState & before, const ServerUncorePowerState & after)
{
    uint64 div = getMCCounter(channel,(rank&1)?3:1,before,after);
    if(div)
      return getDRAMClocks(channel,before,after)/div;
    
    return -1;
}
///////////////////////////////////////////////////////////////////////////////////// pcm-power
uint64 getSelfRefreshCycles(uint32 channel, const ServerUncorePowerState & before, const ServerUncorePowerState & after)
{
    return getMCCounter(channel,0,before,after);
}
///////////////////////////////////////////////////////////////////////////////////// pcm-power
uint64 getSelfRefreshTransitions(uint32 channel, const ServerUncorePowerState & before, const ServerUncorePowerState & after)
{
    return getMCCounter(channel,1,before,after);
}
///////////////////////////////////////////////////////////////////////////////////// pcm-power
uint64 getPPDCycles(uint32 channel, const ServerUncorePowerState & before, const ServerUncorePowerState & after)
{
    return getMCCounter(channel,2,before,after);
}


///////////////////////////////////////////////////////////////////////////////////// pcm-memory
void printSocketBWHeader(uint32 no_columns, uint32 skt)
{
    for (uint32 i=skt; i<(no_columns+skt); ++i) {
        cout << "|---------------------------------------|";
    }
    cout << endl;
    for (uint32 i=skt; i<(no_columns+skt); ++i) {
        cout << "|--             Socket "<<setw(2)<<i<<"             --|";
    }
    cout << endl;
    for (uint32 i=skt; i<(no_columns+skt); ++i) {
        cout << "|---------------------------------------|";
    }
    cout << endl;
    for (uint32 i=skt; i<(no_columns+skt); ++i) {
        cout << "|--     Memory Channel Monitoring     --|";
    }
    cout << endl;
    for (uint32 i=skt; i<(no_columns+skt); ++i) {
        cout << "|---------------------------------------|";
    }
    cout << endl;
}
///////////////////////////////////////////////////////////////////////////////////// pcm-memory
void printSocketRankBWHeader(uint32 no_columns, uint32 skt)
{
    for (uint32 i=skt; i<(no_columns+skt); ++i) {
        cout << "|-------------------------------------------|";
    }
    cout << endl;
    for (uint32 i=skt; i<(no_columns+skt); ++i) {
        cout << "|--               Socket "<<setw(2)<<i<<"               --|";
    }
    cout << endl;
    for (uint32 i=skt; i<(no_columns+skt); ++i) {
        cout << "|-------------------------------------------|";
    }
    cout << endl;
    for (uint32 i=skt; i<(no_columns+skt); ++i) {
        cout << "|--           DIMM Rank Monitoring        --|";
    }
    cout << endl;
    for (uint32 i=skt; i<(no_columns+skt); ++i) {
        cout << "|-------------------------------------------|";
    }
    cout << endl;
}
///////////////////////////////////////////////////////////////////////////////////// pcm-memory
void printSocketChannelBW(uint32 no_columns, uint32 skt, uint32 num_imc_channels, float* iMC_Rd_socket_chan, float* iMC_Wr_socket_chan)
{
    for (uint32 channel = 0; channel < num_imc_channels; ++channel) {
        // check all the sockets for bad channel "channel"
        unsigned bad_channels = 0;
        for (uint32 i=skt; i<(skt+no_columns); ++i) {
            if (iMC_Rd_socket_chan[i*num_imc_channels + channel] < 0.0 || iMC_Wr_socket_chan[i*num_imc_channels + channel] < 0.0) //If the channel read neg. value, the channel is not working; skip it.
                ++bad_channels;
        }
        if (bad_channels == no_columns) { // the channel is missing on all sockets in the row
            continue;
        }
        for (uint32 i=skt; i<(skt+no_columns); ++i) {
            cout << "|-- Mem Ch "<<setw(2)<<channel<<": Reads (MB/s): "<<setw(8)<<iMC_Rd_socket_chan[i*num_imc_channels+channel]<<" --|";
        }
        cout << endl;
        for (uint32 i=skt; i<(skt+no_columns); ++i) {
            cout << "|--            Writes(MB/s): "<<setw(8)<<iMC_Wr_socket_chan[i*num_imc_channels+channel]<<" --|";
        }
        cout << endl;
    }
}
///////////////////////////////////////////////////////////////////////////////////// pcm-memory
void printSocketChannelBW(uint32 no_columns, uint32 skt, uint32 num_imc_channels, const ServerUncorePowerState * uncState1, const ServerUncorePowerState * uncState2, uint64 elapsedTime, int rankA, int rankB)
{
    for (uint32 channel = 0; channel < num_imc_channels; ++channel) {
        if(rankA >= 0) {
          for (uint32 i=skt; i<(skt+no_columns); ++i) {
              cout << "|-- Mem Ch "<<setw(2)<<channel<<" R " << setw(1) << rankA <<": Reads (MB/s): "<<setw(8)<<(float) (getMCCounter(channel,READ_RANK_A,uncState1[i],uncState2[i]) * 64 / 1000000.0 / (elapsedTime/1000.0))<<" --|";
          }
          cout << endl;
          for (uint32 i=skt; i<(skt+no_columns); ++i) {
              cout << "|--                Writes(MB/s): "<<setw(8)<<(float) (getMCCounter(channel,WRITE_RANK_A,uncState1[i],uncState2[i]) * 64 / 1000000.0 / (elapsedTime/1000.0))<<" --|";
          }
          cout << endl;
        }
        if(rankB >= 0) {
          for (uint32 i=skt; i<(skt+no_columns); ++i) {
              cout << "|-- Mem Ch "<<setw(2) << channel<<" R " << setw(1) << rankB <<": Reads (MB/s): "<<setw(8)<<(float) (getMCCounter(channel,READ_RANK_B,uncState1[i],uncState2[i]) * 64 / 1000000.0 / (elapsedTime/1000.0))<<" --|";
          }
          cout << endl;
          for (uint32 i=skt; i<(skt+no_columns); ++i) {
              cout << "|--                Writes(MB/s): "<<setw(8)<<(float) (getMCCounter(channel,WRITE_RANK_B,uncState1[i],uncState2[i]) * 64 / 1000000.0 / (elapsedTime/1000.0))<<" --|";
          }
          cout << endl;
        }
    }
}
///////////////////////////////////////////////////////////////////////////////////// pcm-memory
void printSocketBWFooter(uint32 no_columns, uint32 skt, float* iMC_Rd_socket, float* iMC_Wr_socket, uint64* partial_write)
{
    for (uint32 i=skt; i<(skt+no_columns); ++i) {
        cout << "|-- NODE"<<setw(2)<<i<<" Mem Read (MB/s) : "<<setw(8)<<iMC_Rd_socket[i]<<" --|";
    }
    cout << endl;
    for (uint32 i=skt; i<(skt+no_columns); ++i) {
        cout << "|-- NODE"<<setw(2)<<i<<" Mem Write(MB/s) : "<<setw(8)<<iMC_Wr_socket[i]<<" --|";
    }
    cout << endl;
    for (uint32 i=skt; i<(skt+no_columns); ++i) {
        cout << "|-- NODE"<<setw(2)<<i<<" P. Write (T/s): "<<dec<<setw(10)<<partial_write[i]<<" --|";
    }
    cout << endl;
    for (uint32 i=skt; i<(skt+no_columns); ++i) {
        cout << "|-- NODE"<<setw(2)<<i<<" Memory (MB/s): "<<setw(11)<<std::right<<iMC_Rd_socket[i]+iMC_Wr_socket[i]<<" --|";
    }
    cout << endl;
    for (uint32 i=skt; i<(no_columns+skt); ++i) {
        cout << "|---------------------------------------|";
    }
    cout << endl;
}

///////////////////////////////////////////////////////////////////////////////////// pcm-memory
void display_bandwidth(float *iMC_Rd_socket_chan, float *iMC_Wr_socket_chan, float *iMC_Rd_socket, float *iMC_Wr_socket, uint32 numSockets, uint32 num_imc_channels, uint64 *partial_write, uint32 no_columns )
{
    float sysRead = 0.0, sysWrite = 0.0;
    uint32 skt = 0;
    cout.setf(ios::fixed);
    cout.precision(2);

    while(skt < numSockets)
    {
        // Full row
        if ( (skt+no_columns) <= numSockets )
        {
            printSocketBWHeader (no_columns, skt);
            printSocketChannelBW(no_columns, skt, num_imc_channels, iMC_Rd_socket_chan, iMC_Wr_socket_chan);
            printSocketBWFooter (no_columns, skt, iMC_Rd_socket, iMC_Wr_socket, partial_write);
            for (uint32 i=skt; i<(skt+no_columns); i++) {
                sysRead += iMC_Rd_socket[i];
                sysWrite += iMC_Wr_socket[i];
            }
            skt += no_columns;
        }
        else //Display one socket in this row
        {
            cout << "\
                \r|---------------------------------------|\n\
                \r|--             Socket "<<skt<<"              --|\n\
                \r|---------------------------------------|\n\
                \r|--     Memory Channel Monitoring     --|\n\
                \r|---------------------------------------|\n\
                \r"; 
            for(uint64 channel = 0; channel < num_imc_channels; ++channel)
            {
                if(iMC_Rd_socket_chan[skt*num_imc_channels+channel] < 0.0 && iMC_Wr_socket_chan[skt*num_imc_channels+channel] < 0.0) //If the channel read neg. value, the channel is not working; skip it.
                    continue;
                cout << "|--  Mem Ch "
                    <<channel
                    <<": Reads (MB/s):"
                    <<setw(8)
                    <<iMC_Rd_socket_chan[skt*num_imc_channels+channel]
                    <<"  --|\n|--            Writes(MB/s):"
                    <<setw(8)
                    <<iMC_Wr_socket_chan[skt*num_imc_channels+channel]
                    <<"  --|\n";

            }
            cout << "\
                \r|-- NODE"<<skt<<" Mem Read (MB/s):  "<<setw(8)<<iMC_Rd_socket[skt]<<"  --|\n\
                \r|-- NODE"<<skt<<" Mem Write (MB/s) :"<<setw(8)<<iMC_Wr_socket[skt]<<"  --|\n\
                \r|-- NODE"<<skt<<" P. Write (T/s) :"<<setw(10)<<dec<<partial_write[skt]<<"  --|\n\
                \r|-- NODE"<<skt<<" Memory (MB/s): "<<setw(8)<<iMC_Rd_socket[skt]+iMC_Wr_socket[skt]<<"     --|\n\
                \r|---------------------------------------|\n\
                \r";

            sysRead += iMC_Rd_socket[skt];
            sysWrite += iMC_Wr_socket[skt];
            skt += 1;
        }
    }
    cout << "\
        \r|---------------------------------------||---------------------------------------|\n\
        \r|--                   System Read Throughput(MB/s):"<<setw(10)<<sysRead<<"                  --|\n\
        \r|--                  System Write Throughput(MB/s):"<<setw(10)<<sysWrite<<"                  --|\n\
        \r|--                 System Memory Throughput(MB/s):"<<setw(10)<<sysRead+sysWrite<<"                  --|\n\
        \r|---------------------------------------||---------------------------------------|" << endl;
}

const uint32 max_sockets = 256;
const uint32 max_imc_channels = 8;

///////////////////////////////////////////////////////////////////////////////////// pcm-memory
void calculate_bandwidth(PCM *m, const ServerUncorePowerState uncState1[], const ServerUncorePowerState uncState2[], uint64 elapsedTime, bool csv, bool & csvheader, uint32 no_columns)
{
    //const uint32 num_imc_channels = m->getMCChannelsPerSocket();
    float iMC_Rd_socket_chan[max_sockets][max_imc_channels];
    float iMC_Wr_socket_chan[max_sockets][max_imc_channels];
    float iMC_Rd_socket[max_sockets];
    float iMC_Wr_socket[max_sockets];
    uint64 partial_write[max_sockets];

    for(uint32 skt = 0; skt < m->getNumSockets(); ++skt)
    {
        iMC_Rd_socket[skt] = 0.0;
        iMC_Wr_socket[skt] = 0.0;
        partial_write[skt] = 0;

        for(uint32 channel = 0; channel < max_imc_channels; ++channel)
        {
            if(getMCCounter(channel,READ,uncState1[skt],uncState2[skt]) == 0.0 && getMCCounter(channel,WRITE,uncState1[skt],uncState2[skt]) == 0.0) //In case of JKT-EN, there are only three channels. Skip one and continue.
            {
                iMC_Rd_socket_chan[skt][channel] = -1.0;
                iMC_Wr_socket_chan[skt][channel] = -1.0;
                continue;
            }

            iMC_Rd_socket_chan[skt][channel] = (float) (getMCCounter(channel,READ,uncState1[skt],uncState2[skt]) * 64 / 1000000.0 / (elapsedTime/1000.0));
            iMC_Wr_socket_chan[skt][channel] = (float) (getMCCounter(channel,WRITE,uncState1[skt],uncState2[skt]) * 64 / 1000000.0 / (elapsedTime/1000.0));

            iMC_Rd_socket[skt] += iMC_Rd_socket_chan[skt][channel];
            iMC_Wr_socket[skt] += iMC_Wr_socket_chan[skt][channel];

            partial_write[skt] += (uint64) (getMCCounter(channel,PARTIAL,uncState1[skt],uncState2[skt]) / (elapsedTime/1000.0));
        }
    }

   // if (csv) {
   //   if (csvheader) {
	//display_bandwidth_csv_header(iMC_Rd_socket_chan[0], iMC_Wr_socket_chan[0], iMC_Rd_socket, iMC_Wr_socket, m->getNumSockets(), max_imc_channels, partial_write);
	//csvheader = false;
    //  }
      //display_bandwidth_csv(iMC_Rd_socket_chan[0], iMC_Wr_socket_chan[0], iMC_Rd_socket, iMC_Wr_socket, m->getNumSockets(), max_imc_channels, partial_write, elapsedTime);
    //} else {
      display_bandwidth(iMC_Rd_socket_chan[0], iMC_Wr_socket_chan[0], iMC_Rd_socket, iMC_Wr_socket, m->getNumSockets(), max_imc_channels, partial_write, no_columns);
  //  }
}

///////////////////////////////////////////////////////////////////////////////////// pcm-memory
void calculate_bandwidth(PCM *m, const ServerUncorePowerState uncState1[], const ServerUncorePowerState uncState2[], uint64 elapsedTime, bool csv, bool & csvheader, uint32 no_columns, int rankA, int rankB)
{
    uint32 skt = 0;
    cout.setf(ios::fixed);
    cout.precision(2);
    uint32 numSockets = m->getNumSockets();

    while(skt < numSockets)
    {
        // Full row
        if ( (skt+no_columns) <= numSockets )
        {
            printSocketRankBWHeader(no_columns, skt);
            printSocketChannelBW(no_columns, skt, max_imc_channels, uncState1, uncState2, elapsedTime, rankA, rankB);
            for (uint32 i=skt; i<(no_columns+skt); ++i) {
              cout << "|-------------------------------------------|";
            }
            cout << endl;
            skt += no_columns;
        }
        else //Display one socket in this row
        {
            cout << "\
                \r|-------------------------------------------|\n\
                \r|--               Socket "<<skt<<"                --|\n\
                \r|-------------------------------------------|\n\
                \r|--           DIMM Rank Monitoring        --|\n\
                \r|-------------------------------------------|\n\
                \r";
            for(uint64 channel = 0; channel < max_imc_channels; ++channel)
            {
                if(rankA >=0)
                  cout << "|-- Mem Ch "
                      << setw(2) << channel
                      << " R " << setw(1) << rankA
                      <<": Reads (MB/s):"
                      <<setw(8)
                      <<(float) (getMCCounter(channel,READ_RANK_A,uncState1[skt],uncState2[skt]) * 64 / 1000000.0 / (elapsedTime/1000.0))
                      <<"  --|\n|--                Writes(MB/s):"
                      <<setw(8)
                      <<(float) (getMCCounter(channel,WRITE_RANK_A,uncState1[skt],uncState2[skt]) * 64 / 1000000.0 / (elapsedTime/1000.0))
                      <<"  --|\n";
                if(rankB >=0)
                  cout << "|-- Mem Ch "
                      << setw(2) << channel
                      << " R " << setw(1) << rankB
                      <<": Reads (MB/s):"
                      <<setw(8)
                      <<(float) (getMCCounter(channel,READ_RANK_B,uncState1[skt],uncState2[skt]) * 64 / 1000000.0 / (elapsedTime/1000.0))
                      <<"  --|\n|--                Writes(MB/s):"
                      <<setw(8)
                      <<(float) (getMCCounter(channel,WRITE_RANK_B,uncState1[skt],uncState2[skt]) * 64 / 1000000.0 / (elapsedTime/1000.0))
                      <<"  --|\n";
            }
            cout << "\
                \r|-------------------------------------------|\n\
                \r";

            skt += 1;
        }
    }
}






int main(int argc, char * argv[])
{

   // long long int bsize = N1g;
	int app = 1;

   // set_signal_handlers();

#ifdef PCM_FORCE_SILENT
    null_stream nullStream1, nullStream2;
    std::cout.rdbuf(&nullStream1);
    std::cerr.rdbuf(&nullStream2);
#endif

    cerr << endl;
    cerr << " Intel(r) Performance Counter Monitor " << INTEL_PCM_VERSION << endl;
    cerr << endl;
    cerr << INTEL_PCM_COPYRIGHT << endl;
    cerr << endl;

    // if delay is not specified: use either default (1 second),
    // or only read counters before or after PCM started: keep PCM blocked
   double delay = -1.0;

    char *sysCmd = NULL;
    char **sysArgv = NULL;
    bool show_core_output = true;
    bool show_partial_core_output = false;
    bool show_socket_output = true;
    bool show_system_output = true;
    bool csv_output = false;
    bool reset_pmu = false;
    bool allow_multiple_instances = false;
    bool disable_JKT_workaround = false; // as per http://software.intel.com/en-us/articles/performance-impact-when-sampling-certain-llc-events-on-snb-ep-with-vtune

    long diff_usec = 0; // deviation of clock is useconds between measurements
    int calibrated = PCM_CALIBRATION_INTERVAL - 2; // keeps track is the clock calibration needed
    unsigned int numberOfIterations = 0; // number of iterations
    std::bitset<MAX_CORES> ycores;
	
	bool csv = false;
	bool csvheader=false;
	uint32 no_columns = DEFAULT_DISPLAY_COLUMNS; // Default number of columns is 2
	
	int rankA = -1, rankB = -1; // memory

	int imc_profile = 0; // power
	
    string program = string(argv[0]);

    PCM * m = PCM::getInstance();

    if (argc > 1) do
    {
        argv++;
        argc--;
        if (strncmp(*argv, "--help", 6) == 0 ||
            strncmp(*argv, "-h", 2) == 0 ||
            strncmp(*argv, "/h", 2) == 0)
        {
	    
           // print_help(program);
            exit(EXIT_FAILURE);
        }
      
        else
        {
            // any other options positional that is a floating point number is treated as <delay>,
            // while the other options are ignored with a warning issues to stderr

	unsigned int inapp =1;
            std::istringstream is_str_stream(*argv);
            is_str_stream >> noskipws >> inapp;
            if (is_str_stream.eof() && !is_str_stream.fail()) {
                app = inapp;
                //cerr << "Block size is: " << (bsize/(1024*1024*1024)) << "GB" << endl;
            }

	  /*  unsigned int insize=1;
            std::istringstream is_str_stream(*argv);
            is_str_stream >> noskipws >> insize;
            if (is_str_stream.eof() && !is_str_stream.fail()) {
                bsize = bsize * insize;
                cerr << "Block size is: " << (bsize/(1024*1024*1024)) << "GB" << endl;
            }


            double delay_input;
            std::istringstream is_str_stream(*argv);
            is_str_stream >> noskipws >> delay_input;
            if (is_str_stream.eof() && !is_str_stream.fail() && delay == -1) {
                delay = delay_input;
                cerr << "Delay: " << delay << endl;
            }
            else {
                cerr << "WARNING: unknown command-line option: \"" << *argv << "\". Ignoring it." << endl;
                //print_help(program);
                exit(EXIT_FAILURE);
            }*/
            continue;
        }
    } while (argc > 1); // end of command line partsing loop

    if (disable_JKT_workaround) m->disableJKTWorkaround();

    if (true)
    {
        cerr << "\n Resetting PMU configuration" << endl;
        m->resetPMU();
    }

    if (allow_multiple_instances)
    {
        m->allowMultipleInstances();
    }

    // program() creates common semaphore for the singleton, so ideally to be called before any other references to PCM
    PCM::ErrorCode status = m->program();

    switch (status)
    {
    case PCM::Success:
        break;
    case PCM::MSRAccessDenied:
        cerr << "Access to Intel(r) Performance Counter Monitor has denied (no MSR or PCI CFG space access)." << endl;
        exit(EXIT_FAILURE);
    case PCM::PMUBusy:
        cerr << "Access to Intel(r) Performance Counter Monitor has denied (Performance Monitoring Unit is occupied by other application). Try to stop the application that uses PMU." << endl;
        cerr << "Alternatively you can try running Intel PCM with option -r to reset PMU configuration at your own risk." << endl;
        exit(EXIT_FAILURE);
    default:
        cerr << "Access to Intel(r) Performance Counter Monitor has denied (Unknown error)." << endl;
        exit(EXIT_FAILURE);
    }

    cerr << "\nDetected " << m->getCPUBrandString() << " \"Intel(r) microarchitecture codename " << m->getUArchCodename() << "\"" << endl;

	
	ServerUncorePowerState * BeforeState = new ServerUncorePowerState[m->getNumSockets()];
    ServerUncorePowerState * AfterState = new ServerUncorePowerState[m->getNumSockets()];
    uint64 BeforeTime = 0, AfterTime = 0;
	
	
	
    std::vector<CoreCounterState> cstates1, cstates2;
    std::vector<SocketCounterState> sktstate1, sktstate2;
    SystemCounterState sstate1, sstate2;
    const int cpu_model = m->getCPUModel();
    uint64 TimeAfterSleep = 0;
    PCM_UNUSED(TimeAfterSleep);

    if ((sysCmd != NULL) && (delay <= 0.0)) {
        // in case external command is provided in command line, and
        // delay either not provided (-1) or is zero
        m->setBlocked(true);
    }
    else {
        m->setBlocked(false);
    }

    if (csv_output) {
      //  print_csv_header(m, cpu_model, show_core_output, show_socket_output, show_system_output);
        if (delay <= 0.0) delay = PCM_DELAY_DEFAULT;
    }
    else {
        // for non-CSV mode delay < 1.0 does not make a lot of practical sense:
        // hard to read from the screen, or
        // in case delay is not provided in command line => set default
        if (((delay<1.0) && (delay>0.0)) || (delay <= 0.0)) delay = PCM_DELAY_DEFAULT;
    }
    // cerr << "DEBUG: Delay: " << delay << " seconds. Blocked: " << m->isBlocked() << endl;

    m->getAllCounterStates(sstate1, sktstate1, cstates1);

	
    BeforeTime = m->getTickCount();
 for(uint32 i=0; i<m->getNumSockets(); ++i)
        BeforeState[i] = m->getServerUncorePowerState(i); 

	
    if (sysCmd != NULL) {
        MySystem(sysCmd, sysArgv);
    }

    unsigned int i = 1;

    while (i == 1)
    {
        if (!csv_output) cout << std::flush;
        int delay_ms = int(delay * 1000);
        int calibrated_delay_ms = delay_ms;
#ifdef _MSC_VER
        // compensate slow Windows console output
        if (TimeAfterSleep) delay_ms -= (uint32)(m->getTickCount() - TimeAfterSleep);
        if (delay_ms < 0) delay_ms = 0;
#else
        // compensation of delay on Linux/UNIX
        // to make the sampling interval as monotone as possible
        struct timeval start_ts, end_ts;
        if (calibrated == 0) {
            gettimeofday(&end_ts, NULL);
            diff_usec = (end_ts.tv_sec - start_ts.tv_sec)*1000000.0 + (end_ts.tv_usec - start_ts.tv_usec);
            calibrated_delay_ms = delay_ms - diff_usec / 1000.0;
        }
#endif

/////////////////////////////////////////////////////////////////////////////////////////////// App

cout << "start benchmark program \n";
//system("./formem.o 4");

if(app==1)
{
 system("parsecmgmt -a run -p facesim -i native -n 8");
 cout << "\nfinished facesim \n";
}
else if(app==2)
{
 system("parsecmgmt -a run -p raytrace -i native -n 8");
 cout << "\nfinished raytrace \n";
}
else if(app==3)
{
 system("parsecmgmt -a run -p parsec.ferret -i native -n 8");
 cout << "\nfinished ferret \n";
}
else if(app==4)
{
 system("parsecmgmt -a run -p bodytrack -i native -n 8");
 cout << "\nfinished bodytrack \n";
}
else if(app==5)
{
 system("parsecmgmt -a run -p freqmine -i native -n 8");
  cout << "\nfinished freqmine \n";
}
else if(app==6)
{
 system("parsecmgmt -a run -p vips -i native -n 8");
cout << "\nfinished vips \n";
}
//system("parsecmgmt -a run -p parsec.ferret");
//system("parsecmgmt -a run -p parsec.ferret -i native -n 16");
//system("parsecmgmt -a run -p raytrace -i native -n 16");
//system("parsecmgmt -a run -p facesim -i native -n 16");
//system("parsecmgmt -a run -p bodytrack -i native -n 16");
//system("parsecmgmt -a run -p freqmine -i native -n 16");
//system("parsecmgmt -a run -p x264 -i native -n 16"); // has error during exe
//system("parsecmgmt -a run -p vips -i native -n 16");

//cout << "\nfinished benchmark program \n";


/*
 cout << "\nBs is: " << (bsize/(1024*1024*1024)) << "GB\n";

char *cell = (char *) malloc(sizeof(char) * bsize);
   	long long int ii = 0;
   	

// write to mem
   	for(ii=0;ii < bsize;ii++)
   	{
		*(cell + ii) = 'd';
		
	
   	}
cout << "write is done! \n";
int temp=0;
int temp1=0;	
//read from mem	
   	for(ii=0;ii < bsize;ii++)
   	{
		temp = ((int) *(cell + ii));
		temp1 = temp;			
   	}
cout << "read is done!" << temp1 << "\n";	
free(cell);
*/

///////////////////////////////////////////////////////////////////////////////////////////////
       


    //    MySleepMs(calibrated_delay_ms);

#ifndef _MSC_VER
        calibrated = (calibrated + 1) % PCM_CALIBRATION_INTERVAL;
        if (calibrated == 0) {
            gettimeofday(&start_ts, NULL);
        }
#endif
        TimeAfterSleep = m->getTickCount();

        m->getAllCounterStates(sstate2, sktstate2, cstates2);

		AfterTime = m->getTickCount();
cout << "\nTime elapsed: "<<dec<<fixed<<AfterTime-BeforeTime<<" ms\n";
		
        for(uint32 i=0; i<m->getNumSockets(); ++i)
            AfterState[i] = m->getServerUncorePowerState(i);
		
		
       // if (csv_output)
           // print_csv(m, cstates1, cstates2, sktstate1, sktstate2, sstate1, sstate2,
           // cpu_model, show_core_output, show_socket_output, show_system_output);
      //  else
            print_output(m, cstates1, cstates2, sktstate1, sktstate2, ycores, sstate1, sstate2,
            cpu_model, show_core_output, show_partial_core_output, show_socket_output, show_system_output);

			
		///////////////////////////////////////////////////////////////////////////////////// pcm-memory
        if(rankA >= 0 || rankB >= 0)
          calculate_bandwidth(m,BeforeState,AfterState,AfterTime-BeforeTime,csv,csvheader, no_columns, rankA, rankB);
        else
          calculate_bandwidth(m,BeforeState,AfterState,AfterTime-BeforeTime,csv,csvheader, no_columns);
	  
///////////////////////////////////////////////////////////////////////////////////// pcm-power	  
for(uint32 socket=0;socket<m->getNumSockets();++socket)
      {
          for(uint32 port=0;port<m->getQPILinksPerSocket();++port)
          {
            std::cout << "S"<<socket<<"P"<<port
                  << "; QPIClocks: "<< getQPIClocks(port,BeforeState[socket],AfterState[socket])
                  << "; L0p Tx Cycles: "<< 100.*getNormalizedQPIL0pTxCycles(port,BeforeState[socket],AfterState[socket])<< "%"
                  << "; L1 Cycles: "    << 100.*getNormalizedQPIL1Cycles(port,BeforeState[socket],AfterState[socket])<< "%"
                  << "\n";
          }
          for(uint32 channel=0;channel<m->getMCChannelsPerSocket();++channel)
          {
			  imc_profile = 0;
            
              std::cout << "S"<<socket<<"CH"<<channel <<"; DRAMClocks: "<< getDRAMClocks(channel,BeforeState[socket],AfterState[socket])
                 << "; Rank"<<getFirstRank(imc_profile)<<" CKE Off Residency: "<< std::setw(3) << 
                100.*getCKEOffResidency(channel,getFirstRank(imc_profile),BeforeState[socket],AfterState[socket])<<"%"
                << "; Rank"<<getFirstRank(imc_profile)<<" CKE Off Average Cycles: "<<
                getCKEOffAverageCycles(channel,getFirstRank(imc_profile),BeforeState[socket],AfterState[socket])
                << "; Rank"<<getFirstRank(imc_profile)<<" Cycles per transition: "<<
                getCyclesPerTransition(channel,getFirstRank(imc_profile),BeforeState[socket],AfterState[socket])
                << "\n";

              std::cout << "S"<<socket<<"CH"<<channel <<"; DRAMClocks: "<< getDRAMClocks(channel,BeforeState[socket],AfterState[socket])
                << "; Rank"<<getSecondRank(imc_profile)<<" CKE Off Residency: "<< std::setw(3) <<
              100.*getCKEOffResidency(channel,getSecondRank(imc_profile),BeforeState[socket],AfterState[socket])<<"%"
                << "; Rank"<<getSecondRank(imc_profile)<<" CKE Off Average Cycles: "<< 
              getCKEOffAverageCycles(channel,getSecondRank(imc_profile),BeforeState[socket],AfterState[socket])
                << "; Rank"<<getSecondRank(imc_profile)<<" Cycles per transition: "<< 
              getCyclesPerTransition(channel,getSecondRank(imc_profile),BeforeState[socket],AfterState[socket])
                << "\n";


            
              std::cout << "S"<<socket<<"CH"<<channel
                << "; DRAMClocks: "<< getDRAMClocks(channel,BeforeState[socket],AfterState[socket])
                << "; Self-refresh cycles: "<< getSelfRefreshCycles(channel,BeforeState[socket],AfterState[socket])
                << "; Self-refresh transitions: "<< getSelfRefreshTransitions(channel,BeforeState[socket],AfterState[socket])
                << "; PPD cycles: "<< getPPDCycles(channel,BeforeState[socket],AfterState[socket])
                << "\n";
            
        }
        

        std::cout << "S"<<socket
              << "; Consumed energy units: "<< getConsumedEnergy(BeforeState[socket],AfterState[socket])
              << "; Consumed Joules: "<< getConsumedJoules(BeforeState[socket],AfterState[socket])
              << "; Watts: "<< 1000.*getConsumedJoules(BeforeState[socket],AfterState[socket])/double(AfterTime-BeforeTime)
              << "; Thermal headroom below TjMax: " << AfterState[socket].getPackageThermalHeadroom()
              << "\n";
        std::cout << "S"<<socket
              << "; Consumed DRAM energy units: "<< getDRAMConsumedEnergy(BeforeState[socket],AfterState[socket])
              << "; Consumed DRAM Joules: "<< getDRAMConsumedJoules(BeforeState[socket],AfterState[socket])
                          << "; DRAM Watts: "<< 1000.*getDRAMConsumedJoules(BeforeState[socket],AfterState[socket])/double(AfterTime-BeforeTime)
              << "\n";


      }	
        // sanity checks
        if (cpu_model == PCM::ATOM)
        {
            assert(getNumberOfCustomEvents(0, sstate1, sstate2) == getL2CacheMisses(sstate1, sstate2));
            assert(getNumberOfCustomEvents(1, sstate1, sstate2) == getL2CacheMisses(sstate1, sstate2) + getL2CacheHits(sstate1, sstate2));
        }
        else
        {
            assert(getNumberOfCustomEvents(0, sstate1, sstate2) == getL3CacheMisses(sstate1, sstate2));
            if (m->useSkylakeEvents()) {
                assert(getNumberOfCustomEvents(1, sstate1, sstate2) == getL3CacheHits(sstate1, sstate2));
                assert(getNumberOfCustomEvents(2, sstate1, sstate2) == getL2CacheMisses(sstate1, sstate2));
            }
            else {
                assert(getNumberOfCustomEvents(1, sstate1, sstate2) == getL3CacheHitsNoSnoop(sstate1, sstate2));
                assert(getNumberOfCustomEvents(2, sstate1, sstate2) == getL3CacheHitsSnoop(sstate1, sstate2));
            }
            assert(getNumberOfCustomEvents(3, sstate1, sstate2) == getL2CacheHits(sstate1, sstate2));
        }

        std::swap(sstate1, sstate2);
        std::swap(sktstate1, sktstate2);
        std::swap(cstates1, cstates2);
		
		 swap(BeforeTime, AfterTime);
        swap(BeforeState, AfterState);


        if (m->isBlocked()) {
            // in case PCM was blocked after spawning child application: break monitoring loop here
            break;
        }

        ++i;
    }
if (true)
    {
        cerr << "\n Resetting PMU configuration" << endl;
        m->resetPMU();
    }

    exit(EXIT_SUCCESS);
}
