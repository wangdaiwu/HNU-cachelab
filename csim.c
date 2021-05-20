#include "cachelab.h"
#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>

typedef struct
{
    int valid;  //有效位
    int tag;    //标识位
    int LruNumber;  //LRU算法计数
} Line;

typedef struct
{
    Line* lines;    //指向一组中的行
} Set;

typedef struct
{
    int SetNumber;  //组数
    int LineNumber; //行数
    Set* sets;      //指向Cache中的组
} Sim_Cache;

int misses;     //未命中
int hits;       //命中
int evictions;  //驱逐

//打印help信息
void printHelpMenu()
{
    printf("Usage: ./csim-ref [-hv] -s <num> -E <num> -b <num> -t <file>\n");
    printf("Options:\n");
    printf("    -h Print this help message\n");
    printf("    -v Optional verbose flag\n");
    printf("    -s num Number of set index bits\n");
    printf("    -E num Number of lines per set\n");
    printf("    -b num Number of block offset bits\n");
    printf("    -t file Trace file\n");
    printf("Examples:\n");
    printf("    linux.csim - s 4 - E 1 - b 4 - t tracesyi.trace\n");
    printf("    linux.csim - v - s 8 - E 2 - b 4 - t tracesyi.trace\n");
}

//检查参数合法性
void checkOptarg(char *curOptarg)
{
    if (curOptarg[0] == '-')
    {
        printf(".csim Missing required command line argument\n");
        printHelpMenu();
        exit(0);
    }
}

//分析输入参数
int get_Opt(int argc, char* argv[], int* s, int* E, int* b, char *tracefileName, int* isVerbose)
{
    int c;
    while ((c = getopt(argc, argv, "hvs:E:b:t:")) != -1)
    {
        switch (c)
        {
            case 'v':
                *isVerbose = 1;
                break;
            case 's':
                checkOptarg(optarg);
                *s = atoi(optarg);
                break;
            case 'E':
                checkOptarg(optarg);
                *E = atoi(optarg);
                break;
            case 'b':
                checkOptarg(optarg);
                *b = atoi(optarg);
                break;
            case 't':
                checkOptarg(optarg);
                strcpy(tracefileName, optarg);
                break;
            default:
                printHelpMenu();
        }
    }
    return -1;
}

//初始化cache 
void init_SimCache(int s, int E, int b, Sim_Cache *cache) 
{
    cache->SetNumber = pow(2, s);
    cache->LineNumber = E;
    cache->sets = (Set*) malloc(cache->SetNumber * sizeof(Set));

    int i, j;
    for(i = 0; i < cache->SetNumber; i++) 
    {
        cache->sets[i].lines = (Line*) malloc(E * sizeof(Line));
        for(j = 0; j < cache->LineNumber; j++) 
        {
            cache->sets[i].lines[j].valid = 0;
            cache->sets[i].lines[j].LruNumber = 0;
        }
    }
    return;
}

//释放函数
int free_SimCache(Sim_Cache *cache) 
{
    int i;
    for(i = 0; i < cache->SetNumber; i++) 
    {
        free(cache->sets[i].lines);
    }
    free(cache->sets);
    //cache->sets=NULL;
    return 0;
}

//step2测试--显示各组
int put_Sets(Sim_Cache *cache) 
{
    int i, j;
    for (i = 0; i < cache->SetNumber; i++) 
    {
        printf("set:%d\n", i + 1);
        for (j = 0; j < cache->LineNumber; j++) 
        {
            printf("line:%d valid:%d LruNumber:%d\n",
            j + 1,
            cache->sets[i].lines[j].valid,
            cache->sets[i].lines[j].LruNumber);
        }
    }
    return 0;
}

//更新LRU计数值
void updateLruNumber(Sim_Cache *sim_cache, int setBits, int hitIndex) 
{
    sim_cache->sets[setBits].lines[hitIndex].LruNumber = INT_MAX;
    int i;
    for(i = 0; i < sim_cache->LineNumber; i++) 
    {
        if(i != hitIndex)
        {
            sim_cache->sets[setBits].lines[i].LruNumber--;
        }
    }
}

//查找牺牲行
int findMinLruNumber(Sim_Cache *sim_cache, int setBits) 
{
    int i;
    int minIndex = 0;
    int minLru = INT_MAX;
    for (i = 0; i < sim_cache->LineNumber; i++)
    {
        if (sim_cache->sets[setBits].lines[i].LruNumber < minLru) 
        {
            minIndex = i;
            minLru = sim_cache->sets[setBits].lines[i].LruNumber;
        }
    }
    return minIndex;
}

//是否命中
int isMiss(Sim_Cache *sim_cache, int setBits, int tagBits) 
{
    int i;
    int isMiss = 1;
    for (i = 0; i < sim_cache->LineNumber; i++) 
    {
        if (sim_cache->sets[setBits].lines[i].valid == 1 &&
            sim_cache->sets[setBits].lines[i].tag == tagBits) 
        {
            isMiss = 0;
            updateLruNumber(sim_cache, setBits, i);
            break;
        }
    }
    return isMiss;
}

