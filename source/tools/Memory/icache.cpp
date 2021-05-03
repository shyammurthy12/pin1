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
 *  This file contains an ISA-portable cache simulator
 *  instruction cache hierarchies
 */


#include "pin.H"

#include <iostream>
#include <fstream>
#include <cassert>
#include<string>
#include <stack>

#include "cache.H"
#include "pin_profile.H"


/* ===================================================================== */
/* Commandline Switches */
/* ===================================================================== */

using std::cerr;
using std::string;
using std::endl;
using namespace std;
//#define _THREADID 0
//file preprocessing a little different
//for SPEC versus ARM traces
//#define SPEC_APPS
#define NON_SPEC_APPS

#define SPATIAL_REUSE_STUDY
#define DEGREE_OF_USE 2.0
#define MEDIUM_DEGREE_OF_USE 1.5
#define MISS_PER_FUNCTION_THRESHOLD 30.0
//#define ACTIVE_LOW_FUNCTION_LOGGING 0
#define PERLBENCH_DEBUG 0
#define BURST_PREDICTION_CONFIDENCE 0.9
struct function_details{
	uint64_t cumulative_distance_counts;
	uint64_t cumulative_invocations;
	//un-note a function upon reaching origin. 
	bool function_noted;
};
//paritcular function acts as the origin.
map<uint64_t, function_details> distance_of_all_functions_from_a_particular_function;
#define ORIGIN_FUNCTION 2151475968

#define FUNCTION_OF_INTEREST 2148199424
uint64_t warmup_interval = 0; 


//to track when we move from one function to another in the cache set.
uint64_t current_function_in_set_of_interest = 0;

uint64_t count_seen_so_far = 0;

uint64_t total_instruction_count = 0;
/* ===================================================================== */

//datastructure for the duration that a cache block is present in the cache. 
//which notes that different double words we have accessed in the cache.
map<uint64_t, uint64_t> amount_of_spatial_reuse_for_block;

//this datastructure counts how many blocks had used either 1/4, 1/2 or 3/4 or
//more of a cache block
map<uint64_t, uint64_t> count_of_varying_spatial_reuses;

//count misses from particular pages of a specific function of interest
map<uint64_t, uint64_t> misses_experienced_by_particular_pages_in_function;

VOID SplitAddress(const ADDRINT addr, CACHE_TAG & tag, UINT32 & setIndex) 
{
        
	tag = addr >> 6;
        setIndex = tag & 127;
        tag = tag >> 7;
}




//print the first 2000 function calls. 
uint64_t function_call_seen_so_far = 0;

uint64_t countSetBits(uint64_t n)
{
    uint64_t count = 0;
    while (n) {
        count += n & 1;
        n >>= 1;
    }
    return count;
}

uint64_t total_misses = 0;
uint64_t count_misses_from_low_degree_functions = 0;
uint64_t count_misses_from_high_degree_functions = 0;

uint64_t count_misses_from_low_degree_cache_blocks_modified_cache = 0;
uint64_t count_misses_from_low_degree_cache_blocks_normal_cache = 0;
uint64_t count_misses_from_high_degree_cache_blocks_modified_cache = 0;
uint64_t count_misses_from_high_degree_cache_blocks_normal_cache = 0;

uint64_t count_missses_from_low_degree_functions_after_call = 0;

uint64_t count_misses_from_medium_degree_functions = 0;

uint64_t count_misses_from_low_degree_functions_normal_cache = 0;

uint64_t count_missses_from_low_degree_functions_normal_cache_after_call = 0;

uint64_t count_misses_from_high_degree_functions_normal_cache = 0;
uint64_t count_misses_from_medium_degree_functions_normal_cache = 0;

uint64_t count_of_blocks_displaced_from_high_use_functions_by_low_use_one_functions = 0;
uint64_t count_of_blocks_displaced_from_high_use_functions_by_low_use_one_functions_in_cascade = 0;
uint64_t count_of_blocks_displaced_from_high_use_functions_by_low_use_two_functions = 0;
uint64_t count_of_blocks_displaced_from_high_use_functions_by_high_use_functions = 0;
uint64_t count_of_low_use_displacing_low_use_functions = 0;
uint64_t count_of_low_use_allocated_way0 = 0;
uint64_t total_misses_on_low_use_functions = 0;
set<uint64_t> functions_with_low_use;
set<uint64_t> total_functions;
set<uint64_t> unique_cache_blocks_accessed_by_program;

bool done = false;
bool enable_instrumentation = true;

uint64_t count_of_instructions = 0;

KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE,    "pintool",
    "o", "icache_32k_fg_http_deg2_lru_position_victim8entry.out", "specify icache file name");

KNOB<string> KnobInputInstructionTraceFile(KNOB_MODE_WRITEONCE,    "pintool",
    "in", "/nobackup/fall2020/renaissance/itrace_fg_http.out", "specify instr trace file name");
KNOB<BOOL>   KnobTrackInsts(KNOB_MODE_WRITEONCE,    "pintool",
    "ti", "0", "track individual instructions -- increases profiling time");
KNOB<UINT32> KnobThresholdHit(KNOB_MODE_WRITEONCE , "pintool",
    "rh", "100", "only report ops with hit count above threshold");
KNOB<UINT32> KnobThresholdMiss(KNOB_MODE_WRITEONCE, "pintool",
    "rm","100", "only report ops with miss count above threshold");
