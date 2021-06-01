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
#include <bitset>
#include <cstdlib> 
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

#define FUNCTION_OF_INTEREST 2151655040
uint64_t warmup_interval = 0; 

uint64_t current_page_in_function_of_interest = 0;

uint64_t unused_large_block_chunks = 0;
struct spatial_locality_vector_and_confidence{
	bitset<64> spatial_locality_vector;
	bitset<64> spatial_locality_vector_to_use_because_of_low_confidence;
	//assign the confidence to 1 upon assignment. 
	//Saturate at 0 and 4 respectively. Use if greater than equal to 2.
	uint64_t confidence;
};

struct function_and_page {
    uint64_t page, function;
    function_and_page() {}
    function_and_page (int _page, int _function) {
        page = _page;
        function = _function;
    }
    bool operator<(const function_and_page &rhs) const{
        return make_pair(function,page) < make_pair(rhs.function, rhs.page);
    }
    bool operator==(const function_and_page &rhs) const{
        return make_pair(function,page) == make_pair(rhs.function, rhs.page);
    }
};

struct number_of_spatial_prefetches_and_total_prefetches
{
	uint64_t total_prefetches;
	uint64_t total_block_prefetches;
};
//store a list of all forward pointers corresponding to the 
//different functions residing within the page of interest.
map<uint64_t,set<uint64_t>> mapping_from_page_to_forward_pointers;
set<uint64_t> set_of_unique_instruction_pages_touched_by_program;
map<uint64_t,set<uint64_t>> mapping_from_functions_to_number_of_pages;
map<uint64_t, number_of_spatial_prefetches_and_total_prefetches> mapping_from_function_to_spatial_prefetch_info;

struct function_and_inter_page_distance {
    uint64_t function;
    int64_t page_distance;
    function_and_inter_page_distance() {}
    function_and_inter_page_distance (int64_t _page_distance, uint64_t _function) {
        page_distance = _page_distance;
        function = _function;
    }
    bool operator<(const function_and_inter_page_distance &rhs) const{
        return make_pair(function,page_distance) < make_pair(rhs.function, rhs.page_distance);
    }
    bool operator==(const function_and_inter_page_distance &rhs) const{
        return make_pair(function,page_distance) == make_pair(rhs.function, rhs.page_distance);
    }
};
//spatial locality measurements for functions. 
//measure this for a single function for now. 
map<function_and_page, bitset<64>> spatial_locality_within_10_cache_blocks_from_function_start;

//assign a function a new spatial locality vector when current confidence. 
map<function_and_page, spatial_locality_vector_and_confidence> spatial_locality_from_function_start_and_confidence;

map<uint64_t, uint64_t> block_to_corresponding_function;

map<uint64_t, bool> does_function_have_root;

map<uint64_t, set<uint64_t>> function_to_unique_pages_touched;
//get an idea of how many unique functions that are executed are contained in a page. 
//functions that jump to pages close
set<function_and_inter_page_distance> functions_that_only_jump_to_pages_close;
map<function_and_inter_page_distance,uint64_t> functions_that_jumps_to_distant_pages;

//update this on every invocation of the function. 
map<uint64_t, map<uint64_t,uint64_t>> mapping_from_function_and_page_to_forward_pointers;
map<uint64_t, uint64_t> mapping_from_function_to_current_function_page_on_which_we_are;

set<uint64_t> total_set_of_functions;

//to track when we move from one function to another in the cache set.
uint64_t current_function_in_set_of_interest = 0;

uint64_t count_seen_so_far = 0;

uint64_t total_instruction_count = 0;
/* ===================================================================== */

uint64_t prev_cache_block_seen = 0;
uint64_t prev_instruction = 0;



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



set<uint64_t> total_functions;
set<uint64_t> unique_cache_blocks_accessed_by_program;

bool done = false;
bool enable_instrumentation = true;

uint64_t count_of_instructions = 0;

uint64_t call_stack_hash = 0;


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
    "a","8", "cache associativity (1 for direct mapped)");

KNOB<UINT32> KnobITLBSize(KNOB_MODE_WRITEONCE, "pintool",
    "ci","16", "cache size in kilobytes");
KNOB<UINT32> KnobITLBLineSize(KNOB_MODE_WRITEONCE, "pintool",
    "bi","64", "cache block size in bytes");
KNOB<UINT32> KnobITLBAssociativity(KNOB_MODE_WRITEONCE, "pintool",
                "ai","128", "cache associativity (1 for direct mapped)");


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
    const UINT32 max_associativity = 16; // associativity;
    const CACHE_ALLOC::STORE_ALLOCATION allocation = CACHE_ALLOC::STORE_ALLOCATE;
    
    typedef CACHE_ROUND_ROBIN(max_sets, max_associativity, allocation) CACHE;
}





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
	uint64_t func_invocation_count;
};

//maintain this per callee address or per cache block. 
map<uint64_t, function_stats> function_invocation_count;



//ITLB2::CACHE* itlb2 = NULL;

IL1::CACHE* il1 = NULL;

//IL1::CACHE* il2 = NULL;

//datastructures used to note the number of cache blocks
//that are constitute a function. 
stack <uint64_t> call_stack;
stack<uint64_t> return_cache_block_stack;
map<page_and_cache_block, set<uint64_t> > mapping_from_function_to_number_of_cache_blocks;
//current function identified by the cache block
//that the callee address is a part of
page_and_cache_block current_function(0,0);
uint64_t current_function_callee_address = 1;
uint64_t current_cache_block;
set<uint64_t> number_of_active_low_use_functions;


map<uint64_t, uint64_t> function_invocations_per_cache_block;



map<uint64_t, map<uint64_t,uint64_t>> mapping_from_current_function_to_caller_and_count_of_calls;
map<uint64_t, map<uint64_t,uint64_t>> mapping_from_caller_to_current_function_and_count_of_calls;
//count the number of times a function was invoked.
map<uint64_t, uint64_t> mapping_from_callee_to_invocation_count;



bool recorded = false;


typedef enum
{
    COUNTER_MISS = 0,
    COUNTER_HIT = 1,
    COUNTER_NUM
} COUNTER;



typedef  COUNTER_ARRAY<UINT64, COUNTER_NUM> COUNTER_HIT_MISS;



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
	hit_and_use_information temp1;
       temp1	= il1->Access_selective_allocate(addr, size, CACHE_BASE::ACCESS_TYPE_LOAD, true, true, false, false,
				future_reference_timestamp, warmup_finished, 1.0, current_function_callee_address, function_invocation_count[current_function_callee_address].func_invocation_count, false);
       if ((!temp1.icache_hit) && warmup_finished)
       	unused_large_block_chunks += (8-temp1.number_of_blocks_used);
}	




