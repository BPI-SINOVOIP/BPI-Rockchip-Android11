/*  --------------------------------------------------------------------------------------------------------
 *  File:   custom_log.h 
 *
 *  Desc:   ChenZhen 偏好的, 对 Android log 机构的定制配置. 
 *
 *          -----------------------------------------------------------------------------------
 *          < 习语 和 缩略语 > : 
 *
 *          -----------------------------------------------------------------------------------
 *
 *  Usage:	调用本 log 机构的 .c or .h 文件, 若要使能这里的 log 机制, 
 *          则必须在 inclue 本文件之前, "#define ENABLE_DEBUG_LOG" 先. 
 *          
 *          同 log.h 一样, client 文件在 inclue 本文件之前, "最好" #define LOG_TAG <module_tag>
 *          "module_tag" 是当前待调试的 模块的标识. 
 *          
 *          另外, 可以使用 V() 来输出 verbose 的 log, 但必须先 "#define ENABLE_VERBOSE_LOG". 
 *
 *          若用户要 dump 大量 mem 数据, 此时有大量快速的 log 输出, 可能导致 host 端接收来不及, 
 *          此时用户可以定义宏 MASSIVE_LOG, 通过在特定 log 接口中延时, 保证 host 端接收到完整 log. 
 *
 *  Note:
 *
 *  Author: ChenZhen
 *
 *  --------------------------------------------------------------------------------------------------------
 *  Version:
 *          v1.0
 *  --------------------------------------------------------------------------------------------------------
 *  Log:
	----Tue Mar 02 21:30:33 2010            v.10
 *        
 *  --------------------------------------------------------------------------------------------------------
 */


#ifndef __CUSTOM_LOG_H__
#define __CUSTOM_LOG_H__

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------------------------------------------
 *  Include Files
 * ---------------------------------------------------------------------------------------------------------
 */

#include <log/log.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
    
/* ---------------------------------------------------------------------------------------------------------
 *  Macros Definition 
 * ---------------------------------------------------------------------------------------------------------
 */

/** 
 * 若下列 macro 有被定义, dump 内存块的 log 接口每次执行之后将 sleep 若干时间. 
 */
// #define MASSIVE_LOG 

/**
 * 若定义有 MASSIVE_LOG, dump 内存块的 log 接口每次 sleep 的时间 us 为单位. 
 */
#define RESPITE_TIME_FOR_MASSIVE_LOG_IN_US      (1 * 10)

/** 若下列 macro 有被定义, 才 使能 log 输出 : 参见 "Usage". */ 
// #define ENABLE_DEBUG_LOG

/** .! : 若需要全局地关闭 D log, 可以使能下面的代码. */
/*
#undef ENABLE_DEBUG_LOG
#warning "debug log is disabled globally!"
*/

/** 是否 log 源文件 路径信息. */
#define LOG_FILE_PATH

/*-----------------------------------*/

#ifndef LOGV
#define LOGV ALOGV
#endif

#ifndef LOGD
#define LOGD ALOGD
#endif

#ifndef LOGI
#define LOGI ALOGI
#endif

#ifndef LOGW
#define LOGW ALOGW
#endif

#ifndef LOGE
#define LOGE ALOGE
#endif

/*-----------------------------------*/
#ifdef ENABLE_VERBOSE_LOG

#undef ENABLE_DEBUG_LOG
#define ENABLE_DEBUG_LOG
    