KNOB<UINT32> KnobCacheSize(KNOB_MODE_WRITEONCE, "pintool",
    "c","16", "cache size in kilobytes");
KNOB<UINT32> KnobLineSize(KNOB_MODE_WRITEONCE, "pintool",
    "b","64", "cache block size in bytes");
KNOB<UINT32> KnobAssociativity(KNOB_MODE_WRITEONCE, "pintool",
    "a","10", "cache associativity (1 for direct mapped)");

KNOB<UINT32> KnobITLBSize(KNOB_MODE_WRITEONCE, "pintool",
    "ci","16", "cache size in kilobytes");
KNOB<UINT32> KnobITLBLineSize(KNOB_MODE_WRITEONCE, "pintool",
    "bi","64", "cache block size in bytes");
KNOB<UINT32> KnobITLBAssociativity(KNOB_MODE_WRITEONCE, "pintool",
                "ai","128", "cache associativity (1 for direct mapped)");

//KNOB<UINT32> KnobCache2Size(KNOB_MODE_WRITEONCE, "pintool",
//    "c","16", "cache size in kilobytes");
//KNOB<UINT32> KnobLine2Size(KNOB_MODE_WRITEONCE, "pintool",
//    "b","64", "cache block size in bytes");
//KNOB<UINT32> Knob2Associativity(KNOB_MODE_WRITEONCE, "pintool",
//    "a","8", "cache associativity (1 for direct mapped)");
//
//KNOB<UINT32> KnobITLB2Size(KNOB_MODE_WRITEONCE, "pintool",
//    "ci","16", "cache size in kilobytes");
//KNOB<UINT32> KnobITLB2LineSize(KNOB_MODE_WRITEONCE, "pintool",
//    "bi","64", "cache block size in bytes");
//KNOB<UINT32> KnobITLB2Associativity(KNOB_MODE_WRITEONCE, "pintool",
//                "ai","8", "cache associativity (1 for direct mapped)");

#define MISS_THRESHOLD 50
#define INVOCATION_THRESHOLD 50
INT32 Usage()
{
    cerr <<
        "This tool represents a cache simulator.\n"
        "\n";

    cerr << KNOB_BASE::StringKnobSummary();

    cerr << endl;

    return -1;
}

bool call_instr_seen = false;
bool dir_jump_instr_seen = false;
int64_t prev_dir_jump_page;
bool syscall_seen = false;
bool ind_call_instr_seen = false;
bool return_instr_seen = false;
bool ind_jump_seen = false;
int64_t prev_ind_jump_page;
uint64_t prev_ind_jump_iaddr;
/* ===================================================================== */
/* Global Variables */
/* ===================================================================== */

// wrap configuation constants into their own name space to avoid name clashes
namespace IL1
{
    const UINT32 max_sets = 256*KILO; // cacheSize / (lineSize * associativity);
    const UINT32 max_associativity = 2048; // associativity;
    const CACHE_ALLOC::STORE_ALLOCATION allocation = CACHE_ALLOC::STORE_ALLOCATE;
    
    typedef CACHE_ROUND_ROBIN(max_sets, max_associativity, allocation) CACHE;
}


// wrap configuation constants into their own name space to avoid name clashes
namespace ITLB
{
    const UINT32 max_sets = KILO*256; // cacheSize / (lineSize * associativity);
    const UINT32 max_associativity = 256; // associativity;
    const CACHE_ALLOC::STORE_ALLOCATION allocation = CACHE_ALLOC::STORE_ALLOCATE;

    typedef CACHE_MODIFIED_CACHE(max_sets, max_associativity, allocation) CACHE;
}

//// wrap configuation constants into their own name space to avoid name clashes
//namespace IL2
//{
//    const UINT32 max_sets = 256*KILO; // cacheSize / (lineSize * associativity);
//    const UINT32 max_associativity = 256; // associativity;
//    const CACHE_ALLOC::STORE_ALLOCATION allocation = CACHE_ALLOC::STORE_ALLOCATE;
//    
//    typedef CACHE_ROUND_ROBIN(max_sets, max_associativity, allocation) CACHE;
//}
//
//// wrap configuation constants into their own name space to avoid name clashes
//namespace ITLB2
//{
//    const UINT32 max_sets = KILO*256; // cacheSize / (lineSize * associativity);
//    const UINT32 max_associativity = 256; // associativity;
//    const CACHE_ALLOC::STORE_ALLOCATION allocation = CACHE_ALLOC::STORE_ALLOCATE;
//
//    typedef CACHE_MODIFIED_CACHE(max_sets, max_associativity, allocation) CACHE;
//}


struct page_and_cache_block {
    uint64_t x, y;
    page_and_cache_block() {}
    page_and_cache_block (int _x, int _y) {
        x = _x;
        y = _y;
    }
    bool operator<(const page_and_cache_block &rhs) const{
        return make_pair(y,x) < make_pair(rhs.y, rhs.x);
    }
    bool operator==(const page_and_cache_block &rhs) const{
        return make_pair(y,x) == make_pair(rhs.y, rhs.x);
    }
};


struct cache_block_and_callee {
    uint64_t x, y;
    cache_block_and_callee() {}
    cache_block_and_callee (int _x, int _y) {
        x = _x;
        y = _y;
    }
    bool operator<(const cache_block_and_callee &rhs) const{
        return make_pair(y,x) < make_pair(rhs.y, rhs.x);
    }
    bool operator==(const cache_block_and_callee &rhs) const{
        return make_pair(y,x) == make_pair(rhs.y, rhs.x);
    }
};

