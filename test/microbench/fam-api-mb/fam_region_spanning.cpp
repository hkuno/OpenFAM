/*
* test/microbench/fam-api-mb/fam_region_spanning.cpp
* Copyright (c) 2021 Hewlett Packard Enterprise Development, LP. All rights
* reserved. Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
* 1. Redistributions of source code must retain the above copyright notice,
* this list of conditions and the following disclaimer.
* 2. Redistributions in binary form must reproduce the above copyright notice,
* this list of conditions and the following disclaimer in the documentation
* and/or other materials provided with the distribution.
* 3. Neither the name of the copyright holder nor the names of its contributors
* may be used to endorse or promote products derived from this software without
* specific prior written permission.
*
*    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
* IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
* ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
* LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
*    INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
* CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
* ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
* POSSIBILITY OF SUCH DAMAGE.
*
* See https://spdx.org/licenses/BSD-3-Clause
*
*/
#include <fam/fam_exception.h>
#include <gtest/gtest.h>
#include <iostream>
#include <stdio.h>
#include <string.h>
#include <chrono>

#include <fam/fam.h>

#include "cis/fam_cis_client.h"
#include "common/fam_test_config.h"
#include "common/fam_libfabric.h"

#define BIG_REGION_SIZE (21474836480 * 4)
using namespace std::chrono;

using namespace std;
using namespace openfam;

uint64_t gDataSize = 1048576;
fam *my_fam;
Fam_Options fam_opts;
Fam_Descriptor **itemLocal;
Fam_Region_Descriptor *descLocal;

int *myPE;
int NUM_DATAITEMS = 1;
int NUM_IO_ITERATIONS = 1;
int nodesperPE = 1;
int msrvcnt = 1;


#define warmup_function(funcname, ...) { \
    int64_t *local = (int64_t *)malloc(gDataSize); \
    for (int i = 0; i < NUM_DATAITEMS; i++) { \
                my_fam->funcname(__VA_ARGS__); \
    } \
    my_fam->fam_barrier_all(); \
    fabric_reset_profile(); \
    free(local); \
}
// With microbenchmark tests, please use fam_reset_profile, if any warmup calls are being made.
// fam_reset_profile is a method defined in fam class in fam.cpp, which is used to ensure the warmup calls are not considered
// while collecting profiling data.
// In addition, to be able to use fam_reset_profile, we need to add this method in include/fam.h.
// Add the below mentioned function to warmup_function.
// my_fam->fam_reset_profile(); 

const int MAX = 26;
const int DATAITEM_NAME_LEN = 20;

// Returns a string of random alphabets of
// length n.
string getRandomString() {
    char alphabet[MAX] = {'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i',
                          'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r',
                          's', 't', 'u', 'v', 'w', 'x', 'y', 'z'};

    string res = "";
    for (int i = 0; i < DATAITEM_NAME_LEN; i++)
        res = res + alphabet[rand() % MAX];

    return res;
}

// Following set of functions are used to generate dataitems such that
// PEs create data items in the same switch as that of memory server on specific cluster.
int* get_memsrv(int peId, int totalPEs, int *numsrv) {
    int *memsrv = (int *)calloc(1, sizeof(int) * 8);
    *numsrv = 0;

    int switch_cnt = 8;
    if (nodesperPE == 1) {
    switch(msrvcnt) {
    case 1:      memsrv[0] = 0;
                 *numsrv = 1;
                break;
    case 2 :        memsrv[0] = peId/switch_cnt;
                    *numsrv = msrvcnt/2;
                    break;
    case 4 :
    case 8 :
    case 16:
                    *numsrv = msrvcnt/2;

                    if ( peId < switch_cnt){
                            for (int i = 0; i < msrvcnt/2; i++)
                                memsrv[i] = i;
                    }
                    else {
                            for (int j=0, i = msrvcnt/2; i<msrvcnt; j++, i++)
                                memsrv[j] = i;
                    }
                    break;

    }
    }

    return memsrv;
}
string getDataitemNameForMemsrv(int memsrv) {
    size_t hashVal;
    string dataitemName;
    size_t val = memsrv ;
    do {
        dataitemName = getRandomString() ;
        hashVal = hash<string>{}(dataitemName) % (msrvcnt);
    } while (hashVal != (size_t)val );
    return dataitemName;
}

