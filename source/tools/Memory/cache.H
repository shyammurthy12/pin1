/*BEGIN_LEGAL 
Intel Open Source License 

Copyright (c) 2002-2017 Intel Corporation. All rights reserved.
 
Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.  Redistributions
in binary form must reproduce the above copyright notice, this list of
conditions and the following disclaimer in the documentation and/or
other materials provided with the distribution.  Neither the name of
the Intel Corporation nor the names of its contributors may be used to
endorse or promote products derived from this software without
specific prior written permission.
 
THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE INTEL OR
ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
END_LEGAL */
/*! @file
 *  This file contains a configurable cache class
 */

#ifndef PIN_CACHE_H
#define PIN_CACHE_H

#define SET_OF_INTEREST 35
#define KILO 1024
#define MEGA (KILO*KILO)
#define GIGA (KILO*MEGA)

#define EXTRA_WAYS 3

#define EXTRA_WAYS_1 3
//#define LRU_MINUS_ONE_PLACEMENT
//#define LRU_MINUS_TWO_PLACEMENT
#define LRU_MINUS_THREE_PLACEMENT
//#define LRU_PLACEMENT
//#define LRU_MINUS_FOUR_PLACEMENT
#define HIGH_DEGREE_OF_USE 2.5
#define MEDIUM_DEGREE_OF_USE_1 1.5
#define SATURATION_COUNTER_THRESHOLD 4
#define UPPER_BOUND_BURST_COUNT 4

typedef UINT64 CACHE_STATS; // type of cache hit/miss counters


#include <iostream>
#include <set>
#include <sstream>
#include <cstdlib>
#include <string> 
#include <vector>
#include <set>
#include <bitset>

using std::string;
using std::ostringstream;
using namespace std;

uint64_t total_accesses = 0;
//#define VICTIM_CACHE_ADDITION 0
//#define ALLOCATE_INTO_VICTIM_BUFFER 1
bool warmup_finished = false;
struct victim_buffer{
 uint64_t addr;
 uint64_t tag;
 uint64_t current_function;
 bool valid;
 uint64_t timestamp;
};
#define NUM_VICTIM_ENTRIES 240


victim_buffer low_use_victim_entries[NUM_VICTIM_ENTRIES];
bool victim_buffer_entries_initialized = false;


map<uint64_t, set<uint64_t>> low_use_block_address_and_burst_counts;

map<uint64_t, set<uint64_t>> mapping_from_block_to_functions_used_block;

struct burst_and_confidence{
   uint64_t burst_count;
   //initially this confidence level is at 0.
   //when the confidence level is at 0, then set the new burst count. 
   //if confidence level is over 4, then use the new burst count. 
   uint64_t confidence;
};


uint64_t previous_accessed_function = 0;

map<uint64_t, uint32_t> mapping_from_cache_block_to_burst_count;
map<uint64_t, set<uint64_t>> mapping_from_cache_block_to_corresponding_function;
//we are only interested in the smaller values of burst counts
//
//learn the burst count for cache blocks. 
//also assign a confidence to the learnt burst count.
map<uint64_t, burst_and_confidence> burst_count_learning;
vector<uint64_t> accesses_to_set_56;
vector<bool> hit_miss_accesses_to_set_56;
vector<bool> degree_of_use_block;
vector<float> block_degree_of_use_block;
map<uint64_t, map<uint64_t,uint64_t>> mapping_from_block_to_blocks_evicted_to_make_room;
map<uint64_t, map<uint64_t,uint64_t>> mapping_from_function_to_functions_evicted_to_make_room; 
//count the misses the different blocks experience
//in the cache block of interest
map<uint64_t, uint64_t> mapping_from_cache_block_to_miss_counts;
map<uint64_t, uint64_t> mapping_from_cache_block_to_access_nmru_plus_miss_counts;
map<uint64_t, uint64_t> mapping_from_cache_block_to_total_access_counts;
//count the misses the different functions experience 
//in the set of interest. 
map<uint64_t, uint64_t> mapping_from_function_to_miss_counts;
map<uint64_t, uint64_t>  mapping_from_function_to_access_nmru_plus_miss_counts;
map<uint64_t, uint64_t>  mapping_from_function_to_total_access_counts;
//data structure to note the number of function invocations made while on a cache 
//set. 
map<uint64_t, uint64_t> mapping_from_function_to_current_invocation_count; 
map<uint64_t, uint64_t> mapping_from_function_to_total_invocation_count; 
//note the degree of use of the cache block
map<uint64_t, bool> mapping_from_cache_block_to_degree_of_use;
map<uint64_t, set<uint64_t>> mapping_from_function_to_other_functions_accessed_till_next_invocation;
//used to compute the average function reuse distance. 
struct function_invocation_details{
	uint64_t intermediate_function_invocations;
	uint64_t total_function_invocations;
};

map<uint64_t, function_invocation_details> mapping_from_function_to_invocation_statistics;
map<uint64_t, vector<float>> mapping_from_function_to_reuse_distances_over_1000_function_invocations;

struct hits_and_misses{
	uint64_t hits;
	uint64_t misses;
};

//store the mapping from cache block to the number of misses 
//and nmru hits it experiences.
map<uint64_t, hits_and_misses> mapping_from_cache_block_to_misses_and_nmru_hits_ratio;
//this bit is set or unset after every 10 misses experienced by this cache block. 
//it is set to high use when we start or false. It gets set after learning
//that happens over 10 misses.
map<uint64_t, bool> mapping_from_cache_block_to_mostly_miss;
vector<bool> hot_sets_in_cache;

uint32_t  array_of_misses_per_set[1024] = {0};
uint32_t  array_of_nmru_plus_miss_references_per_set[1024] = {0};
uint32_t  array_of_references_per_set[1024] = {0};

uint32_t  counter_of_misses_per_set[1024] = {0};
uint32_t  counter_of_references_per_set[1024] = {0};
uint32_t counter_of_burst1_blocks_replaced_per_set[1024] = {0};
bool direct_mapped_setting_per_set[1024] = {0};

//so that we know the number of cache sets
//the cache has, so we can iterate the misses per set
//data structure
uint32_t number_of_cache_sets;

//bit is set if the cache set 
//is gauged to be hot.
bitset<256> hotness_of_sets;
//need to clear the bitset in the beginning. 
bool bitset_cleared = false;

uint64_t total_misses_on_low_use_function = 0;
uint64_t total_hits_on_non_mru_position = 0;

//track block references only when we enter this cache block from another block
//while we are on the same cache block, hits are less interesting.
uint64_t current_cache_block_in_use = 0;

double rand_value()
{
    return (double)rand() / RAND_MAX;
}



/*! RMR (rodric@gmail.com) 
 *   - temporary work around because decstr()
 *     casts 64 bit ints to 32 bit ones
 */
static string mydecstr(UINT64 v, UINT32 w)
{
    ostringstream o;
    o.width(w);
    o << v;
    string str(o.str());
    return str;
}

/*!
 *  @brief Checks if n is a power of 2.
 *  @returns true if n is power of 2
 */
static inline bool IsPower2(UINT32 n)
{
    return ((n & (n - 1)) == 0);
}

struct use_and_blk_addr{
	bool function_use_information;
	uint64_t blk_addr;
	uint32_t allocated_way;
	uint32_t total_low_use_misses;
	uint32_t burst_count_of_replaced_block;
	uint64_t replaced_block_function;
};

struct hit_and_use_information{
	bool icache_hit;
	bool function_use_information;
	//line addresses
	vector<uint64_t> blk_addresses;
	uint32_t allocated_way;
	uint32_t total_low_use_misses;
	uint32_t burst_count_of_missed_block;
	bool icache_nmru_hit;
	//block that was fetched as a result of spatial fetch and hit. 
	bool spatial_fetch_hit;
};

/*!
 *  @brief Computes floor(log2(n))
 *  Works by finding position of MSB set.
 *  @returns -1 if n == 0.
 */
static inline INT32 FloorLog2(UINT32 n)
{
    INT32 p = 0;

    if (n == 0) return -1;

    if (n & 0xffff0000) { p += 16; n >>= 16; }
    if (n & 0x0000ff00)	{ p +=  8; n >>=  8; }
    if (n & 0x000000f0) { p +=  4; n >>=  4; }
    if (n & 0x0000000c) { p +=  2; n >>=  2; }
    if (n & 0x00000002) { p +=  1; }

    return p;
}

/*!
 *  @brief Computes floor(log2(n))
 *  Works by finding position of MSB set.
 *  @returns -1 if n == 0.
 */
static inline INT32 CeilLog2(UINT32 n)
{
    return FloorLog2(n - 1) + 1;
}

/*!
 *  @brief Cache tag - self clearing on creation
 */
class CACHE_TAG
{
  private:
    ADDRINT _tag;

  public:
    CACHE_TAG(ADDRINT tag = 0) { _tag = tag; }
    bool operator==(const CACHE_TAG &right) const { return _tag == right._tag; }
    operator ADDRINT() const { return _tag; }
};


/*!
 * Everything related to cache sets
 */
namespace CACHE_SET
{

/*!
 *  @brief Cache set direct mapped
 */
class DIRECT_MAPPED
{
  private:
    CACHE_TAG _tag;

  public:
    DIRECT_MAPPED(UINT32 associativity = 1) { ASSERTX(associativity == 1); }

    VOID SetAssociativity(UINT32 associativity) { ASSERTX(associativity == 1); }
    UINT32 GetAssociativity(UINT32 associativity) { return 1; }

    UINT32 Find(CACHE_TAG tag) { return(_tag == tag); }
    UINT32 Find(CACHE_TAG tag, bool degree_of_use) { return(_tag == tag); }
    VOID Replace(CACHE_TAG tag) { _tag = tag; }
    use_and_blk_addr Replace_GetDegreeOfUse(CACHE_TAG tag, bool degree_of_use, uint64_t blk_addr) {
	    use_and_blk_addr temp;
	    temp.function_use_information = false;
	    temp.blk_addr = 0;
	    return temp;
    }
};


template <UINT32 MAX_ASSOCIATIVITY = 4>
class BURST_BASED_CACHE
{
  private:
    CACHE_TAG _tags[MAX_ASSOCIATIVITY];
    //add the last access number as the proxy for last reference time
    //to enable LRU based replacement
    uint64_t _tag_last_reference_time[MAX_ASSOCIATIVITY];
   
    //invalidate all cache blocks to start with. 
    bool _validity[MAX_ASSOCIATIVITY];

    //track degree of use for each block (whether it comes from a function with high use or low use)
    //default set to low use.false indicates low use, true indicates high use.  
    bool _degree_of_use[MAX_ASSOCIATIVITY];
   
    //note the MRU position in the cache. This is updated on a cache miss where we insert the block 
    //at the MRU position or on a cache hit, were we subsequently insert the block into the MRU position. 
    int32_t _mru_position;

    //store the address of the item also alongside, so we can retrieve it on a replacement. 
    uint64_t _addr[MAX_ASSOCIATIVITY];
    
    //store the function that had this cache block be inserted
    //in the cache. 
    uint64_t _function[MAX_ASSOCIATIVITY];

    //store bursts for different cache blocks present in the cache set. 
    uint64_t _bursts[MAX_ASSOCIATIVITY];
    //burst counts are valid only for low use functions 
    //cache blocks.
    uint64_t _valid_burst[MAX_ASSOCIATIVITY];


    uint64_t _burst_count_for_cache_blocks_when_live[MAX_ASSOCIATIVITY];
    
    UINT32 _tagsLastIndex;
    UINT32 _nextReplaceIndex;

  public:
    BURST_BASED_CACHE(UINT32 associativity = MAX_ASSOCIATIVITY)
      : _tagsLastIndex(associativity - 1)
    {
        ASSERTX(associativity <= MAX_ASSOCIATIVITY);
        _nextReplaceIndex = _tagsLastIndex;

        for (INT32 index = _tagsLastIndex; index >= 0; index--)
        {
         _tags[index] = CACHE_TAG(0);
         _tag_last_reference_time[index] = 0;
	 _degree_of_use[index] = false;
	 _addr[index] = 0;
	 //set the burst counts to some arbitrary value
	 //and set all burst counts to be invalid
	 _bursts[index] = 0;
	 _valid_burst[index] = false;
	 
	 _function[index] = 0;

	 //datastructure to track the burst count of cache blocks when
	 //they are live in the cache. 
	 _burst_count_for_cache_blocks_when_live[index] = 0;
	 
	 //invalidate cache block
	 _validity[index] = false;
	}

	_mru_position = 0;
    }

    VOID SetAssociativity(UINT32 associativity)
    {
        ASSERTX(associativity <= MAX_ASSOCIATIVITY);
        _tagsLastIndex = associativity - 1;
        _nextReplaceIndex = _tagsLastIndex;
    }
    UINT32 GetAssociativity(UINT32 associativity) { return _tagsLastIndex + 1; }
    
    UINT32 Find(CACHE_TAG tag)
    {
        bool result = true;
	total_accesses++;
        
	for (INT32 index = _tagsLastIndex; index >= 0; index--)
        {
            // this is an ugly micro-optimization, but it does cause a
            // tighter assembly loop for ARM that way ...

            if(_tags[index] == tag) {
		    _tag_last_reference_time[index] = total_accesses;
		    _mru_position = index;
		    goto end;
	    }
//		if(_tags[index] == tag) goto end;
        }
        result = false;

        end: return result;
    }

    //functions may start with a low degree of use and then progress to have a
    // high degree of use. 
    UINT32 Find_UpdateDegreeOfUse(ADDRINT addr, CACHE_TAG tag, bool degree_of_use, bool medium_degree_of_use,
		    bool &non_mru_hit, uint64_t future_reference_timestamp,
		    bool victim_search, uint32_t setIndex, bool &is_spatial_fetched_block, 
		    uint64_t current_function)
    {
        bool result = true;
	total_accesses++;
        
	for (INT32 index = _tagsLastIndex; index >= 0; index--)
        {
            // this is an ugly micro-optimization, but it does cause a
            // tighter assembly loop for ARM that way ...
            if((_tags[index] == tag)&&
			   (_validity[index])) {
		    //this is when the block at the MRU position
		    // would move out of the MRU position
		    if (index != _mru_position){
			    non_mru_hit = true;
		           //if the burst count is valid at the current MRU position.
		           //decrement it if it is not already 0. 
		           if (_valid_burst[_mru_position]){
		               if (_bursts[_mru_position] != 0)
		               	_bursts[_mru_position]--;
			       //if the burst count falls to zero, then 
			       //set the block at LRU position
			       if (_bursts[_mru_position] == 0)
				 _tag_last_reference_time[_mru_position] = 0;
		           }

			   _burst_count_for_cache_blocks_when_live[_mru_position]++;
		     }
		    _tag_last_reference_time[index] = total_accesses;
		    //update the mru index
		    _mru_position = index;
		    //update the degree of use. 
		    _degree_of_use[index] = degree_of_use;
		    goto end;
	    }
//		if(_tags[index] == tag) goto end;
        }
        result = false;
        end: return result;
    }
    VOID Replace(CACHE_TAG tag)
    {
        // g++ -O3 too dumb to do CSE on following lines?!
      //  const UINT32 index = _nextReplaceIndex;

      //  _tags[index] = tag;
      //  // condition typically faster than modulo
      //  _nextReplaceIndex = (index == 0 ? _tagsLastIndex : index - 1);
    
        uint64_t min_access_time = _tag_last_reference_time[_tagsLastIndex];
        uint64_t _nextReplaceIndex = _tagsLastIndex;
        for (INT32 index = _tagsLastIndex; index >= 0; index--)
        {
          if(min_access_time>_tag_last_reference_time[index]){
               _nextReplaceIndex = index;
               min_access_time = _tag_last_reference_time[index];
            }
        }
	const UINT32 index = _nextReplaceIndex;
	
        _tags[index] = tag;
	_tag_last_reference_time[index] = total_accesses;
	//update the mru index
	_mru_position = index;
    }