struct function_stats{
	set<uint64_t> unique_cache_blocks_touched_by_function;
	uint64_t func_miss_count;
	uint64_t func_total_itlb_miss_count;
	uint64_t func_total_miss_count;
	uint64_t func_invocation_count;
	uint64_t func_normal_cache_total_misses;
	//function classified as low use function. 
	uint64_t func_root_cache_block;
	
	bool low_degree_function;
	bool medium_degree_function;
	bool initialized;
	bool current_function_miss;
	uint64_t function_callee_cache_block;
	uint64_t total_cache_blocks_accessed_across_invocations;
};

//maintain this per callee address or per cache block. 
map<uint64_t, function_stats> function_invocation_count;


ITLB::CACHE* itlb = NULL;

//ITLB2::CACHE* itlb2 = NULL;

IL1::CACHE* il1 = NULL;

//IL1::CACHE* il2 = NULL;

//datastructures used to note the number of cache blocks
//that are constitute a function. 
stack <uint64_t> call_stack;
map<page_and_cache_block, set<uint64_t> > mapping_from_function_to_number_of_cache_blocks;
//current function identified by the cache block
//that the callee address is a part of
page_and_cache_block current_function(0,0);
uint64_t current_function_callee_address = 1;
uint64_t current_cache_block;
set<uint64_t> number_of_active_low_use_functions;

//statistics to gauge effective cache block use across function invocations.
map<uint64_t, uint64_t> misses_per_cache_block;
map<uint64_t, uint64_t> burst_one_counts_per_normal_cache;
map<uint64_t, uint64_t> burst_two_counts_per_normal_cache;
map<uint64_t, uint64_t> misses_per_cache_block_normal_cache;
map<uint64_t, uint64_t> nmru_hits_per_cache_block_normal_cache;
map<uint64_t, bool> is_cache_block_low_use;

map<uint64_t, uint64_t> function_invocations_per_cache_block;
//current function for which this cache block is being accessed
//is identified by the callee address as well as the function invocation count
map<uint64_t, uint64_t> cache_block_current_function;
map<uint64_t, uint64_t> cache_block_current_function_invocation_count;

map<uint64_t, uint64_t> cache_block_current_function_callee;


map<uint64_t, set<uint64_t> > unique_function_roots_per_cache_block;

uint64_t current_iteration = 0;
uint64_t current_cache_block_iter = 0;
vector<int64_t> cache_blocks_accessed_in_second_iteration;

vector<uint64_t> list_of_active_low_use_function_counts;

//number of cache blocks part of a function in perlbench. 
set<uint64_t> number_of_cache_blocks_part_of_function_of_interest_perlbench;
uint64_t instructions_spent_in_function_of_interest = 0;
bool recorded = false;

//number of unique cache blocks accessed by a function on each invocation.
map<uint64_t, set<uint64_t> > set_of_unique_cache_blocks_accessed_on_each_function_invocation; 

typedef enum
{
    COUNTER_MISS = 0,
    COUNTER_HIT = 1,
    COUNTER_NUM
} COUNTER;


//counters to count the number of itlb misses happening after different kinds of 
//global control transfer instructions.

uint64_t itlb_misses_after_call = 0;
uint64_t icache_misses_after_ind_jump = 0;
uint64_t itlb_misses_after_return = 0;
uint64_t itlb_misses_after_syscall = 0;
uint64_t itlb_misses_after_none_of_above = 0;

uint64_t icache_misses_after_long_jump = 0;

//cache misses from shared library
uint64_t icache_misses_from_shared_library = 0;

typedef  COUNTER_ARRAY<UINT64, COUNTER_NUM> COUNTER_HIT_MISS;

set<uint64_t> list_of_high_use_blocks_replaced;



// holds the counters with misses and hits
// conceptually this is an array indexed by instruction address
COMPRESSOR_COUNTER<ADDRINT, UINT32, COUNTER_HIT_MISS> profile;

/* ===================================================================== */

VOID LoadMulti(ADDRINT addr, UINT32 size, UINT32 instId)
{
    // first level I-cache
    const BOOL il1Hit = il1->Access(addr, size, CACHE_BASE::ACCESS_TYPE_LOAD);

    const COUNTER counter = il1Hit ? COUNTER_HIT : COUNTER_MISS;
    profile[instId][counter]++;
}

/* ===================================================================== */

VOID LoadSingle(ADDRINT addr, UINT32 instId)
{
    // @todo we may access several cache lines for 
    // first level I-cache
    const BOOL il1Hit = il1->AccessSingleLine(addr, CACHE_BASE::ACCESS_TYPE_LOAD);

    const COUNTER counter = il1Hit ? COUNTER_HIT : COUNTER_MISS;
    profile[instId][counter]++;
}

/* ===================================================================== */

VOID LoadMultiFastSimple(ADDRINT addr, UINT32 size, uint64_t future_reference_timestamp, bool warmup_finished)
{
	il1->Access_selective_allocate(addr, size, CACHE_BASE::ACCESS_TYPE_LOAD, true, true, false, false,
				future_reference_timestamp, warmup_finished, 1.0, current_function_callee_address, function_invocation_count[current_function_callee_address].func_invocation_count);
}	