string getDataitemNameForMemsrvList(int *memsrv, int mcnt) {
    size_t hashVal;
    string dataitemName;
    int found = 0;
    do {
        dataitemName = getRandomString() ;
        hashVal = hash<string>{}(dataitemName) % (msrvcnt);
        for (int i = 0; i< mcnt; i++)
                if (hashVal == (size_t)memsrv[i])
                    {found = 1; break;}
    } while (!found);
    return dataitemName;
}

string getDataitemName(int peId, int totalPEs) {
    size_t hashVal;
    size_t val = 0;
    string dataitemName;
    int switch_cnt = 8;
    switch(msrvcnt) {
        case 1:         val = peId;
                        break;
        case 2 :        val = peId/switch_cnt;
                        break;
        case 4 :
                        if ( peId < switch_cnt)
                                val =  peId % 2;
                        else
                                val = msrvcnt / 2 + peId % 2;
                        break;
        case 8 :
                        if ( peId < switch_cnt)
                                val = peId/switch_cnt + peId % 4;
                        else
                                val = msrvcnt / 2 + peId % 4;
                        break;
        case 16:
                        val = peId;
                        break;

    }

    do {
        dataitemName = getRandomString() ;
        hashVal = hash<string>{}(dataitemName) % totalPEs;
    } while (hashVal != (size_t)val);
    return dataitemName;
}

TEST(FamRegionSpanning, FamGetSingleRegionForAllPEs) {
    int64_t *local = (int64_t *)malloc(gDataSize);
    uint64_t offset = 0;
    warmup_function(fam_get_blocking,local, itemLocal[i], offset, gDataSize);
    EXPECT_NO_THROW(my_fam->fam_barrier_all());

    for (int i = 0; i < NUM_DATAITEMS; i++) {
        for (int io =0; io < NUM_IO_ITERATIONS; io++) {

                EXPECT_NO_THROW(my_fam->fam_get_blocking(local, itemLocal[i], offset, gDataSize));
        }

    }
    EXPECT_NO_THROW(my_fam->fam_barrier_all());

    free(local);
}

TEST(FamRegionSpanning, FamPutSingleRegionForAllPEs) {
    int64_t *local = (int64_t *)malloc(gDataSize);
    uint64_t offset = 0;
    warmup_function(fam_put_blocking,local, itemLocal[i], offset, gDataSize);
    EXPECT_NO_THROW(my_fam->fam_barrier_all());

    for (int i = 0; i < NUM_DATAITEMS; i++) {
        for (int io =0; io < NUM_IO_ITERATIONS; io++) {

                EXPECT_NO_THROW(my_fam->fam_put_blocking(local, itemLocal[i], offset, gDataSize));
        }

    }
    EXPECT_NO_THROW(my_fam->fam_barrier_all());

    free(local);
}


// Test case -  FamNonBlockingPut test.
TEST(FamRegionSpanning, FamNonBlockingPutSingleRegionForAllPEs) {
    int64_t *local = (int64_t *)malloc(gDataSize);
    uint64_t offset = 0;
    warmup_function(fam_put_blocking,local, itemLocal[i], offset, gDataSize);
    EXPECT_NO_THROW(my_fam->fam_barrier_all());

    for (int i = 0; i < NUM_DATAITEMS; i++) {
        for (int io =0; io < NUM_IO_ITERATIONS; io++) {
/*               if ( io % 10 ==  0)
                        offset = 0;
                else
                        offset +=  gDataSize ;*/

                EXPECT_NO_THROW(my_fam->fam_put_nonblocking(local, itemLocal[i], offset, gDataSize));
        }

    }
    EXPECT_NO_THROW(my_fam->fam_quiet());
    EXPECT_NO_THROW(my_fam->fam_barrier_all());

    free(local);
}

TEST(FamRegionSpanning, FamNonBlockingGetSingleRegionForAllPEs) {
    int64_t *local = (int64_t *)malloc(gDataSize);
    uint64_t offset = 0;
    warmup_function(fam_get_blocking,local, itemLocal[i], offset, gDataSize);
    EXPECT_NO_THROW(my_fam->fam_barrier_all());

            for (int i = 0; i < NUM_DATAITEMS; i++) {
                for (int io =0; io < NUM_IO_ITERATIONS; io++) {
/*                if ( io % 10 ==  0)
                        offset = 0;
                else
                        offset +=  gDataSize ;*/

                EXPECT_NO_THROW(my_fam->fam_get_nonblocking(local, itemLocal[i], offset, gDataSize));
        }

    }
    EXPECT_NO_THROW(my_fam->fam_quiet());
    EXPECT_NO_THROW(my_fam->fam_barrier_all());


    free(local);
}