    use_and_blk_addr Replace_GetDegreeOfUse(CACHE_TAG tag, bool degree_of_use,uint64_t blk_addr, bool medium_degree_of_use, 
		    uint64_t future_reference_timestamp, float block_degree_of_use, bool direct_mapped,
		    bool straggler_remove, uint64_t current_function, bool victim_search, uint32_t setIndex, bool spatial_fetch)
    {
        // g++ -O3 too dumb to do CSE on following lines?!
      //  const UINT32 index = _nextReplaceIndex;

      //  _tags[index] = tag;
      //  // condition typically faster than modulo
      //  _nextReplaceIndex = (index == 0 ? _tagsLastIndex : index - 1);
	if (_valid_burst[_mru_position]){
	    if (_bursts[_mru_position] != 0)
	    	_bursts[_mru_position]--;
	    //if burst count falls to zero, then set the block as LRU
	    //if the burst count falls to zero, then 
	    //set the block at LRU position
	    if (_bursts[_mru_position] == 0)
	        _tag_last_reference_time[_mru_position] = 0;
	}

	_burst_count_for_cache_blocks_when_live[_mru_position]++;
        uint64_t min_access_time = _tag_last_reference_time[_tagsLastIndex];
        uint64_t _nextReplaceIndex = _tagsLastIndex;
        //way 0 is for low use functions and all other ways are for high use functions. 
	//see how this fares.
	//decrement the burst count for this block which is at the MRU position. 
            for (INT32 index = _tagsLastIndex; index >= 0; index--)
            {
           	if(min_access_time>_tag_last_reference_time[index]){
                     _nextReplaceIndex = index;
                     min_access_time = _tag_last_reference_time[index];
                 }
           	 //if block is invalid, pick this way for insertion
           	 if (!_validity[index]){
           	     _nextReplaceIndex = index;
           	     break;
           	  }
             }
	
	const UINT32 index = _nextReplaceIndex;
	bool replaced_block_degree_of_use = _degree_of_use[index];
        uint64_t replaced_block_address = _addr[index];
	use_and_blk_addr temp;
	temp.function_use_information = replaced_block_degree_of_use;
	temp.blk_addr = replaced_block_address;
	temp.burst_count_of_replaced_block = _burst_count_for_cache_blocks_when_live[index];
	temp.replaced_block_function = _function[index];
	_burst_count_for_cache_blocks_when_live[index] = 0;
	//set the burst validity and burst count
	//upon insertion 
	if ((block_degree_of_use >= HIGH_DEGREE_OF_USE)&&
			(!degree_of_use)){
	   //set the burst count to two
	   //burst validity to true
	   _valid_burst[index] = true;
	   _bursts[index] = 3;

	}
	else if ((block_degree_of_use >= MEDIUM_DEGREE_OF_USE_1)&&
		       (!degree_of_use)){
	   //set the burst count to two
	   //burst validity to true
	   _valid_burst[index] = true;
	   _bursts[index] = 2;
	}
	else if (!degree_of_use){
	   //set the burst count to one
	   //burst validity to true
	   _valid_burst[index] = true;
	   _bursts[index] = 1;
	}
	else{
	  //burst validity to zero
	   _valid_burst[index] = false;
	}
	_tags[index] = tag;
	_tag_last_reference_time[index] = total_accesses;
	//update the mru index
	_mru_position = index;
	//set this block to be valid
	_validity[index] = true;
	_degree_of_use[index] = degree_of_use;
	_addr[index] = blk_addr;
	_function[index] = current_function;
	return temp;
    }
};

//burst based cache design based
//on confidence learning.
template <UINT32 MAX_ASSOCIATIVITY = 4>
class BURST_BASED_CACHE_CONFIDENCE
{
  private:
    CACHE_TAG _tags[MAX_ASSOCIATIVITY];
    //add the last access number as the proxy for last reference time
    //to enable LRU based replacement
    uint64_t _tag_last_reference_time[MAX_ASSOCIATIVITY];
    
    //track degree of use for each block (whether it comes from a function with high use or low use)
    //default set to low use.false indicates low use, true indicates high use.  
    bool _degree_of_use[MAX_ASSOCIATIVITY];
   
    //note the MRU position in the cache. This is updated on a cache miss where we insert the block 
    //at the MRU position or on a cache hit, were we subsequently insert the block into the MRU position. 
    int32_t _mru_position;

    //store the address of the item also alongside, so we can retrieve it on a replacement. 
    uint64_t _addr[MAX_ASSOCIATIVITY];
    
    //store bursts for different cache blocks present in the cache set. 
    uint64_t _bursts[MAX_ASSOCIATIVITY];
    //burst counts are valid only for low use functions 
    //cache blocks.
    uint64_t _valid_burst[MAX_ASSOCIATIVITY];
    
    uint64_t _burst_count_for_cache_blocks_when_live[MAX_ASSOCIATIVITY];

    UINT32 _tagsLastIndex;
    UINT32 _nextReplaceIndex;

  public:
    BURST_BASED_CACHE_CONFIDENCE(UINT32 associativity = MAX_ASSOCIATIVITY)
      : _tagsLastIndex(associativity - 1)
    {
        ASSERTX(associativity <= MAX_ASSOCIATIVITY);
        _nextReplaceIndex = _tagsLastIndex;

        for (INT32 index = _tagsLastIndex; index >= 0; index--)
        {
         _tags[index] = CACHE_TAG(0);
         _tag_last_reference_time[index] = 0;
	 _degree_of_use[index] = false;
	 _addr[index] = 0;
	 //set the burst counts to some arbitrary value
	 //and set all burst counts to be invalid
	 _bursts[index] = 0;
	 //datastructure to track the burst count of cache blocks when
	 //they are live in the cache. 
	 _burst_count_for_cache_blocks_when_live[index] = 0;
	 _valid_burst[index] = false;
	}

	_mru_position = 0;
    }

    VOID SetAssociativity(UINT32 associativity)
    {
        ASSERTX(associativity <= MAX_ASSOCIATIVITY);
        _tagsLastIndex = associativity - 1;
        _nextReplaceIndex = _tagsLastIndex;
    }
    UINT32 GetAssociativity(UINT32 associativity) { return _tagsLastIndex + 1; }
    
    UINT32 Find(CACHE_TAG tag)
    {
        bool result = true;
	total_accesses++;
        
	for (INT32 index = _tagsLastIndex; index >= 0; index--)
        {
            // this is an ugly micro-optimization, but it does cause a
            // tighter assembly loop for ARM that way ...

            if(_tags[index] == tag) {
		    _tag_last_reference_time[index] = total_accesses;
		    _mru_position = index;
		    goto end;
	    }
//		if(_tags[index] == tag) goto end;
        }
        result = false;

        end: return result;
    }

    //functions may start with a low degree of use and then progress to have a
    // high degree of use. 
    UINT32 Find_UpdateDegreeOfUse(ADDRINT addr, CACHE_TAG tag, bool degree_of_use, bool medium_degree_of_use,
		    bool &non_mru_hit, uint64_t future_reference_timestamp,
		    bool victim_search, uint32_t setIndex, bool &is_spatial_fetched_block,
		    uint64_t current_function)
    {
        bool result = true;
	total_accesses++;
        
	for (INT32 index = _tagsLastIndex; index >= 0; index--)
        {
            // this is an ugly micro-optimization, but it does cause a
            // tighter assembly loop for ARM that way ...
            if(_tags[index] == tag) {
		    //this is when the block at the MRU position
		    // would move out of the MRU position
		    if (index != _mru_position){
			    non_mru_hit = true;
		           //if the burst count is valid at the current MRU position.
		           //decrement it if it is not already 0. 
		           if (_valid_burst[_mru_position]){
		               if (_bursts[_mru_position] != 0)
		               	_bursts[_mru_position]--;
			       //if the burst count falls to zero, then 
			       //set the block at LRU position
			       if (_bursts[_mru_position] == 0)
				 _tag_last_reference_time[_mru_position] = 0;
		           }
			   //upon movement out of the MRU position, update the 
			   //burst count for this index.
			   _burst_count_for_cache_blocks_when_live[_mru_position]++;
		     }
		    _tag_last_reference_time[index] = total_accesses;
		    //update the mru index
		    _mru_position = index;
		    //update the degree of use. 
		    _degree_of_use[index] = degree_of_use;
		    goto end;
	    }
//		if(_tags[index] == tag) goto end;
        }
        result = false;
        end: return result;
    }
    VOID Replace(CACHE_TAG tag)
    {
        // g++ -O3 too dumb to do CSE on following lines?!
      //  const UINT32 index = _nextReplaceIndex;

      //  _tags[index] = tag;
      //  // condition typically faster than modulo
      //  _nextReplaceIndex = (index == 0 ? _tagsLastIndex : index - 1);
    
        uint64_t min_access_time = _tag_last_reference_time[_tagsLastIndex];
        uint64_t _nextReplaceIndex = _tagsLastIndex;
        for (INT32 index = _tagsLastIndex; index >= 0; index--)
        {
          if(min_access_time>_tag_last_reference_time[index]){
               _nextReplaceIndex = index;
               min_access_time = _tag_last_reference_time[index];
            }
        }
	const UINT32 index = _nextReplaceIndex;
	
        _tags[index] = tag;
	_tag_last_reference_time[index] = total_accesses;
	//update the mru index
	_mru_position = index;
    }

    use_and_blk_addr Replace_GetDegreeOfUse(CACHE_TAG tag, bool degree_of_use,uint64_t blk_addr, bool medium_degree_of_use, 
		    uint64_t future_reference_timestamp, float block_degree_of_use, bool direct_mapped, bool straggler_remove,
		    uint64_t current_function, bool victim_search, uint32_t setIndex, bool spatial_fetch)
    {
        //initialize an entry in the table for this cache block
	//if one does not exist already.
	if (burst_count_learning.find(blk_addr) == burst_count_learning.end()){
	    burst_count_learning[blk_addr].burst_count = 0;
	    burst_count_learning[blk_addr].confidence = 0;
	}	
	
	//update the burst count for this cache block in the MRU position
	//which is now moving out of the MRU positon.
         _burst_count_for_cache_blocks_when_live[_mru_position]++;
        
	//only certain blocks are assigned a valid burst count based on 
	//confidence study.
	if (_valid_burst[_mru_position]){
	    if (_bursts[_mru_position] != 0)
	    	_bursts[_mru_position]--;
	    //if burst count falls to zero, then set the block as LRU
	    //if the burst count falls to zero, then 
	    //set the block at LRU position
	    if (_bursts[_mru_position] == 0)
	        _tag_last_reference_time[_mru_position] = 0;
	}

	uint64_t min_access_time = _tag_last_reference_time[_tagsLastIndex];
        uint64_t _nextReplaceIndex = _tagsLastIndex;
        //way 0 is for low use functions and all other ways are for high use functions. 
	//see how this fares.
	//decrement the burst count for this block which is at the MRU position. 

	for (INT32 index = _tagsLastIndex; index >= 0; index--)
        {
              if(min_access_time>_tag_last_reference_time[index]){
                 _nextReplaceIndex = index;
                 min_access_time = _tag_last_reference_time[index];
              }
        }
	const UINT32 index = _nextReplaceIndex;
	bool replaced_block_degree_of_use = _degree_of_use[index];
        uint64_t replaced_block_address = _addr[index];
	uint64_t current_burst_count_for_blk_getting_replaced = 
		_burst_count_for_cache_blocks_when_live[index];
	//update the confidence associated with this burst count.

	if (burst_count_learning[replaced_block_address].burst_count == 
			current_burst_count_for_blk_getting_replaced){
		//saturate the counter at 8.
		if (burst_count_learning[replaced_block_address].confidence < 
				SATURATION_COUNTER_THRESHOLD){
		    burst_count_learning[replaced_block_address].confidence++;
		}
	}
	//only if the confidence is zero do we update the burst count 
	//with this new counter.Else, just decrement the confidence 
	//associated with the previous burst count 
	else if (burst_count_learning[replaced_block_address].confidence != 0){
		if (burst_count_learning[replaced_block_address].confidence>0)
			burst_count_learning[replaced_block_address].confidence--;
	}
	//confidence was zero, hence update the burst count.
	else {
		burst_count_learning[replaced_block_address].confidence = 1;
		burst_count_learning[replaced_block_address].burst_count = 
			current_burst_count_for_blk_getting_replaced;
	}
	use_and_blk_addr temp;
	temp.function_use_information = replaced_block_degree_of_use;
	temp.blk_addr = replaced_block_address;
	//set the burst validity and burst count
	//upon insertion 
        	
	//only if confidence in the burst count exceeds this 
	//threshold
	if (burst_count_learning[blk_addr].confidence >=(SATURATION_COUNTER_THRESHOLD/2)){
	//validate the burst count only if the burst count 
	//is less than 3. 
	   if (burst_count_learning[blk_addr].burst_count < 
			   UPPER_BOUND_BURST_COUNT){
		_valid_burst[index] = true;
		_bursts[index] = burst_count_learning[blk_addr].burst_count;
	   }
	   else{
	  	//burst validity to zero
	   	_valid_burst[index] = false;
	   }
	}
	//if the confidence is low, then also burst count is not validated
	else{
	    //burst validity to zero
	     _valid_burst[index] = false;
	}

	_tags[index] = tag;
	//update the burst count for this block to be zero upon insertion. 
	_burst_count_for_cache_blocks_when_live[index] = 0;
	_tag_last_reference_time[index] = total_accesses;
	//update the mru index
	_mru_position = index;
	_degree_of_use[index] = degree_of_use;
	_addr[index] = blk_addr;
	return temp;
    }
};

template <UINT32 MAX_ASSOCIATIVITY = 256>
class ROUND_ROBIN_PLUS_DIRECT_MAPPED
{
  private:
    CACHE_TAG _tags[MAX_ASSOCIATIVITY];
    //add the last access number as the proxy for last reference time
    //to enable LRU based replacement
    uint64_t _tag_last_reference_time[MAX_ASSOCIATIVITY];
    
    //track degree of use for each block (whether it comes from a function with high use or low use)
    //default set to low use.false indicates low use, true indicates high use.  
    bool _degree_of_use[MAX_ASSOCIATIVITY];
   
    //store bursts for different cache blocks present in the cache set. 
    uint64_t _bursts[MAX_ASSOCIATIVITY];
    //burst counts are valid only for low use functions 
    //cache blocks.
    //note the MRU position in the cache. This is updated on a cache miss where we insert the block 
    //at the MRU position or on a cache hit, were we subsequently insert the block into the MRU position. 
    int32_t _mru_position;

    //store the address of the item also alongside, so we can retrieve it on a replacement. 
    uint64_t _addr[MAX_ASSOCIATIVITY];
    
    UINT32 _tagsLastIndex;
    UINT32 _nextReplaceIndex;

  public:
    ROUND_ROBIN_PLUS_DIRECT_MAPPED(UINT32 associativity = MAX_ASSOCIATIVITY)
      : _tagsLastIndex(associativity - 1)
    {
        ASSERTX(associativity <= MAX_ASSOCIATIVITY);
        _nextReplaceIndex = _tagsLastIndex;

        for (INT32 index = _tagsLastIndex; index >= 0; index--)
        {
         _tags[index] = CACHE_TAG(0);
         _tag_last_reference_time[index] = 0;
	 _degree_of_use[index] = false;
	 //set the burst counts to some arbitrary value
	 //and set all burst counts to be invalid
	 _bursts[index] = 0;
	 _addr[index] = 0;
	}
	_mru_position = 0;
    }

    VOID SetAssociativity(UINT32 associativity)
    {
        ASSERTX(associativity <= MAX_ASSOCIATIVITY);
        _tagsLastIndex = associativity - 1;
        _nextReplaceIndex = _tagsLastIndex;
    }
    UINT32 GetAssociativity(UINT32 associativity) { return _tagsLastIndex + 1; }
    
    UINT32 Find(CACHE_TAG tag)
    {
        bool result = true;
	total_accesses++;
        
	for (INT32 index = _tagsLastIndex; index >= 0; index--)
        {
            // this is an ugly micro-optimization, but it does cause a
            // tighter assembly loop for ARM that way ...

            if(_tags[index] == tag) {
		    _tag_last_reference_time[index] = total_accesses;
		    _mru_position = index;
		    goto end;
	    }
//		if(_tags[index] == tag) goto end;
        }
        result = false;

        end: return result;
    }

    //functions may start with a low degree of use and then progress to have a
    // high degree of use. 
    UINT32 Find_UpdateDegreeOfUse(ADDRINT addr, CACHE_TAG tag, bool degree_of_use, bool medium_degree_of_use,
		    bool &non_mru_hit, uint64_t future_reference_timestamp,
		    bool victim_search, uint32_t setIndex, bool &is_spatial_fetched_block,
		    uint64_t current_function)
    {
        bool result = true;
	total_accesses++;
        
	for (INT32 index = _tagsLastIndex; index >= 0; index--)
        {
            // this is an ugly micro-optimization, but it does cause a
            // tighter assembly loop for ARM that way ...
            if(_tags[index] == tag) {
		    if (index != _mru_position){
			    non_mru_hit = true;
		    	    _bursts[index]++;
		    }
		    _tag_last_reference_time[index] = total_accesses;
		    //update the mru index
		    _mru_position = index;
		    //update the degree of use. 
		    _degree_of_use[index] = degree_of_use;
		    goto end;
	    }
//		if(_tags[index] == tag) goto end;
        }
        result = false;
        end: return result;
    }
    VOID Replace(CACHE_TAG tag)
    {
        // g++ -O3 too dumb to do CSE on following lines?!
      //  const UINT32 index = _nextReplaceIndex;

      //  _tags[index] = tag;
      //  // condition typically faster than modulo
      //  _nextReplaceIndex = (index == 0 ? _tagsLastIndex : index - 1);
    
        uint64_t min_access_time = _tag_last_reference_time[_tagsLastIndex];
        uint64_t _nextReplaceIndex = _tagsLastIndex;
        for (INT32 index = _tagsLastIndex; index >= 0; index--)
        {
          if(min_access_time>_tag_last_reference_time[index]){
               _nextReplaceIndex = index;
               min_access_time = _tag_last_reference_time[index];
            }
        }
	const UINT32 index = _nextReplaceIndex;
	
        _tags[index] = tag;
	_tag_last_reference_time[index] = total_accesses;
	//update the mru index
	_mru_position = index;
    }