VOID LoadMultiFast(ADDRINT addr, UINT32 size, uint64_t future_reference_timestamp, 
		bool warmup_finished)
{
       //first step is to identify the function we are executing, sometimes we might jump out to function 
       //to run another function and then get back to executing a function. This necessitates the use of call stack
       //to identify the function we are executing.  
         //initialize the number of cache misses for a new cache block to be 0
       if (call_instr_seen){
	    call_stack.push(current_function_callee_address);
	    current_function_callee_address = addr;
	    function_call_seen_so_far++;
       }
       else if(return_instr_seen){
       //clear the number of cache blocks accessed on this function
       //invocation. 
	    if (call_stack.size()!=0){
		current_function_callee_address = call_stack.top();
		call_stack.pop();
	    }
	}
	    CACHE_TAG tag;
            UINT32 setIndex;
	    SplitAddress(addr, tag, setIndex);
	//    if (setIndex == SET_OF_INTEREST){
	// 	if (current_function_in_set_of_interest != current_function_callee_address ){
	//        if (distance_of_all_functions_from_a_particular_function.find(current_function_callee_address) == 
	//            	    distance_of_all_functions_from_a_particular_function.end()){
	//        	distance_of_all_functions_from_a_particular_function[current_function_callee_address].function_noted = true;
	//            distance_of_all_functions_from_a_particular_function[current_function_callee_address].cumulative_distance_counts = 0;
	//            distance_of_all_functions_from_a_particular_function[current_function_callee_address].cumulative_invocations = 0;
	//        }
	//        else{
	//            //un-note all functions so we can start noting again. 
	//            if (current_function_callee_address == ORIGIN_FUNCTION){
    	//            	cout <<"Hit Origin" << endl;
	//            	for(std::map<uint64_t,function_details>::iterator iter = distance_of_all_functions_from_a_particular_function.begin(); 
	//                iter != distance_of_all_functions_from_a_particular_function.end(); ++iter)
    	//            	{
	//            		distance_of_all_functions_from_a_particular_function[iter->first].function_noted = false;
    	//            	}
	//            }
	//            else{
	//            	if (current_function_callee_address == FUNCTION_OF_INTEREST){
	//            	    cout <<"Here (9760)" << endl;
	//            	}
	//            	for(std::map<uint64_t,function_details>::iterator iter = distance_of_all_functions_from_a_particular_function.begin(); 
	//                iter != distance_of_all_functions_from_a_particular_function.end(); ++iter)
    	//            	{
	//            		if (!distance_of_all_functions_from_a_particular_function[iter->first].function_noted){
	//				if (current_function_callee_address != FUNCTION_OF_INTEREST)
	//            				distance_of_all_functions_from_a_particular_function[iter->first].cumulative_distance_counts++;
	//            		}
    	//            	}
	//            	
	//            	if (!distance_of_all_functions_from_a_particular_function[current_function_callee_address].function_noted){
	//            		
	//            	       if (current_function_callee_address == FUNCTION_OF_INTEREST){
	//			cout << distance_of_all_functions_from_a_particular_function[current_function_callee_address].cumulative_distance_counts - count_seen_so_far << endl;
	//			count_seen_so_far = distance_of_all_functions_from_a_particular_function[current_function_callee_address].cumulative_distance_counts;
	//		       }
	//			distance_of_all_functions_from_a_particular_function[current_function_callee_address].cumulative_distance_counts++;
	//            		distance_of_all_functions_from_a_particular_function[current_function_callee_address].function_noted = true;
	//            	}
	//            }
	//           }
	//	}
	//	current_function_in_set_of_interest = current_function_callee_address;
	//    }

        if (misses_per_cache_block.find(addr/KnobITLBLineSize.Value()) == misses_per_cache_block.end()){ 
		misses_per_cache_block[addr/KnobITLBLineSize.Value()] = 0;	
		burst_one_counts_per_normal_cache[addr&(~(KnobITLBLineSize.Value()-1))] = 0;
		burst_two_counts_per_normal_cache[addr&(~(KnobITLBLineSize.Value()-1))] = 0;
		function_invocations_per_cache_block[addr/KnobITLBLineSize.Value()] = 0;
		cache_block_current_function[addr/KnobITLBLineSize.Value()] = 0;
		cache_block_current_function_invocation_count[addr/KnobITLBLineSize.Value()] = 0;
	 }
         if (function_invocation_count.find(current_function_callee_address) == 
          		function_invocation_count.end()){
		function_invocation_count[current_function_callee_address].func_invocation_count = 0;
	 }
	 if (call_instr_seen)
		 function_invocation_count[current_function_callee_address].func_invocation_count++;
	 uint64_t current_function_invocation_count;
	 current_function_invocation_count = function_invocation_count[current_function_callee_address].func_invocation_count; 
	 //for every cache block, set a degree of use based on how much use it has seen across 
	 //invocations of functions.
	 float block_degree_of_use;
	 bool block_degree_of_use_bool;
	 bool medium_block_degree_of_use_bool = false;
	 uint64_t number_of_cache_block_misses = misses_per_cache_block[addr/KnobITLBLineSize.Value()];
	 uint64_t number_of_cache_block_function_invocations = function_invocations_per_cache_block[addr/KnobITLBLineSize.Value()];
	 
	 uint64_t cache_block_burst_one_count = burst_one_counts_per_normal_cache[addr&(~(KnobITLBLineSize.Value()-1))];
	 uint64_t cache_block_burst_two_count = burst_two_counts_per_normal_cache[addr&(~(KnobITLBLineSize.Value()-1))];
	 uint64_t total_burst_counts = cache_block_burst_one_count + cache_block_burst_two_count;
	 //do we have one confident cache burst count
	 uint64_t confident_cache_burst_count = 0;
	 
	 if (((float)(cache_block_burst_one_count)/(total_burst_counts))>=BURST_PREDICTION_CONFIDENCE)
		 confident_cache_burst_count = 1;
	 else if (((float)(cache_block_burst_two_count)/(total_burst_counts))>=BURST_PREDICTION_CONFIDENCE)
		 confident_cache_burst_count = 2;
	 if (number_of_cache_block_misses == 0)
		 number_of_cache_block_misses = 1;
	 
	 
	 block_degree_of_use = (float)(number_of_cache_block_function_invocations)/number_of_cache_block_misses;
	 

	 //wait for some number of misses before classifying a block as a low use cache block. 
	 //treating blocks with degree of use under 1.0 similar to blocks with degree of use over 1.0
	 if ((block_degree_of_use>=1.0)&&(block_degree_of_use<=DEGREE_OF_USE)&&(number_of_cache_block_function_invocations>= INVOCATION_THRESHOLD)&&(confident_cache_burst_count)){
            block_degree_of_use_bool = false;
	    if ((block_degree_of_use > MEDIUM_DEGREE_OF_USE))
		    medium_block_degree_of_use_bool = true;
	 }
	 else
	   block_degree_of_use_bool = true;
	 if (confident_cache_burst_count == 2)
		block_degree_of_use = 2.0;
	 else if (confident_cache_burst_count == 1)
		block_degree_of_use = 1.0;
	 hit_and_use_information temp1;
	 //set the degree of use flag to true for code
       //from functions with a high degree of use. 
       //because degree of use affects placement in the cache, allow for a few misses before we start to place functions
       //assuming they are a low use function.  
       
	 if ((!block_degree_of_use_bool)){
		 bool medium_degree_of_use = false;
		 if (medium_block_degree_of_use_bool)
			 medium_degree_of_use = true;
		 temp1 = il1->Access_selective_allocate(addr, size, CACHE_BASE::ACCESS_TYPE_LOAD, true, false, medium_degree_of_use, false,future_reference_timestamp, warmup_finished,
				 block_degree_of_use, current_function_callee_address,
				 current_function_invocation_count);
	 }
	 else
       		temp1 = il1->Access_selective_allocate(addr, size,  CACHE_BASE::ACCESS_TYPE_LOAD, true, true, false, false,future_reference_timestamp, warmup_finished,
				block_degree_of_use, current_function_callee_address,
				current_function_invocation_count);
 	 	 
	if (!temp1.icache_hit){
	   //count the misses per cache block 
	   misses_per_cache_block[addr/KnobITLBLineSize.Value()]++;
	   if (temp1.burst_count_of_missed_block == 1)
	   	burst_one_counts_per_normal_cache[addr&(~(KnobITLBLineSize.Value()-1))]++;
	   else if (temp1.burst_count_of_missed_block == 2)
	   	burst_two_counts_per_normal_cache[addr&(~(KnobITLBLineSize.Value()-1))]++;

	}
	//update cache block statistics based on function invocation
	 if (cache_block_current_function[addr/KnobITLBLineSize.Value()] != current_function_callee_address){
 		function_invocations_per_cache_block[addr/KnobITLBLineSize.Value()]++;	 		
	 }
	 else if (cache_block_current_function_invocation_count[addr/KnobITLBLineSize.Value()] != 
			 function_invocation_count[current_function_callee_address].func_invocation_count){
 		function_invocations_per_cache_block[addr/KnobITLBLineSize.Value()]++;	 		
	 }
	 //else, we are on the same function's invocation count and block use stays the same. 
	
	 cache_block_current_function[addr/KnobITLBLineSize.Value()] = current_function_callee_address;
	 cache_block_current_function_invocation_count[addr/KnobITLBLineSize.Value()] = 
		 function_invocation_count[current_function_callee_address].func_invocation_count;
       	 call_instr_seen = false;
         ind_jump_seen = false;
         return_instr_seen = false;
         syscall_seen = false;
         dir_jump_instr_seen = false;
}