// Test case -  Blocking scatter and gather (Index) test.
TEST(FamScatter, BlockingScatterIndex) {
    int64_t *local = (int64_t *)malloc(gDataSize);
    int count = 4;
    int64_t size = gDataSize / count;
    uint64_t *indexes = (uint64_t *)malloc(count * sizeof(uint64_t));
    uint64_t offset = 0;
    warmup_function(fam_put_blocking,local, itemLocal[i], offset, gDataSize);

    for (int e = 0; e < count; e++) {
        indexes[e] = e;
    }
    for (int i = 0; i < NUM_DATAITEMS; i++) {
        for (int io =0; io < NUM_IO_ITERATIONS; io++) {
                EXPECT_NO_THROW(
                    my_fam->fam_scatter_blocking(local, itemLocal[i], count, indexes, size));
        }
    }
    free(local);
}

TEST(FamScatter, BlockingGatherIndex) {
    int64_t *local = (int64_t *)malloc(gDataSize);
    int count = 4;
    int64_t size = gDataSize / count;
    uint64_t *indexes = (uint64_t *)malloc(count * sizeof(uint64_t));
    uint64_t offset = 0;
    warmup_function(fam_get_blocking,local, itemLocal[i], offset, gDataSize);

    for (int e = 0; e < count; e++) {
        indexes[e] = e;
    }
    for (int i = 0; i < NUM_DATAITEMS; i++) {
        for (int io =0; io < NUM_IO_ITERATIONS; io++) {
                EXPECT_NO_THROW(
                    my_fam->fam_gather_blocking(local, itemLocal[i], count, indexes, size));
        }
    }
    free(local);
}

// Test case -  Non-Blocking scatter (Index) test.
TEST(FamScatter, NonBlockingScatterIndex) {

    int64_t *local = (int64_t *)malloc(gDataSize);
    int count = 4;
    int64_t size = gDataSize / count;
    uint64_t *indexes = (uint64_t *)malloc(count * sizeof(uint64_t));
    uint64_t offset = 0;
    warmup_function(fam_put_blocking,local, itemLocal[i], offset, gDataSize);

    for (int e = 0; e < count; e++) {
        indexes[e] = e;
    }
    for (int i = 0; i < NUM_DATAITEMS; i++) {
        for (int io =0; io < NUM_IO_ITERATIONS; io++) {
            EXPECT_NO_THROW(
                    my_fam->fam_scatter_nonblocking(local, itemLocal[i], count, indexes, size));
        }
    }
    EXPECT_NO_THROW(my_fam->fam_quiet());
    free(local);
}

// Test case -  Non-Blocking gather (Index) test.
TEST(FamScatter, NonBlockingGatherIndex) {

    int64_t *local = (int64_t *)malloc(gDataSize);
    int count = 4;
    int64_t size = gDataSize / count;
    uint64_t *indexes = (uint64_t *)malloc(count * sizeof(uint64_t));
    uint64_t offset = 0;
    warmup_function(fam_get_blocking,local, itemLocal[i], offset, gDataSize);

    for (int e = 0; e < count; e++) {
        indexes[e] = e;
    }
    for (int i = 0; i < NUM_DATAITEMS; i++) {
        for (int io =0; io < NUM_IO_ITERATIONS; io++) {
                EXPECT_NO_THROW(
                    my_fam->fam_gather_nonblocking(local, itemLocal[i], count, indexes, size));
            }
    }
    EXPECT_NO_THROW(my_fam->fam_quiet());
    free(local);
}

// Test case -  Blocking scatter and gather (Index) test (Full size).
TEST(FamScatter, BlockingScatterIndexSize) {
    int count = 4;
    int64_t *local = (int64_t *)malloc(gDataSize * count);
    int64_t size = gDataSize / count;

    uint64_t *indexes = (uint64_t *)malloc(count * sizeof(uint64_t));
    uint64_t offset = 0;
    warmup_function(fam_put_blocking,local, itemLocal[i], offset, gDataSize);

    for (int e = 0; e < count; e++) {
        indexes[e] = e;
    }
    for (int i = 0; i < NUM_DATAITEMS; i++) {
        for (int io =0; io < NUM_IO_ITERATIONS; io++) {
                EXPECT_NO_THROW(
                    my_fam->fam_scatter_blocking(local, itemLocal[i], count, indexes, size));
        }
    }
    free(local);
}