    use_and_blk_addr Replace_GetDegreeOfUse(CACHE_TAG tag, bool degree_of_use,uint64_t blk_addr, bool medium_degree_of_use, 
		    uint64_t future_reference_timestamp, float block_degree_of_use, bool direct_mapped, bool straggler_remove,
		    uint64_t current_function, bool victim_search, uint32_t setIndex, bool spatial_fetch)
    {
        // g++ -O3 too dumb to do CSE on following lines?!
      //  const UINT32 index = _nextReplaceIndex;

      //  _tags[index] = tag;
      //  // condition typically faster than modulo
      //  _nextReplaceIndex = (index == 0 ? _tagsLastIndex : index - 1);
    
        uint64_t min_access_time = _tag_last_reference_time[_tagsLastIndex];
        uint64_t _nextReplaceIndex = _tagsLastIndex;
        //way 0 is for low use functions and all other ways are for high use functions. 
	//see how this fares.
	 if (!direct_mapped){
	   for (INT32 index = _tagsLastIndex; index >= 0; index--)
           {
               if(min_access_time>_tag_last_reference_time[index]){
                   _nextReplaceIndex = index;
                   min_access_time = _tag_last_reference_time[index];
                }
           }
	 }
	 else {
	   //compute the way index because we want to fix the placement 
	   //of the block to one way when we gauge that direct mapped
	   //works better for the block.
	   _nextReplaceIndex = tag&7;
	//   //pick between one of two ways
	//   if (_tag_last_reference_time[_nextReplaceIndex]> 
	//		   _tag_last_reference_time[_nextReplaceIndex+4])
	//	   _nextReplaceIndex = _nextReplaceIndex+4;
	 }
	const UINT32 index = _nextReplaceIndex;
	bool replaced_block_degree_of_use = _degree_of_use[index];
        uint64_t replaced_block_address = _addr[index];
	use_and_blk_addr temp;
	temp.function_use_information = replaced_block_degree_of_use;
	temp.blk_addr = replaced_block_address;
	temp.burst_count_of_replaced_block = _bursts[index];
	//burst begins when placed in the MRU position. 
	_bursts[index] = 1;
	_tags[index] = tag;
	_tag_last_reference_time[index] = total_accesses;
	//update the mru index
	_mru_position = index;
	_degree_of_use[index] = degree_of_use;
	_addr[index] = blk_addr;
	return temp;
    }
};


/*!
 *  @brief Cache set with round robin replacement
 */
template <UINT32 MAX_ASSOCIATIVITY = 256>
class ROUND_ROBIN
{
  private:
    CACHE_TAG _tags[MAX_ASSOCIATIVITY];
    //add the last access number as the proxy for last reference time
    //to enable LRU based replacement
    uint64_t _tag_last_reference_time[MAX_ASSOCIATIVITY];
    
    //track degree of use for each block (whether it comes from a function with high use or low use)
    //default set to low use.false indicates low use, true indicates high use.  
    bool _degree_of_use[MAX_ASSOCIATIVITY];
   
    //note the MRU position in the cache. This is updated on a cache miss where we insert the block 
    //at the MRU position or on a cache hit, were we subsequently insert the block into the MRU position. 
    int32_t _mru_position;

    //store the function that had this cache block be inserted
    //in the cache. 
    uint64_t _function[MAX_ASSOCIATIVITY];
    
    //store the address of the item also alongside, so we can retrieve it on a replacement. 
    uint64_t _addr[MAX_ASSOCIATIVITY];
    
    bool _spatial_fetched_block[MAX_ASSOCIATIVITY];
    
    UINT32 _tagsLastIndex;
    UINT32 _nextReplaceIndex;

    uint64_t _burst_count_for_cache_blocks_when_live[MAX_ASSOCIATIVITY];
    
  public:
    ROUND_ROBIN(UINT32 associativity = MAX_ASSOCIATIVITY)
      : _tagsLastIndex(associativity - 1)
    {
        ASSERTX(associativity <= MAX_ASSOCIATIVITY);
        _nextReplaceIndex = _tagsLastIndex;

        for (INT32 index = _tagsLastIndex; index >= 0; index--)
        {
         _tags[index] = CACHE_TAG(0);
         _tag_last_reference_time[index] = 0;
	 _degree_of_use[index] = false;
	 _addr[index] = 0;
         //want to note the burst count for all live cache blocks	
	 _burst_count_for_cache_blocks_when_live[index] = 0;
	 _spatial_fetched_block[index] = 0;
	 _function[index] = 0;
	}
	_mru_position = 0;
    }

    VOID SetAssociativity(UINT32 associativity)
    {
        ASSERTX(associativity <= MAX_ASSOCIATIVITY);
        _tagsLastIndex = associativity - 1;
        _nextReplaceIndex = _tagsLastIndex;
    }
    UINT32 GetAssociativity(UINT32 associativity) { return _tagsLastIndex + 1; }
    
    UINT32 Find(CACHE_TAG tag)
    {
        bool result = true;
	total_accesses++;
        
	for (INT32 index = _tagsLastIndex; index >= 0; index--)
        {
            // this is an ugly micro-optimization, but it does cause a
            // tighter assembly loop for ARM that way ...

            if(_tags[index] == tag) {
		    _tag_last_reference_time[index] = total_accesses;
		    _mru_position = index;
		    goto end;
	    }
//		if(_tags[index] == tag) goto end;
        }
        result = false;

        end: return result;
    }

    //functions may start with a low degree of use and then progress to have a
    // high degree of use. 
    UINT32 Find_UpdateDegreeOfUse(ADDRINT addr, CACHE_TAG tag, bool degree_of_use, bool medium_degree_of_use,
		    bool &non_mru_hit, uint64_t future_reference_timestamp,
		    bool victim_search, uint32_t setIndex, bool &is_spatial_fetched_block,
		    uint64_t current_function)
    {
        bool result = true;
	total_accesses++;
        
	for (INT32 index = _tagsLastIndex; index >= 0; index--)
        {
            // this is an ugly micro-optimization, but it does cause a
            // tighter assembly loop for ARM that way ...
            if(_tags[index] == tag) {
		    //if the block was spatially fetched, then set the bool
		    //and reset this field as reference in original cache would've 
		    //anyways bought this into the cache
		    //uint64_t blk_addr_for_this_block = _addr[index];

		    if (_spatial_fetched_block[index] == 1){
			   is_spatial_fetched_block = true; 
			   _spatial_fetched_block[index] = 0;
		    }
		   // if (mapping_from_block_to_functions_used_block[blk_addr_for_this_block].find(current_function) 
		   //     	    == mapping_from_block_to_functions_used_block[blk_addr_for_this_block].end()){
		   //         is_spatial_fetched_block = true;
		   //         mapping_from_block_to_functions_used_block[blk_addr_for_this_block].insert(current_function);
		   // }
		    if (index != _mru_position){
			    non_mru_hit = true;
		    	    _burst_count_for_cache_blocks_when_live[_mru_position]++; 
		    	    
			vector<uint64_t> low_use_blocks; 
			uint32_t number_of_looping_blocks = 0;
			for (INT32 index = _tagsLastIndex; index >= 0; index--)
		        {
			      uint64_t block_address_for_block_at_index;
			      block_address_for_block_at_index = _addr[index];
		               
			      if(mapping_from_cache_block_to_mostly_miss[block_address_for_block_at_index]){
		              	 low_use_blocks.push_back(block_address_for_block_at_index);
				 number_of_looping_blocks++;
			      }
		        }
	        	if ((number_of_looping_blocks >= 4)&&
	        			(number_of_looping_blocks>0)){
	        	   // for (uint32_t i = 0;i<number_of_looping_blocks;i++)
	        	   //         cout << low_use_blocks.at(i) <<"," ;
	        	   // cout << endl;
	        	   // cout <<"The current set is a hot set " << setIndex <<" and the number of looping blocks are " << 
	        	   //        number_of_looping_blocks << endl;	
	        	   // cout << "The set of interest is " << setIndex << endl;
	        	    hotness_of_sets.set(setIndex);
	        	   // cout << "Number of hot sets in the cache are " << hotness_of_sets.count() << endl;
	        	}
	        	else{
	   		   // if (hotness_of_sets.test(setIndex)) 
			   // 	cout << "Unsetting: set of interest is " << setIndex << endl;
			    hotness_of_sets.reset(setIndex);
			}    
			    uint64_t blk_address_exp_nmru_hit = _addr[index];
			    mapping_from_cache_block_to_misses_and_nmru_hits_ratio[blk_address_exp_nmru_hit].hits++;
		           //update bit and stats on NMRU hits as well. 
			   if (mapping_from_cache_block_to_misses_and_nmru_hits_ratio[blk_address_exp_nmru_hit].hits 
					== 10){
				float miss_ratio = (float)mapping_from_cache_block_to_misses_and_nmru_hits_ratio[blk_address_exp_nmru_hit].misses/(mapping_from_cache_block_to_misses_and_nmru_hits_ratio[blk_address_exp_nmru_hit].hits + mapping_from_cache_block_to_misses_and_nmru_hits_ratio[blk_address_exp_nmru_hit].misses);
			   if (miss_ratio >= 0.5)
				mapping_from_cache_block_to_mostly_miss[blk_address_exp_nmru_hit] = true;
			   else
				mapping_from_cache_block_to_mostly_miss[blk_address_exp_nmru_hit] = false;
			   mapping_from_cache_block_to_misses_and_nmru_hits_ratio[blk_address_exp_nmru_hit].misses = 0;
			   mapping_from_cache_block_to_misses_and_nmru_hits_ratio[blk_address_exp_nmru_hit].hits = 0;
		         }
		    }
		    _tag_last_reference_time[index] = total_accesses;
		    //update the mru index
		    _mru_position = index;
		    //update the degree of use. 
		    _degree_of_use[index] = degree_of_use;
		    goto end;
	    }
//		if(_tags[index] == tag) goto end;
        }
        result = false;
        end: return result;
    }
    VOID Replace(CACHE_TAG tag)
    {
        // g++ -O3 too dumb to do CSE on following lines?!
      //  const UINT32 index = _nextReplaceIndex;

      //  _tags[index] = tag;
      //  // condition typically faster than modulo
      //  _nextReplaceIndex = (index == 0 ? _tagsLastIndex : index - 1);
    
        uint64_t min_access_time = _tag_last_reference_time[_tagsLastIndex];
        uint64_t _nextReplaceIndex = _tagsLastIndex;
        for (INT32 index = _tagsLastIndex; index >= 0; index--)
        {
          if(min_access_time>_tag_last_reference_time[index]){
               _nextReplaceIndex = index;
               min_access_time = _tag_last_reference_time[index];
            }
        }
	const UINT32 index = _nextReplaceIndex;
	
        _tags[index] = tag;
	_tag_last_reference_time[index] = total_accesses;
	//update the mru index
	_mru_position = index;
    }

    use_and_blk_addr Replace_GetDegreeOfUse(CACHE_TAG tag, bool degree_of_use,uint64_t blk_addr, bool medium_degree_of_use, 
		    uint64_t future_reference_timestamp, float block_degree_of_use, bool direct_mapped, bool straggler_remove,
		    uint64_t current_function, bool victim_search, uint32_t setIndex, bool spatial_fetch)
    {
        // g++ -O3 too dumb to do CSE on following lines?!
      //  const UINT32 index = _nextReplaceIndex;

      //  _tags[index] = tag;
      //  // condition typically faster than modulo
      //  _nextReplaceIndex = (index == 0 ? _tagsLastIndex : index - 1);
    
	_burst_count_for_cache_blocks_when_live[_mru_position]++;
        uint64_t min_access_time = _tag_last_reference_time[_tagsLastIndex];
        uint64_t _nextReplaceIndex = _tagsLastIndex;
        
	//check if looping pattern exists for the set and print if it doesn. 
	//if more than 4 blocks have the low use bit set, then print.
	uint32_t number_of_looping_blocks = 0;

	vector<uint64_t> low_use_blocks; 
	for (INT32 index = _tagsLastIndex; index >= 0; index--)
        {
	      uint64_t block_address_for_block_at_index;
	      block_address_for_block_at_index = _addr[index];
               
	      if(mapping_from_cache_block_to_mostly_miss[block_address_for_block_at_index]){
              	 low_use_blocks.push_back(block_address_for_block_at_index);
		 number_of_looping_blocks++;
	      }
        }
	if ((number_of_looping_blocks >= 4)&&
			(number_of_looping_blocks>0)){
	  //  for (uint32_t i = 0;i<number_of_looping_blocks;i++)
	  //          cout << low_use_blocks.at(i) <<"," ;
	  //  cout << endl;
	  //  cout <<"The current set is a hot set " << setIndex <<" and the number of looping blocks are " << 
	  //         number_of_looping_blocks << endl;	
	  //  cout << "The set of interest is " << setIndex << endl;
	    hotness_of_sets.set(setIndex);
	  //  cout << "Number of hot sets in the cache are " << hotness_of_sets.count() << endl;
	}
	else{
	  // if (hotness_of_sets.test(setIndex)) 
 	  // 	cout << "Unsetting: set of interest is " << setIndex << endl;
	   hotness_of_sets.reset(setIndex);
	}
        
	low_use_blocks.clear();	
	//way 0 is for low use functions and all other ways are for high use functions. 
	//see how this fares.
	
	for (INT32 index = _tagsLastIndex; index >= 0; index--)
        {
              if(min_access_time>_tag_last_reference_time[index]){
                 _nextReplaceIndex = index;
                 min_access_time = _tag_last_reference_time[index];
              }
        }
	const UINT32 index = _nextReplaceIndex;
	bool replaced_block_degree_of_use = _degree_of_use[index];
        uint64_t replaced_block_address = _addr[index];
	uint64_t replaced_block_burst_count = 
		_burst_count_for_cache_blocks_when_live[index];
	if ((replaced_block_burst_count == 2)||(replaced_block_burst_count == 1))
		low_use_block_address_and_burst_counts[replaced_block_address].insert(replaced_block_burst_count);
	_burst_count_for_cache_blocks_when_live[index] = 0;
	use_and_blk_addr temp;
	temp.function_use_information = replaced_block_degree_of_use;
	temp.blk_addr = replaced_block_address;
	temp.replaced_block_function = _function[index];
	_tags[index] = tag;
	_tag_last_reference_time[index] = total_accesses;
	//update the mru index
	_mru_position = index;
	_degree_of_use[index] = degree_of_use;
	_addr[index] = blk_addr;
	//keep track of which function brought this block into the cache. 
//	mapping_from_block_to_functions_used_block[blk_addr].clear();
//	mapping_from_block_to_functions_used_block[blk_addr].insert(current_function);
	
	if (spatial_fetch)
		_spatial_fetched_block[index] = true;
	else
		_spatial_fetched_block[index] = false;
	//block experiencing miss for the first time. 
	if (mapping_from_cache_block_to_misses_and_nmru_hits_ratio.find(blk_addr) ==
			mapping_from_cache_block_to_misses_and_nmru_hits_ratio.end()){
		mapping_from_cache_block_to_misses_and_nmru_hits_ratio[blk_addr].misses = 1;
		mapping_from_cache_block_to_mostly_miss[blk_addr] = false;
	}
	else{
		mapping_from_cache_block_to_misses_and_nmru_hits_ratio[blk_addr].misses++;
		if (mapping_from_cache_block_to_misses_and_nmru_hits_ratio[blk_addr].misses 
				== 10){
			float miss_ratio = (float)mapping_from_cache_block_to_misses_and_nmru_hits_ratio[blk_addr].misses/(mapping_from_cache_block_to_misses_and_nmru_hits_ratio[blk_addr].hits + mapping_from_cache_block_to_misses_and_nmru_hits_ratio[blk_addr].misses);
			if (miss_ratio >= 0.5)
				mapping_from_cache_block_to_mostly_miss[blk_addr] = true;
			else
				mapping_from_cache_block_to_mostly_miss[blk_addr] = false;
			mapping_from_cache_block_to_misses_and_nmru_hits_ratio[blk_addr].misses = 0;
			mapping_from_cache_block_to_misses_and_nmru_hits_ratio[blk_addr].hits = 0;
		}

	}
	_function[index] = current_function;
	return temp;
    }
};


template <UINT32 MAX_ASSOCIATIVITY = 256>
class ROUND_ROBIN_PLUS_SELECTIVE_VICTIM
{
  private:
    CACHE_TAG _tags[MAX_ASSOCIATIVITY];
    //add the last access number as the proxy for last reference time
    //to enable LRU based replacement
    uint64_t _tag_last_reference_time[MAX_ASSOCIATIVITY];
    
    //track degree of use for each block (whether it comes from a function with high use or low use)
    //default set to low use.false indicates low use, true indicates high use.  
    bool _degree_of_use[MAX_ASSOCIATIVITY];
   
    //note the MRU position in the cache. This is updated on a cache miss where we insert the block 
    //at the MRU position or on a cache hit, were we subsequently insert the block into the MRU position. 
    int32_t _mru_position;

    //store the function that had this cache block be inserted
    //in the cache. 
    uint64_t _function[MAX_ASSOCIATIVITY];
    
    //store the address of the item also alongside, so we can retrieve it on a replacement. 
    uint64_t _addr[MAX_ASSOCIATIVITY];
    
    UINT32 _tagsLastIndex;
    UINT32 _nextReplaceIndex;

    uint64_t _burst_count_for_cache_blocks_when_live[MAX_ASSOCIATIVITY];
  public:
    ROUND_ROBIN_PLUS_SELECTIVE_VICTIM(UINT32 associativity = MAX_ASSOCIATIVITY)
      : _tagsLastIndex(associativity - 1)
    {
        ASSERTX(associativity <= MAX_ASSOCIATIVITY);
        _nextReplaceIndex = _tagsLastIndex;

        for (INT32 index = _tagsLastIndex; index >= 0; index--)
        {
         _tags[index] = CACHE_TAG(0);
         _tag_last_reference_time[index] = 0;
	 _degree_of_use[index] = false;
	 _addr[index] = 0;
         //want to note the burst count for all live cache blocks	
	 _burst_count_for_cache_blocks_when_live[index] = 0;
	 _function[index] = 0;
	}
        if (!victim_buffer_entries_initialized){
           for (INT32 index = (NUM_VICTIM_ENTRIES-1);
                  index >=0; index--)
          {
            low_use_victim_entries[_nextReplaceIndex].valid = false;
            low_use_victim_entries[_nextReplaceIndex].addr = 0;
            low_use_victim_entries[_nextReplaceIndex].timestamp = 0;
          }
          victim_buffer_entries_initialized = true;
        }
	_mru_position = 0;
    }

