#include <iostream>
#include <fstream>
#include <stdlib.h>
using namespace std;

int main()
{
ofstream myfile;
	myfile.open("H_wc.csv");
	  
      myfile << "mem_size,mem_freq,channels,cpu_freq,exe_time,IPC,L3_hit,L2_hit,C0_residency,CPU_Energy,DRAM_Energy,EDP(k)(k),CPU_watt,DRAM_watt,BW\n";
      myfile << "\n";
    myfile.close();
	
	myfile.open("H_srt.csv");
	
      myfile << "mem_size,mem_freq,channels,cpu_freq,exe_time,IPC,L3_hit,L2_hit,C0_residency,CPU_Energy,DRAM_Energy,EDP(k),CPU_watt,DRAM_watt,BW\n";
      myfile << "\n";
    myfile.close();
	
	myfile.open("H_grep.csv");
	
      myfile << "mem_size,mem_freq,channels,cpu_freq,exe_time,IPC,L3_hit,L2_hit,C0_residency,CPU_Energy,DRAM_Energy,EDP(k),CPU_watt,DRAM_watt,BW\n";
      myfile << "\n";
    myfile.close();
	
	myfile.open("H_pagerank.csv");
	
      myfile << "mem_size,mem_freq,channels,cpu_freq,exe_time,IPC,L3_hit,L2_hit,C0_residency,CPU_Energy,DRAM_Energy,EDP(k),CPU_watt,DRAM_watt,BW\n";
      myfile << "\n";
    myfile.close();
	
	myfile.open("H_nbayes.csv");
	
      myfile << "mem_size,mem_freq,channels,cpu_freq,exe_time,IPC,L3_hit,L2_hit,C0_residency,CPU_Energy,DRAM_Energy,EDP(k),CPU_watt,DRAM_watt,BW\n";
      myfile << "\n";
    myfile.close();
   
   myfile.open("S_wc.csv");
  
      myfile << "mem_size,mem_freq,channels,cpu_freq,exe_time,IPC,L3_hit,L2_hit,C0_residency,CPU_Energy,DRAM_Energy,EDP(k),CPU_watt,DRAM_watt,BW\n";
      myfile << "\n";
    myfile.close();
	
	myfile.open("S_srt.csv");
	
      myfile << "mem_size,mem_freq,channels,cpu_freq,exe_time,IPC,L3_hit,L2_hit,C0_residency,CPU_Energy,DRAM_Energy,EDP(k),CPU_watt,DRAM_watt,BW\n";
      myfile << "\n";
    myfile.close();
	
	myfile.open("S_grep.csv");
	
      myfile << "mem_size,mem_freq,channels,cpu_freq,exe_time,IPC,L3_hit,L2_hit,C0_residency,CPU_Energy,DRAM_Energy,EDP(k),CPU_watt,DRAM_watt,BW\n";
      myfile << "\n";
    myfile.close();
	
	myfile.open("S_pagerank.csv");
	
      myfile << "mem_size,mem_freq,channels,cpu_freq,exe_time,IPC,L3_hit,L2_hit,C0_residency,CPU_Energy,DRAM_Energy,EDP(k),CPU_watt,DRAM_watt,BW\n";
      myfile << "\n";
    myfile.close();
	
	myfile.open("S_nbayes.csv");
	
      myfile << "mem_size,mem_freq,channels,cpu_freq,exe_time,IPC,L3_hit,L2_hit,C0_residency,CPU_Energy,DRAM_Energy,EDP(k),CPU_watt,DRAM_watt,BW\n";
      myfile << "\n";
    myfile.close();
	
	myfile.open("S_kmeans.csv");

      myfile << "mem_size,mem_freq,channels,cpu_freq,exe_time,IPC,L3_hit,L2_hit,C0_residency,CPU_Energy,DRAM_Energy,EDP(k),CPU_watt,DRAM_watt,BW\n";
      myfile << "\n";
    myfile.close();
	
	 myfile.open("M_wc.csv");
	
      myfile << "mem_size,mem_freq,channels,cpu_freq,exe_time,IPC,L3_hit,L2_hit,C0_residency,CPU_Energy,DRAM_Energy,EDP(k),CPU_watt,DRAM_watt,BW\n";
      myfile << "\n";
    myfile.close();
	
	myfile.open("M_srt.csv");

      myfile << "mem_size,mem_freq,channels,cpu_freq,exe_time,IPC,L3_hit,L2_hit,C0_residency,CPU_Energy,DRAM_Energy,EDP(k),CPU_watt,DRAM_watt,BW\n";
      myfile << "\n";
    myfile.close();
	
	myfile.open("M_grep.csv");

      myfile << "mem_size,mem_freq,channels,cpu_freq,exe_time,IPC,L3_hit,L2_hit,C0_residency,CPU_Energy,DRAM_Energy,EDP(k),CPU_watt,DRAM_watt,BW\n";
      myfile << "\n";
    myfile.close();
		
	myfile.open("M_nbayes.csv");

      myfile << "mem_size,mem_freq,channels,cpu_freq,exe_time,IPC,L3_hit,L2_hit,C0_residency,CPU_Energy,DRAM_Energy,EDP(k),CPU_watt,DRAM_watt,BW\n";
      myfile << "\n";
    myfile.close();
	
	myfile.open("M_kmeans.csv");
	
      myfile << "mem_size,mem_freq,channels,cpu_freq,exe_time,IPC,L3_hit,L2_hit,C0_residency,CPU_Energy,DRAM_Energy,EDP(k),CPU_watt,DRAM_watt,BW\n";
      myfile << "\n";
    myfile.close();
	
      return 0;
}
