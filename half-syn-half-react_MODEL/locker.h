#ifndef LOCKER_H
#define LOCKER_H

#include <exception>
#include <pthread.h>
#include <semaphore.h>
//三个主要的同步工具：信号量，互斥锁，条件变量
class sem
{
public:
    sem()
    {
	//第二个参数如果为0，则表明该信号量不是跨进程的，是本进程的局部信号量
	//第三个参数指定信号量的初始值
        if( sem_init( &m_sem, 0, 0 ) != 0 )
        {
            throw std::exception();
        }
    }
    ~sem()
    {
	//销毁一个信号量，释放其占用的内核资源
        sem_destroy( &m_sem );
    }

    //一般来说这种系统调用返回0就是成功
    bool wait()
    {
	//以原子操作将信号量的值减1，如果信号量的值为0，那么一直阻塞。
	//on error, the value of the semaphore is left unchanged, 
	//-1 is returned, and errno is set to indicate the error.
        return sem_wait( &m_sem ) == 0;
    }
    bool post()
    {
	//以原子操作将信号量的值加1
	//on error, the value of the
        //semaphore is left unchanged, -1 is returned, 
	//and errno is set to indicate the error.
        return sem_post( &m_sem ) == 0;
    }

private:
    sem_t m_sem;
};

//互斥锁的pshared属性可以决定是否跨进程
class locker
{
public:
    locker()
    {
	//第二个参数是互斥锁的属性，如果null，那么就是默认属性
        if( pthread_mutex_init( &m_mutex, NULL ) != 0 )
        {
            throw std::exception();
        }
    }
    ~locker()
    {
	//释放其占用的内核资源
        pthread_mutex_destroy( &m_mutex );
    }

    //If successful, the pthread_mutex_lock(), 
    //pthread_mutex_trylock(), and pthread_mutex_unlock() functions shall return zero; otherwise, 
    //an error number shall be returned to indicate the error.
    bool lock()
    {
	//阻塞函数
        return pthread_mutex_lock( &m_mutex ) == 0;
    }
    bool unlock()
    {
        return pthread_mutex_unlock( &m_mutex ) == 0;
    }

private:
    pthread_mutex_t m_mutex;
};

class cond
{
public:
    cond()
    {
        if( pthread_mutex_init( &m_mutex, NULL ) != 0 )
        {
            throw std::exception();
        }
        if ( pthread_cond_init( &m_cond, NULL ) != 0 )
        {
            pthread_mutex_destroy( &m_mutex );
            throw std::exception();
        }
    }
    ~cond()
    {
        pthread_mutex_destroy( &m_mutex );
        pthread_cond_destroy( &m_cond );
    }
    bool wait()
    {
        int ret = 0;
        pthread_mutex_lock( &m_mutex );
        ret = pthread_cond_wait( &m_cond, &m_mutex );
        pthread_mutex_unlock( &m_mutex );
        return ret == 0;
    }
    bool signal()
    {
        return pthread_cond_signal( &m_cond ) == 0;
    }

private:
    pthread_mutex_t m_mutex;
    pthread_cond_t m_cond;
};

#endif