    VOID SetAssociativity(UINT32 associativity)
    {
        ASSERTX(associativity <= MAX_ASSOCIATIVITY);
        _tagsLastIndex = associativity - 1;
        _nextReplaceIndex = _tagsLastIndex;
    }
    UINT32 GetAssociativity(UINT32 associativity) { return _tagsLastIndex + 1; }
    
    UINT32 Find(CACHE_TAG tag)
    {
        bool result = true;
	total_accesses++;
        
	for (INT32 index = _tagsLastIndex; index >= 0; index--)
        {
            // this is an ugly micro-optimization, but it does cause a
            // tighter assembly loop for ARM that way ...

            if(_tags[index] == tag) {
		    _tag_last_reference_time[index] = total_accesses;
		    _mru_position = index;
		    goto end;
	    }
//		if(_tags[index] == tag) goto end;
        }
        result = false;

        end: return result;
    }

    //functions may start with a low degree of use and then progress to have a
    // high degree of use. 
    UINT32 Find_UpdateDegreeOfUse(ADDRINT addr, CACHE_TAG tag, bool degree_of_use, bool medium_degree_of_use,
		    bool &non_mru_hit, uint64_t future_reference_timestamp,
		    bool victim_search, uint32_t setIndex, bool &is_spatial_fetched_block,
		    uint64_t current_function)
    {
        bool result = true;
	total_accesses++;
        bool found = false; 
        
	//record the LRU block, in case we have a miss in cache and a hit in 
	//victim buffer. Then we can swap the two elements. 
	uint64_t min_access_time = _tag_last_reference_time[_tagsLastIndex];
        uint64_t _nextReplaceIndex = _tagsLastIndex;
	for (INT32 index = _tagsLastIndex; index >= 0; index--)
        {
            // this is an ugly micro-optimization, but it does cause a
            // tighter assembly loop for ARM that way ...
            if(_tags[index] == tag) {
		    if (index != _mru_position){
			    non_mru_hit = true;
		    	    _burst_count_for_cache_blocks_when_live[_mru_position]++;

			vector<uint64_t> low_use_blocks; 
			uint32_t number_of_looping_blocks = 0;
			for (INT32 index = _tagsLastIndex; index >= 0; index--)
		        {
			      uint64_t block_address_for_block_at_index;
			      block_address_for_block_at_index = _addr[index];
		               
			      if(mapping_from_cache_block_to_mostly_miss[block_address_for_block_at_index]){
		              	 low_use_blocks.push_back(block_address_for_block_at_index);
				 number_of_looping_blocks++;
			      }
		        }
	        	if ((number_of_looping_blocks >= 4)&&
	        			(number_of_looping_blocks>0)){
	        	   // for (uint32_t i = 0;i<number_of_looping_blocks;i++)
	        	   //         cout << low_use_blocks.at(i) <<"," ;
	        	   // cout << endl;
	        	   // cout <<"The current set is a hot set " << setIndex <<" and the number of looping blocks are " << 
	        	   //        number_of_looping_blocks << endl;	
	        	   // cout << "The set of interest is " << setIndex << endl;
	        	    hotness_of_sets.set(setIndex);
	        	   // cout << "Number of hot sets in the cache are " << hotness_of_sets.count() << endl;
	        	}
	        	else{
	   		   // if (hotness_of_sets.test(setIndex)) 
			   // 	cout << "Unsetting: set of interest is " << setIndex << endl;
			    hotness_of_sets.reset(setIndex);
			}   
			   uint64_t blk_address_exp_nmru_hit = _addr[index];
			   mapping_from_cache_block_to_misses_and_nmru_hits_ratio[blk_address_exp_nmru_hit].hits++;
		          //update bit and stats on NMRU hits as well. 
			  if (mapping_from_cache_block_to_misses_and_nmru_hits_ratio[blk_address_exp_nmru_hit].hits 
			       	== 10){
			       float miss_ratio = (float)mapping_from_cache_block_to_misses_and_nmru_hits_ratio[blk_address_exp_nmru_hit].misses/(mapping_from_cache_block_to_misses_and_nmru_hits_ratio[blk_address_exp_nmru_hit].hits + mapping_from_cache_block_to_misses_and_nmru_hits_ratio[blk_address_exp_nmru_hit].misses);
			  if (miss_ratio >= 0.5)
			       mapping_from_cache_block_to_mostly_miss[blk_address_exp_nmru_hit] = true;
			  else
			       mapping_from_cache_block_to_mostly_miss[blk_address_exp_nmru_hit] = false;
			  mapping_from_cache_block_to_misses_and_nmru_hits_ratio[blk_address_exp_nmru_hit].misses = 0;
			  mapping_from_cache_block_to_misses_and_nmru_hits_ratio[blk_address_exp_nmru_hit].hits = 0;
		        }
		    }
		    _tag_last_reference_time[index] = total_accesses;
		    //update the mru index
		    _mru_position = index;
		    //update the degree of use. 
		    _degree_of_use[index] = degree_of_use;
		    found = true;
		    //goto end;
	    }
            if(min_access_time>_tag_last_reference_time[index]){
               _nextReplaceIndex = index;
               min_access_time = _tag_last_reference_time[index];
            }
//		if(_tags[index] == tag) goto end;
        }
        //generate block address and compare against
        //entries in the victim buffer.
        uint64_t notLineMask = ~(64 - 1);
	ADDRINT block_address = addr&notLineMask;
        bool victim_found = false;
	uint64_t victim_block_index;
	if ((!found) && (victim_search)){
          for (int i = 0;i<NUM_VICTIM_ENTRIES;i++){
             if (low_use_victim_entries[i].valid){
               if (low_use_victim_entries[i].addr ==
                               block_address){
                   found = true;
		   victim_found = true;
		   victim_block_index = i;
                   //update the timstamp so that stale low use blocks that are 
                   //kicked out can get thrown out of the cache
                   //sooner. 
                   low_use_victim_entries[i].timestamp = total_accesses;
                   //cout << "Function " <<low_use_victim_entries[victim_block_index].current_function << " hit in victim cache " << endl;
	        }
             }
           }
        }
	//had a hit in the victim cache, then swap LRU block in the cache with 
	//this block in the victim cache.
	if (victim_found){
	  	uint64_t temp_addr, temp_tag, temp_current_function, temp_timestamp;
		temp_addr = low_use_victim_entries[victim_block_index].addr;
		temp_tag = low_use_victim_entries[victim_block_index].tag;
		temp_current_function = low_use_victim_entries[victim_block_index].current_function;
		temp_timestamp = low_use_victim_entries[victim_block_index].timestamp;
		low_use_victim_entries[victim_block_index].addr = _addr[_nextReplaceIndex];
		low_use_victim_entries[victim_block_index].tag = _tags[_nextReplaceIndex];
		low_use_victim_entries[victim_block_index].current_function = _function[_nextReplaceIndex];
		low_use_victim_entries[victim_block_index].timestamp = _tag_last_reference_time[_nextReplaceIndex];
		low_use_victim_entries[victim_block_index].valid = true;
		//cout << "Swapping and placing " << low_use_victim_entries[victim_block_index].current_function << " in victim cache and it's timestamp is: " <<low_use_victim_entries[victim_block_index].timestamp << endl;
		_addr[_nextReplaceIndex] = temp_addr;
		_tags[_nextReplaceIndex] = temp_tag;
		_function[_nextReplaceIndex] = temp_current_function;
		_tag_last_reference_time[_nextReplaceIndex] = temp_timestamp;
	}
        if (!found)
           result = false;
         return result; 
    }
    VOID Replace(CACHE_TAG tag)
    {
        // g++ -O3 too dumb to do CSE on following lines?!
      //  const UINT32 index = _nextReplaceIndex;

      //  _tags[index] = tag;
      //  // condition typically faster than modulo
      //  _nextReplaceIndex = (index == 0 ? _tagsLastIndex : index - 1);
    
        uint64_t min_access_time = _tag_last_reference_time[_tagsLastIndex];
        uint64_t _nextReplaceIndex = _tagsLastIndex;
        for (INT32 index = _tagsLastIndex; index >= 0; index--)
        {
          if(min_access_time>_tag_last_reference_time[index]){
               _nextReplaceIndex = index;
               min_access_time = _tag_last_reference_time[index];
            }
        }
	const UINT32 index = _nextReplaceIndex;
	
        _tags[index] = tag;
	_tag_last_reference_time[index] = total_accesses;
	//update the mru index
	_mru_position = index;
    }

    use_and_blk_addr Replace_GetDegreeOfUse(CACHE_TAG tag, bool degree_of_use,uint64_t blk_addr, bool medium_degree_of_use, 
		    uint64_t future_reference_timestamp, float block_degree_of_use, bool direct_mapped, bool straggler_remove,
		    uint64_t current_function, bool victim_search, uint32_t setIndex, bool spatial_fetch)
    {
        // g++ -O3 too dumb to do CSE on following lines?!
      //  const UINT32 index = _nextReplaceIndex;

      //  _tags[index] = tag;
      //  // condition typically faster than modulo
      //  _nextReplaceIndex = (index == 0 ? _tagsLastIndex : index - 1);
    
	_burst_count_for_cache_blocks_when_live[_mru_position]++;
        uint64_t min_access_time = _tag_last_reference_time[_tagsLastIndex];
        uint64_t _nextReplaceIndex = _tagsLastIndex;
        
	
	//check if looping pattern exists for the set and print if it doesn. 
	//if more than 4 blocks have the low use bit set, then print.
	uint32_t number_of_looping_blocks = 0;

	vector<uint64_t> low_use_blocks; 
	for (INT32 index = _tagsLastIndex; index >= 0; index--)
        {
	      uint64_t block_address_for_block_at_index;
	      block_address_for_block_at_index = _addr[index];
               
	      if(mapping_from_cache_block_to_mostly_miss[block_address_for_block_at_index]){
              	 low_use_blocks.push_back(block_address_for_block_at_index);
		 number_of_looping_blocks++;
	      }
        }
	if ((number_of_looping_blocks >= 4)&&
			(number_of_looping_blocks>0)){
	  //  for (uint32_t i = 0;i<number_of_looping_blocks;i++)
	  //          cout << low_use_blocks.at(i) <<"," ;
	  //  cout << endl;
	  //  cout <<"The current set is a hot set " << setIndex <<" and the number of looping blocks are " << 
	  //         number_of_looping_blocks << endl;	
	  //  cout << "The set of interest is " << setIndex << endl;
	    hotness_of_sets.set(setIndex);
	  //  cout << "Number of hot sets in the cache are " << hotness_of_sets.count() << endl;
	}
	else{
	  // if (hotness_of_sets.test(setIndex)) 
 	  // 	cout << "Unsetting: set of interest is " << setIndex << endl;
	   hotness_of_sets.reset(setIndex);
	}
        
	low_use_blocks.clear();	
	
	//way 0 is for low use functions and all other ways are for high use functions. 
	//see how this fares.
	for (INT32 index = _tagsLastIndex; index >= 0; index--)
        {
              if(min_access_time>_tag_last_reference_time[index]){
                 _nextReplaceIndex = index;
                 min_access_time = _tag_last_reference_time[index];
              }
        }
	const UINT32 index = _nextReplaceIndex;
	bool replaced_block_degree_of_use = _degree_of_use[index];
        uint64_t replaced_block_address = _addr[index];
	uint64_t replaced_block_tag = _tags[index];
	uint64_t replaced_block_function = _function[index];
	uint64_t replaced_block_access_time = _tag_last_reference_time[index];
	_burst_count_for_cache_blocks_when_live[index] = 0;
	use_and_blk_addr temp;
	temp.function_use_information = replaced_block_degree_of_use;
	temp.blk_addr = replaced_block_address;
	//if replaced block is from a low use function(deg of use > 1),
        //then find a spot to place in the victim buffer and 
        //place this block there. 
        if (victim_search){
//              cout << "Placing entry in the victim cache " << endl;
	      uint64_t block_address = temp.blk_addr;
              uint64_t min_access_time = low_use_victim_entries[NUM_VICTIM_ENTRIES-1].timestamp;
              uint64_t _nextReplaceIndex = (NUM_VICTIM_ENTRIES-1);
              for (INT32 index = (NUM_VICTIM_ENTRIES-1);
                              index >=0; index--)
              {
                 if(min_access_time>low_use_victim_entries[index].timestamp){
                    _nextReplaceIndex = index;
                    min_access_time = low_use_victim_entries[index].timestamp;
                 }
              }
	      //cout <<"Replacing block whose min access time is " << min_access_time << " and whose block address is: " << low_use_victim_entries[_nextReplaceIndex].addr << endl;
              low_use_victim_entries[_nextReplaceIndex].valid = true;
              low_use_victim_entries[_nextReplaceIndex].addr = block_address;
              low_use_victim_entries[_nextReplaceIndex].tag = replaced_block_tag;
	      low_use_victim_entries[_nextReplaceIndex].current_function = 
		      replaced_block_function;
	      low_use_victim_entries[_nextReplaceIndex].timestamp = replaced_block_access_time;
	      //cout << "Placing function " << replaced_block_function << " in victim cache and timestamps is " <<low_use_victim_entries[_nextReplaceIndex].timestamp << endl;
        }
	
	temp.replaced_block_function = _function[index];
	_tags[index] = tag;
	_tag_last_reference_time[index] = total_accesses;
	//update the mru index
	_mru_position = index;
	_degree_of_use[index] = degree_of_use;
	_addr[index] = blk_addr;
	
	//block experiencing miss for the first time. 
	if (mapping_from_cache_block_to_misses_and_nmru_hits_ratio.find(blk_addr) ==
			mapping_from_cache_block_to_misses_and_nmru_hits_ratio.end()){
		mapping_from_cache_block_to_misses_and_nmru_hits_ratio[blk_addr].misses = 1;
		mapping_from_cache_block_to_mostly_miss[blk_addr] = false;
	}
	else{
		mapping_from_cache_block_to_misses_and_nmru_hits_ratio[blk_addr].misses++;
		if (mapping_from_cache_block_to_misses_and_nmru_hits_ratio[blk_addr].misses 
				== 10){
			float miss_ratio = (float)mapping_from_cache_block_to_misses_and_nmru_hits_ratio[blk_addr].misses/(mapping_from_cache_block_to_misses_and_nmru_hits_ratio[blk_addr].hits + mapping_from_cache_block_to_misses_and_nmru_hits_ratio[blk_addr].misses);
			if (miss_ratio >= 0.5)
				mapping_from_cache_block_to_mostly_miss[blk_addr] = true;
			else
				mapping_from_cache_block_to_mostly_miss[blk_addr] = false;
			mapping_from_cache_block_to_misses_and_nmru_hits_ratio[blk_addr].misses = 0;
			mapping_from_cache_block_to_misses_and_nmru_hits_ratio[blk_addr].hits = 0;
		}

	}
	
	_function[index] = current_function;
	return temp;
    }
};


template <UINT32 MAX_ASSOCIATIVITY = 4>
class CACHE_BELADY_OPT
{
  private:
    CACHE_TAG _tags[MAX_ASSOCIATIVITY];
    //add the last access number as the proxy for last reference time
    //to enable LRU based replacement
    uint64_t _tag_last_reference_time[MAX_ASSOCIATIVITY];
    
    //track degree of use for each block (whether it comes from a function with high use or low use)
    //default set to low use.false indicates low use, true indicates high use.  
    bool _degree_of_use[MAX_ASSOCIATIVITY];
   
    //note the MRU position in the cache. This is updated on a cache miss where we insert the block 
    //at the MRU position or on a cache hit, were we subsequently insert the block into the MRU position. 
    int32_t _mru_position;

    //store the address of the item also alongside, so we can retrieve it on a replacement. 
    uint64_t _addr[MAX_ASSOCIATIVITY];
    
    UINT32 _tagsLastIndex;
    UINT32 _nextReplaceIndex;

  public:
    CACHE_BELADY_OPT(UINT32 associativity = MAX_ASSOCIATIVITY)
      : _tagsLastIndex(associativity - 1)
    {
        ASSERTX(associativity <= MAX_ASSOCIATIVITY);
        _nextReplaceIndex = _tagsLastIndex;

        for (INT32 index = _tagsLastIndex; index >= 0; index--)
        {
         _tags[index] = CACHE_TAG(0);
         _tag_last_reference_time[index] = 100000000000;
	 _degree_of_use[index] = false;
	 _addr[index] = 0;
	}
	_mru_position = 0;
    }

    VOID SetAssociativity(UINT32 associativity)
    {
        ASSERTX(associativity <= MAX_ASSOCIATIVITY);
        _tagsLastIndex = associativity - 1;
        _nextReplaceIndex = _tagsLastIndex;
    }
    UINT32 GetAssociativity(UINT32 associativity) { return _tagsLastIndex + 1; }
    
    UINT32 Find(CACHE_TAG tag)
    {
        bool result = true;
	total_accesses++;
        
	for (INT32 index = _tagsLastIndex; index >= 0; index--)
        {
            // this is an ugly micro-optimization, but it does cause a
            // tighter assembly loop for ARM that way ...

            if(_tags[index] == tag) {
		    _tag_last_reference_time[index] = total_accesses;
		    _mru_position = index;
		    goto end;
	    }
//		if(_tags[index] == tag) goto end;
        }
        result = false;

        end: return result;
    }