VOID LoadMultiFast_Spatial(ADDRINT addr, UINT32 size, uint64_t future_reference_timestamp, 
		bool warmup_finished)
{

       //first step is to identify the function we are executing, sometimes we might jump out to function 
       //to run another function and then get back to executing a function. This necessitates the use of call stack
       //to identify the function we are executing.  
         //initialize the number of cache misses for a new cache block to be 0
       if (call_instr_seen){
	    if (mapping_from_callee_to_invocation_count.find(current_function_callee_address) ==
	        	    mapping_from_callee_to_invocation_count.end()){
	           mapping_from_callee_to_invocation_count[current_function_callee_address] = 1;
	    }
	    else{
	         mapping_from_callee_to_invocation_count[current_function_callee_address]++;
	    }
	    
	    call_stack.push(current_function_callee_address);
	    
	    call_stack_hash ^= current_function_callee_address;
	    current_function_callee_address = addr;
	    if (mapping_from_function_to_spatial_prefetch_info.find(current_function_callee_address) == 
			    mapping_from_function_to_spatial_prefetch_info.end()){
	    	mapping_from_function_to_spatial_prefetch_info[current_function_callee_address].total_prefetches = 0;
		mapping_from_function_to_spatial_prefetch_info[current_function_callee_address].total_block_prefetches = 0;
	    }
	    if (mapping_from_function_and_page_to_forward_pointers.find(current_function_callee_address) == 
			    mapping_from_function_and_page_to_forward_pointers.end()){
		    //set the forward pointer to null upon function entry. 
		    mapping_from_function_and_page_to_forward_pointers[current_function_callee_address][current_function_callee_address/4096] = 0;
            }
	    function_call_seen_so_far++;
	    
       }
       else if(return_instr_seen){
       //clear the number of cache blocks accessed on this function
       //invocation. 
	    //pop the cache block from the return cache block stack
	    //if we are within one cache block of what we pushed, then go ahead and pop
	    //from call stack. Else, do nothing.
	    bool should_we_pop_from_call_stack = false; 
	    if(return_cache_block_stack.size()!=0){
	    	uint64_t block_to_return_to = return_cache_block_stack.top();
		if ((block_to_return_to == (addr/64))||
				((block_to_return_to+1) == (addr/64))){
			should_we_pop_from_call_stack = true;
			return_cache_block_stack.pop();
		}
	    }
	    if ((call_stack.size()!=0) && (should_we_pop_from_call_stack)){
		current_function_callee_address = call_stack.top();
		call_stack_hash ^= current_function_callee_address;
		call_stack.pop();
	    }
	}
       set_of_unique_instruction_pages_touched_by_program.insert(addr/4096);
       //store the page and the corresponding function.
       //keep track of on what page we are for every function
       //we are executing. 
       uint64_t current_page = addr/4096;
//       if (current_function_callee_address == CALLEE_OF_INTEREST){
//		if (current_page_in_function_of_interest  != 
//			current_page){
//		    if (ind_jump_seen)
//		     cout <<current_page << "(ind jump),";
//	            else if (dir_jump_instr_seen)
//		     cout <<current_page << "(dir jump),";
//		    else if (syscall_seen)
//		     cout <<current_page << "(syscall),";
//		    else
//		     cout <<current_page << ",";
//		}
//	//	mapping_from_function_and_page_to_forward_pointers[current_function_callee_address][current_page_in_function_of_interest] = current_page;
//	//	//if no forward pointer set for this function page pair, then set it to NULL to start with. 
//	//	if (mapping_from_function_and_page_to_forward_pointers[current_function_callee_address].find(current_page) == 
//	//			mapping_from_function_and_page_to_forward_pointers[current_function_callee_address].end())
//	//		mapping_from_function_and_page_to_forward_pointers[current_function_callee_address][current_page] = 0;
//		current_page_in_function_of_interest = current_page;
//	}	
        //when we move to another page within the same function. 
	if (current_page !=
			mapping_from_function_to_current_function_page_on_which_we_are[current_function_callee_address]){
	//	if (mapping_from_function_to_current_function_page_on_which_we_are[current_function_callee_address] == 133709040){
	//	   if (ind_jump_seen)
	//		   cout << "Ind jump " << endl;
	//	   else if (dir_jump_instr_seen)
	//		   cout << "Dir jump " << endl;
	//	   else
	//		   cout <<"Diff cfl " << endl;
	//	 }
		uint64_t current_page_in_function_of_interest = mapping_from_function_to_current_function_page_on_which_we_are[current_function_callee_address];
		if (mapping_from_function_and_page_to_forward_pointers[current_function_callee_address].find(current_page_in_function_of_interest) != 
				mapping_from_function_and_page_to_forward_pointers[current_function_callee_address].end()){
			uint64_t temp =  mapping_from_function_and_page_to_forward_pointers[current_function_callee_address][current_page_in_function_of_interest];
			//only store a list of non-zero forward pointers per page.
			if (temp!= 0)
				mapping_from_page_to_forward_pointers[current_page_in_function_of_interest].erase(temp);
		}
		mapping_from_page_to_forward_pointers[current_page_in_function_of_interest].insert(current_page);
		mapping_from_function_and_page_to_forward_pointers[current_function_callee_address][current_page_in_function_of_interest] = current_page;
		//if no forward pointer set for this function page pair, then set it to NULL to start with. 
		if (mapping_from_function_and_page_to_forward_pointers[current_function_callee_address].find(current_page) == 
				mapping_from_function_and_page_to_forward_pointers[current_function_callee_address].end())
			mapping_from_function_and_page_to_forward_pointers[current_function_callee_address][current_page] = 0;
		mapping_from_function_to_current_function_page_on_which_we_are[current_function_callee_address] = current_page;
	}
        total_set_of_functions.insert(current_function_callee_address);
	 uint64_t current_function_invocation_count;
	 current_function_invocation_count = function_invocation_count[current_function_callee_address].func_invocation_count; 
	 //for every cache block, set a degree of use based on how much use it has seen across 
	 //invocations of functions.
	 hit_and_use_information temp1;
	 //set the degree of use flag to true for code
       //from functions with a high degree of use. 
       //because degree of use affects placement in the cache, allow for a few misses before we start to place functions
       //assuming they are a low use function.  
	temp1 = il1->Access_selective_allocate(addr, size, CACHE_BASE::ACCESS_TYPE_LOAD, true, true, false, false,
				future_reference_timestamp, warmup_finished, 1.0, current_function_callee_address, current_function_invocation_count, false);

	
	vector<uint64_t> blk_addresses_evicted_due_to_spatial_fetch;
	if (!temp1.icache_hit){	
           function_and_inter_page_distance temp;
           temp.page_distance = current_page;
           temp.function = current_function_callee_address;
           if (functions_that_jumps_to_distant_pages.find(temp) == functions_that_jumps_to_distant_pages.end())
		   functions_that_jumps_to_distant_pages[temp] = 1;
	   else
		   functions_that_jumps_to_distant_pages[temp]++;
	}
	function_to_unique_pages_touched[current_function_callee_address].insert(addr/4096);
	if ((!temp1.icache_hit)||(temp1.spatial_fetch_hit)){
	//	if (current_function_callee_address == CALLEE_OF_INTEREST){
		if (call_instr_seen){
		  if (!temp1.icache_hit)
		  	cout << "Callee miss " << endl;
		  else
		  	cout << "Callee spatial hit " << endl;
		 
		 // std::stringstream buffer;
                 // buffer << spatial_locality_from_function_start_and_confidence[current_function_callee_address].spatial_locality_vector <<","<<spatial_locality_from_function_start_and_confidence[current_function_callee_address].confidence <<endl;
	         // string str =  buffer.str();
		 // cout << str;
		 // issue spatial prefetch only when we have some amount of confidence on the block access pattern. 
		   
		   function_and_page callee_func_page;
		   callee_func_page.function = current_function_callee_address;
		   callee_func_page.page = addr/4096;
		   //start with the current page.Chase pointers till we hit NULL.  
		   uint64_t current_function_page = current_function_callee_address/4096;
		   uint64_t start_page = current_function_callee_address/4096; 
		   uint64_t num_iters = 0;
	           uint64_t num_pages_visited = 0;		   
		   set<uint64_t> visited_pages;
		   uint64_t num_spat_fetches = 0;
		   uint64_t num_spat_total_fetches = 0;
		   mapping_from_functions_to_number_of_pages[current_function_callee_address].clear();
		   do{

			if (visited_pages.find(current_function_page) != 
					visited_pages.end())
				break;
			visited_pages.insert(current_function_page);
			mapping_from_functions_to_number_of_pages[current_function_callee_address].insert(current_function_page);
			num_pages_visited++;
			function_and_page temp;
			temp.function = current_function_callee_address;
			temp.page = current_function_page;
			if (spatial_locality_from_function_start_and_confidence[temp].confidence >= 2){
			      for (size_t i=0; i<spatial_locality_from_function_start_and_confidence[temp].spatial_locality_vector.size();
		  	   		 ++i){
			      //issue a spatial fetch for the corresponding cache blocks. 
		   	      uint64_t root_function_block = temp.page*4096; 
			      if (spatial_locality_from_function_start_and_confidence[temp].spatial_locality_vector.test(i)){
		  	             
			      	      hit_and_use_information temp2;
				     bool spatial_fetch = true;
       		  	     	       uint64_t blk_addr_to_fetch = root_function_block + i*64;
				     temp2 = il1->AccessSingleLine_selective_allocate(blk_addr_to_fetch, CACHE_BASE::ACCESS_TYPE_LOAD, 
		  	      		true, true, false, false,future_reference_timestamp, warmup_finished,
		  	      		1.0, current_function_callee_address,
		  	      		current_function_invocation_count, spatial_fetch);
					if (!temp2.icache_hit){
					        blk_addresses_evicted_due_to_spatial_fetch.push_back(temp2.blk_addresses.at(0));
					}

					  num_spat_fetches++;
					  num_spat_total_fetches++;
				     
		  	       }
		  	    }
		   	    spatial_locality_within_10_cache_blocks_from_function_start[temp].reset();
		  	 }

			 else{
		  	      for (size_t i=0; i<spatial_locality_from_function_start_and_confidence[temp].spatial_locality_vector_to_use_because_of_low_confidence.size();
		  	   		 ++i){
		  	       //issue a spatial fetch for the corresponding cache blocks. 
    		  	       
		   	      uint64_t root_function_block = temp.page*4096; 
			      if (spatial_locality_from_function_start_and_confidence[temp].spatial_locality_vector_to_use_because_of_low_confidence.test(i)){
				      if (current_function_callee_address == CALLEE_OF_INTEREST)
					cout << "Low confidence (multi) spatial fetch" << endl; 
				      hit_and_use_information temp2;
				      bool spatial_fetch = true;
       		  	     	       uint64_t blk_addr_to_fetch = root_function_block + i*64;
       		  	      	temp2 = il1->AccessSingleLine_selective_allocate(blk_addr_to_fetch, CACHE_BASE::ACCESS_TYPE_LOAD, 
		  	      		true, true, false, false,future_reference_timestamp, warmup_finished,
		  	      		1.0, current_function_callee_address,
		  	      		current_function_invocation_count, spatial_fetch);
				if (!temp2.icache_hit){
				        blk_addresses_evicted_due_to_spatial_fetch.push_back(temp2.blk_addresses.at(0));
				}

				  num_spat_fetches++;
				  num_spat_total_fetches++;
		  	       }
		  	    }
		   	    spatial_locality_within_10_cache_blocks_from_function_start[temp].reset();
			 }
			 num_iters++;
			 if (num_iters >= 10)
				cout <<"What?? " << current_function_callee_address << endl;
			current_function_page = mapping_from_function_and_page_to_forward_pointers[current_function_callee_address][current_function_page];
		   }while (current_function_page != 0);
		   visited_pages.clear();
		   //update the data structure if we perform a non-zero number of 
		   //spatial fetches
		   if (num_spat_total_fetches!=0){
		  	mapping_from_function_to_spatial_prefetch_info[current_function_callee_address].total_block_prefetches += num_spat_fetches;
			mapping_from_function_to_spatial_prefetch_info[current_function_callee_address].total_prefetches++;
		   }
		   //relearn pointers post spatial prefetch. 
		   mapping_from_function_and_page_to_forward_pointers[current_function_callee_address][start_page] = 0;
		   //don't set the bit corresponding to callee as we are anyways fetching it on a miss.
		  // spatial_locality_within_10_cache_blocks_from_function_start[callee_func_page].set((addr/64)&63);
		   if (block_to_is_function_root.find(addr/64) == block_to_is_function_root.end())
			block_to_is_function_root[addr/64] = false;
		   block_to_is_function_root[addr/64] = true;
		   //this field is reset when the functio (or this root) is evicted from the cache.
		   does_function_have_root[current_function_callee_address] = true; 
                   block_to_corresponding_function[addr/64] = current_function_callee_address; 
               }
	       //check if the block is a miss and it could've been a part of the spatial fetched. 
	       //
	       else if (!temp1.icache_hit){
	       //check if the block is close to the root of the function. 
                   int64_t distance_from_root_of_function_cache_block = addr/64 - current_function_callee_address/64;
		  // cout << "Miss "<< addr/4096<<  " and distance (single) from root is " <<distance_from_root_of_function_cache_block << " (calls seen so far)" << function_call_seen_so_far << endl;
		   //check if the block is in the bitvector 
		   bool need_to_prefetch = false;
		   if (distance_from_root_of_function_cache_block <=5){
			uint64_t page = addr/4096;
			uint64_t start_page = addr/4096;
			function_and_page temp, temp2;
			temp2.function = current_function_callee_address;
			temp2.page = page;
			if (spatial_locality_from_function_start_and_confidence[temp2].confidence >= 2){
			 if (spatial_locality_from_function_start_and_confidence[temp2].spatial_locality_vector.test((addr/64)&63)){
			        //issue a spatial fetch for the corresponding cache blocks. 
			        need_to_prefetch = true;
			//	cout << "performing spatial fetch1 (non callee)" << endl; 
		          }
	                }		 
			else{
			  if (spatial_locality_from_function_start_and_confidence[temp2].spatial_locality_vector.test((addr/64)&63)){
		  	  //        cout << "performing spatial fetch2 (non callee)" << endl; 
			          //issue a spatial fetch for the corresponding cache blocks. 
			          need_to_prefetch = true;
			  }
			} 
		        if (need_to_prefetch){	
		                //start with the current page.Chase pointers till we hit NULL.  
		                uint64_t current_function_page = current_function_callee_address/4096;
		                
		                uint64_t num_iters = 0;
		                
		                set<uint64_t> visited_pages;
			        uint64_t num_spat_fetches = 0;
				uint64_t num_spat_total_fetches = 0;	
	           		uint64_t num_pages_visited = 0;		   
		   		mapping_from_functions_to_number_of_pages[current_function_callee_address].clear();
				do{

		                    if (visited_pages.find(current_function_page) != 
		                   			visited_pages.end())
		                   		break;
		                     visited_pages.insert(current_function_page);
				     mapping_from_functions_to_number_of_pages[current_function_callee_address].insert(current_function_page);
				     num_pages_visited++;
		                     function_and_page temp;
		                     temp.function = current_function_callee_address;
		                     temp.page = current_function_page;
		                     if (spatial_locality_from_function_start_and_confidence[temp].confidence >= 2){
		                           for (size_t i=0; i<spatial_locality_from_function_start_and_confidence[temp].spatial_locality_vector.size();
		                         		 ++i){
		                                   //issue a spatial fetch for the corresponding cache blocks. 
    		                             
		                	      uint64_t root_function_block = temp.page*4096; 
		                           if (spatial_locality_from_function_start_and_confidence[temp].spatial_locality_vector.test(i)){
			      	      		   hit_and_use_information temp2;
						   bool spatial_fetch = true;
       		                           	       uint64_t blk_addr_to_fetch = root_function_block + i*64;
		                     	    temp2 =  il1->AccessSingleLine_selective_allocate(blk_addr_to_fetch, CACHE_BASE::ACCESS_TYPE_LOAD, 
		                            		true, true, false, false,future_reference_timestamp, warmup_finished,
		                            		1.0, current_function_callee_address,
		                            		current_function_invocation_count, spatial_fetch);
				    		 if (!temp2.icache_hit){
				    		         blk_addresses_evicted_due_to_spatial_fetch.push_back(temp2.blk_addresses.at(0));
				    		 }
				    		   num_spat_fetches++;
						   num_spat_total_fetches++;
		                            }
		                        }
				      }
		                      else{
		                               for (size_t i=0; i<spatial_locality_from_function_start_and_confidence[temp].spatial_locality_vector_to_use_because_of_low_confidence.size();
		                                 	 ++i){
		                                //issue a spatial fetch for the corresponding cache blocks. 
		                                 uint64_t root_function_block = temp.page*4096; 
		                              if (spatial_locality_from_function_start_and_confidence[temp].spatial_locality_vector_to_use_because_of_low_confidence.test(i)){
						      if (current_function_callee_address == CALLEE_OF_INTEREST)
							cout << "Low confidence (multi) spatial fetch" << endl; 
						      hit_and_use_information temp2;
						      bool spatial_fetch = true;
       		                              	       uint64_t blk_addr_to_fetch = root_function_block + i*64;
       		                               	temp2 = il1->AccessSingleLine_selective_allocate(blk_addr_to_fetch, CACHE_BASE::ACCESS_TYPE_LOAD, 
		                               		true, true, false, false,future_reference_timestamp, warmup_finished,
		                               		1.0, current_function_callee_address,
		                               		current_function_invocation_count, spatial_fetch);
					           
						     if (!temp2.icache_hit){
				             		     blk_addresses_evicted_due_to_spatial_fetch.push_back(temp2.blk_addresses.at(0));
						     }
						     num_spat_fetches++;
						     num_spat_total_fetches++;
		                                }
		                              }
		                      }


			          num_iters++;
			 	  if (num_iters >= 10)
					cout <<"What?? " << current_function_callee_address << endl;

				   current_function_page = 
					   mapping_from_function_and_page_to_forward_pointers[current_function_callee_address][current_function_page];
		                  
				 }while(current_function_page != 0);
				//relearn pointers post spatial prefetch	
		   		mapping_from_function_and_page_to_forward_pointers[current_function_callee_address][start_page] = 0;
				visited_pages.clear();
				if (num_spat_total_fetches!=0){
				  	if (current_function_callee_address == CALLEE_OF_INTEREST)
						cout << "Num spat fetches are " << num_spat_fetches << endl;
					mapping_from_function_to_spatial_prefetch_info[current_function_callee_address].total_block_prefetches += num_spat_fetches;
					mapping_from_function_to_spatial_prefetch_info[current_function_callee_address].total_prefetches++;
				}
		   	      // spatial_locality_within_10_cache_blocks_from_function_start[temp].reset();
		   	       spatial_locality_within_10_cache_blocks_from_function_start[temp2].set(((addr/64)&63));
		               //don't want to reset this bit for the block if it is already a 
		               //root of some other function to start with. 
                              
		               if (block_to_is_function_root.find(addr/64) == block_to_is_function_root.end())
		                   block_to_is_function_root[addr/64] = false;
			   }
		   }
		   //in this case, atleast update the bit vector. 
		   if (!need_to_prefetch){
		       function_and_page current_func_page;
		       current_func_page.function = current_function_callee_address;
		       current_func_page.page = addr/4096;
		        if (!temp1.icache_hit && (current_function_callee_address == CALLEE_OF_INTEREST)){
                           int64_t distance_from_root_of_function_cache_block = addr/64 - current_function_callee_address/64;
		           cout << "Miss "<< current_func_page.page<<  " and distance (single) from root is " <<distance_from_root_of_function_cache_block << " (calls seen so far)" << function_call_seen_so_far << endl;
		        }
		        if (temp1.spatial_fetch_hit && (current_function_callee_address == CALLEE_OF_INTEREST)){
                           int64_t distance_from_root_of_function_cache_block = addr/64 - current_function_callee_address/64;
		           cout << "Spatial hit " << current_func_page.page << " and distance (single) from root is " <<distance_from_root_of_function_cache_block << " (calls seen so far)" << function_call_seen_so_far << endl;
		        }
                         if (current_function_callee_address == CALLEE_OF_INTEREST){
		         	cout << "Block address is: " << addr/64<<" and page address is: " << addr/4096 << endl;
		         }
                         
		         spatial_locality_within_10_cache_blocks_from_function_start[current_func_page].set((addr/64)&63);
		         //don't want to reset this bit for the block if it is already a 
		         //root of some other function to start with. 
                        
		         if (block_to_is_function_root.find(addr/64) == block_to_is_function_root.end())
		             block_to_is_function_root[addr/64] = false;
		   }
	       }
               else {
		     
		    function_and_page current_func_page;
		    current_func_page.function = current_function_callee_address;
		    current_func_page.page = addr/4096;
		     if (!temp1.icache_hit && (current_function_callee_address == CALLEE_OF_INTEREST)){
                        int64_t distance_from_root_of_function_cache_block = addr/64 - current_function_callee_address/64;
		        cout << "Miss "<< current_func_page.page<<  " and distance (single) from root is " <<distance_from_root_of_function_cache_block << " (calls seen so far)" << function_call_seen_so_far << endl;
		     }
		     if (temp1.spatial_fetch_hit && (current_function_callee_address == CALLEE_OF_INTEREST)){
                        int64_t distance_from_root_of_function_cache_block = addr/64 - current_function_callee_address/64;
		        cout << "Spatial hit " << current_func_page.page << " and distance (single) from root is " <<distance_from_root_of_function_cache_block << " (calls seen so far)" << function_call_seen_so_far << endl;
		     }
                      if (current_function_callee_address == CALLEE_OF_INTEREST){
		      	cout << "Block address is: " << addr/64<<" and page address is: " << addr/4096 << endl;
		      }
                      
		      spatial_locality_within_10_cache_blocks_from_function_start[current_func_page].set((addr/64)&63);
		      //don't want to reset this bit for the block if it is already a 
		      //root of some other function to start with. 
                     
		       
               }
	   //}
	}
   	 
	if (!temp1.icache_hit){
	   //when the root of the function is evicted, stop recording the 
	   //spatial locality vector.
	  
	  for(std::vector<uint64_t>::iterator it = temp1.blk_addresses.begin(); it != temp1.blk_addresses.end(); ++it) {
	        uint64_t blk_addr = (*it)/64;
		//if it is the root of a function
		if (block_to_is_function_root[blk_addr]){
		   //reset this bit now.will get set upon insertion
		   block_to_is_function_root[blk_addr] = false;
		   //find the corresponding callee address and reset the bitvector. 
		   uint64_t callee_address = block_to_corresponding_function[blk_addr];
		  // if (callee_address == CALLEE_OF_INTEREST){
		   
		   if (callee_address == CALLEE_OF_INTEREST)
			   cout << "Evicting callee of interest (multi) " << endl;
		   //learning spatial bit vector when the root of the function is evicted from the instruction cache. 

	           //start with the current page.Chase pointers till we hit NULL.  
	           uint64_t current_function_page = callee_address/4096;
	          
		   uint64_t num_iters = 0;
		   
		   set<uint64_t> visited_pages;
		   do{

			if (visited_pages.find(current_function_page) != 
					visited_pages.end())
				break;
			visited_pages.insert(current_function_page);
			function_and_page temp;
			temp.function = callee_address;
			temp.page = current_function_page;
		         if (spatial_locality_from_function_start_and_confidence.find(temp) == 
		              	  spatial_locality_from_function_start_and_confidence.end()){
		                spatial_locality_from_function_start_and_confidence[temp].spatial_locality_vector =  
		              	  spatial_locality_within_10_cache_blocks_from_function_start[temp];
		         	  spatial_locality_from_function_start_and_confidence[temp].confidence = 
		              	  1;
		         }
		         else{
		              if (spatial_locality_from_function_start_and_confidence[temp].spatial_locality_vector == 
		              		spatial_locality_within_10_cache_blocks_from_function_start[temp]){
		              	spatial_locality_from_function_start_and_confidence[temp].confidence = min(
		              		(int)(spatial_locality_from_function_start_and_confidence[temp].confidence+1),4);
				if (callee_address == CALLEE_OF_INTEREST){
					cout << "Matches earlier and page is " << current_function_page << endl;
		              
		               		 std::stringstream buffer;
                               		 buffer << spatial_locality_from_function_start_and_confidence[temp].spatial_locality_vector <<","<<spatial_locality_from_function_start_and_confidence[temp].confidence <<endl;
	                       		 string str =  buffer.str();
		               		 cout <<"Old vector: " <<  str;
		               		 
		               		 std::stringstream buffer_1;
                               		 buffer_1 << spatial_locality_within_10_cache_blocks_from_function_start[temp] <<endl;
	                       		 str =  buffer_1.str();
		               		 cout <<"New vector: " <<  str;
				}
			      }
		              else{
				if (callee_address == CALLEE_OF_INTEREST){
					cout << "Does not match earlier and page is " << current_function_page << endl;
		        	      	
		        	        std::stringstream buffer;
                        	        buffer << spatial_locality_from_function_start_and_confidence[temp].spatial_locality_vector <<","<<spatial_locality_from_function_start_and_confidence[temp].confidence <<endl;
	                	        string str =  buffer.str();
		        	        cout <<"Old vector: " <<  str;
		        		
		        	        std::stringstream buffer_1;
                        	        buffer_1 << spatial_locality_within_10_cache_blocks_from_function_start[temp] <<endl;
	                	        str =  buffer_1.str();
		        	        cout <<"New vector: " <<  str;
				}	
				if (spatial_locality_from_function_start_and_confidence[temp].confidence == 0){
		              	   spatial_locality_from_function_start_and_confidence[temp].spatial_locality_vector =
		              	   	spatial_locality_within_10_cache_blocks_from_function_start[temp];
		              	   spatial_locality_from_function_start_and_confidence[temp].confidence = 1;	   
		              	}
		              	else{
		              	   spatial_locality_from_function_start_and_confidence[temp].confidence--;	  
			           if (spatial_locality_from_function_start_and_confidence[temp].confidence <= 1){
					   spatial_locality_from_function_start_and_confidence[temp].spatial_locality_vector_to_use_because_of_low_confidence = 
						   spatial_locality_from_function_start_and_confidence[temp].spatial_locality_vector&spatial_locality_within_10_cache_blocks_from_function_start[temp];
		                  //         std::stringstream buffer;
                                  //         buffer << spatial_locality_from_function_start_and_confidence[temp].spatial_locality_vector_to_use_because_of_low_confidence  <<endl;
	                          //         string str =  buffer.str();
		                  //         cout << str;
				   }	   
		              	}
		              		
		              }

		         }

			 num_iters++;
			 if (num_iters >= 10)
				cout <<"What?? " << current_function_callee_address << endl;
			current_function_page = mapping_from_function_and_page_to_forward_pointers[callee_address][current_function_page];
		   }while (current_function_page!= 0);
			        
		   visited_pages.clear();
		//}
	   }
	  }
	   //count the misses per cache block 
	}
	
					           
	if (blk_addresses_evicted_due_to_spatial_fetch.size()!=0){
	   //when the root of the function is evicted, stop recording the 
	   //spatial locality vector.
	  
	  for(std::vector<uint64_t>::iterator it = blk_addresses_evicted_due_to_spatial_fetch.begin(); 
			  it != blk_addresses_evicted_due_to_spatial_fetch.end(); ++it) {
	        uint64_t blk_addr = (*it)/64;
		//if it is the root of a function
		if (block_to_is_function_root[blk_addr]){
		   //reset this bit now.will get set upon insertion
		   
		   
		  block_to_is_function_root[blk_addr] = false;

		   //find the corresponding callee address and reset the bitvector. 
		   uint64_t callee_address = block_to_corresponding_function[blk_addr];
		  // if (callee_address == CALLEE_OF_INTEREST){
		   
		   if (callee_address == CALLEE_OF_INTEREST)
			   cout << "Evicting callee of interest and page is" <<(*it)/4096  << endl;
		   //learning spatial bit vector when the root of the function is evicted from the instruction cache. 

	           //start with the current page.Chase pointers till we hit NULL.  
	           uint64_t current_function_page = callee_address/4096;
	           uint64_t num_iters = 0;
		   set<uint64_t> visited_pages;
		   do{
			if (visited_pages.find(current_function_page) != 
					visited_pages.end()){
				break;
			}
			if (callee_address == CALLEE_OF_INTEREST)
				cout <<"function page visited is " << current_function_page << endl;
			visited_pages.insert(current_function_page);
			function_and_page temp;
			temp.function = callee_address;
			temp.page = current_function_page;
		         if (spatial_locality_from_function_start_and_confidence.find(temp) == 
		              	  spatial_locality_from_function_start_and_confidence.end()){
		                spatial_locality_from_function_start_and_confidence[temp].spatial_locality_vector =  
		              	  spatial_locality_within_10_cache_blocks_from_function_start[temp];
		         	  spatial_locality_from_function_start_and_confidence[temp].confidence = 
		              	  1;
		         }
		         else{
		              if (spatial_locality_from_function_start_and_confidence[temp].spatial_locality_vector == 
		              		spatial_locality_within_10_cache_blocks_from_function_start[temp]){
		              	spatial_locality_from_function_start_and_confidence[temp].confidence = min(
		              		(int)(spatial_locality_from_function_start_and_confidence[temp].confidence+1),4);
				if (callee_address == CALLEE_OF_INTEREST)
					cout << "Matches earlier and page is " << current_function_page << endl;
				if (callee_address == CALLEE_OF_INTEREST){
		               		 std::stringstream buffer;
                               		 buffer << spatial_locality_from_function_start_and_confidence[temp].spatial_locality_vector <<","<<spatial_locality_from_function_start_and_confidence[temp].confidence <<endl;
	                       		 string str =  buffer.str();
		               		 cout <<"Old vector: " <<  str;
		               		 
		               		 std::stringstream buffer_1;
                               		 buffer_1 << spatial_locality_within_10_cache_blocks_from_function_start[temp] <<endl;
	                       		 str =  buffer_1.str();
		               		 cout <<"New vector: " <<  str;
				}
			      }
		              else{
		               	
				if (callee_address == CALLEE_OF_INTEREST)
					cout << "Does not match earlier and page is " <<current_function_page << endl;
		              	
				if (callee_address == CALLEE_OF_INTEREST){
		               		 std::stringstream buffer;
                               		 buffer << spatial_locality_from_function_start_and_confidence[temp].spatial_locality_vector <<","<<spatial_locality_from_function_start_and_confidence[temp].confidence <<endl;
	                       		 string str =  buffer.str();
		               		 cout <<"Old vector: " <<  str;
		               		 
		               		 std::stringstream buffer_1;
                               		 buffer_1 << spatial_locality_within_10_cache_blocks_from_function_start[temp] <<endl;
	                       		 str =  buffer_1.str();
		               		 cout <<"New vector: " <<  str;
				}
				if (spatial_locality_from_function_start_and_confidence[temp].confidence == 0){
		              	   spatial_locality_from_function_start_and_confidence[temp].spatial_locality_vector =
		              	   	spatial_locality_within_10_cache_blocks_from_function_start[temp];
		              	   spatial_locality_from_function_start_and_confidence[temp].confidence = 1;	   
		              	}
		              	else{
		              	   spatial_locality_from_function_start_and_confidence[temp].confidence--;	  
			           if (spatial_locality_from_function_start_and_confidence[temp].confidence <= 1){
					   spatial_locality_from_function_start_and_confidence[temp].spatial_locality_vector_to_use_because_of_low_confidence = 
						   spatial_locality_from_function_start_and_confidence[temp].spatial_locality_vector&spatial_locality_within_10_cache_blocks_from_function_start[temp];
		                  //         std::stringstream buffer;
                                  //         buffer << spatial_locality_from_function_start_and_confidence[temp].spatial_locality_vector_to_use_because_of_low_confidence  <<endl;
	                          //         string str =  buffer.str();
		                  //         cout << str;
				   }	   
		              	}
		              		
		              }

		         }
			current_function_page = mapping_from_function_and_page_to_forward_pointers[callee_address][current_function_page];
			num_iters++;
			if (num_iters >= 10){
				cout <<"What1?? " << callee_address << " and current page is and iter count is " <<current_function_page << " " <<
				       num_iters << endl;
			}
		   
		   }while (current_function_page!= 0);
		   visited_pages.clear();
		//}
	   }
	  }
	   //count the misses per cache block 
	}
	
	 call_instr_seen = false;
         ind_jump_seen = false;
         return_instr_seen = false;
         syscall_seen = false;
         dir_jump_instr_seen = false;

}