// #error
#ifdef LOG_FILE_PATH
#define V(fmt, args...) \
    { D(fmt, ## args); }
    // { LOGV("[File] : %s; [Line] : %d; [Func] : %s; " fmt , __FILE__, __LINE__, __FUNCTION__, ## args); }

#else
#define V(fmt, args...) \
    { D(fmt, ## args); }
    // { LOGV("[Line] : %d; [Func] : %s; " fmt , __LINE__, __FUNCTION__, ## args); }
#endif

#else
#define V(...)  ((void)0)
#endif

/*-----------------------------------*/
#ifdef ENABLE_DEBUG_LOG

#ifdef LOG_FILE_PATH
#define D(fmt, args...) \
    { LOGD("[File] : %s; [Line] : %d; [Func] : %s;\n" fmt , __FILE__, __LINE__, __FUNCTION__, ## args); }

#else
#define D(fmt, args...) \
    { LOGD("[Line] : %d; [Func] : %s; " fmt , __LINE__, __FUNCTION__, ## args); }
#endif

#else
#define D(...)  ((void)0)
#endif


/*-----------------------------------*/
// #ifdef ENABLE_DEBUG_LOG

#ifdef LOG_FILE_PATH
#define I(fmt, args...) \
    { LOGI("[File] : %s; [Line] : %d; [Func] : %s;\n" fmt , __FILE__, __LINE__, __FUNCTION__, ## args); }
#else
#define I(fmt, args...) \
    { LOGI("[Line] : %d; [Func] : %s; " fmt , __LINE__, __FUNCTION__, ## args); }
#endif

/*
#else       // #ifdef ENABLE_DEBUG_LOG
    
#define I(fmt, args...) \
    { LOGI(fmt, ## args); }

#endif
*/

/*-----------------------------------*/
#ifdef LOG_FILE_PATH
#define W(fmt, args...) \
    { LOGW("[File] : %s; [Line] : %d; [Func] : %s;\n" fmt , __FILE__, __LINE__, __FUNCTION__, ## args); }
#else
#define W(fmt, args...) \
    { LOGW("[Line] : %d; [Func] : %s; " fmt , __LINE__, __FUNCTION__, ## args); }
#endif

/*-----------------------------------*/
#ifdef LOG_FILE_PATH
#define E(fmt, args...) \
    { LOGE("[File] : %s; [Line] : %d; [Func] : %s;\n" fmt , __FILE__, __LINE__, __FUNCTION__, ## args); }
#else
#define E(fmt, args...) \
    { LOGE("[Line] : %d; [Func] : %s; " fmt , __LINE__, __FUNCTION__, ## args); }
#endif

/*-----------------------------------*/
/** 
 * 若程序重复运行到当前位置的次数等于 threshold 或者第一次到达, 则打印指定的 log 信息. 
 */
#ifdef ENABLE_DEBUG_LOG
#define D_WHEN_REPEAT(threshold, fmt, args...) \
    do { \
        static int count = 0; \
        if ( 0 == count || (count++) == threshold ) { \
            D(fmt, ##args); \
            count = 1; \
        } \
    } while (0)
#else
#define D_WHEN_REPEAT(...)  ((void)0)
#endif
    
/*-------------------------------------------------------*/
/** 使用 D(), 以十进制的形式打印变量 'var' 的 value. */
#define D_DEC(var) \
{ \
    long long num = (var); \
    D(#var " = %lld.", num); \
}

/** 使用 D(), 以十六进制的形式打印变量 'var' 的 value. */
#define D_HEX(var) \
{ \
    long long num = (var); \
    D(#var " = 0x%llx.", num); \
}

#define D_FLOAT(var) \
{ \
    float num = (var); \
    D(#var " = %f.", num); \
}

/** 使用 D(), 以十六进制的形式 打印指针类型变量 'ptr' 的 value. */
#define D_PTR(ptr)  D(#ptr " = %p.", ptr);

/** 使用 D(), 打印 char 字串. */
#define D_STR(pStr) \
{\
    if ( NULL == pStr )\
    {\
        D(#pStr" = NULL.");\
    }\
    else\
    {\
        D(#pStr" = '%s'.", pStr);\
    }\
}

/*---------------------------------------------------------------------------*/

/** 使用 V(), 以十进制的形式打印变量 'var' 的 value. */
#define V_DEC(var)  V(#var " = %d.", var);

/** 使用 V(), 以十六进制的形式打印变量 'var' 的 value. */
#define V_HEX(var)  V(#var " = 0x%x.", var);

/** 使用 V(), 以十六进制的形式打印 类型是 unsigned long long 变量 'var' 的 value. */
#define V_HEX_ULL(var)  V(#var " = 0x%016llx.", var);

/** 使用 V(), 以十六进制的形式 打印指针类型变量 'ptr' 的 value. */
#define V_PTR(ptr)  V(#ptr " = %p.", ptr);

/** 使用 V(), 打印 char 字串. */
#define V_STR(pStr) \
{\
    if ( NULL == pStr )\
    {\
        V(#pStr" = NULL.");\
    }\
    else\
    {\
        V(#pStr" = '%s'.", pStr);\
    }\
}

/*-------------------------------------------------------*/

/**
 * 调用函数, 并检查返回值, 根据返回值决定是否跳转到指定的错误处理代码. 
 * @param functionCall
 *          对特定函数的调用, 该函数的返回值必须是 表征 成功 or err 的 整型数. 
 *          这里, 被调用函数 "必须" 是被定义为 "返回 0 表示操作成功". 
 * @param result
 *		    用于记录函数返回的 error code 的 整型变量, 通常是 "ret" or "result" 等.
 * @param label
 *		    若函数返回错误, 程序将要跳转到的 错误处理处的 标号, 通常就是 "EXIT". 
 */
#define CHECK_FUNC_CALL(functionCall, result, label) \
{\
	if ( 0 != ( (result) = (functionCall) ) )\
	{\
		E("Function call returned error : " #result " = %d.", result);\
		goto label;\
	}\
}

/**
 * 分配 heap 空间, 检查返回地址, 将分配得到的 heap 空间全部清 0.
 * 若失败, 则将指定的 error code 赋值给指定的变量, 之后跳转到指定的错误处理代码. 
 * @param pDest
 *		    用于保存分配得到的 heap block 地址的目标指针变量.
 * @param type
 *          待分配的空间中, 数据单元 的类型. 
 * @param size
 *          待分配空间中包含 "数据单元"(对应类型 'type') 的具体个数. 
 *          所以, 待分配空间的字节大小 是 "sizeof(type) * size". 
 * @param retVar
 *		    用于记录函数返回的 error code 的 整型变量, 通常是 "ret" or "result" 等.
 * @param errCode
 *          用来表征 "内存不足" 的 error code, 不同模块中可能使用不同的常数标识. 
 *          标准 linux 下, 通常是 ENOMEM.
 * @param label
 *		    若空间分配失败, 程序将要跳转到的 错误处理代码的 标号, 通常就是 "EXIT". 
 */
#define CHECK_MALLOC(pDest, type, size, retVar, errCode, label) \
{\
	unsigned int bug_len = sizeof(type) * (size);\
	if ( NULL == ( (pDest) = (type*)malloc(bug_len)))\
	{\
        retVar = errCode;\
        E("Failed to malloc %u bytes.", bug_len);\
		goto label;\
	}\
	memset( (void*)(pDest), 0, sizeof(bug_len));\
}

/**
 * 在特定条件下, 判定 error 发生, 对变量 'retVar' 设置 'errCode', 
 * Log 输出对应的 Error Caution, 然后跳转 'label' 指定的代码处执行. 
 * @param msg
 *          纯字串形式的提示信息. 
 * @param retVar
 *		    标识函数执行状态或者结果的变量, 将被设置具体的 Error Code. 
 *		    通常是 'ret' or 'result'. 
 * @param errCode
 *          表征特定 error 的常数标识, 通常是 宏的形态. 
 * @param label
 *          程序将要跳转到的错误处理代码的标号, 通常就是 'EXIT'. 
 * @param args...
 *          对应 'msgFmt' 实参中 '%s', '%d', ... 等 转换说明符 的具体可变长实参. 
 */
#define SET_ERROR_AND_JUMP(msgFmt, retVar, errCode, label, args...) \
{\
    E("To set '" #retVar "' to %d('" #errCode "') : " msgFmt, (errCode), ## args);\
	(retVar) = (errCode);\
	goto label;\
}

#define EXIT_FOR_DEBUG \
{\
    E("To exit for debug.");\
    return 1;\
}

/*---------------------------------------------------------------------------*/

/**
 * 返回指定数组中的元素个数. 
 * @param array
 *      有效的数组实例标识符. 
 */
#define ELEMENT_NUM(array)      ( sizeof(array) /  sizeof(array[0]) )

/*---------------------------------------------------------------------------*/

/**
 * 断言.
 * 期望 'expect' 的 实参表达式 "成立", 即 非 0, 否则直接 abort 当前进程. 
 * @param msgFmt
 *          字串形式的提示信息, 可以包含 '%s', '%d' 等 转换说明符. 
 * @param args... 
 *          对应 'msgFmt' 实参中 '%s', '%d', ... 等 转换说明符 的具体可变长实参. 
 */
#define ASSERT(expect, msgFmt, args...) \
    do { \
        if ( !(expect) ) \
        { \
            E("assert('" #expect "') FAILED, to ABORT. " msgFmt, ## args); \
            abort(); \
        } \
    } while ( 0 )

/* ---------------------------------------------------------------------------------------------------------
 *  Types and Structures Definition
 * ---------------------------------------------------------------------------------------------------------
 */


/* ---------------------------------------------------------------------------------------------------------
 *  Global Functions' Prototype
 * ---------------------------------------------------------------------------------------------------------
 */


/* ---------------------------------------------------------------------------------------------------------
 *  Inline Functions Implementation 
 * ---------------------------------------------------------------------------------------------------------
 */
 
/*-------------------------------------------------------*/
/**
 * dump 从 地址 'pStart' 开始, 长度为 'len' 字节的 memory block 的内容(16 进制数的形式). 
 */
#ifdef ENABLE_DEBUG_LOG
#define D_MEM(pStart, len) \
    { \
        D("dump memory from addr of '" #pStart"' : "); \
        dumpMemory( (pStart), (len) ); \
    }
inline static void dumpMemory(const void* pStart, uint32_t len)
{
    char* pBuf = NULL;          /* dump buffer. 用来以文本的形式("0xaa, 0xbb, ...") 表示 一行, 最多 16 字节的 十六进制数. */
    size_t bufLen = 0;    /* *pBuf 的字节长度, 包括末尾的 '\0'. */

    unsigned char* pSource = (unsigned char*)pStart;          /* 单次 sprintf 操作的源地址. */
    char* pTarget = NULL;                                     /* 目标地址. */

    size_t i, j;

#define BYTES_PER_LINE (16)                         /* dump 输出每行打印的 source bytes 的数目. */
    size_t BUF_SIZE_PER_SRC_BYTE  = strlen("0xXX, ");  // const. 每个待 dump 的 字节在 *pBuf 中需要的空间大小.
                                                    // memory 中的每个字节被表示为 "0xXX, " 或者 "0xXX\n " 的形式. 
    
    size_t fullLines = len / BYTES_PER_LINE;  // 满 16 字节的行数.
    size_t leftBytes = len % BYTES_PER_LINE;  // 最后可能的不满行的 字节的个数.

    if ( NULL == pStart || 0 == len )
    {
        return;
    }
    
    bufLen = (BYTES_PER_LINE * BUF_SIZE_PER_SRC_BYTE) + 1;  
    pBuf = (char*)malloc(bufLen); 
    if ( NULL == pBuf )
    {
        E("no enough memory.");
        return;
    }

    LOGD("from %p; length %d : ", pStart, len);

    /* 处理所有的 full line, ... */
    for ( j = 0; j < fullLines; j++ )
    {
        pTarget = pBuf;
        /* 遍历每个待 log 的 字节, ... */
        for ( i = 0; i < BYTES_PER_LINE; i++ )
        {
            /* 打印到 *pTarget. */
            sprintf(pTarget, "0x%02x, ", *pSource);

            pTarget += BUF_SIZE_PER_SRC_BYTE;
            pSource++;
        }
        
        *(++pTarget) = '\0';
        /* log out 当前行. */
        LOGD("\t%s", pBuf);
    }
    
    /* 处理最后的 不满的行. */
    leftBytes = len % BYTES_PER_LINE;
    if ( 0 != leftBytes )
    {
        pTarget = pBuf;

        for ( i = 0; i < leftBytes; i++ )
        {
            sprintf(pTarget, "0x%02x, ", *pSource);
            pTarget += BUF_SIZE_PER_SRC_BYTE;
            pSource++;
        }

        *(++pTarget) = '\0';
        LOGD("\t%s", pBuf);
    }
    
    free(pBuf);
    
#ifdef MASSIVE_LOG
    usleep(RESPITE_TIME_FOR_MASSIVE_LOG_IN_US);
#endif
    return;
}

#else       // #ifdef ENABLE_DEBUG_LOG
#define D_MEM(...)  ((void)0)
#endif

/*-------------------------------------------------------*/

#ifdef ENABLE_VERBOSE_LOG
#define V_MEM(pStart, len) \
    { \
        D("dump memory from addr of '" #pStart"' : "); \
        dumpMemory( (pStart), (len) ); \
    }
#else       // #ifdef ENABLE_VERBOSE_LOG
#define V_MEM(...)  ((void)0)
#endif

/*---------------------------------------------------------------------------*/

#define DUMP_SIZE_PER_SRC_BYTE       (sizeof("0xXX, ") - 1)             // "1" : 字串最后的 '\0'.

/** 
 * 将 'pStart' 和 'len' 指定的一块内存数据, 以  hex ASCII 的形态 dump 'pBuf' 指定的 buf 中. 
 * 函数返回之后, "*pBuf' 尾部有 '\0' 字符. 
 * .! : 调用者 "必须" 保证 'pDest' 指向的 buf 的不小于 'len * DUMP_SIZE_PER_SRC_BYTE'. 
 */
inline static void dumpMemInHexAsciiToMem(const void* pStart, unsigned int len, char* pBuf) 
{
    unsigned int i = 0;
    unsigned char* pSource = (unsigned char*)pStart;
    char* pTarget = pBuf;       // 特定写入操作的目标地址. 

    if ( NULL == pStart || 0 == len || NULL == pBuf )
    {
        return;
    }
    
    for ( i = 0; i < len; i++ )
    {
        if ( i < len - 1 )
        {
            sprintf(pTarget, "0x%02x, ", *pSource);
        }
        else
        {
            sprintf(pTarget, "0x%02x", *pSource);
        }
        pSource++;
        pTarget += DUMP_SIZE_PER_SRC_BYTE;
    }
    
    pBuf[len * DUMP_SIZE_PER_SRC_BYTE - 1] = '\0';
}

/*---------------------------------------------------------------------------*/

/**
 * 以 hex ASCII 格式 log dump 从 'pStart' 开始, 长度为 'len' 字节的 mem 的内容, 16 字节为一行. 
 * 前导有 'indentNum' 个 '\t' 缩进. 
 * 支持的最大缩进不超过 15. 
 */
inline static void dumpMemoryWithIndents(const void* pStart, unsigned int len, unsigned int indentNum)
{
    char indents[16];

    const unsigned int SRC_BYTES_NUM_PER_LINE = 16;     // 输出中每行显示的源字节的个数. 

    unsigned int bufLen = SRC_BYTES_NUM_PER_LINE * DUMP_SIZE_PER_SRC_BYTE;
    char* pBuf = (char*)malloc(bufLen);

    unsigned int linesNum = 0;        // 输出的行数.

    unsigned char* pSrcStartInLine = NULL;      // 当前行输出对应的源字节序列的起始地址. 
    unsigned int srcBytesNumInLine = 0; // 当前行要输出端的源字节的个数. 
    
    unsigned int i = 0;
    
    /* 若原数据可以被若干个 满行 正好打印, 则... */
    if ( 0 == (len % SRC_BYTES_NUM_PER_LINE) )
    {
        linesNum = len / SRC_BYTES_NUM_PER_LINE;
    }
    /* 否则, ... */
    else
    {
        /* 增加最后一行的 不满行. */
        linesNum = (len / SRC_BYTES_NUM_PER_LINE) + 1;
    }

    /*-------------------------------------------------------*/

    indents[0] = '\0';
    // for ( i = 0; i < indentNum - 1; i++ )       // "-1" : 对输出内容的地址标识, 可以替代一个缩进. 
    for ( i = 0; i < indentNum; i++ )       // "-1" : 对输出内容的地址标识, 可以替代一个缩进. 
    {
        strcat(indents , "\t");
    }
    *(indents + indentNum) = '\0';
    
    /*-------------------------------------------------------*/

    pBuf[0] = '\0';
    pSrcStartInLine = (unsigned char*)pStart;

    /* 逐行输出. */
    for ( i = 0; i < linesNum; i++ )
    {
        srcBytesNumInLine = (len - (i * SRC_BYTES_NUM_PER_LINE) ) % SRC_BYTES_NUM_PER_LINE;

        /* 若当前行 "不是" 最后一行(第 (linesNum - 1) 行 , 则... */
        if ( i < (linesNum - 1) )
        {
            srcBytesNumInLine = SRC_BYTES_NUM_PER_LINE;
        }
        /* 否则, 即当前行 "是" 最后一行", 则... */
        else
        {
            /* 若是满行, 则... */
            if ( 0 == (len % SRC_BYTES_NUM_PER_LINE ) )
            {
                srcBytesNumInLine = SRC_BYTES_NUM_PER_LINE;
            }
            else 
            {
                srcBytesNumInLine = len % SRC_BYTES_NUM_PER_LINE;
            }
        }
        
        dumpMemInHexAsciiToMem(pSrcStartInLine, srcBytesNumInLine, pBuf);
        LOGD("%s" " [0x%04x] : " "%s", indents, SRC_BYTES_NUM_PER_LINE * i, pBuf);
        
        pSrcStartInLine += srcBytesNumInLine;

#ifdef MASSIVE_LOG
    usleep(RESPITE_TIME_FOR_MASSIVE_LOG_IN_US);
#endif
    }

    /*-------------------------------------------------------*/

    free(pBuf);

}

/**
 * 在 'pIndentsBuf' 指向的 buf 中, 从头插入 'indentNum' 个 '\t' 字符(缩进), 并跟上一个 '\0'. 
 * 通常 pIndentsBuf 指向一个 16 字节的 buffer. 
 */
inline static void setIndents(char* pIndentsBuf, unsigned char indentNum)
{
    unsigned char i = 0; 

    const unsigned char MAX_NUM_OF_INDENT = 16 - sizeof('\0');
    if ( indentNum > MAX_NUM_OF_INDENT )
    {
        indentNum = MAX_NUM_OF_INDENT;
    }

    *pIndentsBuf = '\0';
    for ( i = 0; i < indentNum; i++ )
    {
        strcat(pIndentsBuf, "\t");
    }
    *(pIndentsBuf + indentNum) = '\0';
}

inline static int dumpCharArray(const char* pStart, unsigned int len, unsigned char indentNum)
{
    int ret = 0;
    //unsigned long ret1 = -1;
    char indents[16];
    setIndents(indents, indentNum);

    char* pBuf = NULL;

    /*-------------------------------------------------------*/
    
    if ( NULL == pStart )
    {
        LOGD("%s %s", indents, "null");
        goto EXIT;
    }
    
    CHECK_MALLOC( pBuf, char, len + 1, ret, -1, EXIT);
    
    memcpy(pBuf, pStart, len);
    pBuf[len] = '\0';
    // pBuf[len] = 0;
    LOGD("%s %s", indents, pBuf);

EXIT:
    if ( NULL != pBuf )
    {
        free(pBuf);
    }

    return ret;
}

inline static int dumpStr(const char* pStr, unsigned char indentNum)
{
    int ret = 0;

    char indents[16];
    setIndents(indents, indentNum);
    /*-------------------------------------------------------*/

    if ( NULL == pStr )
    {
        LOGD("%s %s", indents, "null");
    }
    else
    {
        LOGD("%s %s", indents, pStr);
    }
    
    return ret;
}

/* ############################################################################################# */
/* < 单独针对 Android 平台的调试宏. > */

/**
 * 打印 sp<T> 实例 "sp" 指向的实例对象的当前 强被引用计数. 
 * "D_SC" : Debug log Strong Count.
 */
#define D_SC(sp)    D("strong count of '" #sp "' is '%d'.", (sp)->getStrongCount() )


#ifdef __cplusplus
}
#endif

#endif /* __CUSTOM_LOG_H__ */