    //functions may start with a low degree of use and then progress to have a
    // high degree of use. 
    UINT32 Find_UpdateDegreeOfUse(ADDRINT addr, CACHE_TAG tag, bool degree_of_use, bool medium_degree_of_use,
		    bool &non_mru_hit, uint64_t future_reference_timestamp,
		    bool victim_search, uint32_t setIndex, bool &is_spatial_fetched_block,
		    uint64_t current_function)
    {
        bool result = true;
	total_accesses++;
        
	for (INT32 index = _tagsLastIndex; index >= 0; index--)
        {
            // this is an ugly micro-optimization, but it does cause a
            // tighter assembly loop for ARM that way ...
            if(_tags[index] == tag) {
		    if (index != _mru_position)
			    non_mru_hit = true;
		    //update with the timestamp corresponding to future reference
		    _tag_last_reference_time[index] = future_reference_timestamp;
		    //update the mru index
		    _mru_position = index;
		    //update the degree of use. 
		    _degree_of_use[index] = degree_of_use;
		    goto end;
	    }
//		if(_tags[index] == tag) goto end;
        }
        result = false;
        end: return result;
    }
    VOID Replace(CACHE_TAG tag)
    {
        // g++ -O3 too dumb to do CSE on following lines?!
      //  const UINT32 index = _nextReplaceIndex;

      //  _tags[index] = tag;
      //  // condition typically faster than modulo
      //  _nextReplaceIndex = (index == 0 ? _tagsLastIndex : index - 1);
    
        uint64_t min_access_time = _tag_last_reference_time[_tagsLastIndex];
        uint64_t _nextReplaceIndex = _tagsLastIndex;
        for (INT32 index = _tagsLastIndex; index >= 0; index--)
        {
          if(min_access_time>_tag_last_reference_time[index]){
               _nextReplaceIndex = index;
               min_access_time = _tag_last_reference_time[index];
            }
        }
	const UINT32 index = _nextReplaceIndex;
	
        _tags[index] = tag;
	_tag_last_reference_time[index] = total_accesses;
	//update the mru index
	_mru_position = index;
    }

    use_and_blk_addr Replace_GetDegreeOfUse(CACHE_TAG tag, bool degree_of_use,uint64_t blk_addr, bool medium_degree_of_use,
		    uint64_t future_reference_timestamp, float block_degree_of_use, bool direct_mapped, bool straggler_remove,
		    uint64_t current_function, bool victim_search, uint32_t setIndex, bool spatial_fetch)
    {
        uint64_t max_future_reference_timestamp = _tag_last_reference_time[_tagsLastIndex];
        uint64_t _nextReplaceIndex = _tagsLastIndex;
	//pick the block with largest future reference timestamp for replacement
	//cout <<" The future reference timestamps" << endl;
	for (INT32 index = _tagsLastIndex; index >= 0; index--)
        {
              if(max_future_reference_timestamp < _tag_last_reference_time[index]){
                 _nextReplaceIndex = index;
                 max_future_reference_timestamp = _tag_last_reference_time[index];
              }
        }
	const UINT32 index = _nextReplaceIndex;
	bool replaced_block_degree_of_use = _degree_of_use[index];
        uint64_t replaced_block_address = _addr[index];
	use_and_blk_addr temp;
	temp.function_use_information = replaced_block_degree_of_use;
	temp.blk_addr = replaced_block_address;
	_tags[index] = tag;
	_tag_last_reference_time[index] = future_reference_timestamp;
	//update the mru index
	_mru_position = index;
	_degree_of_use[index] = degree_of_use;
	_addr[index] = blk_addr;
	return temp;
    }
};

template <UINT32 MAX_ASSOCIATIVITY = 4>
class MODIFIED_CACHE
{
  private:
    CACHE_TAG _tags[MAX_ASSOCIATIVITY];
    //add the last access number as the proxy for last reference time
    //to enable LRU based replacement
    uint64_t _tag_last_reference_time[MAX_ASSOCIATIVITY];
    
    //note the MRU position in the cache. This is updated on a cache miss where we insert the block 
    //at the MRU position or on a cache hit, were we subsequently insert the block into the MRU position. 
    uint64_t _mru_position;
        
    //track degree of use for each block (whether it comes from a function with high use or low use)
    //default set to low use.false indicates low use, true indicates high use.  
    bool _degree_of_use[MAX_ASSOCIATIVITY];
   
    //bit is set for low use functions whose degree of use is greater than one. 
    bool _medium_degree_of_use[MAX_ASSOCIATIVITY];

    //store the address of the item also alongside, so we can retrieve it on a replacement. 
    uint64_t _addr[MAX_ASSOCIATIVITY];
    
    UINT32 _tagsLastIndex;
    UINT32 _nextReplaceIndex;

  public:
    MODIFIED_CACHE(UINT32 associativity = MAX_ASSOCIATIVITY)
      : _tagsLastIndex(associativity - 1)
    {
        ASSERTX(associativity <= MAX_ASSOCIATIVITY);
        _nextReplaceIndex = _tagsLastIndex;

        for (INT32 index = _tagsLastIndex; index >= 0; index--)
        {
         _tags[index] = CACHE_TAG(0);
         _tag_last_reference_time[index] = 0;
	 _degree_of_use[index] = false;
	 _medium_degree_of_use[index] = false;
	 _addr[index] = 0;
	}
	if (!victim_buffer_entries_initialized){
	   for (INT32 index = (NUM_VICTIM_ENTRIES-1); 
	      	  index >=0; index--)
	  {
	    low_use_victim_entries[_nextReplaceIndex].valid = false;
	    low_use_victim_entries[_nextReplaceIndex].addr = 0;
	    low_use_victim_entries[_nextReplaceIndex].timestamp = 0;
	  }
	  victim_buffer_entries_initialized = true;
	}
	_mru_position = 0;
    }

    VOID SetAssociativity(UINT32 associativity)
    {
        ASSERTX(associativity <= MAX_ASSOCIATIVITY);
        _tagsLastIndex = associativity - 1;
        _nextReplaceIndex = _tagsLastIndex;
    }
    UINT32 GetAssociativity(UINT32 associativity) { return _tagsLastIndex + 1; }
    
    UINT32 Find(CACHE_TAG tag)
    {
        bool result = true;
	total_accesses++;
        
	for (INT32 index = _tagsLastIndex; index >= 0; index--)
        {
            // this is an ugly micro-optimization, but it does cause a
            // tighter assembly loop for ARM that way ...

            if(_tags[index] == tag) {
		    _tag_last_reference_time[index] = total_accesses;
		    goto end;
	    }
//		if(_tags[index] == tag) goto end;
        }
        result = false;

        end: return result;
    }