/* ===================================================================== */
VOID LoadSingleFastSimple(ADDRINT addr, uint64_t future_reference_timestamp, bool warmup_finished)
{
	hit_and_use_information temp1;
	temp1 =	il1->AccessSingleLine_selective_allocate(addr, CACHE_BASE::ACCESS_TYPE_LOAD, true, true, false, false, future_reference_timestamp, warmup_finished, 1.0,
			current_function_callee_address,
			function_invocation_count[current_function_callee_address].func_invocation_count, false);
	if ((!temp1.icache_hit) && warmup_finished)
		unused_large_block_chunks += (8-temp1.number_of_blocks_used);
}

VOID LoadSingleFast_Spatial(ADDRINT addr, uint64_t future_reference_timestamp, 
		bool warmup_finished)
{

       //first step is to identify the function we are executing, sometimes we might jump out to function 
       //to run another function and then get back to executing a function. This necessitates the use of call stack
       //to identify the function we are executing.  
         //initialize the number of cache misses for a new cache block to be 0
        //cout << "Size 1 " << mapping_from_callee_to_invocation_count.size() << endl;
	//cout << "Size 2 " << spatial_locality_from_function_start_and_confidence.size() << endl;
	if (call_instr_seen){
	    if (mapping_from_callee_to_invocation_count.find(current_function_callee_address) ==
	        	    mapping_from_callee_to_invocation_count.end()){
	           mapping_from_callee_to_invocation_count[current_function_callee_address] = 1;
	    }
	    else{
	         mapping_from_callee_to_invocation_count[current_function_callee_address]++;
	    }
	    
	    call_stack.push(current_function_callee_address);
	    call_stack_hash ^= current_function_callee_address;
	    current_function_callee_address = addr;
	    if (current_function_callee_address == CALLEE_OF_INTEREST)
		    cout << "Calling function " << function_call_seen_so_far << endl;
	    if (mapping_from_function_to_spatial_prefetch_info.find(current_function_callee_address) == 
			    mapping_from_function_to_spatial_prefetch_info.end()){
	    	mapping_from_function_to_spatial_prefetch_info[current_function_callee_address].total_prefetches = 0;
		mapping_from_function_to_spatial_prefetch_info[current_function_callee_address].total_block_prefetches = 0;
	    }
	    if (mapping_from_function_and_page_to_forward_pointers.find(current_function_callee_address) == 
			    mapping_from_function_and_page_to_forward_pointers.end()){
		    //set the forward pointer to null upon function entry. 
		    mapping_from_function_and_page_to_forward_pointers[current_function_callee_address][current_function_callee_address/4096] = 0;
            }
	    function_call_seen_so_far++;
	    
       }
       
       else if(return_instr_seen){
       //clear the number of cache blocks accessed on this function
       //invocation. 
	    //pop the cache block from the return cache block stack
	    //if we are within one cache block of what we pushed, then go ahead and pop
	    //from call stack. Else, do nothing.
	    bool should_we_pop_from_call_stack = false; 
	    if(return_cache_block_stack.size()!=0){
	    	uint64_t block_to_return_to = return_cache_block_stack.top();
		if ((block_to_return_to == (addr/64))||
				((block_to_return_to+1) == (addr/64))){
			should_we_pop_from_call_stack = true;
			return_cache_block_stack.pop();
		}
	    }
	    if ((call_stack.size()!=0) && (should_we_pop_from_call_stack)){
		current_function_callee_address = call_stack.top();
		call_stack_hash ^= current_function_callee_address;
		call_stack.pop();
	    }
	}
       set_of_unique_instruction_pages_touched_by_program.insert(addr/4096);
       //store the page and the corresponding function.
       uint64_t current_page = addr/4096;
       if (current_function_callee_address == CALLEE_OF_INTEREST){
		if (current_page_in_function_of_interest  != 
			current_page){
		    if (ind_jump_seen)
		     cout << current_page << "(ind jump),";
	            else if (dir_jump_instr_seen)
		     cout << current_page << "(dir jump),";
		    else if (syscall_seen)
		     cout << current_page << "(syscall),";
		    else if (call_instr_seen)
		     cout << current_page << "(call),";
		    else
		     cout << current_page << "(" << future_reference_timestamp <<  "),";
		}	
	//	mapping_from_function_and_page_to_forward_pointers[current_function_callee_address][current_page_in_function_of_interest] = current_page;
	//	//if no forward pointer set for this function page pair, then set it to NULL to start with. 
	//	if (mapping_from_function_and_page_to_forward_pointers[current_function_callee_address].find(current_page) == 
	//			mapping_from_function_and_page_to_forward_pointers[current_function_callee_address].end())
	//		mapping_from_function_and_page_to_forward_pointers[current_function_callee_address][current_page] = 0;
		current_page_in_function_of_interest = current_page;
	}	

        //when we move to another page within the same function. 
	if (current_page !=
			mapping_from_function_to_current_function_page_on_which_we_are[current_function_callee_address]){
		uint64_t current_page_in_function_of_interest = mapping_from_function_to_current_function_page_on_which_we_are[current_function_callee_address];
		if (mapping_from_function_and_page_to_forward_pointers[current_function_callee_address].find(current_page_in_function_of_interest) != 
				mapping_from_function_and_page_to_forward_pointers[current_function_callee_address].end()){
			uint64_t temp =  mapping_from_function_and_page_to_forward_pointers[current_function_callee_address][current_page_in_function_of_interest];
			//only store a list of non-zero forward pointers per page.
			if (temp!= 0)
				mapping_from_page_to_forward_pointers[current_page_in_function_of_interest].erase(temp);
		}
		
	//	if (mapping_from_function_to_current_function_page_on_which_we_are[current_function_callee_address] == 133709040){
	//	   if (ind_jump_seen)
	//		   cout << "Ind jump " << endl;
	//	   else if (dir_jump_instr_seen)
	//		   cout << "Dir jump " << endl;
	//	   else if (call_instr_seen)
	//		   cout <<"Call instr " << endl;
	//	   else
	//		   cout <<"Diff cfl" << endl;
	//	 }
		mapping_from_page_to_forward_pointers[current_page_in_function_of_interest].insert(current_page);
		mapping_from_function_and_page_to_forward_pointers[current_function_callee_address][current_page_in_function_of_interest] = current_page;
		//if no forward pointer set for this function page pair, then set it to NULL to start with. 
		if (mapping_from_function_and_page_to_forward_pointers[current_function_callee_address].find(current_page) == 
				mapping_from_function_and_page_to_forward_pointers[current_function_callee_address].end())
			mapping_from_function_and_page_to_forward_pointers[current_function_callee_address][current_page] = 0;
		mapping_from_function_to_current_function_page_on_which_we_are[current_function_callee_address] = current_page;
	}
	total_set_of_functions.insert(current_function_callee_address);
	 uint64_t current_function_invocation_count;
	 current_function_invocation_count = function_invocation_count[current_function_callee_address].func_invocation_count; 
	 //for every cache block, set a degree of use based on how much use it has seen across 
	 //invocations of functions.
	 hit_and_use_information temp1;
	 //set the degree of use flag to true for code
       //from functions with a high degree of use. 
       //because degree of use affects placement in the cache, allow for a few misses before we start to place functions
       //assuming they are a low use function.  
	temp1 = il1->AccessSingleLine_selective_allocate(addr, CACHE_BASE::ACCESS_TYPE_LOAD, true, false, false, false,future_reference_timestamp, warmup_finished,
	       	 1.0, current_function_callee_address,
	       	 current_function_invocation_count, false);
	
	if (!temp1.icache_hit){	
           function_and_inter_page_distance temp;
           temp.page_distance = current_page;
           temp.function = current_function_callee_address;
           if (functions_that_jumps_to_distant_pages.find(temp) == functions_that_jumps_to_distant_pages.end())
		   functions_that_jumps_to_distant_pages[temp] = 1;
	   else
		   functions_that_jumps_to_distant_pages[temp]++;
	}
	function_to_unique_pages_touched[current_function_callee_address].insert(addr/4096);

	vector<uint64_t> blk_addresses_evicted_due_to_spatial_fetch;
	if ((!temp1.icache_hit)||(temp1.spatial_fetch_hit)){
//		if (current_function_callee_address == CALLEE_OF_INTEREST){
		if (call_instr_seen){
		  if (current_function_callee_address == CALLEE_OF_INTEREST){
		 	 if (!temp1.icache_hit)
		 	 	cout << "Callee miss " << endl;
		 	 else
		 	 	cout << "Callee spatial hit " << endl;
		  }
		 // std::stringstream buffer;
                 // buffer << spatial_locality_from_function_start_and_confidence[current_function_callee_address].spatial_locality_vector <<","<<spatial_locality_from_function_start_and_confidence[current_function_callee_address].confidence <<endl;
	         // string str =  buffer.str();
		 // cout << str;
		 // issue spatial prefetch only when we have some amount of confidence on the block access pattern. 
		   
		   function_and_page callee_func_page;
		   callee_func_page.function = current_function_callee_address;
		   callee_func_page.page = addr/4096;
		   //start with the current page.Chase pointers till we hit NULL.  
		   uint64_t current_function_page = current_function_callee_address/4096;
		   uint64_t start_page = current_function_callee_address/4096; 
		   uint64_t num_iters = 0;
		   
		   set<uint64_t> visited_pages;
		   uint64_t num_spat_fetches = 0;
		   uint64_t num_spat_total_fetches = 0;
		   uint64_t num_pages = 0;
	           uint64_t num_pages_visited = 0;		   
		   mapping_from_functions_to_number_of_pages[current_function_callee_address].clear();
		   do{
			num_pages++;
			if (current_function_callee_address == CALLEE_OF_INTEREST){
				cout << "Page is " << current_function_page << endl;
				cout << "Size is " << mapping_from_functions_to_number_of_pages[current_function_callee_address].size() << endl;
			}
		 	if (visited_pages.find(current_function_page) != 
					visited_pages.end())
				break;
			visited_pages.insert(current_function_page);
			mapping_from_functions_to_number_of_pages[current_function_callee_address].insert(current_function_page);
			num_pages_visited++;
			
				     
			function_and_page temp;
			temp.function = current_function_callee_address;
			temp.page = current_function_page;
			if (spatial_locality_from_function_start_and_confidence[temp].confidence >= 2){
			      for (size_t i=0; i<spatial_locality_from_function_start_and_confidence[temp].spatial_locality_vector.size();
		  	   		 ++i){
		  	       //issue a spatial fetch for the corresponding cache blocks. 
		   	      uint64_t root_function_block = temp.page*4096; 
			      if (spatial_locality_from_function_start_and_confidence[temp].spatial_locality_vector.test(i)){
		  	             
			      	      hit_and_use_information temp2;
				      bool spatial_fetch = true;
       		  	     	       uint64_t blk_addr_to_fetch = root_function_block + i*64;
				     temp2 = il1->AccessSingleLine_selective_allocate(blk_addr_to_fetch, CACHE_BASE::ACCESS_TYPE_LOAD, 
		  	      		true, true, false, false,future_reference_timestamp, warmup_finished,
		  	      		1.0, current_function_callee_address,
		  	      		current_function_invocation_count, spatial_fetch);
				     if (!temp2.icache_hit){
					     blk_addresses_evicted_due_to_spatial_fetch.push_back(temp2.blk_addresses.at(0));
				     }
				     num_spat_fetches++;
				     num_spat_total_fetches++;
		  	       }
		  	    }
			    if (num_spat_fetches != 0){
				if (current_function_callee_address == CALLEE_OF_INTEREST)
			  		cout << "High confidence spatial fetch performed for the function of interest" << endl; 
			    } 
			      spatial_locality_within_10_cache_blocks_from_function_start[temp].reset();
		  	 }

			 else{
		  	      for (size_t i=0; i<spatial_locality_from_function_start_and_confidence[temp].spatial_locality_vector_to_use_because_of_low_confidence.size();
		  	   		 ++i){
		  	       //issue a spatial fetch for the corresponding cache blocks. 
		   	      uint64_t root_function_block = temp.page*4096; 
			      if (spatial_locality_from_function_start_and_confidence[temp].spatial_locality_vector_to_use_because_of_low_confidence.test(i)){
			      	      hit_and_use_information temp2;
				      bool spatial_fetch = true;
       		  	     	       uint64_t blk_addr_to_fetch = root_function_block + i*64;
       		  	      	   temp2 = il1->AccessSingleLine_selective_allocate(blk_addr_to_fetch, CACHE_BASE::ACCESS_TYPE_LOAD, 
		  	      		true, true, false, false,future_reference_timestamp, warmup_finished,
		  	      		1.0, current_function_callee_address,
		  	      		current_function_invocation_count, spatial_fetch);
				     if (!temp2.icache_hit){
					     blk_addresses_evicted_due_to_spatial_fetch.push_back(temp2.blk_addresses.at(0));
				     }
				     num_spat_fetches++;
				     num_spat_total_fetches++;
		  	       }
		  	    }
			    if (num_spat_fetches != 0){
				if (current_function_callee_address == CALLEE_OF_INTEREST)
			  		cout << "Low confidence spatial fetch performed for the function of interest" << endl; 
			    } 
			   spatial_locality_within_10_cache_blocks_from_function_start[temp].reset();
			 }
			 num_iters++;
			 if (num_iters >= 10)
				cout <<"What?? " << current_function_callee_address << endl;
			 current_function_page = mapping_from_function_and_page_to_forward_pointers[current_function_callee_address][current_function_page];
		   }while (current_function_page != 0);
		 //  if (current_function_callee_address == CALLEE_OF_INTEREST)
		 //  	cout << "The number of spatial fetches for callee of interest is " << num_spat_fetches << " and number of pages traversed is " << num_pages << endl;
		   if (num_spat_total_fetches!=0){
			if (current_function_callee_address == CALLEE_OF_INTEREST)
				cout << "Num spat fetches are " << num_spat_fetches << endl;
			mapping_from_function_to_spatial_prefetch_info[current_function_callee_address].total_block_prefetches += num_spat_fetches;
			mapping_from_function_to_spatial_prefetch_info[current_function_callee_address].total_prefetches++;
		   }

		   visited_pages.clear();
		   //relearn pointers post spatial prefetch	
		   mapping_from_function_and_page_to_forward_pointers[current_function_callee_address][start_page] = 0;
		   //don't set the bit corresponding to callee as we are anyways fetching it on a miss.
		  // spatial_locality_within_10_cache_blocks_from_function_start[callee_func_page].set((addr/64)&63);
		   
		   if (block_to_is_function_root.find(addr/64) == block_to_is_function_root.end())
			block_to_is_function_root[addr/64] = false;
		   block_to_is_function_root[addr/64] = true;
                   if (current_function_callee_address == CALLEE_OF_INTEREST)
			   cout << "Setting this root of the function field for the current function " << endl;
		   block_to_corresponding_function[addr/64] = current_function_callee_address; 
               }
	       //check if the block is a miss and it could've been a part of the spatial fetched. 
	       //
	       else if (!temp1.icache_hit){
	       //check if the block is close to the root of the function. 
                   int64_t distance_from_root_of_function_cache_block = addr/64 - current_function_callee_address/64;
		  // cout << "Miss "<< addr/4096<<  " and distance (single) from root is " <<distance_from_root_of_function_cache_block << " (calls seen so far)" << function_call_seen_so_far << endl;
		   //check if the block is in the bitvector 
		   bool need_to_prefetch = false;
		   if (distance_from_root_of_function_cache_block <=5){
			uint64_t page = addr/4096;
			uint64_t start_page = addr/4096;
			function_and_page temp, temp2;
			temp2.function = current_function_callee_address;
			temp2.page = page;
			if (spatial_locality_from_function_start_and_confidence[temp2].confidence >= 2){
			 if (spatial_locality_from_function_start_and_confidence[temp2].spatial_locality_vector.test((addr/64)&63)){
			        //issue a spatial fetch for the corresponding cache blocks. 
				need_to_prefetch = true;
			//	cout << "performing spatial fetch1 (non callee)" << endl; 
		          }
	                }		 
			else{
			  if (spatial_locality_from_function_start_and_confidence[temp2].spatial_locality_vector.test((addr/64)&63)){
		  	  //        cout << "performing spatial fetch2 (non callee)" << endl; 
			          //issue a spatial fetch for the corresponding cache blocks. 
			          need_to_prefetch = true;
			  }
			} 
		        if (need_to_prefetch){	
		                //start with the current page.Chase pointers till we hit NULL.  
		                uint64_t current_function_page = current_function_callee_address/4096;
		                uint64_t num_iters = 0; 
		   		set<uint64_t> visited_pages;
	           		uint64_t num_pages_visited = 0;		   
		   		uint64_t num_spat_fetches = 0;
				uint64_t num_spat_total_fetches = 0;
				mapping_from_functions_to_number_of_pages[current_function_callee_address].clear();
				do{

				    if (visited_pages.find(current_function_page) != 
							visited_pages.end()){
					 break;
				     }
				     visited_pages.insert(current_function_page);
		                     function_and_page temp;
				     mapping_from_functions_to_number_of_pages[current_function_callee_address].insert(current_function_page);
				     num_pages_visited++;
				     temp.function = current_function_callee_address;
		                     temp.page = current_function_page;
		                     if (spatial_locality_from_function_start_and_confidence[temp].confidence >= 2){
		                           for (size_t i=0; i<spatial_locality_from_function_start_and_confidence[temp].spatial_locality_vector.size();
		                         		 ++i){
		                                   //issue a spatial fetch for the corresponding cache blocks. 
    		                             
		                	      uint64_t root_function_block = temp.page*4096; 
		                           if (spatial_locality_from_function_start_and_confidence[temp].spatial_locality_vector.test(i)){
			      	      		   hit_and_use_information temp2;
						   bool spatial_fetch = true;
       		                           	       uint64_t blk_addr_to_fetch = root_function_block + i*64;
		                     	    	temp2 =  il1->AccessSingleLine_selective_allocate(blk_addr_to_fetch, CACHE_BASE::ACCESS_TYPE_LOAD, 
		                            		true, true, false, false,future_reference_timestamp, warmup_finished,
		                            		1.0, current_function_callee_address,
		                            		current_function_invocation_count, spatial_fetch);
						     if (!temp2.icache_hit){
				             		     blk_addresses_evicted_due_to_spatial_fetch.push_back(temp2.blk_addresses.at(0));
						     }
						     num_spat_fetches++;
						     num_spat_total_fetches++;
		                               }
		                            }
			   		    if (num_spat_fetches != 0){
			   		        if (current_function_callee_address == CALLEE_OF_INTEREST)
			   		        	cout << "High confidence spatial fetch performed for the function of interest" << endl; 
			   		    } 
		                      }
		                      else{
		                               for (size_t i=0; i<spatial_locality_from_function_start_and_confidence[temp].spatial_locality_vector_to_use_because_of_low_confidence.size();
		                                 	 ++i){
		                                //issue a spatial fetch for the corresponding cache blocks. 
		                                 uint64_t root_function_block = temp.page*4096; 
		                              if (spatial_locality_from_function_start_and_confidence[temp].spatial_locality_vector_to_use_because_of_low_confidence.test(i)){
						      hit_and_use_information temp2;
						      bool spatial_fetch = true;
       		                              	       uint64_t blk_addr_to_fetch = root_function_block + i*64;
       		                                 temp2 = il1->AccessSingleLine_selective_allocate(blk_addr_to_fetch, CACHE_BASE::ACCESS_TYPE_LOAD, 
		                               		true, true, false, false,future_reference_timestamp, warmup_finished,
		                               		1.0, current_function_callee_address,
		                               		current_function_invocation_count, spatial_fetch);
						     if (!temp2.icache_hit){
				             		     
							     blk_addresses_evicted_due_to_spatial_fetch.push_back(temp2.blk_addresses.at(0));
						     }
						     num_spat_fetches++;
						     num_spat_total_fetches++;
		                                }
		                              }
			   		      if (num_spat_fetches != 0){
			   		          if (current_function_callee_address == CALLEE_OF_INTEREST)
			   		          	cout << "Low confidence spatial fetch performed for the function of interest" << endl; 
			   		      } 
		                      }
			          num_iters++;
			          if (num_iters >= 10)
			         	cout <<"What?? " << current_function_callee_address << endl;
				   current_function_page = 
					   mapping_from_function_and_page_to_forward_pointers[current_function_callee_address][current_function_page];
				}while(current_function_page != 0);
				//relearn pointers post spatial fetch	
		   		mapping_from_function_and_page_to_forward_pointers[current_function_callee_address][start_page] = 0;
		   		if (num_spat_total_fetches!=0){
					if (current_function_callee_address == CALLEE_OF_INTEREST)
						cout << "Num spat fetches are " << num_spat_fetches << endl;
					mapping_from_function_to_spatial_prefetch_info[current_function_callee_address].total_block_prefetches += num_spat_fetches;
					mapping_from_function_to_spatial_prefetch_info[current_function_callee_address].total_prefetches++;
				}
				visited_pages.clear();
		   	      // spatial_locality_within_10_cache_blocks_from_function_start[temp].reset();
		   	       spatial_locality_within_10_cache_blocks_from_function_start[temp2].set(((addr/64)&63));
		               //don't want to reset this bit for the block if it is already a 
		               //root of some other function to start with. 
		               if (block_to_is_function_root.find(addr/64) == block_to_is_function_root.end())
		                   block_to_is_function_root[addr/64] = false;
			   }
		   }
		   //in this case, atleast update the bit vector. 
		   if (!need_to_prefetch){
		       function_and_page current_func_page;
		       current_func_page.function = current_function_callee_address;
		       current_func_page.page = addr/4096;
		        if (!temp1.icache_hit && (current_function_callee_address == CALLEE_OF_INTEREST)){
                           int64_t distance_from_root_of_function_cache_block = addr/64 - current_function_callee_address/64;
		           cout << "Miss "<< current_func_page.page<<  " and distance (single) from root is " <<distance_from_root_of_function_cache_block << " (calls seen so far)" << function_call_seen_so_far << endl;
		        }
		        if (temp1.spatial_fetch_hit && (current_function_callee_address == CALLEE_OF_INTEREST)){
                           int64_t distance_from_root_of_function_cache_block = addr/64 - current_function_callee_address/64;
		           cout << "Spatial hit " << current_func_page.page << " and distance (single) from root is " <<distance_from_root_of_function_cache_block << " (calls seen so far)" << function_call_seen_so_far << endl;
		        }
                         if (current_function_callee_address == CALLEE_OF_INTEREST){
		         	cout << "Block address is: " << addr/64<<" and page address is: " << addr/4096 << endl;
		         }
		         if (current_function_callee_address == CALLEE_OF_INTEREST){ 
				 std::stringstream buffer_1;
                        	 buffer_1 << spatial_locality_within_10_cache_blocks_from_function_start[current_func_page] <<endl;
	                	 string str =  buffer_1.str();
		        	 cout <<"Vector contents: "<< " page is " <<addr/4096 <<" " << str << endl;
			 }
		         
			 spatial_locality_within_10_cache_blocks_from_function_start[current_func_page].set((addr/64)&63);
		         //don't want to reset this bit for the block if it is already a 
		         //root of some other function to start with. 
                        
		         if (block_to_is_function_root.find(addr/64) == block_to_is_function_root.end())
		             block_to_is_function_root[addr/64] = false;
		   }
	       }
               else {
		     
		    function_and_page current_func_page;
		    current_func_page.function = current_function_callee_address;
		    current_func_page.page = addr/4096;
		     if (!temp1.icache_hit && (current_function_callee_address == CALLEE_OF_INTEREST)){
                        int64_t distance_from_root_of_function_cache_block = addr/64 - current_function_callee_address/64;
		        cout << "Miss "<< current_func_page.page<<  " and distance (single) from root is " <<distance_from_root_of_function_cache_block << " (calls seen so far)" << function_call_seen_so_far << endl;
		     }
		     if (temp1.spatial_fetch_hit && (current_function_callee_address == CALLEE_OF_INTEREST)){
                        int64_t distance_from_root_of_function_cache_block = addr/64 - current_function_callee_address/64;
		        cout << "Spatial hit " << current_func_page.page << " and distance (single) from root is " <<distance_from_root_of_function_cache_block << " (calls seen so far)" << function_call_seen_so_far << endl;
		     }
                      if (current_function_callee_address == CALLEE_OF_INTEREST){
		      	cout << "Block address is: " << addr/64<<" and page address is: " << addr/4096 << endl;
		      }
                    
		         if (current_function_callee_address == CALLEE_OF_INTEREST){ 
				 std::stringstream buffer_1;
				 string str;
                        	 buffer_1 << spatial_locality_within_10_cache_blocks_from_function_start[current_func_page] <<endl;
	                	 str =  buffer_1.str();
		        	 cout <<"Vector contents: "<< " page is " <<addr/4096 <<" " << str << endl;
			 }
		      spatial_locality_within_10_cache_blocks_from_function_start[current_func_page].set((addr/64)&63);
		      //don't want to reset this bit for the block if it is already a 
		      //root of some other function to start with. 
                     
		      if (block_to_is_function_root.find(addr/64) == block_to_is_function_root.end())
			  block_to_is_function_root[addr/64] = false;
		       
               }
	   //}
	}
   	 
	if (!temp1.icache_hit){
	   //when the root of the function is evicted, stop recording the 
	   //spatial locality vector.
	  
	  for(std::vector<uint64_t>::iterator it = temp1.blk_addresses.begin(); it != temp1.blk_addresses.end(); ++it) {
	        uint64_t blk_addr = (*it)/64;
		//if it is the root of a function
		if (block_to_is_function_root[blk_addr]){
		   //reset this bit now.will get set upon insertion
		   block_to_is_function_root[blk_addr] = false;

		   //find the corresponding callee address and reset the bitvector. 
		   uint64_t callee_address = block_to_corresponding_function[blk_addr];
		  // if (callee_address == CALLEE_OF_INTEREST){
		   
		   if (callee_address == CALLEE_OF_INTEREST)
			   cout << "Evicting callee of interest and page is" <<(*it)/4096  << endl;
		   //learning spatial bit vector when the root of the function is evicted from the instruction cache. 

	           //start with the current page.Chase pointers till we hit NULL.  
	           uint64_t current_function_page = callee_address/4096;
	           uint64_t num_iters = 0;
		   set<uint64_t> visited_pages;
		   do{
			if (visited_pages.find(current_function_page) != 
					visited_pages.end()){
				break;
			}
			if (callee_address == CALLEE_OF_INTEREST)
				cout <<"function page visited is " << current_function_page << endl;
			visited_pages.insert(current_function_page);
			function_and_page temp;
			temp.function = callee_address;
			temp.page = current_function_page;
		         if (spatial_locality_from_function_start_and_confidence.find(temp) == 
		              	  spatial_locality_from_function_start_and_confidence.end()){
		                spatial_locality_from_function_start_and_confidence[temp].spatial_locality_vector =  
		              	  spatial_locality_within_10_cache_blocks_from_function_start[temp];
		         	  spatial_locality_from_function_start_and_confidence[temp].confidence = 
		              	  1;
		         }
		         else{
		              if (spatial_locality_from_function_start_and_confidence[temp].spatial_locality_vector == 
		              		spatial_locality_within_10_cache_blocks_from_function_start[temp]){
		              	spatial_locality_from_function_start_and_confidence[temp].confidence = min(
		              		(int)(spatial_locality_from_function_start_and_confidence[temp].confidence+1),4);
				if (callee_address == CALLEE_OF_INTEREST)
					cout << "Matches earlier and page is " << current_function_page << endl;
				if (callee_address == CALLEE_OF_INTEREST){
		               		 std::stringstream buffer;
                               		 buffer << spatial_locality_from_function_start_and_confidence[temp].spatial_locality_vector <<","<<spatial_locality_from_function_start_and_confidence[temp].confidence <<endl;
	                       		 string str =  buffer.str();
		               		 cout <<"Old vector: " <<  str;
		               		 
		               		 std::stringstream buffer_1;
                               		 buffer_1 << spatial_locality_within_10_cache_blocks_from_function_start[temp] <<endl;
	                       		 str =  buffer_1.str();
		               		 cout <<"New vector: " <<  str;
				}
			      }
		              else{
		               	
				if (callee_address == CALLEE_OF_INTEREST)
					cout << "Does not match earlier and page is " <<current_function_page << endl;
		              	
				if (callee_address == CALLEE_OF_INTEREST){
		               		 std::stringstream buffer;
                               		 buffer << spatial_locality_from_function_start_and_confidence[temp].spatial_locality_vector <<","<<spatial_locality_from_function_start_and_confidence[temp].confidence <<endl;
	                       		 string str =  buffer.str();
		               		 cout <<"Old vector: " <<  str;
		               		 
		               		 std::stringstream buffer_1;
                               		 buffer_1 << spatial_locality_within_10_cache_blocks_from_function_start[temp] <<endl;
	                       		 str =  buffer_1.str();
		               		 cout <<"New vector: " <<  str;
				}
				if (spatial_locality_from_function_start_and_confidence[temp].confidence == 0){
		              	   spatial_locality_from_function_start_and_confidence[temp].spatial_locality_vector =
		              	   	spatial_locality_within_10_cache_blocks_from_function_start[temp];
		              	   spatial_locality_from_function_start_and_confidence[temp].confidence = 1;	   
		              	}
		              	else{
		              	   spatial_locality_from_function_start_and_confidence[temp].confidence--;	  
			           if (spatial_locality_from_function_start_and_confidence[temp].confidence <= 1){
					   spatial_locality_from_function_start_and_confidence[temp].spatial_locality_vector_to_use_because_of_low_confidence = 
						   spatial_locality_from_function_start_and_confidence[temp].spatial_locality_vector&spatial_locality_within_10_cache_blocks_from_function_start[temp];
		                  //         std::stringstream buffer;
                                  //         buffer << spatial_locality_from_function_start_and_confidence[temp].spatial_locality_vector_to_use_because_of_low_confidence  <<endl;
	                          //         string str =  buffer.str();
		                  //         cout << str;
				   }	   
		              	}
		              		
		              }

		         }
			current_function_page = mapping_from_function_and_page_to_forward_pointers[callee_address][current_function_page];
			num_iters++;
			if (num_iters >= 10){
				cout <<"What1?? " << callee_address << " and current page is and iter count is " <<current_function_page << " " <<
				       num_iters << endl;
			}
		   
		   }while (current_function_page!= 0);
		   visited_pages.clear();
		//}
	   }
	  }
	   //count the misses per cache block 
	}
	
	if (blk_addresses_evicted_due_to_spatial_fetch.size()!=0){
	   //when the root of the function is evicted, stop recording the 
	   //spatial locality vector.
	  for(std::vector<uint64_t>::iterator it = blk_addresses_evicted_due_to_spatial_fetch.begin(); 
			  it != blk_addresses_evicted_due_to_spatial_fetch.end(); ++it) {
	        uint64_t blk_addr = (*it)/64;
		//if it is the root of a function
		if (block_to_is_function_root[blk_addr]){
		   //reset this bit now.will get set upon insertion
		   block_to_is_function_root[blk_addr] = false;

		   //find the corresponding callee address and reset the bitvector. 
		   uint64_t callee_address = block_to_corresponding_function[blk_addr];
		  // if (callee_address == CALLEE_OF_INTEREST){
		   
		   if (callee_address == CALLEE_OF_INTEREST)
			   cout << "Evicting callee of interest and page is" <<(*it)/4096  << endl;
		   //learning spatial bit vector when the root of the function is evicted from the instruction cache. 

	           //start with the current page.Chase pointers till we hit NULL.  
	           uint64_t current_function_page = callee_address/4096;
	           uint64_t num_iters = 0;
		   set<uint64_t> visited_pages;
		   do{
			if (visited_pages.find(current_function_page) != 
					visited_pages.end()){
				break;
			}
			if (callee_address == CALLEE_OF_INTEREST)
				cout <<"function page visited is " << current_function_page << endl;
			visited_pages.insert(current_function_page);
			function_and_page temp;
			temp.function = callee_address;
			temp.page = current_function_page;
		         if (spatial_locality_from_function_start_and_confidence.find(temp) == 
		              	  spatial_locality_from_function_start_and_confidence.end()){
		                spatial_locality_from_function_start_and_confidence[temp].spatial_locality_vector =  
		              	  spatial_locality_within_10_cache_blocks_from_function_start[temp];
		         	  spatial_locality_from_function_start_and_confidence[temp].confidence = 
		              	  1;
		         }
		         else{
		              if (spatial_locality_from_function_start_and_confidence[temp].spatial_locality_vector == 
		              		spatial_locality_within_10_cache_blocks_from_function_start[temp]){
		              	spatial_locality_from_function_start_and_confidence[temp].confidence = min(
		              		(int)(spatial_locality_from_function_start_and_confidence[temp].confidence+1),4);
				if (callee_address == CALLEE_OF_INTEREST)
					cout << "Matches earlier and page is " << current_function_page << endl;
				if (callee_address == CALLEE_OF_INTEREST){
		               		 std::stringstream buffer;
                               		 buffer << spatial_locality_from_function_start_and_confidence[temp].spatial_locality_vector <<","<<spatial_locality_from_function_start_and_confidence[temp].confidence <<endl;
	                       		 string str =  buffer.str();
		               		 cout <<"Old vector: " <<  str;
		               		 
		               		 std::stringstream buffer_1;
                               		 buffer_1 << spatial_locality_within_10_cache_blocks_from_function_start[temp] <<endl;
	                       		 str =  buffer_1.str();
		               		 cout <<"New vector: " <<  str;
				}
			      }
		              else{
		               	
				if (callee_address == CALLEE_OF_INTEREST)
					cout << "Does not match earlier and page is " <<current_function_page << endl;
		              	
				if (callee_address == CALLEE_OF_INTEREST){
		               		 std::stringstream buffer;
                               		 buffer << spatial_locality_from_function_start_and_confidence[temp].spatial_locality_vector <<","<<spatial_locality_from_function_start_and_confidence[temp].confidence <<endl;
	                       		 string str =  buffer.str();
		               		 cout <<"Old vector: " <<  str;
		               		 
		               		 std::stringstream buffer_1;
                               		 buffer_1 << spatial_locality_within_10_cache_blocks_from_function_start[temp] <<endl;
	                       		 str =  buffer_1.str();
		               		 cout <<"New vector: " <<  str;
				}
				if (spatial_locality_from_function_start_and_confidence[temp].confidence == 0){
		              	   spatial_locality_from_function_start_and_confidence[temp].spatial_locality_vector =
		              	   	spatial_locality_within_10_cache_blocks_from_function_start[temp];
		              	   spatial_locality_from_function_start_and_confidence[temp].confidence = 1;	   
		              	}
		              	else{
		              	   spatial_locality_from_function_start_and_confidence[temp].confidence--;	  
			           if (spatial_locality_from_function_start_and_confidence[temp].confidence <= 1){
					   spatial_locality_from_function_start_and_confidence[temp].spatial_locality_vector_to_use_because_of_low_confidence = 
						   spatial_locality_from_function_start_and_confidence[temp].spatial_locality_vector&spatial_locality_within_10_cache_blocks_from_function_start[temp];
		                  //         std::stringstream buffer;
                                  //         buffer << spatial_locality_from_function_start_and_confidence[temp].spatial_locality_vector_to_use_because_of_low_confidence  <<endl;
	                          //         string str =  buffer.str();
		                  //         cout << str;
				   }	   
		              	}
		              		
		              }

		         }
			current_function_page = mapping_from_function_and_page_to_forward_pointers[callee_address][current_function_page];
			num_iters++;
			if (num_iters >= 10){
				cout <<"What1?? " << callee_address << " and current page is and iter count is " <<current_function_page << " " <<
				       num_iters << endl;
			}
		   
		   }while (current_function_page!= 0);
		   visited_pages.clear();
		//}
	   }
	  }
	   //count the misses per cache block 
	}
	
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
    out << "For every function, printing number of unique functions called and the count of calls made to that function by this function " << endl;
    out << "Unused block chunks " << unused_large_block_chunks << endl; 

    out << "Total functions accessing only close by pages " << functions_that_only_jump_to_pages_close.size() << endl;
    out << "Total functions accessing distant pages " << functions_that_jumps_to_distant_pages.size() << endl;
    out << "Total functions in the program " << total_set_of_functions.size() << endl;  
     
  //   for(std::set<uint64_t>::iterator iter =  set_of_unique_instruction_pages_touched_by_program.begin(); 
  //      	    iter != set_of_unique_instruction_pages_touched_by_program.end(); ++iter)
  //  {
  //      uint64_t page_of_interest = *(iter);
  //      out << "The page of interest is : " << page_of_interest << " and number of unique forward pointers are " << mapping_from_page_to_forward_pointers[page_of_interest].size() 
  //             << endl;
  //      out << "{";
  //      for(std::set<uint64_t>::iterator iter =  mapping_from_page_to_forward_pointers[page_of_interest].begin(); 
  //         	    iter != mapping_from_page_to_forward_pointers[page_of_interest].end(); ++iter)
  //      	out << *(iter) <<",";
  //      out << endl;	
  //  }
     
    for(std::map<uint64_t,number_of_spatial_prefetches_and_total_prefetches>::iterator iter =  mapping_from_function_to_spatial_prefetch_info.begin(); 
        	    iter != mapping_from_function_to_spatial_prefetch_info.end(); ++iter)
    {
	float avg_spatial_prefetch = 0.0;
	if (iter->second.total_prefetches != 0){
		avg_spatial_prefetch = (float)iter->second.total_block_prefetches/iter->second.total_prefetches;
	
	}
        out <<"The callee address is: " << iter->first <<" and average spatial fetches are: "<<avg_spatial_prefetch << endl;
//        for (std::set<uint64_t>::iterator iter1 = iter->second.begin();
//			iter1 != iter->second.end();++iter1)
//		out << *(iter1) << ",";
//	out <<"}" << endl;
    }
    
   // for(std::map<function_and_inter_page_distance,uint64_t>::iterator iter =  functions_that_jumps_to_distant_pages.begin(); 
   //     	    iter != functions_that_jumps_to_distant_pages.end(); ++iter)
   // {
   //     out <<"The caller address is : " << iter->first.function <<" function page is : "<<iter->first.function/4096  << " and dest page is " << iter->first.page_distance << " and miss count is " << iter->second << endl;
   //     out  << endl;
   // }
    // for(std::map<uint64_t,map<uint64_t,uint64_t>>::iterator iter =  mapping_from_caller_to_current_function_and_count_of_calls.begin(); 
   //     	    iter != mapping_from_caller_to_current_function_and_count_of_calls.end(); ++iter)
   // {
   //     out <<"The caller address is : " << iter->first << endl;
   //     out <<"Unique callees are : " << iter->second.size()  << endl; 
   //    // out <<"Total misses are : " << mapping_from_call_stack_hash_to_misses[iter->first]  << endl; 
   //     out <<"{";
   //     for(std::map<uint64_t,uint64_t>::iterator iter1 = iter->second.begin(); 
   //     	    iter1 != iter->second.end(); ++iter1)
   //     	out << "(" << iter1->first << "," << iter1->second <<"),";
   // 	out << "}" << endl;
   //     out  << endl;
   // }
}




