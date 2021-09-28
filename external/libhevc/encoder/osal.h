/******************************************************************************
 *
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *****************************************************************************
 * Originally developed and contributed by Ittiam Systems Pvt. Ltd, Bangalore
*/

/*****************************************************************************/
/*                                                                           */
/*  File Name         : osal.h                                               */
/*                                                                           */
/*  Description       : This file contains all the necessary OSAL Constants, */
/*                      Enums, Structures and API declarations.              */
/*                                                                           */
/*  List of Functions : None                                                 */
/*                                                                           */
/*  Issues / Problems : None                                                 */
/*                                                                           */
/*  Revision History  :                                                      */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         03 03 2006   Ittiam          Draft                                */
/*                                                                           */
/*****************************************************************************/

#ifndef OSAL_H
#define OSAL_H

/* C linkage specifiers for C++ declarations. */
#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

/*****************************************************************************/
/* Constants                                                                 */
/*****************************************************************************/

/* OSAL handle size */
#define OSAL_HANDLE_SIZE 40

/* Number of select entries */
#define OSAL_SELECT_MAX 20

/* OSAL Return Status */
#define OSAL_SUCCESS 0
#define OSAL_ERROR -1
#define OSAL_NOT_SUPPORTED -2
#define OSAL_TIMEOUT -3

/* OSAL thread priority levels.                                              */
/*     OSAL_PRIORITY_1         represents    MINIMUM,                        */
/*     OSAL_PRIORITY_10        represents    MAXIMUM,                        */
/*     OSAL_PRIORITY_DEFAULT   represnts     DEFAULT SYSTEM PRIROTIY LEVEL   */
#define OSAL_PRIORITY_DEFAULT 0
#define OSAL_PRIORITY_1 1
#define OSAL_PRIORITY_2 2
#define OSAL_PRIORITY_3 3
#define OSAL_PRIORITY_4 4
#define OSAL_PRIORITY_5 5
#define OSAL_PRIORITY_6 6
#define OSAL_PRIORITY_7 7
#define OSAL_PRIORITY_8 8
#define OSAL_PRIORITY_9 9
#define OSAL_PRIORITY_10 10

/* OSAL socket option levels */
#define OSAL_SOL_SOCKET 10000
#define OSAL_IPPROTO_IP 10001