    //functions may start with a low degree of use and then progress to have a
    // high degree of use. 
    UINT32 Find_UpdateDegreeOfUse(ADDRINT addr, CACHE_TAG tag, bool degree_of_use, bool medium_degree_of_use, 
        	    bool &non_mru_hit, uint64_t future_reference_timestamp,
		    bool victim_search, uint32_t setIndex, bool &is_spatial_fetched_block,
		    uint64_t current_function)
    {
        bool result = true;
        total_accesses++;
        bool found = false; 
        for (INT32 index = _tagsLastIndex; index >= 0; index--)
        {
            // this is an ugly micro-optimization, but it does cause a
            // tighter assembly loop for ARM that way ...

            if(_tags[index] == tag) {
        	    
           	   //Retain at LRU position when the degree of use of function is 
           	   //low. 
        	   //only low use functions can selectively get classified as 
        	   //medium use functions.

#ifdef LRU_PLACEMENT 
           	   if ((!degree_of_use || medium_degree_of_use) && 
        			   ((_tag_last_reference_time[index] == 0))){
#endif
#ifdef LRU_MINUS_ONE_PLACEMENT 
           	   if ((!degree_of_use || medium_degree_of_use) && 
        			   ((_tag_last_reference_time[index] == 0)||
        			    (_tag_last_reference_time[index] == 1))){
#endif

#ifdef LRU_MINUS_TWO_PLACEMENT 
           	   if ((!degree_of_use || medium_degree_of_use) && 
        			   ((_tag_last_reference_time[index] == 0)||
        			    (_tag_last_reference_time[index] == 1)||
				    (_tag_last_reference_time[index] == 2))){
#endif

#ifdef LRU_MINUS_THREE_PLACEMENT 
           	   if ((!degree_of_use || medium_degree_of_use) && 
        			   ((_tag_last_reference_time[index] == 0)||
        			    (_tag_last_reference_time[index] == 1)||
        			    (_tag_last_reference_time[index] == 2)||
				    (_tag_last_reference_time[index] == 3))){
#endif

#ifdef LRU_MINUS_FOUR_PLACEMENT 
           	   if ((!degree_of_use || medium_degree_of_use) && 
        			   ((_tag_last_reference_time[index] == 0)||
        			    (_tag_last_reference_time[index] == 1)||
        			    (_tag_last_reference_time[index] == 2)||
        			    (_tag_last_reference_time[index] == 3)||
				    (_tag_last_reference_time[index] == 4))){
#endif
  			   //do nothing.
        	   }

        	   else{
        	    _tag_last_reference_time[index] = total_accesses;
        	   }
        	    //update the degree of use. 
        	    _degree_of_use[index] = degree_of_use;
        	    found = true;
        	   // goto end;
            }
//      	if(_tags[index] == tag) goto end;
        }
        if (!found)	
           result = false;
        return result;
    }
    VOID Replace(CACHE_TAG tag)
    {
        // g++ -O3 too dumb to do CSE on following lines?!
      //  const UINT32 index = _nextReplaceIndex;

      //  _tags[index] = tag;
      //  // condition typically faster than modulo
      //  _nextReplaceIndex = (index == 0 ? _tagsLastIndex : index - 1);
    
        uint64_t min_access_time = _tag_last_reference_time[_tagsLastIndex];
        uint64_t _nextReplaceIndex = _tagsLastIndex;
        for (INT32 index = _tagsLastIndex; index >= 0; index--)
        {
          if(min_access_time>_tag_last_reference_time[index]){
               _nextReplaceIndex = index;
               min_access_time = _tag_last_reference_time[index];
            }
        }
	const UINT32 index = _nextReplaceIndex;
	
        _tags[index] = tag;
	_tag_last_reference_time[index] = total_accesses;
    }

#ifdef LRU_PLACEMENT
//function to place the medium degree of use cache block at LRU position-1. 
    use_and_blk_addr Replace_GetDegreeOfUse(CACHE_TAG tag, bool degree_of_use,uint64_t blk_addr, bool medium_degree_of_use,
		    uint64_t future_reference_timestamp, float block_degree_of_use, bool direct_mapped, bool straggler_remove,
		    uint64_t current_function, bool victim_search, uint32_t setIndex, bool spatial_fetch)
    {
        uint64_t min_access_time = _tag_last_reference_time[_tagsLastIndex];
        uint64_t _nextReplaceIndex = _tagsLastIndex;
        //tagsLastIndex equals associativity-1.
        int n = _tagsLastIndex+1; 
    	uint64_t* arr = (uint64_t*)malloc(sizeof(uint64_t)*n);
        for (int i = 0;i<n;i++){
           arr[i] = _tag_last_reference_time[i];
        }
        sort(arr, arr + n);
        min_access_time = arr[0];
        for (INT32 index = _tagsLastIndex; index >=0; index--)
        {
            if (_tag_last_reference_time[index] == min_access_time)
        	    _nextReplaceIndex = index;
        }
        free(arr);
        const UINT32 index = _nextReplaceIndex;
        bool replaced_block_degree_of_use = _degree_of_use[index];
        uint64_t replaced_block_address = _addr[index];
        use_and_blk_addr temp;
        temp.function_use_information = replaced_block_degree_of_use;
        //bool medium_block_degree_of_use = _medium_degree_of_use[index]; 	
         temp.blk_addr = replaced_block_address;
        temp.allocated_way = index;
        _tags[index] = tag;
        _tag_last_reference_time[index] = total_accesses;
        //insert at LRU position when the degree of use of function is 
        //low. 
        //insert at LRU-1 position if the block has a degree of use greater than 1. 
        if ((medium_degree_of_use)){
              _tag_last_reference_time[index] = 0;
        }
        else if ((!degree_of_use))
                _tag_last_reference_time[index] = 0;
        _degree_of_use[index] = degree_of_use;
        _medium_degree_of_use[index] = medium_degree_of_use;
        _addr[index] = blk_addr;
        return temp;
    }
#endif 

#ifdef LRU_MINUS_ONE_PLACEMENT
//function to place the medium degree of use cache block at LRU position-1. 
    use_and_blk_addr Replace_GetDegreeOfUse(CACHE_TAG tag, bool degree_of_use,uint64_t blk_addr, bool medium_degree_of_use, uint64_t future_reference_timestamp, 
		    float block_degree_of_use, bool direct_mapped, bool straggler_remove,
		    uint64_t current_function, bool victim_search, uint32_t setIndex, bool spatial_fetch)
    {
        uint64_t min_access_time = _tag_last_reference_time[_tagsLastIndex];
        uint64_t _nextReplaceIndex = _tagsLastIndex;
        uint64_t second_min_index;
        uint64_t second_min_access_time;
        //hold onto the LRU-1 index as well. 
        second_min_index = _nextReplaceIndex;
        //tagsLastIndex equals associativity-1.
        int n = _tagsLastIndex+1; 
    	uint64_t* arr = (uint64_t*)malloc(sizeof(uint64_t)*n);
        for (int i = 0;i<n;i++){
           arr[i] = _tag_last_reference_time[i];
        }
        sort(arr, arr + n);
        min_access_time = arr[0];
        second_min_access_time = arr[1];
        second_min_index = _nextReplaceIndex;
        for (INT32 index = _tagsLastIndex; index >=0; index--)
        {
            if (_tag_last_reference_time[index] == min_access_time)
        	    _nextReplaceIndex = index;
            if (_tag_last_reference_time[index] == second_min_access_time)
        	    second_min_index = index;
        }
        free(arr);
        const UINT32 index = _nextReplaceIndex;
        bool replaced_block_degree_of_use = _degree_of_use[index];
        uint64_t replaced_block_address = _addr[index];
        use_and_blk_addr temp;
        temp.function_use_information = replaced_block_degree_of_use;
        //bool medium_block_degree_of_use = _medium_degree_of_use[index]; 	
         temp.blk_addr = replaced_block_address;
        temp.allocated_way = index;
        _tags[index] = tag;
        _tag_last_reference_time[index] = total_accesses;
        //insert at LRU position when the degree of use of function is 
        //low. 
        //insert at LRU-1 position if the block has a degree of use greater than 1. 
        if ((medium_degree_of_use)){
              _tag_last_reference_time[second_min_index] = 0;
              _tag_last_reference_time[index] = 1;
        }
        else if ((!degree_of_use))
                _tag_last_reference_time[index] = 0;
        _degree_of_use[index] = degree_of_use;
        _medium_degree_of_use[index] = medium_degree_of_use;
        _addr[index] = blk_addr;
        return temp;
    }
#endif 

#ifdef LRU_MINUS_TWO_PLACEMENT    
//function to place the medium degree of use cache block at LRU position-2. 
    use_and_blk_addr Replace_GetDegreeOfUse(CACHE_TAG tag, bool degree_of_use,uint64_t blk_addr, bool medium_degree_of_use,
		    uint64_t future_reference_timestamp, float block_degree_of_use, bool direct_mapped,
		    bool straggler_remove,
		    uint64_t current_function, bool victim_search, 
		    uint32_t setIndex, bool spatial_fetch)
    {
        uint64_t min_access_time = _tag_last_reference_time[_tagsLastIndex];
        uint64_t _nextReplaceIndex = _tagsLastIndex;
        uint64_t second_min_index, third_min_index;
        uint64_t second_min_access_time, third_min_access_time;
        //hold onto the LRU-1 index as well. 
        second_min_index = _nextReplaceIndex;
        //tagsLastIndex equals associativity-1.
        int n = _tagsLastIndex+1; 
    	uint64_t* arr = (uint64_t*)malloc(sizeof(uint64_t)*n);
        for (int i = 0;i<n;i++){
           arr[i] = _tag_last_reference_time[i];
        }
        sort(arr, arr + n);
        min_access_time = arr[0];
        second_min_access_time = arr[1];
        third_min_access_time = arr[2];
        second_min_index = _nextReplaceIndex;
        third_min_index = _nextReplaceIndex;
        for (INT32 index = _tagsLastIndex; index >=0; index--)
        {
            if (_tag_last_reference_time[index] == min_access_time)
        	    _nextReplaceIndex = index;
            if (_tag_last_reference_time[index] == second_min_access_time)
        	    second_min_index = index;
            if (_tag_last_reference_time[index] == third_min_access_time)
        	    third_min_index = index;
        }
        free(arr);
        const UINT32 index = _nextReplaceIndex;
        bool replaced_block_degree_of_use = _degree_of_use[index];
        uint64_t replaced_block_address = _addr[index];
        use_and_blk_addr temp;
        temp.function_use_information = replaced_block_degree_of_use;
        //bool medium_block_degree_of_use = _medium_degree_of_use[index]; 	
         temp.blk_addr = replaced_block_address;
        temp.allocated_way = index;
        _tags[index] = tag;
        _tag_last_reference_time[index] = total_accesses;
        //insert at LRU position when the degree of use of function is 
        //low. 
        //insert at LRU-2 position if the block has a degree of use greater than 1. 
        if ((medium_degree_of_use)){
              _tag_last_reference_time[second_min_index] = 0;
              _tag_last_reference_time[third_min_index] = 1;
              _tag_last_reference_time[index] = 2;
        }
        else if ((!degree_of_use))
                _tag_last_reference_time[index] = 0;
        _degree_of_use[index] = degree_of_use;
        _medium_degree_of_use[index] = medium_degree_of_use;
        _addr[index] = blk_addr;
        return temp;
    }
#endif

#ifdef LRU_MINUS_THREE_PLACEMENT    
   //function to place the medium degree of use cache block at LRU position-3. 
    use_and_blk_addr Replace_GetDegreeOfUse(CACHE_TAG tag, bool degree_of_use,uint64_t blk_addr, bool medium_degree_of_use,
		    uint64_t future_reference_timestamp, float block_degree_of_use, bool direct_mapped,
		    bool straggler_remove,
		    uint64_t current_function, bool victim_search,
		    uint32_t setIndex, bool spatial_fetch)
    {
        uint64_t min_access_time = _tag_last_reference_time[_tagsLastIndex];
        uint64_t _nextReplaceIndex = _tagsLastIndex;
        uint64_t second_min_index, third_min_index, fourth_min_index;
        uint64_t second_min_access_time, third_min_access_time, fourth_min_access_time;
        //hold onto the LRU-1 index as well. 
        second_min_index = _nextReplaceIndex;
        //tagsLastIndex equals associativity-1.
        int n = _tagsLastIndex+1; 
    	uint64_t* arr = (uint64_t*)malloc(sizeof(uint64_t)*n);
        for (int i = 0;i<n;i++){
           arr[i] = _tag_last_reference_time[i];
        }
        sort(arr, arr + n);
        min_access_time = arr[0];
        second_min_access_time = arr[1];
        third_min_access_time = arr[2];
	fourth_min_access_time = arr[3];
        second_min_index = _nextReplaceIndex;
        third_min_index = _nextReplaceIndex;
	fourth_min_index = _nextReplaceIndex;
        for (INT32 index = _tagsLastIndex; index >=0; index--)
        {
            if (_tag_last_reference_time[index] == min_access_time)
        	    _nextReplaceIndex = index;
            if (_tag_last_reference_time[index] == second_min_access_time)
        	    second_min_index = index;
            if (_tag_last_reference_time[index] == third_min_access_time)
        	    third_min_index = index;
            if (_tag_last_reference_time[index] == fourth_min_access_time)
        	    fourth_min_index = index;
        }
        free(arr);
        const UINT32 index = _nextReplaceIndex;
        bool replaced_block_degree_of_use = _degree_of_use[index];
        uint64_t replaced_block_address = _addr[index];
        use_and_blk_addr temp;
        temp.function_use_information = replaced_block_degree_of_use;
        //bool medium_block_degree_of_use = _medium_degree_of_use[index]; 	
         temp.blk_addr = replaced_block_address;
        temp.allocated_way = index;
        _tags[index] = tag;
        _tag_last_reference_time[index] = total_accesses;
        //insert at LRU position when the degree of use of function is 
        //low. 
        //insert at LRU-2 position if the block has a degree of use greater than 1. 
        if ((medium_degree_of_use)){
              _tag_last_reference_time[second_min_index] = 0;
              _tag_last_reference_time[third_min_index] = 1;
              _tag_last_reference_time[fourth_min_index] = 2;
              _tag_last_reference_time[index] = 3;
        }
        else if ((!degree_of_use))
                _tag_last_reference_time[index] = 0;
        _degree_of_use[index] = degree_of_use;
        _medium_degree_of_use[index] = medium_degree_of_use;
        _addr[index] = blk_addr;
        return temp;
    }
#endif

#ifdef LRU_MINUS_FOUR_PLACEMENT    
   //function to place the medium degree of use cache block at LRU position-4. 
    use_and_blk_addr Replace_GetDegreeOfUse(CACHE_TAG tag, bool degree_of_use,uint64_t blk_addr, bool medium_degree_of_use,
		    uint64_t future_reference_timestamp, float block_degree_of_use, bool direct_mapped,
		    bool straggler_remove,
		    uint64_t current_function, bool victim_search,
		    uint32_t setIndex, bool spatial_fetch)
    {
        uint64_t min_access_time = _tag_last_reference_time[_tagsLastIndex];
        uint64_t _nextReplaceIndex = _tagsLastIndex;
        uint64_t second_min_index, third_min_index, fourth_min_index, fifth_min_index;
        uint64_t second_min_access_time, third_min_access_time, fourth_min_access_time, fifth_min_access_time;
        //hold onto the LRU-1 index as well. 
        second_min_index = _nextReplaceIndex;
        //tagsLastIndex equals associativity-1.
        int n = _tagsLastIndex+1; 
    	uint64_t* arr = (uint64_t*)malloc(sizeof(uint64_t)*n);
        for (int i = 0;i<n;i++){
           arr[i] = _tag_last_reference_time[i];
        }
        sort(arr, arr + n);
        min_access_time = arr[0];
        second_min_access_time = arr[1];
        third_min_access_time = arr[2];
	fourth_min_access_time = arr[3];
	fifth_min_access_time = arr[4];
	second_min_index = _nextReplaceIndex;
        third_min_index = _nextReplaceIndex;
	fourth_min_index = _nextReplaceIndex;
	fifth_min_index = _nextReplaceIndex;
	for (INT32 index = _tagsLastIndex; index >=0; index--)
        {
            if (_tag_last_reference_time[index] == min_access_time)
        	    _nextReplaceIndex = index;
            if (_tag_last_reference_time[index] == second_min_access_time)
        	    second_min_index = index;
            if (_tag_last_reference_time[index] == third_min_access_time)
        	    third_min_index = index;
            if (_tag_last_reference_time[index] == fourth_min_access_time)
        	    fourth_min_index = index;
            if (_tag_last_reference_time[index] == fifth_min_access_time)
        	    fifth_min_index = index;
	}
        free(arr);
        const UINT32 index = _nextReplaceIndex;
        bool replaced_block_degree_of_use = _degree_of_use[index];
        uint64_t replaced_block_address = _addr[index];
        use_and_blk_addr temp;
        temp.function_use_information = replaced_block_degree_of_use;
        //bool medium_block_degree_of_use = _medium_degree_of_use[index]; 	
         temp.blk_addr = replaced_block_address;
        temp.allocated_way = index;
        _tags[index] = tag;
        _tag_last_reference_time[index] = total_accesses;
        //insert at LRU position when the degree of use of function is 
        //low. 
        //insert at LRU-2 position if the block has a degree of use greater than 1. 
        if ((medium_degree_of_use)){
              _tag_last_reference_time[second_min_index] = 0;
              _tag_last_reference_time[third_min_index] = 1;
              _tag_last_reference_time[fourth_min_index] = 2;
              _tag_last_reference_time[fifth_min_index] = 3;
	      _tag_last_reference_time[index] = 4;
        }
        else if ((!degree_of_use))
                _tag_last_reference_time[index] = 0;
        _degree_of_use[index] = degree_of_use;
        _medium_degree_of_use[index] = medium_degree_of_use;
        _addr[index] = blk_addr;
        return temp;
    }
#endif    
};

template <UINT32 MAX_ASSOCIATIVITY = 4>
class MODIFIED_CACHE_2
{
  private:
    CACHE_TAG _tags[MAX_ASSOCIATIVITY];
    //add the last access number as the proxy for last reference time
    //to enable LRU based replacement
    uint64_t _tag_last_reference_time[MAX_ASSOCIATIVITY];
    
    //track degree of use for each block (whether it comes from a function with high use or low use)
    //default set to low use.false indicates low use, true indicates high use.  
    bool _degree_of_use[MAX_ASSOCIATIVITY];
    
    //store the address of the item also alongside, so we can retrieve it on a replacement. 
    uint64_t _addr[MAX_ASSOCIATIVITY];
    
    UINT32 _tagsLastIndex;
    UINT32 _nextReplaceIndex;

  public:
    MODIFIED_CACHE_2(UINT32 associativity = MAX_ASSOCIATIVITY)
      : _tagsLastIndex(associativity - 1)
    {
        ASSERTX(associativity <= MAX_ASSOCIATIVITY);
        _nextReplaceIndex = _tagsLastIndex;

        for (INT32 index = _tagsLastIndex; index >= 0; index--)
        {
         _tags[index] = CACHE_TAG(0);
         _tag_last_reference_time[index] = 0;
	 _degree_of_use[index] = false;
	 _addr[index] = 0;
	}
    }

    VOID SetAssociativity(UINT32 associativity)
    {
        ASSERTX(associativity <= MAX_ASSOCIATIVITY);
        _tagsLastIndex = associativity - 1;
        _nextReplaceIndex = _tagsLastIndex;
    }
    UINT32 GetAssociativity(UINT32 associativity) { return _tagsLastIndex + 1; }
    
    UINT32 Find(CACHE_TAG tag)
    {
        bool result = true;
	total_accesses++;
        
	for (INT32 index = _tagsLastIndex; index >= 0; index--)
        {
            // this is an ugly micro-optimization, but it does cause a
            // tighter assembly loop for ARM that way ...

            if(_tags[index] == tag) {
		    _tag_last_reference_time[index] = total_accesses;
		    goto end;
	    }
//		if(_tags[index] == tag) goto end;
        }
        result = false;

        end: return result;
    }

    //functions may start with a low degree of use and then progress to have a
    // high degree of use. 
    UINT32 Find_UpdateDegreeOfUse(ADDRINT addr, CACHE_TAG tag, bool degree_of_use, bool medium_degree_of_use, 
		    bool &non_mru_hit, uint64_t future_reference_timestamp,
		    bool victim_search, uint32_t setIndex, bool &is_spatial_fetched_block,
		    uint64_t current_function)
    {
        bool result = true;
	total_accesses++;
        
	for (INT32 index = _tagsLastIndex; index >= 0; index--)
        {
            // this is an ugly micro-optimization, but it does cause a
            // tighter assembly loop for ARM that way ...

            if(_tags[index] == tag) {
		    _tag_last_reference_time[index] = total_accesses;
		    //update the degree of use. 
		    _degree_of_use[index] = degree_of_use;
		    goto end;
	    }
//		if(_tags[index] == tag) goto end;
        }
        result = false;

        end: return result;
    }
    VOID Replace(CACHE_TAG tag)
    {
        // g++ -O3 too dumb to do CSE on following lines?!
      //  const UINT32 index = _nextReplaceIndex;

      //  _tags[index] = tag;
      //  // condition typically faster than modulo
      //  _nextReplaceIndex = (index == 0 ? _tagsLastIndex : index - 1);
    
        uint64_t min_access_time = _tag_last_reference_time[_tagsLastIndex];
        uint64_t _nextReplaceIndex = _tagsLastIndex;
        for (INT32 index = _tagsLastIndex; index >= 0; index--)
        {
          if(min_access_time>_tag_last_reference_time[index]){
               _nextReplaceIndex = index;
               min_access_time = _tag_last_reference_time[index];
            }
        }
	const UINT32 index = _nextReplaceIndex;
	
        _tags[index] = tag;
	_tag_last_reference_time[index] = total_accesses;
    }

    use_and_blk_addr Replace_GetDegreeOfUse(CACHE_TAG tag, bool degree_of_use,uint64_t blk_addr, bool medium_degree_of_use,
		    uint64_t future_reference_timestamp, float block_degree_of_use, bool direct_mapped,
		    bool straggler_remove,
		    uint64_t current_function, bool victim_search,
		    uint32_t setIndex, bool spatial_fetch)
    {
        // g++ -O3 too dumb to do CSE on following lines?!
      //  const UINT32 index = _nextReplaceIndex;

      //  _tags[index] = tag;
      //  // condition typically faster than modulo
      //  _nextReplaceIndex = (index == 0 ? _tagsLastIndex : index - 1);
	
	uint64_t min_access_time = _tag_last_reference_time[_tagsLastIndex];
        uint64_t _nextReplaceIndex = _tagsLastIndex;
        //way 0 is for low use functions and all other ways are for high use functions. 
	//see how this fares.
	if (degree_of_use){
		for (INT32 index = _tagsLastIndex; index > (EXTRA_WAYS_1-1); index--)
        	{
          	   if(min_access_time>_tag_last_reference_time[index]){
               	      _nextReplaceIndex = index;
               	      min_access_time = _tag_last_reference_time[index];
            	   }
        	}
	}
	else{
		min_access_time = _tag_last_reference_time[(EXTRA_WAYS_1-1)];
		_nextReplaceIndex = (EXTRA_WAYS_1-1);
		for (INT32 index = (EXTRA_WAYS_1-1); index >= 0; index--)
        	{
          	   if(min_access_time>_tag_last_reference_time[index]){
               	      _nextReplaceIndex = index;
               	      min_access_time = _tag_last_reference_time[index];
            	   }
        	}
	   // _nextReplaceIndex = 0;
	}
	if (((_nextReplaceIndex == 0)||(_nextReplaceIndex == 1)) && (!degree_of_use))
		total_misses_on_low_use_function++;
	const UINT32 index = _nextReplaceIndex;
	bool replaced_block_degree_of_use = _degree_of_use[index];
        uint64_t replaced_block_address = _addr[index];
	use_and_blk_addr temp;
	temp.function_use_information = replaced_block_degree_of_use;
	temp.blk_addr = replaced_block_address;
	temp.allocated_way = index;
	_tags[index] = tag;
	_tag_last_reference_time[index] = total_accesses;
	_degree_of_use[index] = degree_of_use;
	_addr[index] = blk_addr;
	return temp;
    }
};

} // namespace CACHE_SET

namespace CACHE_ALLOC
{
    typedef enum 
    {
        STORE_ALLOCATE,
        STORE_NO_ALLOCATE
    } STORE_ALLOCATION;
}

/*!
 *  @brief Generic cache base class; no allocate specialization, no cache set specialization
 */
class CACHE_BASE
{
  public:
    // types, constants
    typedef enum 
    {
        ACCESS_TYPE_LOAD,
        ACCESS_TYPE_STORE,
        ACCESS_TYPE_NUM
    } ACCESS_TYPE;
    typedef enum
    {
        CACHE_TYPE_ICACHE,
        CACHE_TYPE_DCACHE,
        CACHE_TYPE_NUM
    } CACHE_TYPE;

  protected:
    //add extra field for hits at non-MRU position.
    static const UINT32 HIT_MISS_NUM = 2;
    CACHE_STATS _access[ACCESS_TYPE_NUM][HIT_MISS_NUM];

  private:    // input params
    const std::string _name;
    const UINT32 _cacheSize;
    const UINT32 _lineSize;
    const UINT32 _associativity;

    // computed params
    const UINT32 _lineShift;
    const UINT32 _setIndexMask;
    const UINT32 _setShift;

    const UINT64 _warmup_interval;
    CACHE_STATS SumAccess(int hit) const
    {
        CACHE_STATS sum = 0;

        for (UINT32 accessType = 0; accessType < ACCESS_TYPE_NUM; accessType++)
        {
            sum += _access[accessType][hit];
        }

        return sum;
    }

  protected:
    UINT32 NumSets() const { return _setIndexMask + 1; }

  public:
    
    uint64_t total_accesses_made_so_far;
    // constructors/destructors
    CACHE_BASE(std::string name, UINT32 cacheSize, UINT32 lineSize, UINT32 associativity, UINT64 warmup_interval);

    // accessors
    UINT32 CacheSize() const { return _cacheSize; }
    UINT32 LineSize() const { return _lineSize; }
    UINT32 Associativity() const { return _associativity; }
    UINT64 WarmupInterval() const {return _warmup_interval; }
    //
    CACHE_STATS Hits(ACCESS_TYPE accessType) const { return _access[accessType][true];}
    CACHE_STATS Misses(ACCESS_TYPE accessType) const { return _access[accessType][false];}
    CACHE_STATS Accesses(ACCESS_TYPE accessType) const { return Hits(accessType) + Misses(accessType);}
    CACHE_STATS Hits() const { return SumAccess(true);}
    CACHE_STATS Misses() const { return SumAccess(false);}
    CACHE_STATS Accesses() const { return Hits() + Misses();}

    VOID SplitAddress(const ADDRINT addr, CACHE_TAG & tag, UINT32 & setIndex) const
    {
        tag = addr >> _lineShift;
	setIndex = tag & _setIndexMask;
	tag = tag >> _setShift;
    }


    VOID SplitAddress(const ADDRINT addr, CACHE_TAG & tag, UINT32 & setIndex, UINT32 & lineIndex) const
    {
        const UINT32 lineMask = _lineSize - 1;
        lineIndex = addr & lineMask;
        SplitAddress(addr, tag, setIndex);
    }


    VOID SpecialSplitAddress(const ADDRINT addr, CACHE_TAG & tag, UINT32 & setIndex) const
    {
        tag = addr >> _lineShift;
        setIndex = tag & _setIndexMask;
	uint64_t extra_tag_mask = tag >> _lineShift;
	uint64_t extra_index_to_xor_with = extra_tag_mask&_setIndexMask;
	setIndex = setIndex^extra_index_to_xor_with;	
    }

    VOID SpecialSplitAddress(const ADDRINT addr, CACHE_TAG & tag, UINT32 & setIndex, UINT32 & lineIndex) const
    {
        const UINT32 lineMask = _lineSize - 1;
        lineIndex = addr & lineMask;
        SpecialSplitAddress(addr, tag, setIndex);
    }
    string StatsLong(string prefix = "", CACHE_TYPE = CACHE_TYPE_DCACHE) const;
};

CACHE_BASE::CACHE_BASE(std::string name, UINT32 cacheSize, UINT32 lineSize, UINT32 associativity, UINT64 warmup_interval=0)
  : _name(name),
    _cacheSize(cacheSize),
    _lineSize(lineSize),
    _associativity(associativity),
    _lineShift(FloorLog2(lineSize)),
    _setIndexMask((cacheSize / (associativity * lineSize)) - 1),
    _setShift(FloorLog2(cacheSize/(associativity * lineSize))),
    _warmup_interval(warmup_interval)
{

    ASSERTX(IsPower2(_lineSize));
    ASSERTX(IsPower2(_setIndexMask + 1));
    total_accesses_made_so_far = 0;
    for (UINT32 accessType = 0; accessType < ACCESS_TYPE_NUM; accessType++)
    {
        _access[accessType][false] = 0;
        _access[accessType][true] = 0;
    }
}

/*!
 *  @brief Stats output method
 */

string CACHE_BASE::StatsLong(string prefix, CACHE_TYPE cache_type) const
{
    const UINT32 headerWidth = 19;
    const UINT32 numberWidth = 12;

    string out;
    
    out += prefix + _name + ":" + "\n";

    if (cache_type != CACHE_TYPE_ICACHE) {
       for (UINT32 i = 0; i < ACCESS_TYPE_NUM; i++)
       {
           const ACCESS_TYPE accessType = ACCESS_TYPE(i);

           std::string type(accessType == ACCESS_TYPE_LOAD ? "Load" : "Store");

           out += prefix + ljstr(type + "-Hits:      ", headerWidth)
                  + mydecstr(Hits(accessType), numberWidth)  +
                  "  " +fltstr(100.0 * Hits(accessType) / Accesses(accessType), 2, 6) + "%\n";

           out += prefix + ljstr(type + "-Misses:    ", headerWidth)
                  + mydecstr(Misses(accessType), numberWidth) +
                  "  " +fltstr(100.0 * Misses(accessType) / Accesses(accessType), 2, 6) + "%\n";
        
           out += prefix + ljstr(type + "-Accesses:  ", headerWidth)
                  + mydecstr(Accesses(accessType), numberWidth) +
                  "  " +fltstr(100.0 * Accesses(accessType) / Accesses(accessType), 2, 6) + "%\n";
        
           out += prefix + "\n";
       }
    }

    out += prefix + ljstr("Total-Hits:      ", headerWidth)
           + mydecstr(Hits(), numberWidth) +
           "  " +fltstr(100.0 * Hits() / Accesses(), 2, 6) + "%\n";

    out += prefix + ljstr("Total-Misses:    ", headerWidth)
           + mydecstr(Misses(), numberWidth) +
           "  " +fltstr(100.0 * Misses() / Accesses(), 2, 6) + "%\n";

    out += prefix + ljstr("Total-Accesses:  ", headerWidth)
           + mydecstr(Accesses(), numberWidth) +
           "  " +fltstr(100.0 * Accesses() / Accesses(), 2, 6) + "%\n";
    
    out += prefix + ljstr("Total-Low use misses:  ", headerWidth)
           + mydecstr(total_misses_on_low_use_function, numberWidth) +
           "%\n";

    out += prefix + ljstr("Total-hits in non-MRU position: ", headerWidth)
           + mydecstr(total_hits_on_non_mru_position, numberWidth) +
           "%\n";
    for (uint32_t i = 0;i < number_of_cache_sets; i++){
   	std::stringstream ss;
        ss << "Miss in set " << i << ": " << array_of_misses_per_set[i] << endl;
	ss << "References to set (NMRU+miss) "<< i << ": " << array_of_nmru_plus_miss_references_per_set[i] << endl;
	ss << "References to set "<< i << ": "<< array_of_references_per_set[i] << endl;
	ss << "Miss ratio in set " << i << ": " << float(array_of_misses_per_set[i])/array_of_references_per_set[i] << endl;
        ss << "Burst count 1 blocks replaced in set " << i << ": " << counter_of_burst1_blocks_replaced_per_set[i] << endl;	
	out += ss.str();
    }
    out += "\n";
    std::stringstream ss_1;

    ss_1 << "Misses for different cache blocks in the set" << endl;
    for(std::map<uint64_t,uint64_t>::iterator iter = mapping_from_cache_block_to_miss_counts.begin(); 
		    iter != mapping_from_cache_block_to_miss_counts.end(); ++iter)
    {
	ss_1 << "Misses for " << iter->first <<": " << iter->second << endl;
	ss_1 << "Accesses (nmru+misses) for " << iter->first <<": " << 
		mapping_from_cache_block_to_access_nmru_plus_miss_counts[iter->first] << endl;
	ss_1 << "Accesses (total) for " << iter->first <<": " << 
		mapping_from_cache_block_to_total_access_counts[iter->first] << endl;
//	ss_1 <<"Functions corresponding to cache blocks " << endl << "(";
//	for (std::set<uint64_t>::iterator iter1 = mapping_from_cache_block_to_corresponding_function[iter->first].begin();
//			iter1 != mapping_from_cache_block_to_corresponding_function[iter->first].end();
//			++iter1){
//		ss_1 << *(iter1) << ",";
//	}
	ss_1 << endl;
    }



//uncomment start here
    ss_1 << "Misses for different functions in the set" << endl;
    for(std::map<uint64_t,uint64_t>::iterator iter = mapping_from_function_to_miss_counts.begin(); 
		    iter != mapping_from_function_to_miss_counts.end(); ++iter)
    {
	ss_1 << "Misses for " << iter->first <<": " << iter->second << endl;
	//uint64_t total_misses_exp_by_function_in_set = iter->second;
	ss_1 << "Accesses (nmru+misses) for " << iter->first <<": " << 
		mapping_from_function_to_access_nmru_plus_miss_counts[iter->first] << endl;
	ss_1 << "Accesses (total) for " << iter->first <<": " << 
		mapping_from_function_to_total_access_counts[iter->first] << endl;
	ss_1 << "Function invocation counts (total) for " << iter->first <<": " << 
		mapping_from_function_to_total_invocation_count[iter->first] << endl;
	
//	uint64_t total_intermediate_function_accesses =	mapping_from_function_to_invocation_statistics[iter->first].intermediate_function_invocations;
//	uint64_t total_function_invocations = mapping_from_function_to_invocation_statistics[iter->first].total_function_invocations;
//	float average_reuse_distance = (float)total_intermediate_function_accesses/total_function_invocations;
//	ss_1 << "Average reuse distance of the function is " << average_reuse_distance << endl; 
//	ss_1 << "Temporal average reuse distances across periodic 1000 function invocations is " << endl << "(";
//	for (vector<float>::iterator iter1 = mapping_from_function_to_reuse_distances_over_1000_function_invocations[iter->first].begin();
//                    iter1 != mapping_from_function_to_reuse_distances_over_1000_function_invocations[iter->first].end(); ++iter1){
//       		ss_1 << *(iter1) << ",";
//	}
//	ss_1 << ")" << endl;
//
//	ss_1 <<"Previous accessed function (which was an NMRU hit or a miss) before the current function (most of the time): " << endl;
//        for(std::map<uint64_t,uint64_t>::iterator iter1 = mapping_from_function_to_functions_evicted_to_make_room[iter->first].begin(); 
//		    iter1 != mapping_from_function_to_functions_evicted_to_make_room[iter->first].end(); ++iter1){
//		    uint64_t total_evictions_caused_by_function_of_interest = mapping_from_function_to_functions_evicted_to_make_room[iter->first][iter1->first];
//	//print if the function accounts for atleast 1000 of the eviction of this function
//        if(total_evictions_caused_by_function_of_interest>=500)	
//		ss_1 << "("<<iter1->first <<","<<mapping_from_function_to_functions_evicted_to_make_room[iter->first][iter1->first] <<"),";
//	}

	ss_1 << endl;

    }

//uncomment stop here
    out += ss_1.str();
    out += "\n";
    return out;
}


/*!
 *  @brief Templated cache class with specific cache set allocation policies
 *
 *  All that remains to be done here is allocate and deallocate the right
 *  type of cache sets.
 */
template <class SET, UINT32 MAX_SETS, UINT32 STORE_ALLOCATION>
class CACHE : public CACHE_BASE
{
  private:
    SET _sets[MAX_SETS];
    
  public:
    // constructors/destructors
    CACHE(std::string name, UINT32 cacheSize, UINT32 lineSize, UINT32 associativity, UINT64 warmup_interval)
      : CACHE_BASE(name, cacheSize, lineSize, associativity, warmup_interval)
    {
        ASSERTX(NumSets() <= MAX_SETS);
	number_of_cache_sets = NumSets();
        for (UINT32 i = 0; i < NumSets(); i++)
        {
            _sets[i].SetAssociativity(associativity);
	    //initialize the misses for each cache set to 0.
	    array_of_misses_per_set[i] = 0;
        }
    }

    // modifiers
    /// Cache access from addr to addr+size-1
    bool Access(ADDRINT addr, UINT32 size, ACCESS_TYPE accessType);
    //smurthy
    //selectively allocate a line based on a allocate condition
    hit_and_use_information Access_selective_allocate(ADDRINT addr, UINT32 size, ACCESS_TYPE accessType, bool allocate, bool degree_of_use,bool medium_degree_of_use, bool special_cache_type, uint64_t future_reference_timestamp, bool warmup_finished, float block_degree_of_use,uint64_t current_function, uint64_t current_function_invocation, bool spatial_fetch);
    /// Cache access at addr that does not span cache lines
    bool AccessSingleLine(ADDRINT addr, ACCESS_TYPE accessType);
    //smurthy
    //selectively allocate a line based on a allocate condition
    hit_and_use_information AccessSingleLine_selective_allocate(ADDRINT addr, ACCESS_TYPE accessType, bool allocate, bool degree_of_use, bool medium_degree_of_use, bool special_cache_type, uint64_t future_reference_timestamp, bool warmup_finished, float block_degree_of_use, uint64_t current_function, uint64_t current_function_invocation, bool spatial_fetch);
};

/*!
 *  @return true if all accessed cache lines hit
 */

template <class SET, UINT32 MAX_SETS, UINT32 STORE_ALLOCATION>
bool CACHE<SET,MAX_SETS,STORE_ALLOCATION>::Access(ADDRINT addr, UINT32 size, ACCESS_TYPE accessType)
{
    const ADDRINT highAddr = addr + size;
    bool allHit = true;

    const ADDRINT lineSize = LineSize();
    const ADDRINT notLineMask = ~(lineSize - 1);
    do
    {
        CACHE_TAG tag;
        UINT32 setIndex;

        SplitAddress(addr, tag, setIndex);

        SET & set = _sets[setIndex];

        bool localHit = set.Find(tag);
        allHit &= localHit;

        // on miss, loads always allocate, stores optionally
        if ( (! localHit) && (accessType == ACCESS_TYPE_LOAD || STORE_ALLOCATION == CACHE_ALLOC::STORE_ALLOCATE))
        {
            set.Replace(tag);
        }

        addr = (addr & notLineMask) + lineSize; // start of next cache line
    }
    while (addr < highAddr);
    total_accesses_made_so_far++;
    //count accesses only after warmup period.
    if (total_accesses_made_so_far > WarmupInterval()) 
   	 _access[accessType][allHit]++;

    return allHit;
}


template <class SET, UINT32 MAX_SETS, UINT32 STORE_ALLOCATION>
hit_and_use_information CACHE<SET,MAX_SETS,STORE_ALLOCATION>::Access_selective_allocate(ADDRINT addr, UINT32 size, ACCESS_TYPE accessType, bool selective_allocate, bool degree_of_use, bool medium_degree_of_use,  bool special_cache_type, uint64_t future_reference_timestamp, bool _warmup_finished, float block_degree_of_use, uint64_t current_function, uint64_t current_function_invocation_count, bool spatial_fetch)
{
    const ADDRINT highAddr = addr + size;
    bool allHit = true;

    //non-mru-hit
    bool all_non_mru_hit = false;
    //all-spatial-fetch-hit
    bool all_spatial_fetch_hit = false;

    //clear the bitset if not done already. 
    if (!bitset_cleared){
        bitset_cleared = true;	
	hotness_of_sets.reset();
    }

    const ADDRINT lineSize = LineSize();
    const ADDRINT notLineMask = ~(lineSize - 1);
    hit_and_use_information temp;
    temp.icache_hit = false;
    temp.function_use_information = false;
    temp.icache_nmru_hit = false;

    use_and_blk_addr temp1;
    do
    {
        CACHE_TAG tag;
        UINT32 setIndex;


        //SET & set;
       //	= _sets[setIndex];
        //bool localHit;
       //	= set.Find_UpdateDegreeOfUse(tag, degree_of_use);
        //search for low use function in the alternate location too, in case it is not found in the 
	//original location. Here on we allocate this block only in the alternate location. 
	if ((!degree_of_use) && (special_cache_type)){
        	SplitAddress(addr, tag, setIndex);
	}
	else{
        	SplitAddress(addr, tag, setIndex);
	}
	
        SET &set = _sets[setIndex];
	bool non_mru_hit = false;
        bool spatial_fetch_hit = false; 
        bool victim_search = false;
	//if (setIndex == SET_OF_INTEREST)
		victim_search = true;
	bool localHit = set.Find_UpdateDegreeOfUse(addr, tag, degree_of_use, medium_degree_of_use,
			non_mru_hit, future_reference_timestamp, victim_search, setIndex,
			spatial_fetch_hit, current_function);
	if (setIndex == SET_OF_INTEREST){
		if (mapping_from_function_to_current_invocation_count.find(current_function) == mapping_from_function_to_current_invocation_count.end()){
			mapping_from_function_to_current_invocation_count[current_function] = 0;
			//clear this structure when this function is invoked.
		        mapping_from_function_to_other_functions_accessed_till_next_invocation[current_function].clear();
			mapping_from_function_to_invocation_statistics[current_function].intermediate_function_invocations = 0;
		 	mapping_from_function_to_invocation_statistics[current_function].total_function_invocations = 0;	
		}
		if (mapping_from_function_to_current_invocation_count[current_function] != current_function_invocation_count){
			mapping_from_function_to_total_invocation_count[current_function]++;
			mapping_from_function_to_current_invocation_count[current_function] = current_function_invocation_count;
			//add the current function to the map of all other functions in the data structure, to correct tap the reuse distance. 

			//ignore the misses coming due to spatial fetch of a function
			if (!localHit && (!spatial_fetch)){
				mapping_from_function_to_invocation_statistics[current_function].intermediate_function_invocations += mapping_from_function_to_other_functions_accessed_till_next_invocation[current_function].size();
				mapping_from_function_to_invocation_statistics[current_function].total_function_invocations += 1;
			}
			mapping_from_function_to_other_functions_accessed_till_next_invocation[current_function].clear();	
		}
	}	
	if (current_cache_block_in_use != (addr&notLineMask)){
	      //want to count only misses and non-MRU hits 
	      //as references.
	       if (non_mru_hit || (!localHit)){
		   array_of_nmru_plus_miss_references_per_set[setIndex]++;
		 //  if ((setIndex == SET_OF_INTEREST)){
			   //note the invocation of the function. 
		           uint64_t blk_addr = addr&notLineMask;
			   mapping_from_cache_block_to_corresponding_function[blk_addr].insert(current_function);
		           if (!localHit)
			   	accesses_to_set_56.push_back(current_function);
			   if (mapping_from_cache_block_to_miss_counts.find(blk_addr) == 
					   mapping_from_cache_block_to_miss_counts.end()){
			   	mapping_from_cache_block_to_miss_counts[blk_addr] = 0;
			   }
			   if (mapping_from_cache_block_to_total_access_counts.find(blk_addr) == 
					   mapping_from_cache_block_to_total_access_counts.end()){
			   	mapping_from_cache_block_to_total_access_counts[blk_addr] = 0;
			   }
			   if (mapping_from_cache_block_to_access_nmru_plus_miss_counts.find(blk_addr) == 
					   mapping_from_cache_block_to_access_nmru_plus_miss_counts.end()){
			   	mapping_from_cache_block_to_access_nmru_plus_miss_counts[blk_addr] = 0;
			   }
			   if (mapping_from_function_to_miss_counts.find(current_function) == 
					   mapping_from_function_to_miss_counts.end()){
			   	mapping_from_function_to_miss_counts[current_function] = 0;
			   }
			   
			   if (mapping_from_function_to_access_nmru_plus_miss_counts.find(current_function) == 
					   mapping_from_function_to_access_nmru_plus_miss_counts.end()){
			   	mapping_from_function_to_access_nmru_plus_miss_counts[current_function] = 0;
			   }
			   if (mapping_from_function_to_total_access_counts.find(current_function) == 
					   mapping_from_function_to_total_access_counts.end()){
			   	mapping_from_function_to_total_access_counts[current_function] = 0;
			   }
			   mapping_from_cache_block_to_degree_of_use[blk_addr] = degree_of_use;
			   //ignore the misses coming due to spatial fetch of a function
			   if (!localHit && (!spatial_fetch)){
				   mapping_from_cache_block_to_miss_counts[blk_addr]++;
			   	   mapping_from_function_to_miss_counts[current_function]++;
			   }
			   mapping_from_cache_block_to_access_nmru_plus_miss_counts[blk_addr]++;
		  	   mapping_from_function_to_access_nmru_plus_miss_counts[current_function]++;
			   
			  // block_degree_of_use_block.push_back(block_degree_of_use); 
		   //}
		   counter_of_references_per_set[setIndex]++;
	       }
	}

	array_of_references_per_set[setIndex]++;
        uint64_t blk_addr = addr&notLineMask;
	if (setIndex == SET_OF_INTEREST){	
		mapping_from_cache_block_to_total_access_counts[blk_addr]++;
		mapping_from_function_to_total_access_counts[current_function]++;
	}
	
	current_cache_block_in_use = addr&notLineMask;

	//ignore the misses coming due to spatial fetch of a function
	if (!localHit && (!spatial_fetch)){
		counter_of_misses_per_set[setIndex]++;
		array_of_misses_per_set[setIndex]++;
	}
	//compute set block reference hit ratio.
	uint64_t misses_exp_by_set = counter_of_misses_per_set[setIndex];
	uint64_t references_to_set = counter_of_references_per_set[setIndex];
	float miss_ratio = (float)misses_exp_by_set/references_to_set;
	if ((miss_ratio >= 0.5)&&
		(references_to_set>=50)&&
			(!direct_mapped_setting_per_set[setIndex])){
		direct_mapped_setting_per_set[setIndex] = true;
	}
        bool direct_mapped_bit = direct_mapped_setting_per_set[setIndex];		
	//direct_mapped_bit = false;
	allHit &= localHit;
	all_non_mru_hit |= non_mru_hit;
	all_spatial_fetch_hit |= spatial_fetch_hit;
        // on miss, loads always allocate, stores optionally
        
	
	if ((selective_allocate) && (!localHit) && (accessType == ACCESS_TYPE_LOAD || STORE_ALLOCATION == CACHE_ALLOC::STORE_ALLOCATE))
        {
	     temp1 = set.Replace_GetDegreeOfUse(tag, degree_of_use,addr&notLineMask, medium_degree_of_use, 
			   future_reference_timestamp, block_degree_of_use, direct_mapped_bit, false, current_function,
			   victim_search, setIndex, spatial_fetch);
	     temp.function_use_information |=temp1.function_use_information;
	     temp.blk_addresses.push_back(temp1.blk_addr);
	    if (mapping_from_cache_block_to_burst_count.find(temp1.blk_addr) ==
			  mapping_from_cache_block_to_burst_count.end())
		mapping_from_cache_block_to_burst_count[temp1.blk_addr] = 0;
	    if (mapping_from_cache_block_to_burst_count.find(blk_addr) ==
			  mapping_from_cache_block_to_burst_count.end())
		mapping_from_cache_block_to_burst_count[blk_addr] = 0;
	   // if (setIndex == SET_OF_INTEREST){
	   //   cout <<"Replacing block "<<temp1.blk_addr <<" with a burst count of " << 
	   //           temp1.burst_count_of_replaced_block << endl;
	   //   cout << "Inserting block " << blk_addr << " with a burst count of " <<
	   //           mapping_from_cache_block_to_burst_count[blk_addr]  << " line mask is " <<  
	   //           notLineMask << endl;
	   // }
	    mapping_from_cache_block_to_burst_count[temp1.blk_addr] = temp1.burst_count_of_replaced_block;
	    temp.allocated_way = temp1.allocated_way;
	    temp.burst_count_of_missed_block = mapping_from_cache_block_to_burst_count[blk_addr];
        }
	if (references_to_set>=500){
		counter_of_references_per_set[setIndex] = 0;
		counter_of_misses_per_set[setIndex] = 0;
		counter_of_burst1_blocks_replaced_per_set[setIndex] = 0;
		direct_mapped_setting_per_set[setIndex] = false;
	}
        addr = (addr & notLineMask) + lineSize; // start of next cache line
    }
    while (addr < highAddr);
    warmup_finished = _warmup_finished;
    //need to change this soon
    total_accesses_made_so_far = future_reference_timestamp;
    //count accesses only after warmup period
    //count only accesses that are not coming from spatial fetches for a function.
    if (warmup_finished && (!spatial_fetch)) 
   	 _access[accessType][allHit]++;
    
    temp.icache_hit = allHit;
    temp.spatial_fetch_hit = all_spatial_fetch_hit;
    temp.total_low_use_misses = total_misses_on_low_use_function;
    if (all_non_mru_hit){
	total_hits_on_non_mru_position++;
    	temp.icache_nmru_hit = true;
    }	
    return temp;
}

/*!
 *  @return true if accessed cache line hits
 */
template <class SET, UINT32 MAX_SETS, UINT32 STORE_ALLOCATION>
bool CACHE<SET,MAX_SETS,STORE_ALLOCATION>::AccessSingleLine(ADDRINT addr, ACCESS_TYPE accessType)
{
    CACHE_TAG tag;
    UINT32 setIndex;

    SplitAddress(addr, tag, setIndex);

    SET & set = _sets[setIndex];

    bool hit = set.Find(tag);

    // on miss, loads always allocate, stores optionally
    if ( (! hit) && (accessType == ACCESS_TYPE_LOAD || STORE_ALLOCATION == CACHE_ALLOC::STORE_ALLOCATE))
    {
        set.Replace(tag);
    }

    total_accesses_made_so_far++;
    //count accesses only after warmup period.
    if (total_accesses_made_so_far > WarmupInterval()) 
   	 _access[accessType][hit]++;

    return hit;
}


template <class SET, UINT32 MAX_SETS, UINT32 STORE_ALLOCATION>
hit_and_use_information CACHE<SET,MAX_SETS,STORE_ALLOCATION>::AccessSingleLine_selective_allocate(ADDRINT addr, ACCESS_TYPE accessType, bool selective_allocate, 
						bool degree_of_use, bool medium_degree_of_use,  bool special_cache_type, 
						uint64_t future_reference_timestamp, bool _warmup_finished,
						float block_degree_of_use,
						uint64_t current_function, uint64_t current_function_invocation_count, bool spatial_fetch)
{
    CACHE_TAG tag;
    UINT32 setIndex;

    //SET & set; 
    const ADDRINT lineSize = LineSize();
    const ADDRINT notLineMask = ~(lineSize - 1);
    //bool hit;
   // = set.Find_UpdateDegreeOfUse(tag, degree_of_use);
    //search for low use function in the alternate location too, in case it is not found in the 
    //original location. Here on we allocate this block only in the alternate location. 
    if ((!degree_of_use) && (special_cache_type)){
    	SplitAddress(addr, tag, setIndex);
    }
    else {
    	SplitAddress(addr, tag, setIndex);
    }
    //clear the bitset if not done already. 
    if (!bitset_cleared){
	bitset_cleared = true;
    	hotness_of_sets.reset();
    }
    SET &set = _sets[setIndex];
    bool non_mru_hit = false;
    bool victim_search = false;
    //if (setIndex == SET_OF_INTEREST)
	victim_search = true;
    
    bool is_spatial_fetch_block = false;	
    bool hit = set.Find_UpdateDegreeOfUse(addr, tag, degree_of_use, medium_degree_of_use,
		    non_mru_hit, future_reference_timestamp, victim_search, setIndex, is_spatial_fetch_block,
		    current_function);
 
    if (setIndex == SET_OF_INTEREST){

	if (mapping_from_function_to_current_invocation_count.find(current_function) == mapping_from_function_to_current_invocation_count.end()){
		mapping_from_function_to_current_invocation_count[current_function] = 0;
		//clear this structure when this function is invoked.
	        mapping_from_function_to_other_functions_accessed_till_next_invocation[current_function].clear();
		mapping_from_function_to_invocation_statistics[current_function].intermediate_function_invocations = 0;
	 	mapping_from_function_to_invocation_statistics[current_function].total_function_invocations = 0;	
	}
	if (mapping_from_function_to_current_invocation_count[current_function] != current_function_invocation_count){
		mapping_from_function_to_total_invocation_count[current_function]++;
		mapping_from_function_to_current_invocation_count[current_function] = current_function_invocation_count;
		//add the current function to the map of all other functions in the data structure, to correct tap the reuse distance. 

		//update these counts on a miss. 
		//ignore the misses coming due to spatial fetch of a function
		if (!hit && (!spatial_fetch)){
			mapping_from_function_to_invocation_statistics[current_function].intermediate_function_invocations += mapping_from_function_to_other_functions_accessed_till_next_invocation[current_function].size();
			mapping_from_function_to_invocation_statistics[current_function].total_function_invocations += 1;
		}
	
	}
	    
    }
    if (current_cache_block_in_use != (addr&notLineMask)){
       //want to count only misses and non-MRU hits 
       //as references.
       
	if ((non_mru_hit) || (!hit)){
	   array_of_nmru_plus_miss_references_per_set[setIndex]++;
	   
//	if ((setIndex == SET_OF_INTEREST)){
//		if (mapping_from_function_to_functions_evicted_to_make_room[current_function].find(previous_accessed_function) 
//				== mapping_from_function_to_functions_evicted_to_make_room[current_function].end())
//			mapping_from_function_to_functions_evicted_to_make_room[current_function][previous_accessed_function] = 0;
//		else
//			mapping_from_function_to_functions_evicted_to_make_room[current_function][previous_accessed_function]++;
//		previous_accessed_function = current_function;
//	}  
	   
	//   if ((setIndex == SET_OF_INTEREST)){
		uint64_t blk_addr = addr&notLineMask;
		mapping_from_cache_block_to_corresponding_function[blk_addr].insert(current_function);
		if (mapping_from_cache_block_to_miss_counts.find(blk_addr) == 
					   mapping_from_cache_block_to_miss_counts.end()){
			mapping_from_cache_block_to_miss_counts[blk_addr] = 0;
		}
		if (mapping_from_cache_block_to_total_access_counts.find(blk_addr) == 
				mapping_from_cache_block_to_total_access_counts.end()){
			  mapping_from_cache_block_to_total_access_counts[blk_addr] = 0;
		}
		if (mapping_from_cache_block_to_access_nmru_plus_miss_counts.find(blk_addr) == 
				mapping_from_cache_block_to_access_nmru_plus_miss_counts.end()){
			  mapping_from_cache_block_to_access_nmru_plus_miss_counts[blk_addr] = 0;
		}
		if (mapping_from_function_to_miss_counts.find(current_function) == 
				mapping_from_function_to_miss_counts.end()){
			   mapping_from_function_to_miss_counts[current_function] = 0;
		}
		
		if (mapping_from_function_to_access_nmru_plus_miss_counts.find(current_function) == 
			mapping_from_function_to_access_nmru_plus_miss_counts.end()){
			  mapping_from_function_to_access_nmru_plus_miss_counts[current_function] = 0;
		}
		if (mapping_from_function_to_total_access_counts.find(current_function) == 
				mapping_from_function_to_total_access_counts.end()){
			  mapping_from_function_to_total_access_counts[current_function] = 0;
		}
	//	if (non_mru_hit)
	//	    hit_miss_accesses_to_set_56.push_back(true);
	//	else
	//	    hit_miss_accesses_to_set_56.push_back(false);
	//	if (degree_of_use)
	//	    degree_of_use_block.push_back(true);
	//	else
	//	    degree_of_use_block.push_back(false);
		//ignore the misses coming due to spatial fetch of a function
		if (!hit && (!spatial_fetch)){
		    mapping_from_cache_block_to_miss_counts[blk_addr]++;
		    mapping_from_function_to_miss_counts[current_function]++;
	        }
		mapping_from_cache_block_to_access_nmru_plus_miss_counts[blk_addr]++;
		mapping_from_function_to_access_nmru_plus_miss_counts[current_function]++;
		mapping_from_cache_block_to_degree_of_use[blk_addr] = degree_of_use;	
	//	block_degree_of_use_block.push_back(block_degree_of_use); 
	   //}
	   counter_of_references_per_set[setIndex]++;
       }
     }

    array_of_references_per_set[setIndex]++;
    uint64_t blk_addr = addr&notLineMask;
    if (setIndex == SET_OF_INTEREST){
    	mapping_from_cache_block_to_total_access_counts[blk_addr]++;
    	mapping_from_function_to_total_access_counts[current_function]++;
    }
    current_cache_block_in_use = addr&notLineMask;
    //maintain a distribution of misses across sets.
    //maintain a distribution of misses across sets. 
    //ignore the misses coming due to spatial fetch of a function
    if (!hit && (!spatial_fetch)){
	 counter_of_misses_per_set[setIndex]++;
	 array_of_misses_per_set[setIndex]++;
    }
    
    //compute set block reference hit ratio.
    uint64_t misses_exp_by_set = counter_of_misses_per_set[setIndex];
    uint64_t references_to_set = counter_of_references_per_set[setIndex];
    float miss_ratio = (float)misses_exp_by_set/references_to_set;
    if ((miss_ratio >= 0.5)&&
		(references_to_set>=50)&&
			(!direct_mapped_setting_per_set[setIndex])){
		direct_mapped_setting_per_set[setIndex] = true;
    }
    
    bool direct_mapped_bit = direct_mapped_setting_per_set[setIndex];		
    //direct_mapped_bit = false;
    hit_and_use_information temp;
    use_and_blk_addr temp1;
    temp.icache_hit = hit;
    temp.spatial_fetch_hit = is_spatial_fetch_block; 
    //by default return low use
    temp.function_use_information = false;
    temp.icache_nmru_hit = false;
    // on miss, loads always allocate, stores optionally
    if ((selective_allocate)&& (! hit) && (accessType == ACCESS_TYPE_LOAD || STORE_ALLOCATION == CACHE_ALLOC::STORE_ALLOCATE))
    {
	temp1 = set.Replace_GetDegreeOfUse(tag, degree_of_use,addr&notLineMask, medium_degree_of_use,
			future_reference_timestamp, block_degree_of_use,direct_mapped_bit, false, current_function,
			victim_search, setIndex, spatial_fetch);
	temp.function_use_information = temp1.function_use_information;
	
	temp.blk_addresses.push_back(temp1.blk_addr);
	
	if (mapping_from_cache_block_to_burst_count.find(temp1.blk_addr) ==
			mapping_from_cache_block_to_burst_count.end())
		mapping_from_cache_block_to_burst_count[temp1.blk_addr] = 0;
	if (mapping_from_cache_block_to_burst_count.find(blk_addr) ==
			  mapping_from_cache_block_to_burst_count.end())
		mapping_from_cache_block_to_burst_count[blk_addr] = 0;
	
//	if (setIndex == SET_OF_INTEREST){
//	      cout <<"Replacing block "<<temp1.blk_addr <<" with a burst count of " << 
//		      temp1.burst_count_of_replaced_block << endl;
//	      cout << "Inserting block " << blk_addr << " with a burst count of " <<
//		      mapping_from_cache_block_to_burst_count[blk_addr]  << " line mask is "<< 
//		      notLineMask << endl;
//	}
	mapping_from_cache_block_to_burst_count[temp1.blk_addr] = temp1.burst_count_of_replaced_block;
	temp.allocated_way = temp1.allocated_way;
	temp.burst_count_of_missed_block = mapping_from_cache_block_to_burst_count[blk_addr];
    }
    if (references_to_set>=500){
	 counter_of_references_per_set[setIndex] = 0;
	 counter_of_misses_per_set[setIndex] = 0;
	 counter_of_burst1_blocks_replaced_per_set[setIndex] = 0;
//	 direct_mapped_setting_per_set[setIndex] = false;

    }
    warmup_finished = _warmup_finished;
    total_accesses_made_so_far = future_reference_timestamp;
    //count accesses only after warmup period.
    //count only accesses that are not coming from spatial fetches for a function.
    if (warmup_finished && (!spatial_fetch)) 
   	 _access[accessType][hit]++;
    if (non_mru_hit){
	total_hits_on_non_mru_position++;	
    	temp.icache_nmru_hit = true;
    }
    temp.total_low_use_misses = total_misses_on_low_use_function;
    return temp;
}


// define shortcuts
#define CACHE_DIRECT_MAPPED(MAX_SETS, ALLOCATION) CACHE<CACHE_SET::DIRECT_MAPPED, MAX_SETS, ALLOCATION>
#define CACHE_ROUND_ROBIN(MAX_SETS, MAX_ASSOCIATIVITY, ALLOCATION) CACHE<CACHE_SET::ROUND_ROBIN<MAX_ASSOCIATIVITY>, MAX_SETS, ALLOCATION>
#define CACHE_ROUND_ROBIN_PLUS_DIRECT_MAPPED(MAX_SETS, MAX_ASSOCIATIVITY, ALLOCATION) CACHE<CACHE_SET::ROUND_ROBIN_PLUS_DIRECT_MAPPED<MAX_ASSOCIATIVITY>, MAX_SETS, ALLOCATION>
#define CACHE_ROUND_ROBIN_PLUS_SELECTIVE_VICTIM(MAX_SETS, MAX_ASSOCIATIVITY, ALLOCATION) CACHE<CACHE_SET::ROUND_ROBIN_PLUS_SELECTIVE_VICTIM<MAX_ASSOCIATIVITY>, MAX_SETS, ALLOCATION>
#define CACHE_BELADY_OPT(MAX_SETS, MAX_ASSOCIATIVITY, ALLOCATION) CACHE<CACHE_SET::CACHE_BELADY_OPT<MAX_ASSOCIATIVITY>, MAX_SETS, ALLOCATION>
#define CACHE_MODIFIED_CACHE(MAX_SETS, MAX_ASSOCIATIVITY, ALLOCATION) CACHE<CACHE_SET::MODIFIED_CACHE<MAX_ASSOCIATIVITY>, MAX_SETS, ALLOCATION>
#define CACHE_BURST_BASED_CACHE(MAX_SETS, MAX_ASSOCIATIVITY, ALLOCATION) CACHE<CACHE_SET::BURST_BASED_CACHE<MAX_ASSOCIATIVITY>, MAX_SETS, ALLOCATION>
#define CACHE_BURST_BASED_CACHE_CONFIDENCE(MAX_SETS, MAX_ASSOCIATIVITY, ALLOCATION) CACHE<CACHE_SET::BURST_BASED_CACHE_CONFIDENCE<MAX_ASSOCIATIVITY>, MAX_SETS, ALLOCATION>
#define CACHE_MODIFIED_CACHE_2(MAX_SETS, MAX_ASSOCIATIVITY, ALLOCATION) CACHE<CACHE_SET::MODIFIED_CACHE_2<MAX_ASSOCIATIVITY>, MAX_SETS, ALLOCATION>
#endif // PIN_CACHE_H