/* ===================================================================== */

VOID Instruction(ADDRINT iaddr, uint32_t size, uint32_t control_flow_type, uint64_t future_reference_timestamp, 
		bool warmup_finished)
{
    
    const BOOL   single = (size <= 4);
                
    if (single)
	LoadSingleFast_Spatial(iaddr, future_reference_timestamp, warmup_finished);
    else
	LoadMultiFast_Spatial(iaddr,size,future_reference_timestamp, warmup_finished);
    //returns
    if (control_flow_type == 6){
         return_instr_seen = true;
	 if (current_function_callee_address == CALLEE_OF_INTEREST){
	     cout << endl;
	     //also reset the current page in function as we exit the function.
	     //this is the last page for the function. Print the chain of function pages for this function.
	  //   uint64_t current_page_in_function = mapping_from_function_to_current_function_page_on_which_we_are[current_function_callee_address];
	  //   mapping_from_function_and_page_to_forward_pointers[current_function_callee_address][current_page_in_function] = 0;
	     cout <<"Pages Traversed are " << current_function_callee_address/4096 <<",";
	     cout <<"Page we are returning from is " << iaddr/4096 << endl;
	     uint64_t temp = current_function_callee_address/4096;
	     uint64_t num_iters = 0;
	     while (mapping_from_function_and_page_to_forward_pointers[current_function_callee_address][temp] != 0){
	     	cout << mapping_from_function_and_page_to_forward_pointers[current_function_callee_address][temp] <<",";
	        temp = mapping_from_function_and_page_to_forward_pointers[current_function_callee_address][temp];
		num_iters++;
		if (num_iters >=10){
		   cout << "Num iters exceeded 10" << endl;
		   break;
		}
	     }
	     cout << endl;
	     current_page_in_function_of_interest = 0;
	//	 cout << "Return Address :"<< iaddr/64 << endl;
	//	 cout << "Caller Address :" << current_function_callee_address/64 << endl;
	//	cout <<"The size of the function is " << (iaddr-current_function_callee_address)/64 << endl; 
	 }
	 //terminate the chain of pages we have seen upon hitting function return. 
	 
	 //also reset the current page in function as we exit the function.
	 //this is the last page for the function. Print the chain of function pages for this function.
	 uint64_t current_page_in_function = mapping_from_function_to_current_function_page_on_which_we_are[current_function_callee_address];
	 if (mapping_from_function_and_page_to_forward_pointers[current_function_callee_address].find(current_page_in_function) == 
			 mapping_from_function_and_page_to_forward_pointers[current_function_callee_address].end())
		mapping_from_function_and_page_to_forward_pointers[current_function_callee_address][current_page_in_function] = 0;
	 mapping_from_function_to_current_function_page_on_which_we_are[current_function_callee_address] = 0;
    }
    //direct calls
    else if (control_flow_type == 2 ){
         call_instr_seen = true;
	 //push the current cache block address onto the stack, because we have to return to this point. 
	 return_cache_block_stack.push(iaddr/64);
    }
    //direct jumps
    else if (control_flow_type == 3){
         dir_jump_instr_seen = true;
    }
    //indirect calls
    else if (control_flow_type == 4){
         call_instr_seen = true;
         ind_call_instr_seen = true;
	 //push the current cache block address onto the stack, because we have to return to this point. 
	 return_cache_block_stack.push(iaddr/64);
    }
    //indirect jumps
    else if (control_flow_type == 5){
         ind_jump_seen = true;
//	 if (current_function_callee_address == CALLEE_OF_INTEREST){
//	 	cout <<"Indirect Jump seen" << endl;
//	 }
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
