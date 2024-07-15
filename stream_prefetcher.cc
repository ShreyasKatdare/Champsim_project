#include <algorithm>
#include <set>
#include <map>
#include <optional>
#include <vector>
#include "cache.h"
#include "msl/lru_table.h"

namespace
{   


uint64_t cycs =0;
struct tracker{

    struct monitory_region {
        int direction;
        uint64_t start_address;
        uint64_t end_address;
        uint64_t latest_used_cycle ;

    };
    struct stream{
        uint64_t X;
        uint64_t Y;
        uint64_t Z;
        int dir;
        bool dir_confirm;
    };
    
    std::vector<stream> streams;
    std::vector<monitory_region> table;
    constexpr static int prefetche_degree = 15;
    constexpr static int prefetch_dist = 3;
    constexpr static std::size_t table_size = 64;
    uint64_t last_miss=0;
   


    // DONE
    monitory_region create_new_region()
    {   monitory_region n;
        n.direction = 0;
        n.start_address = 0;
        n.end_address = 0;
        n.latest_used_cycle = 0;
        return n;
    }

    //DONE
    void miss(uint64_t addr, CACHE* cache, uint64_t current_cycle)
    {   for(auto i:table){
            if(addr >= i.start_address && addr <= i.end_address )
            {
                miss_inside_monitory_region(i, cache, current_cycle);
                return;
            }
        }
        miss_outside_monitory_region(addr, cache, current_cycle);
    }

    // block_size is size of a line
    // DONE
    void update_region(monitory_region i, uint64_t degree)
    {   i.start_address += static_cast<uint64_t>(i.direction*static_cast<int64_t>(prefetche_degree)) ;
        i.end_address += static_cast<uint64_t>(i.direction*static_cast<int64_t>(prefetche_degree)) ;
    }

    //DONE
    void prefetch(monitory_region m, CACHE* cache)
  {     
        uint64_t prefetch_addr =  static_cast<uint64_t>(static_cast<int64_t>(m.end_address) + m.direction) ;

        for (int d=0; d<prefetche_degree; d++)
        {
            uint64_t curr_prefetch_addr = static_cast<uint64_t>(static_cast<int64_t>(prefetch_addr) + d*m.direction);
            
            bool success = cache->prefetch_line(curr_prefetch_addr<<LOG2_BLOCK_SIZE, true, 0);
        }       

  }


    // start prefetching and change monitory region 
    void miss_inside_monitory_region(monitory_region i, CACHE* cache, uint64_t current_cycle)
    {   prefetch(i, cache);
        update_region(i,prefetche_degree);
        i.latest_used_cycle = current_cycle;
    }

    // DONE
    monitory_region evict()
    {   uint64_t min = table[0].latest_used_cycle;
        monitory_region e = table[0];
        for(auto abc:table){
            if(abc.latest_used_cycle < min)
            {
                min = abc.latest_used_cycle;
                e= abc;
            }
        }
        return e;
    }

//    void update(monitory_region& m, uint64_t addr)
//    {    if(m.direction==0 && m.start_address!=0)
//         {
//             if(addr > m.start_address) {m.direction=1;}
//             else{m.direction=-1;}
            
//         }

//    }

    // creation of monitory region
    void miss_outside_monitory_region(uint64_t addr, CACHE* cache, uint64_t current_cycle)
    {       
            if(streams.empty()){
                stream s;
                s.X = addr;
                s.Y=0;
                s.Z=0;
                s.dir=0;
                s.dir_confirm=0;
                streams.push_back(s);
            }

            else{  // streams is not empty
                bool f = 0;
                for(long unsigned int k=0;k<streams.size();k++)
                {
                    if(streams[k].Y!=0 && streams[k].Z == 0 && streams[k].X<streams[k].Y && addr > streams[k].Y && addr - streams[k].X < prefetch_dist){
                        if(table.size()<table_size)
                        {   
                            monitory_region new_region =create_new_region();
                            new_region.start_address = streams[k].X;
                            new_region.direction = 1;
                            new_region.end_address = streams[k].X + prefetch_dist;
                            
                            new_region.latest_used_cycle = current_cycle ; 
                            table.push_back(new_region);

                        }
                        else{
                            
                            monitory_region new_region = evict();
                            new_region.start_address = streams[k].X;
                            new_region.direction = 1;
                            new_region.end_address = streams[k].X + prefetch_dist;
                            new_region.latest_used_cycle = current_cycle ; 
                            table.push_back(new_region);
                        }
                        streams.erase(std::next(streams.begin(), k));
                        f=1;
                        break ;
                    }

                    else if (streams[k].Y!=0 && streams[k].Z==0 && streams[k].X>streams[k].Y && addr < streams[k].Y && streams[k].X - addr < prefetch_dist)
                    {   if(table.size()<table_size)
                        {   
                            monitory_region new_region =create_new_region();
                            new_region.start_address = streams[k].X - prefetch_dist;
                            new_region.direction = -1;
                            new_region.end_address = streams[k].X ;
                            
                            new_region.latest_used_cycle = current_cycle ; 
                            table.push_back(new_region);

                        }
                        else{
                            
                            monitory_region new_region = evict();
                            new_region.start_address = streams[k].X - prefetch_dist;
                            new_region.direction = -1;
                            new_region.end_address = streams[k].X ;
                            new_region.latest_used_cycle = current_cycle ; 
                            table.push_back(new_region);
                        }
                        streams.erase(std::next(streams.begin(), k));
                        break ;
                        f=1;
                    }

                }
                if(f==0)
                {   bool f1=0;
                    for(long unsigned int k=0;k<streams.size();k++)
                    {   if(streams[k].Y == 0 && streams[k].Z == 0 )
                        {   if(addr > streams[k].X && addr - streams[k].X < prefetch_dist)
                            {
                                streams[k].Y=addr;
                                streams[k].dir = 1;
                                f1=1;
                            }
                            else if(addr< streams[k].X && streams[k].X - addr < prefetch_dist)
                            {   f1=1;
                                streams[k].dir=-1;
                                streams[k].Y = addr;
                            }
                            
                        }
                        if(f1==1){break;}
                    }

                    if(f1==0)
                    {   stream s;
                        s.X = addr;
                        s.Y=0;
                        s.Z=0;
                        s.dir=0;
                        s.dir_confirm=0;
                        streams.push_back(s);

                    }
                }


                

            }
        
    }

};


    std::map<CACHE*,tracker> trackers;

} // namespace


void CACHE::prefetcher_initialize() {}

// This function is called each current_cycle, after all other operation has completed.
void CACHE::prefetcher_cycle_operate() {
    cycs++;
 }

// This function must be implemented by instruction prefetchers.
uint32_t CACHE::prefetcher_cache_operate(uint64_t addr, uint64_t ip, uint8_t cache_hit, bool useful_prefetch, uint8_t type, uint32_t metadata_in)
{   if(!cache_hit){
    
        ::trackers[this].miss(addr >> LOG2_BLOCK_SIZE, this, cycs);
    }
  return metadata_in;
}

// This function is called when a miss is filled in the cache.
uint32_t CACHE::prefetcher_cache_fill(uint64_t addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_addr, uint32_t metadata_in)
{
  return metadata_in;
}

void CACHE::prefetcher_final_stats() {}
   