//更新高速缓存数据
int updateCache(Sim_Cache *sim_cache, int setBits, int tagBits)
{
    int i;
    int isfull = 1;
    for (i = 0; i < sim_cache->LineNumber; i++) 
    {
        if (sim_cache->sets[setBits].lines[i].valid == 0) 
        {
            isfull = 0;
            break;
        }
    }
    if (isfull == 0) 
    {
        sim_cache->sets[setBits].lines[i].valid = 1;
        sim_cache->sets[setBits].lines[i].tag = tagBits;
        updateLruNumber(sim_cache, setBits, i);
    } 
    else 
    {
        int evictionIndex = findMinLruNumber(sim_cache, setBits);
        sim_cache->sets[setBits].lines[evictionIndex].valid = 1;
        sim_cache->sets[setBits].lines[evictionIndex].tag = tagBits;
        updateLruNumber(sim_cache, setBits, evictionIndex);
    }
    return isfull;
}

//step3测试--验证LRU运行相关函数
int runLru(Sim_Cache *sim_cache, int setBits, int tagBits) 
{
    if (isMiss(sim_cache, setBits, tagBits))
    {
        updateCache(sim_cache, setBits, tagBits);
    }
    return 0;
}

//加载数据L
void loadData(Sim_Cache *sim_cache, int setBits, int tagBits, int isVerbose)
{
    if (isMiss(sim_cache, setBits, tagBits))
    {
        misses++;
        if (isVerbose == 1)
        {
            printf(" miss");
        }
        if (updateCache(sim_cache, setBits, tagBits))
        {
            evictions++;
            if (isVerbose == 1)
            {
                printf(" eviction");
            }
        }
    }
    else
    {
        hits++;
        if (isVerbose == 1)
        {
            printf(" hit");
        }
    }
}

//存储数据S
void storeData(Sim_Cache *sim_cache, int setBits, int tagBits, int isVerbose)
{
    loadData(sim_cache, setBits, tagBits, isVerbose);
}

//修改数据M
void modifyData(Sim_Cache *sim_cache, int setBits, int tagBits, int isVerbose)
{
    loadData(sim_cache, setBits, tagBits, isVerbose);
    storeData(sim_cache, setBits, tagBits, isVerbose);
}

//获取组索引
int getSet(int addr, int s, int b)
{
    addr = addr >> b;
    int m = (1 << s) - 1;
    return addr & m; 
}

//获取标志位
int getTag(int addr, int s, int b)
{
    int m = s + b;
    return addr >> m;
}

int main(int argc, char *argv[])
{
    //step2用： 用户补充检验代码 
    /* int s, E, b, isVerbose = 0;
	char fileName[100];
    Sim_Cache cache;

    get_Opt(argc, argv, &s, &E, &b, fileName, &isVerbose);
    init_SimCache(s, E, b, &cache);

    put_Sets(&cache);
    free_SimCache(&cache); */

    //step3用： 用户补充检验代码 
    /* int s, E, b, isVerbose = 0;
	char fileName[100];
    Sim_Cache cache;

    get_Opt(argc, argv, &s, &E, &b, fileName, &isVerbose);
    init_SimCache(s, E, b, &cache);

    runLru(&cache, 0, 0);
    runLru(&cache, 1, 666);
    put_Sets(&cache);
    free_SimCache(&cache); */

    //step4用： 用户补充检验代码 
    int s, E, b, isVerbose = 0;
	char fileName[100];
    Sim_Cache cache;

    get_Opt(argc, argv, &s, &E, &b, fileName, &isVerbose);
    init_SimCache(s, E, b, &cache);

    char opt[10];
    int addr, size;
    FILE *tracefile = fopen(fileName, "r");
    if (!tracefile)
    {
        printf("Error: Cann't open file %s\n", fileName);
        return -1;
    }

    while (fscanf(tracefile, "%s %x,%d", opt, &addr, &size) != EOF)
    {
        if (strcmp(opt, "I") == 0)
        {
            continue;
        }
        int setBits = getSet(addr, s, b);
        int tagBits = getTag(addr, s, b);
        //printf("setBits:%x tagBits:%x\n", setBits, tagBits);
        if (isVerbose == 1)
        {
            printf("%s %x %d", opt, addr, size);
        }
        if (strcmp(opt, "S") == 0)
        {
            storeData(&cache, setBits, tagBits, isVerbose);
        }
        if (strcmp(opt, "M") == 0)
        {
            modifyData(&cache, setBits, tagBits, isVerbose);
        }
        if (strcmp(opt, "L") == 0)
        {
            loadData(&cache, setBits, tagBits, isVerbose);
        }
        if (isVerbose == 1)
        {
            printf("\n");
        }
    }
    printSummary(hits, misses, evictions);
    free_SimCache(&cache);

    return 0;
}