/* OSAL socket options */
#define OSAL_BROADCAST 1000
#define OSAL_REUSEADDR 1001
#define OSAL_KEEPALIVE 1002
#define OSAL_LINGER 1003
#define OSAL_OOBINLINE 1004
#define OSAL_SNDBUF 1005
#define OSAL_RCVBUF 1006
#define OSAL_RCVTIMEO 1007
#define OSAL_SNDTIMEO 1008
#define OSAL_ADD_MEMBERSHIP 1009
#define OSAL_DROP_MEMBERSHIP 1010
#define OSAL_TTL 1011
#define OSAL_DSCP 1012
#define OSAL_MULTICAST_TTL 1013
#define OSAL_ADDSRC_MEMBERSHIP 1014
#define OSAL_DROPSRC_MEMBERSHIP 1015

    /*****************************************************************************/
    /* Enums                                                                     */
    /*****************************************************************************/

    /* Protocols supported. */
    typedef enum
    {
        OSAL_TCP, /* Address family = AF_INET, Type = SOCK_STREAM, Protocol = 0 */
        OSAL_UDP /* Address family = AF_INET, Type = SOCK_DGRAM,  Protocol = 0 */
    } OSAL_PROTOCOL_T;

    /* File Descriptor types. Used to specify the type of activity to check on   */
    /* a socket.                                                                 */
    typedef enum
    {
        OSAL_READ_FD,
        OSAL_WRITE_FD,
        OSAL_EXCEPT_FD
    } OSAL_FD_TYPE_T;

    /* Scheduling policies supported */
    typedef enum
    {
        OSAL_SCHED_RR,
        OSAL_SCHED_FIFO,
        OSAL_SCHED_OTHER
    } OSAL_SCHED_POLICY_TYPE_T;

    /*****************************************************************************/
    /* Structures                                                                */
    /*****************************************************************************/

    /* Structure to initialize OSAL */
    typedef struct
    {
        /* Handle of memory manager being used. NULL is a valid argument.*/
        void *mmr_handle;

        /* Call back API to be called during allocation */
        void *(*osal_alloc)(void *mmr_handle, UWORD32 size);

        /* Call back API for freeing */
        void (*osal_free)(void *mmr_handle, void *mem);
    } osal_cb_funcs_t;

    /* The structure (osal_mbox_attr_t) contains the attributes of the thread    */
    /* which are passed to osal_mbox_create() API. The created Mail box has      */
    /* attributes specified using the structure variable.                        */
    typedef struct
    {
        void *thread_handle; /* Thread to be associated with mail box.      */
        STRWORD8 *name; /* NULL terminated string name for mail box   */
        UWORD32 msg_size; /* Length of each message.                     */
        UWORD32 mbx_len; /* Maximum number of messages.                 */
    } osal_mbox_attr_t;

    /* The structure (osal_sem_attr_t) contains the attributes of the semaphore  */
    /* which are passed to osal_sem_create() API. The Semaphore attributes like  */
    /* initial value of semaphore.                                               */
    typedef struct
    {
        WORD32 value;
    } osal_sem_attr_t;

    /* The Structure (osal_thread_attr_t) contains the attributes of the thread  */
    /* which are passed to osal_thread_create() API. The created thread has      */
    /* attributes specified using the structure variable.                        */
    typedef struct
    {
        /* Function from where thread execution starts */
        void *thread_func;

        /* Parameters for thread function. */
        void *thread_param;

        /* Stack size in bytes. For default value, set to '0' */
        UWORD32 stack_size;

        /* This attribute specifies a pre-allocated block of size 'stack_size'   */
        /* to be used for the task's private stack. For default value, set to    */
        /* 'NULL'.                                                               */
        void *stack_addr;

        /* NULL terminated string name for thread. For default value, set to     */
        /* 'NULL'.                                                               */
        WORD8 *name;

        /* Flag determining whether to use OSAL Thread priority mapping or not.  */
        /* Value set to 1 - use OSAL thread priority mapping.                    */
        /* Value set to 0 - Direct value set as thread priority                  */
        WORD32 priority_map_flag;

        /* Priority range shall be considered + ve values for increasing         */
        /* priority and negative values for decreasing priority. The range shall */
        /* be mapped to specific OS range internally through OSAL. For default   */
        /* value, set to '0'.                                                    */
        WORD32 priority;

        /* Exit return value on which thread shall exit */
        WORD32 exit_code;

        /* Scheduling policy of the thread */
        OSAL_SCHED_POLICY_TYPE_T sched_policy;

        /* Mask to specify on which cores the thread can run */
        ULWORD64 core_affinity_mask;

        /* Specifies on which group of processors the thread can run */
        WORD16 group_num;

    } osal_thread_attr_t;

    /* The structure (osal_socket_attr_t) contains the attributes of the socket  */
    /* which are to be specified during socket creation.                         */
    typedef struct
    {
        OSAL_PROTOCOL_T protocol;
    } osal_socket_attr_t;

    /* The structure (osal_sockaddr_t) is used to uniquely determine a socket in */
    /* the network. The socket can be addressed using IP address and port number.*/
    typedef struct
    {
        WORD8 ip_addr[16];
        UWORD16 port;
    } osal_sockaddr_t;

    /* The structure contains the select engine thread parameters like thread    */
    /* name thread priority etc.                                                 */
    typedef struct
    {
        /* Flag determining whether to use OSAL Thread priority mapping or not.  */
        /* Value set to 1 - use OSAL thread priority mapping.                    */
        /* Value set to 0 - Direct value set as thread priority                  */
        WORD32 priority_map_flag;

        /* Priority range shall be considered + ve values for increasing         */
        /* priority and negative values for decreasing priority. The range shall */
        /* be mapped to specific OS range internally through OSAL. For default   */
        /* value, set to '0'.                                                    */
        WORD32 priority;

        /* NULL terminated string name for thread. For default value, set to     */
        /* 'NULL'.                                                               */
        WORD8 *name;

        /* Timeout for thread sleep in micro seconds */
        UWORD32 select_timeout;

        /* Timeout for SELECT system called by osal library in micro seconds */
        UWORD32 select_poll_interval;
    } osal_select_engine_attr_t;

    /* The structure used to register sockets to select engine. This structure   */
    /* has to be updated for each socket handle and select register has to be    */
    /* done. Currently registration is supported one at a time.                  */
    /* Note: Function 'init' is assumed to return the socket handle.             */
    typedef struct osal_select_entry_t
    {
        /* Socket handle to be registered. */
        void *socket_handle;

        /* Activity to select for. */
        OSAL_FD_TYPE_T type;

        /* Call back called before doing select. The function init is assumed to */
        /* return the socket handle. In case of NULL being returning by this     */
        /* function, The socket will be unregistered                             */
        void *(*init)(void *);

        /* Argument to init function */
        void *init_param;

        /* Call back function on select success */
        WORD32 (*call_back)(void *socket_handle, void *call_back_param);

        /* Call back function parameters */
        void *call_back_param;

        /* Call back called when the socket is unregistered. If set to NULL,     */
        /* this will not be called. The socket that has been registered is the   */
        /* first argument, the second argument will be terminate_param           */
        void (*terminate)(void *, void *);

        /* Argument to terminate callback */
        void *terminate_param;

        /* Exit code of the call back function. */
        WORD32 exit_code;

        /* Identifier. Do not initialize this. */
        WORD32 id;
    } osal_select_entry_t;

    /* File descriptor structure. Used in osal_socket_select() API call.         */
    /* Currently maximum number of sockets that can be set is fixed to           */
    /* SELECT_MAX                                                                */
    /* Note : To initialize osal_fd_set structure variable, call API             */
    /*        osal_socket_fd_zero() for Initialization. If initialization is not */
    /*        done, behaviour of osal_socket_select() and fd_set API's is        */
    /*        undefined.                                                         */
    typedef struct
    {
        void *array[OSAL_SELECT_MAX]; /* Array for holding the socket descriptors*/
        WORD32 count; /* Number of socket descriptors in array   */
    } osal_fd_set_t;

    /* Timeout value for osal_socket_select() API. */
    typedef struct
    {
        WORD32 tv_sec; /* Time in seconds.                            */
        WORD32 tv_usec; /* Time in micro seconds.                      */
    } osal_timeval_t;

    /* Attributes for setting Linger option for socket */
    typedef struct
    {
        UWORD16 l_onoff;
        UWORD16 l_linger;
    } osal_sockopt_linger_t;

    /* Attributes for Joining or dropping from a multicast group */
    typedef struct
    {
        WORD8 imr_multiaddr[16];
        WORD8 imr_interface[16];
        WORD8 imr_srcaddr[16];
    } osal_ip_mreq_t;

    /*****************************************************************************/
    /* Extern OSAL Initialization Function Declarations                          */
    /*****************************************************************************/

    /* Allocates memory for the OSAL instance handle. It also allocates memory   */
    /* for storing debug information.                                            */
    extern WORD32 osal_init(IN void *osal_handle);

    /* Releases all the resources held by the OSAL handle */
    extern WORD32 osal_close(IN void *osal_handle);

    /* This function registers MMR call backs for OSAL */
    extern WORD32 osal_register_callbacks(IN void *osal_handle, IN osal_cb_funcs_t *cb_funcs);

    /*****************************************************************************/
    /* Extern Mail Box Function Declarations                                     */
    /*****************************************************************************/

    /* Allocates memory for mail box handle. Creates a mail box which is         */
    /* associated with the thread and updates the mail box, which returned for   */
    /* further actions to be performed on the mail box.                          */
    extern void *osal_mbox_create(IN void *osal_handle, IN osal_mbox_attr_t *attr);

    /* Closes the mail box and frees the memory allocated for mail box handle. */
    extern WORD32 osal_mbox_destroy(IN void *mbox_handle);

    /* Posts a message to the mail box */
    extern WORD32 osal_mbox_post(IN void *mbox_handle, IN void *buf, IN UWORD32 len);

    /* Gets the message form the specified mail box. If there are not messages   */
    /* in mail box, it waits infinitely till a message arrives.                  */
    extern WORD32 osal_mbox_get(IN void *mbox_handle, OUT void *buf, IN UWORD32 len);

    /* Gets the message from the specified mail box within the timeout period.   */
    /* If no messages are present in specified time, error code is returned. The */
    /* error can be got from osal_get_last_error() API                           */
    extern WORD32
        osal_mbox_get_timed(IN void *mbox_handle, OUT void *buf, IN UWORD32 len, IN UWORD32 timeout);

    /*****************************************************************************/
    /* Extern Custom Mail Box Function Declarations                               */
    /*****************************************************************************/

    /* Allocates memory for mail box handle. Creates a mail box which is         */
    /* associated with the thread and updates the mail box, which returned for   */
    /* further actions to be performed on the mail box.                          */
    extern void *osal_custom_mbox_create(IN void *osal_handle, IN osal_mbox_attr_t *attr);

    /* Closes the mail box and frees the memory allocated for mail box handle. */
    extern WORD32 osal_custom_mbox_destroy(IN void *mbox_handle);

    /* Posts a message to the mail box */
    extern WORD32 osal_custom_mbox_post(IN void *cust_mbox_handle, IN void *buf, IN UWORD32 len);

    /* Gets the message form the specified mail box. If there are not messages   */
    /* in mail box, it waits infinitely till a message arrives.                  */
    extern WORD32 osal_custom_mbox_get(IN void *cust_mbox_handle, OUT void *buf, IN UWORD32 len);

    /* Gets the message from the specified mail box within the timeout period.   */
    /* If no messages are present in specified time, error code is returned. The */
    /* error can be got from osal_get_last_error() API                           */
    extern WORD32 osal_custom_mbox_get_timed(
        IN void *cust_mbox_handle, OUT void *buf, IN UWORD32 len, IN UWORD32 timeout);

    /*****************************************************************************/
    /* Extern Mutex Function Declarations                                        */
    /*****************************************************************************/

    /* Creates a mutex and returns the to mutex */
    extern void *osal_mutex_create(IN void *osal_handle);

    /* Closes the mutex. */
    extern WORD32 osal_mutex_destroy(IN void *mutex_handle);

    /* Waits infinitely till mutex lock is got. */
    extern WORD32 osal_mutex_lock(IN void *mutex_handle);

    /* Releases the lock held on the mutex. */
    extern WORD32 osal_mutex_unlock(IN void *mutex_handle);

    /*****************************************************************************/
    /* Extern Semaphore Function Declarations                                    */
    /*****************************************************************************/

    /* Creates a semaphore and returns the handle to semaphore. */
    extern void *osal_sem_create(IN void *osal_handle, IN osal_sem_attr_t *attr);

    /* Closes the semaphore. */
    extern WORD32 osal_sem_destroy(IN void *sem_handle);

    /* Waits infinitely till semaphore is zero. */
    extern WORD32 osal_sem_wait(IN void *sem_handle);

    /* Increments the value of semaphore by one. */
    extern WORD32 osal_sem_post(IN void *sem_handle);

    /* Returns the current value of semaphore. */
    extern WORD32 osal_sem_count(IN void *sem_handle, OUT WORD32 *count);

    /*****************************************************************************/
    /* Extern Conditional Variable Function Declarations                         */
    /*****************************************************************************/

    /* Creates a conditional variable and returns the handle to it. */
    extern void *osal_cond_var_create(IN void *osal_handle);

    /* Destroys the conditional variable. */
    extern WORD32 osal_cond_var_destroy(IN void *cond_var_handle);

    /* Waits infinitely till conditional variable receives signal. */
    extern WORD32 osal_cond_var_wait(IN void *cond_var_handle, IN void *mutex_handle);

    /* Signals on conditional variable. */
    extern WORD32 osal_cond_var_signal(IN void *cond_var_handle);

    /*****************************************************************************/
    /* Extern Thread Function Declarations                                       */
    /*****************************************************************************/

    /* Creates a thread with specified parameters */
    extern void *osal_thread_create(IN void *osal_handle, IN osal_thread_attr_t *attr);

    /* Closes or halts the execution of thread specified by the handle. */
    extern WORD32 osal_thread_destroy(IN void *thread_handle);

    /* Makes the thread sleep for specified number of milliseconds */
    extern WORD32 osal_thread_sleep(IN UWORD32 milli_seconds);

    /* Yields the execution of thread. */
    extern WORD32 osal_thread_yield(void);

    /* Suspends the execution of thread until osal_thread_resume API is called. */
    extern WORD32 osal_thread_suspend(IN void *thread_handle);

    /* Resumes the execution of thread which was suspended by                    */
    /* osal_thread_suspend API call.                                             */
    extern WORD32 osal_thread_resume(IN void *thread_handle);

    /* Waits infinitely till the thread, whose handle is passed, completes       */
    /* execution.                                                                */
    extern WORD32 osal_thread_wait(IN void *thread_handle);

    /* Returns current thread handle */
    extern void *osal_get_thread_handle(IN void *osal_handle);

    /*****************************************************************************/
    /* Extern Network Socket Function Declarations                               */
    /*****************************************************************************/

    /* Initializes network resources */
    extern WORD32 osal_network_init(void);

    /* Un-initializes all the network resources */
    extern WORD32 osal_network_close(void);

    /* Creates the socket and returns the socket descriptor. */
    extern void *osal_socket_create(IN void *osal_handle, IN osal_socket_attr_t *attr);

    /* Closes the open socket. */
    extern WORD32 osal_socket_destroy(IN void *socket_handle);

    /* Binds to the specified port number on the local machine. Socket_create    */
    /* API has to be called before calling socket_bind.                          */
    extern WORD32 osal_socket_bind(IN void *socket_handle, IN osal_sockaddr_t *addr);

    /* Starts listening at the specified port for any incoming connections.      */
    /* Socket descriptor should be bound before calling socket_listen            */
    extern WORD32 osal_socket_listen(IN void *socket_handle, IN WORD32 backlog);

    /* Accepts incoming connection. If listen queue is empty it blocks till a    */
    /* successful connection is made.                                            */
    extern void *osal_socket_accept(IN void *socket_handle, OUT osal_sockaddr_t *addr);

    /* Makes a connection request to the remote address specified. */
    extern WORD32 osal_socket_connect(IN void *socket_handle, IN osal_sockaddr_t *addr);

    /* Sends the specified number of bytes of data */
    extern WORD32 osal_socket_send(
        IN void *socket_handle, IN const WORD8 *buf, IN WORD32 len, IN WORD32 flags);

    /* Receives data over TCP connection. */
    extern WORD32
        osal_socket_recv(IN void *socket_handle, OUT WORD8 *buf, IN WORD32 len, IN WORD32 flags);

    /* Sends data over a datagram protocol */
    extern WORD32 osal_socket_sendto(
        IN void *socket_handle,
        IN const WORD8 *buf,
        IN WORD32 len,
        IN WORD32 flags,
        IN osal_sockaddr_t *to);

    /* Receives packet over a UDP connection */
    extern WORD32 osal_socket_recvfrom(
        IN void *socket_handle,
        OUT WORD8 *buf,
        IN WORD32 len,
        IN WORD32 flags,
        OUT osal_sockaddr_t *from);

    /* Polls the specified sockets for specified activity */
    extern WORD32 osal_socket_select(
        INOUT osal_fd_set_t *readfds,
        INOUT osal_fd_set_t *writefds,
        INOUT osal_fd_set_t *exceptfds,
        INOUT osal_timeval_t *timeout);

    /* Gets the socket options */
    extern WORD32 osal_socket_getsockopt(
        IN void *socket_handle,
        IN WORD32 level,
        IN WORD32 optname,
        OUT WORD8 *optval,
        INOUT WORD32 *optlen);

    /* Sets the socket options to specified values */
    extern WORD32 osal_socket_setsockopt(
        IN void *socket_handle,
        IN WORD32 level,
        IN WORD32 optname,
        IN const WORD8 *optval,
        IN WORD32 optlen);

    /* Adds the specified socket handle to the file descriptor set */
    extern WORD32 osal_socket_fd_set(IN void *socket_handle, OUT osal_fd_set_t *set);

    /* Checks the file descriptor set for the presence of socket handle. */
    extern WORD32 osal_socket_fd_isset(IN void *socket_handle, IN osal_fd_set_t *set);

    /* Resets the file descriptor set */
    extern void osal_socket_fd_zero(INOUT osal_fd_set_t *set);

    /* Removes the specified socket handle from the file descriptor set */
    extern WORD32 osal_socket_fd_clr(IN void *socket_handle, OUT osal_fd_set_t *set);

    /* To convert short integer from host byte order to network byte order */
    extern UWORD16 osal_htons(IN UWORD16 hostshort);

    /* To convert long integer from host to network byte order */
    extern UWORD32 osal_htonl(IN UWORD32 hostlong);

    /* To convert short integer from network to host byte order */
    extern UWORD16 osal_ntohs(IN UWORD16 netshort);

    /* To convert long integer from network to host byte order */
    extern UWORD32 osal_ntohl(IN UWORD32 netlong);

    /*****************************************************************************/
    /* Extern Select Engine Function Declarations                                */
    /*****************************************************************************/

    /* Initializes the select engine. */
    extern void *
        osal_select_engine_init(IN void *osal_handle, IN osal_select_engine_attr_t *se_attr);

    /* Closes the select engine. */
    extern WORD32 osal_select_engine_close(IN void *select_engine);

    /* Registers the socket handle specified in the entry. */
    extern WORD32
        osal_select_engine_register(IN void *select_engine, IN osal_select_entry_t *entry);

    /* Un-registers the specified socket handle. */
    extern WORD32 osal_select_engine_unregister(
        IN void *select_engine, IN void *socket_handle, IN OSAL_FD_TYPE_T fd_type);
    /*****************************************************************************/
    /* Extern Other Function Declarations                                        */
    /*****************************************************************************/

    /* Returns time in milliseconds */
    extern UWORD32 osal_get_time(void);

    /* For time in micro-second resolution */
    extern WORD32 osal_get_time_usec(UWORD32 *sec, UWORD32 *usec);

    /* Returns the last error code. 0 is no error */
    extern UWORD32 osal_get_last_error(void);

    /* Prints the last error code. 0 is no error */
    extern void osal_print_last_error(IN const STRWORD8 *string);

    /* Gets the version of library in NULL terminated string form. */
    extern WORD8 *osal_get_version(void);

    /* Gets the tid of the thread in whose context this call was made */
    extern WORD32 osal_get_current_tid(void);

/* C linkage specifiers for C++ declarations. */
#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* OSAL_H */