/* ===================================================================== */

VOID LoadSingleFastSimple(ADDRINT addr, uint64_t future_reference_timestamp, bool warmup_finished)
{
	il1->AccessSingleLine_selective_allocate(addr, CACHE_BASE::ACCESS_TYPE_LOAD, true, true, false, false, future_reference_timestamp, warmup_finished, 1.0,
			current_function_callee_address,
			function_invocation_count[current_function_callee_address].func_invocation_count);
}

VOID LoadSingleFast(ADDRINT addr, uint64_t future_reference_timestamp, 
		bool warmup_finished)
{

       //first step is to identify the function we are executing, sometimes we might jump out to function 
       //to run another function and then get back to executing a function. This necessitates the use of call stack
       //to identify the function we are executing.  
         //initialize the number of cache misses for a new cache block to be 0
       if (call_instr_seen){
	    call_stack.push(current_function_callee_address);
	    current_function_callee_address = addr;
	    function_call_seen_so_far++;
	    
       }
       else if(return_instr_seen){
       //clear the number of cache blocks accessed on this function
       //invocation. 
	    if (call_stack.size()!=0){
		current_function_callee_address = call_stack.top();
		call_stack.pop();
	    }
	}
       
	    CACHE_TAG tag;
            UINT32 setIndex;
	    SplitAddress(addr, tag, setIndex);
	   // if (setIndex == SET_OF_INTEREST){
	   //     if (current_function_in_set_of_interest != current_function_callee_address ){
	   //     if (distance_of_all_functions_from_a_particular_function.find(current_function_callee_address) == 
	   //         	    distance_of_all_functions_from_a_particular_function.end()){
	   //     	distance_of_all_functions_from_a_particular_function[current_function_callee_address].function_noted = true;
	   //         distance_of_all_functions_from_a_particular_function[current_function_callee_address].cumulative_distance_counts = 0;
	   //         distance_of_all_functions_from_a_particular_function[current_function_callee_address].cumulative_invocations = 0;
	   //     }
	   //     else{
	   //         //un-note all functions so we can start noting again. 
	   //         if (current_function_callee_address == ORIGIN_FUNCTION){
    	   //         	cout <<"Hit Origin" << endl;
	   //         	for(std::map<uint64_t,function_details>::iterator iter = distance_of_all_functions_from_a_particular_function.begin(); 
	   //             iter != distance_of_all_functions_from_a_particular_function.end(); ++iter)
    	   //         	{
	   //         		distance_of_all_functions_from_a_particular_function[iter->first].function_noted = false;
    	   //         	}
	   //         }
	   //         else{
	   //         	if (current_function_callee_address == FUNCTION_OF_INTEREST){
	   //         	    cout <<"Here (9760)" << endl;
	   //         	}
	   //         	for(std::map<uint64_t,function_details>::iterator iter = distance_of_all_functions_from_a_particular_function.begin(); 
	   //             iter != distance_of_all_functions_from_a_particular_function.end(); ++iter)
    	   //         	{
	   //         		if (!distance_of_all_functions_from_a_particular_function[iter->first].function_noted){
	   //     			if (current_function_callee_address != FUNCTION_OF_INTEREST)
	   //     			distance_of_all_functions_from_a_particular_function[iter->first].cumulative_distance_counts++;
	   //         		}
    	   //         	}
	   //         	
	   //         	if (!distance_of_all_functions_from_a_particular_function[current_function_callee_address].function_noted){
	   //         	       if (current_function_callee_address == FUNCTION_OF_INTEREST){
	   //     		cout << distance_of_all_functions_from_a_particular_function[current_function_callee_address].cumulative_distance_counts - count_seen_so_far << endl;
	   //     		count_seen_so_far = distance_of_all_functions_from_a_particular_function[current_function_callee_address].cumulative_distance_counts;
	   //     	       }
	   //     		distance_of_all_functions_from_a_particular_function[current_function_callee_address].cumulative_distance_counts++;
	   //         		distance_of_all_functions_from_a_particular_function[current_function_callee_address].function_noted = true;
	   //         	}
	   //         }
	   //        }
	   //     }
	   //     current_function_in_set_of_interest = current_function_callee_address;
	   // }

       if (misses_per_cache_block.find(addr/KnobITLBLineSize.Value()) == misses_per_cache_block.end()){ 
		burst_one_counts_per_normal_cache[addr&(~(KnobITLBLineSize.Value()-1))] = 0;
		burst_two_counts_per_normal_cache[addr&(~(KnobITLBLineSize.Value()-1))] = 0;
		misses_per_cache_block[addr/KnobITLBLineSize.Value()] = 0;	
		function_invocations_per_cache_block[addr/KnobITLBLineSize.Value()] = 0;
		cache_block_current_function[addr/KnobITLBLineSize.Value()] = 0;
		cache_block_current_function_invocation_count[addr/KnobITLBLineSize.Value()] = 0;
	 }
         if (function_invocation_count.find(current_function_callee_address) == 
          		function_invocation_count.end()){
		function_invocation_count[current_function_callee_address].func_invocation_count = 0;
	 }
	 if (call_instr_seen)
		 function_invocation_count[current_function_callee_address].func_invocation_count++;
	 
	 uint64_t current_function_invocation_count;
	 current_function_invocation_count = function_invocation_count[current_function_callee_address].func_invocation_count; 
	 //for every cache block, set a degree of use based on how much use it has seen across 
	 //invocations of functions.
	 float block_degree_of_use;
	 bool block_degree_of_use_bool;
	 bool medium_block_degree_of_use_bool = false;
	 uint64_t number_of_cache_block_misses = misses_per_cache_block[addr/KnobITLBLineSize.Value()];
	 uint64_t number_of_cache_block_function_invocations = function_invocations_per_cache_block[addr/KnobITLBLineSize.Value()];
	 
	 uint64_t cache_block_burst_one_count = burst_one_counts_per_normal_cache[addr&(~(KnobITLBLineSize.Value()-1))];
	 uint64_t cache_block_burst_two_count = burst_two_counts_per_normal_cache[addr&(~(KnobITLBLineSize.Value()-1))];
	 uint64_t total_burst_counts = cache_block_burst_one_count + cache_block_burst_two_count;
	 //do we have one confident cache burst count
	 uint64_t confident_cache_burst_count = 0;
	 if (((float)(cache_block_burst_one_count)/(total_burst_counts))>=BURST_PREDICTION_CONFIDENCE)
		 confident_cache_burst_count = 1;
	 else if (((float)(cache_block_burst_two_count)/(total_burst_counts))>=BURST_PREDICTION_CONFIDENCE)
		 confident_cache_burst_count = 2;
	 if (number_of_cache_block_misses == 0)
		 number_of_cache_block_misses = 1;
	 block_degree_of_use = (float)(number_of_cache_block_function_invocations)/number_of_cache_block_misses;
	 

	 //wait for some number of misses before classifying a block as a low use cache block. 
	 //treating blocks with degree of use under 1.0 similar to blocks with degree of use over 1.0
//want to have a confident cache burst count to assign to a cache block.
	 if ((block_degree_of_use>=1.0)&&(block_degree_of_use<=DEGREE_OF_USE)&&(number_of_cache_block_function_invocations>= INVOCATION_THRESHOLD) &&(confident_cache_burst_count)){
	    block_degree_of_use_bool = false;
	    if ((block_degree_of_use > MEDIUM_DEGREE_OF_USE))
		    medium_block_degree_of_use_bool = true;
	 }
	 else
	   block_degree_of_use_bool = true;
	 if (confident_cache_burst_count == 2)
		block_degree_of_use = 2.0;
	 else if (confident_cache_burst_count == 1)
		block_degree_of_use = 1.0;
	 hit_and_use_information temp1;
	 //set the degree of use flag to true for code
       //from functions with a high degree of use. 
       //because degree of use affects placement in the cache, allow for a few misses before we start to place functions
       //assuming they are a low use function.  
       
	 if ((!block_degree_of_use_bool)){
		 bool medium_degree_of_use = false;
		 if (medium_block_degree_of_use_bool)
			 medium_degree_of_use = true;
		 temp1 = il1->AccessSingleLine_selective_allocate(addr, CACHE_BASE::ACCESS_TYPE_LOAD, true, false, medium_degree_of_use, false,future_reference_timestamp, warmup_finished,
				 block_degree_of_use, current_function_callee_address,
				 current_function_invocation_count);
	 }
	 else
       		temp1 = il1->AccessSingleLine_selective_allocate(addr, CACHE_BASE::ACCESS_TYPE_LOAD, true, true, false, false,future_reference_timestamp, warmup_finished,
				block_degree_of_use, current_function_callee_address,
				current_function_invocation_count);
 	 	 
	if (!temp1.icache_hit){
	   //count the misses per cache block 
	   misses_per_cache_block[addr/KnobITLBLineSize.Value()]++;
	   if (temp1.burst_count_of_missed_block == 1)
	   	burst_one_counts_per_normal_cache[addr&(~(KnobITLBLineSize.Value()-1))]++;
	   else if (temp1.burst_count_of_missed_block == 2)
	   	burst_two_counts_per_normal_cache[addr&(~(KnobITLBLineSize.Value()-1))]++;
	}
	//update cache block statistics based on function invocation
	 if (cache_block_current_function[addr/KnobITLBLineSize.Value()] != current_function_callee_address){
 		function_invocations_per_cache_block[addr/KnobITLBLineSize.Value()]++;	 		
	 }
	 else if (cache_block_current_function_invocation_count[addr/KnobITLBLineSize.Value()] != 
			 function_invocation_count[current_function_callee_address].func_invocation_count){
 		function_invocations_per_cache_block[addr/KnobITLBLineSize.Value()]++;	 		
	 }
	 //else, we are on the same function's invocation count and block use stays the same. 
	
	 cache_block_current_function[addr/KnobITLBLineSize.Value()] = current_function_callee_address;
	 cache_block_current_function_invocation_count[addr/KnobITLBLineSize.Value()] = 
		 function_invocation_count[current_function_callee_address].func_invocation_count;
       	 call_instr_seen = false;
         ind_jump_seen = false;
         return_instr_seen = false;
         syscall_seen = false;
         dir_jump_instr_seen = false;
}