TEST(FamScatter, BlockingGatherIndexSize) {
    int count = 4;
    int64_t *local = (int64_t *)malloc(gDataSize * count);
    int64_t size = gDataSize;
    uint64_t offset = 0;
    warmup_function(fam_get_blocking,local, itemLocal[i], offset, gDataSize);
    uint64_t *indexes = (uint64_t *)malloc(count * sizeof(uint64_t));

    for (int e = 0; e < count; e++) {
        indexes[e] = e;
    }
    for (int i = 0; i < NUM_DATAITEMS; i++) {
        for (int io =0; io < NUM_IO_ITERATIONS; io++) {
            EXPECT_NO_THROW(
                    my_fam->fam_gather_blocking(local, itemLocal[i], count, indexes, size));
        }
    }
    free(local);
}

// Test case -  Non-Blocking scatter (Index) test (Full size).
TEST(FamScatter, NonBlockingScatterIndexSize) {

    int count = 4;
    int64_t *local = (int64_t *)malloc(gDataSize * count);
    int64_t size = gDataSize / 4;
    uint64_t *indexes = (uint64_t *)malloc(count * sizeof(uint64_t));
    uint64_t offset = 0;
    warmup_function(fam_put_blocking,local, itemLocal[i], offset, gDataSize);

    for (int e = 0; e < count; e++) {
        indexes[e] = e;
    }
    for (int i = 0; i < NUM_DATAITEMS; i++) {
        for (int io =0; io < NUM_IO_ITERATIONS; io++) {
            EXPECT_NO_THROW(
                    my_fam->fam_scatter_nonblocking(local, itemLocal[i], count, indexes, size));
        }
    }
    EXPECT_NO_THROW(my_fam->fam_quiet());
    free(local);
}

// Test case -  Non-Blocking gather (Index) test (Full size).
TEST(FamScatter, NonBlockingGatherIndexSize) {

    int count = 4;
    int64_t *local = (int64_t *)malloc(gDataSize * count);
    int64_t size = gDataSize;
    uint64_t *indexes = (uint64_t *)malloc(count * sizeof(uint64_t));
    uint64_t offset = 0;
    warmup_function(fam_get_blocking,local, itemLocal[i], offset, gDataSize);

    for (int e = 0; e < count; e++) {
        indexes[e] = e;
    }
    for (int i = 0; i < NUM_DATAITEMS; i++) {
        for (int io =0; io < NUM_IO_ITERATIONS; io++) {
                EXPECT_NO_THROW(
                    my_fam->fam_gather_nonblocking(local, itemLocal[i], count, indexes, size));
        }
    }
    EXPECT_NO_THROW(my_fam->fam_quiet());
    free(local);
}