// The running count of instructions is kept here
// make it static to help the compiler optimize docount
//static UINT64 icount = 0;

// This function is called before every instruction is executed
VOID Fini() { 
         std::ofstream out(KnobOutputFile.Value().c_str());
         // print I-cache profile
         // @todo what does this print
         out << "PIN:MEMLATENCIES 1.0. 0x0\n";
         out <<
             "#\n"
             "# ICACHE stats\n"
             "#\n";
         out << il1->StatsLong("# ", CACHE_BASE::CACHE_TYPE_ICACHE);
         out << "Total instructions in program is " <<  total_instruction_count << endl;
	 cout << "Function calls seen so far " << function_call_seen_so_far << endl;

    for(std::map<uint64_t,function_details>::iterator iter = distance_of_all_functions_from_a_particular_function.begin(); 
		    iter != distance_of_all_functions_from_a_particular_function.end(); ++iter)
    {
	out <<"Origin is : " << ORIGIN_FUNCTION << endl; 
	out << "Function is " << iter->first <<": and distance from origin is " <<(float)distance_of_all_functions_from_a_particular_function[iter->first].cumulative_distance_counts/distance_of_all_functions_from_a_particular_function[iter->first].cumulative_invocations << endl;
    }
}




/* ===================================================================== */

VOID Instruction(ADDRINT iaddr, uint32_t size, uint32_t control_flow_type, uint64_t future_reference_timestamp, 
		bool warmup_finished)
{
    
    const BOOL   single = (size <= 4);
                
    if (single)
	LoadSingleFast(iaddr, future_reference_timestamp, warmup_finished);
    else
	LoadMultiFast(iaddr,size,future_reference_timestamp, warmup_finished);
    //returns
    if (control_flow_type == 6){
         return_instr_seen = true; 
    }
    //direct calls
    else if (control_flow_type == 2 ){
         call_instr_seen = true;
    }
    //direct jumps
    else if (control_flow_type == 3){
         dir_jump_instr_seen = true;
    }
    //indirect calls
    else if (control_flow_type == 4){
         call_instr_seen = true;
         ind_call_instr_seen = true;
    }
    //indirect jumps
    else if (control_flow_type == 5){
         ind_jump_seen = true;
         prev_ind_jump_page = iaddr/4096;
    }
    //system calls
    else if(control_flow_type == 1){
         syscall_seen = true;
    }
}