int main(int argc, char **argv) {
    int ret;
    std::string config_type = "specific";
    ::testing::InitGoogleTest(&argc, argv);

    if (argc >= 2) {
        config_type = argv[1];
        if ( ( config_type.compare("-h") == 0 ) || ( config_type.compare("--help") == 0) ) {
                cout << "config_type: even/specific/random depending on how we want the data item distribution to happen" << endl;
                cout << "num_dataitems: number of data items to be allocated by PE" << endl;
                cout << "num_io_iters: number of I/O iterations to be done on each data item" << endl;
                cout << "data_size: Data Item size" << endl;
                cout << "num_msrv: Number of memory servers " << endl;
                cout << "nodesperPE: Number of nodes per PE" << endl;

                exit(-1);
        }
    }

    if (argc >= 3) {
        NUM_DATAITEMS = atoi(argv[2]);
    }

    if (argc >= 4) {
        NUM_IO_ITERATIONS = atoi(argv[3]);
    }

    if (argc >= 5) {
        gDataSize = atoi(argv[4]);
    }

    if (argc >= 6) {
        msrvcnt = atoi(argv[5]);
    }
    if (argc >= 7) {
        nodesperPE = atoi(argv[6]);
    }

    my_fam = new fam();

    init_fam_options(&fam_opts);
    EXPECT_NO_THROW(my_fam->fam_initialize("default", &fam_opts));
    itemLocal = new Fam_Descriptor *[NUM_DATAITEMS];
    Fam_Region_Descriptor *descLocal;
    const char *firstItemLocal = get_uniq_str("firstLocal", my_fam);
    const char *testRegionLocal = get_uniq_str("testLocal", my_fam);

    EXPECT_NO_THROW(myPE = (int *)my_fam->fam_get_option(strdup("PE_ID")));

    EXPECT_NE((void *)NULL, myPE);
    int *numPEs;
    EXPECT_NO_THROW(numPEs = (int *)my_fam->fam_get_option(strdup("PE_COUNT")));
    EXPECT_NE((void *)NULL, numPEs);
    EXPECT_NO_THROW(my_fam->fam_barrier_all());
    if (*myPE == 0) {
    	for (int i = 1; i < argc; ++i) {
        	printf("arg %2d = %s\n", i, (argv[i]));
	    }

        uint64_t regionSize = gDataSize * NUM_DATAITEMS * *numPEs;
        regionSize = regionSize < BIG_REGION_SIZE ? BIG_REGION_SIZE : regionSize;
        EXPECT_NO_THROW(descLocal =
                  my_fam->fam_create_region("test0", regionSize , 0777, RAID1));
        EXPECT_NE((void *)NULL, descLocal);
    }

    EXPECT_NO_THROW(my_fam->fam_barrier_all());

    if (*myPE != 0) {
        EXPECT_NO_THROW(descLocal = my_fam->fam_lookup_region("test0"));
        EXPECT_NE((void *)NULL, descLocal);
    }
    EXPECT_NO_THROW(my_fam->fam_barrier_all());
    int mcnt = 0;

    int *memsrv =  get_memsrv(*myPE, *numPEs, &mcnt);

    int number_of_dataitems_per_pe = NUM_DATAITEMS;
    if ( config_type.compare("even") == 0 ) {
            int mid = *myPE % mcnt;
            for (int i = 0; i < number_of_dataitems_per_pe; i++) {
                 if (mid == mcnt)
                        mid = 0;
                std::string name = getDataitemNameForMemsrv(memsrv[mid++]);
                const char *itemInfo = strdup(name.c_str());

                // Allocating data items

                EXPECT_NO_THROW(itemLocal[i] = my_fam->fam_allocate(
                                    itemInfo, gDataSize * 1, 0777, descLocal));
                EXPECT_NE((void *)NULL, itemLocal[i]);
              cout << "PE" <<  *myPE << "," << itemInfo << "," << itemLocal[i]->get_memserver_id() << endl;
        }

    } else if ( config_type.compare("specific") == 0 ) {
            for (int i = 0; i < number_of_dataitems_per_pe; i++) {

                std::string name;
                        name = getDataitemName(*myPE,*numPEs);
                const char *itemInfo = strdup(name.c_str());
                // Allocating data items
                EXPECT_NO_THROW(itemLocal[i] = my_fam->fam_allocate(
                                    itemInfo, gDataSize * 1 , 0777, descLocal));
                EXPECT_NE((void *)NULL, itemLocal[i]);
              cout << "PE" <<  *myPE << "," << itemInfo << "," << itemLocal[i]->get_memserver_id() << endl;
            }

    } else {


            for (int i = 0; i < number_of_dataitems_per_pe; i++) {
                std::string name = getDataitemNameForMemsrvList(memsrv,mcnt);
                const char *itemInfo = strdup(name.c_str());
                // Allocating data items
                EXPECT_NO_THROW(itemLocal[i] = my_fam->fam_allocate(
                                    itemInfo, gDataSize * 1, 0777, descLocal));
                EXPECT_NE((void *)NULL, itemLocal[i]);
              cout << "PE" <<  *myPE << "," << itemInfo << "," << itemLocal[i]->get_memserver_id() << endl;
            }


    }
    EXPECT_NO_THROW(my_fam->fam_barrier_all());
    ret = RUN_ALL_TESTS();
    // Deallocate dataitems
    EXPECT_NO_THROW(my_fam->fam_barrier_all());
    for (int i = 0; i < number_of_dataitems_per_pe; i++) {
        EXPECT_NO_THROW(my_fam->fam_deallocate(itemLocal[i]));
    }

    EXPECT_NO_THROW(my_fam->fam_barrier_all());
    if ( *myPE == 0 ) {
            EXPECT_NO_THROW(my_fam->fam_destroy_region(descLocal));
     }
    EXPECT_NO_THROW(my_fam->fam_barrier_all());
    free((void *)testRegionLocal);
    free((void *)firstItemLocal);

    EXPECT_NO_THROW(my_fam->fam_finalize("default"));
    delete my_fam;
    return ret;
}