int main(int argc, char *argv[])
{
    PIN_InitSymbols();

    if( PIN_Init(argc,argv) )
    {
        return Usage();
    }

    
//    INS_AddInstrumentFunction(Instruction, 0);
//    PIN_AddFiniFunction(Fini, 0);
    // Never returns

 //   PIN_StartProgram();
    
    ifstream file1(KnobInputInstructionTraceFile.Value().c_str());
    //assuming that initially state of the call stack has 5 functions. 
#ifdef SPEC_APPS     
    uint64_t inst_addr;
    uint32_t size, control_flow_type;
    //SPEC uncomment
    //uint64_t future_reference_timestamp;
    string line;
    bool read_first_line = false;
    uint64_t total_instructions_in_program = 0;
    bool warmup_finished = false;
    //changing this limit to 0 for the SPEC and Java apps
    uint64_t limit_warmup_instructions = 0;
    while (std::getline(file1, line)){
	 //simulate a maximum of 1/2B instructions
	 if (!read_first_line){
	    stringstream ss(line);
	    ss >> total_instructions_in_program;
	    warmup_interval = min(total_instructions_in_program/2,limit_warmup_instructions);
	    total_instruction_count = total_instructions_in_program - warmup_interval;
	    cout << "Warmup interval is: "  << warmup_interval << endl;
    	    il1 = new IL1::CACHE("L1 Inst Cache",
                         KnobCacheSize.Value() * KILO,
                         KnobLineSize.Value(),
                         KnobAssociativity.Value(),
			 warmup_interval);
    	    profile.SetKeyName("iaddr          ");
    	    profile.SetCounterName("icache:miss        icache:hit");
    	    COUNTER_HIT_MISS threshold;
    	    threshold[COUNTER_HIT] = KnobThresholdHit.Value();
            threshold[COUNTER_MISS] = KnobThresholdMiss.Value();
    	    profile.SetThreshold( threshold );
	    read_first_line = true;
	    continue;
	 }
	 
	 count_of_instructions++;
	 //remove this for SPEC applications
	 if (count_of_instructions > total_instructions_in_program)
	  break;
	 //remove this for SPEC applications
	 warmup_finished = true;
	 
	 replace(line.begin(), line.end(), ',', ' ');
	 stringstream ss(line);
	 	
	 ss >> inst_addr >> size >> control_flow_type;
	 //ss >> inst_addr >> size >> control_flow_type >>future_reference_timestamp ;
	 if (control_flow_type == 10){
             warmup_finished = true;
	     continue;
	 }
	 Instruction(inst_addr, size, control_flow_type,
			 0, 
			 //future_reference_timestamp, 
			 warmup_finished);
       //  cout << inst_addr << endl;	
//	 ss >> inst_addr >> size >> control_flow_type;
       	//cout << future_reference_timestamp << endl;
    }
#endif    
#ifdef NON_SPEC_APPS
    uint64_t inst_addr;
    uint32_t size, control_flow_type;
    uint64_t future_reference_timestamp;
    string line;
    bool read_first_line = false;
    uint64_t total_instructions_in_program = 0;
    bool warmup_finished = false;
    //changing this limit to 0 for the SPEC and Java apps
    uint64_t limit_warmup_instructions = 200000000;
    while (std::getline(file1, line)){
	 //simulate a maximum of 1/2B instructions
	 if (!read_first_line){
	    stringstream ss(line);
	    ss >> total_instructions_in_program;
	    warmup_interval = min(total_instructions_in_program/2,limit_warmup_instructions);
	    total_instruction_count = total_instructions_in_program - warmup_interval;
	    

	    cout << "Warmup interval is: "  << warmup_interval << endl;
    	    il1 = new IL1::CACHE("L1 Inst Cache",
                         KnobCacheSize.Value() * KILO,
                         KnobLineSize.Value(),
                         KnobAssociativity.Value(),
			 warmup_interval);
    	    profile.SetKeyName("iaddr          ");
    	    profile.SetCounterName("icache:miss        icache:hit");
    	    COUNTER_HIT_MISS threshold;
    	    threshold[COUNTER_HIT] = KnobThresholdHit.Value();
            threshold[COUNTER_MISS] = KnobThresholdMiss.Value();
    	    profile.SetThreshold( threshold );
	    read_first_line = true;
	    continue;
	 }
	 
	 count_of_instructions++;
	
	 replace(line.begin(), line.end(), ',', ' ');
	 stringstream ss(line);
	 	
	 ss >> inst_addr >> size >> control_flow_type >>future_reference_timestamp ;
	 if (control_flow_type == 10){
             warmup_finished = true;
	     continue;
	 }
	 Instruction(inst_addr, size, control_flow_type,
			 future_reference_timestamp, 
			 warmup_finished);
       //  cout << inst_addr << endl;	
//	 ss >> inst_addr >> size >> control_flow_type;
       	//cout << future_reference_timestamp << endl;
    }
#endif
    Fini();
    return 0;
}

/* ===================================================================== */
/* eof */
/* ===================================================================== */
