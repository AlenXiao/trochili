/*************************************************************************************************
 *                                     Trochili RTOS Kernel                                      *
 *                                  Copyright(C) 2016 LIUXUMING                                  *
 *                                       www.trochili.com                                        *
 *************************************************************************************************/
#include <string.h>

#include "tcl.types.h"
#include "tcl.config.h"
#include "tcl.object.h"
#include "tcl.debug.h"
#include "tcl.cpu.h"
#include "tcl.ipc.h"
#include "tcl.kernel.h"
#include "tcl.thread.h"
#include "tcl.timer.h"

#if (TCLC_TIMER_ENABLE)

/*
 * �ں˶�ʱ����Ϊ2�֣��ֱ������߳���ʱ��ʱ�޷�ʽ������Դ���̶߳�ʱ�����û���ʱ��
 * ��ģ��ֻ�����û���ʱ����
 */
static TTimerList TimerList;


/*************************************************************************************************
 *  ���ܣ���ʱ��ִ�д�����                                                                     *
 *  ������(1) pTimer ��ʱ��                                                                      *
 *  ���أ���                                                                                     *
 *  ˵��: (1)�߳���ʱ��ʱ���̱߳������ں��̸߳���������                                        *
 *        (2)�߳���ʱ�޷�ʽ������Դ��ʱ������ò�����Դ�����̻߳ᱻͬʱ������Դ���߳���������  *
 *           ���ں��̸߳��������С�                                                              *
 *        (3)�û������Զ�ʱ�������󣬻����̽�����һ�ּ�ʱ��ͬʱҲ������������С������û���ʱ��  *
 *           ������ϵͳ��ʱ���ػ��̴߳���Ҳ����˵���û���ʱ���Ļص����������߳�ִ̬�еġ�      *
 *        (4)������Ȼ���̶߳��в������ǲ����е��ȣ�����Ϊ������������ж��е��õģ�              *
 *           �����һ���жϷ��غ󣬻᳢�Խ���һ���߳��л����������������л��Ļ��ǰװ��˷�ʱ��    *
 *************************************************************************************************/
static void DispatchExpiredTimer(TTimer* pTimer)
{
    TIndex spoke;

    /*
     * ����ʱ�������ں˶�ʱ�������б�
     * ����ɶ�ʱ���ػ��̴߳��������Ķ�ʱ�����ȴ���;
     */
    if (!(pTimer->Property & TIMER_PROP_EXPIRED))
    {
        uObjListAddPriorityNode(&(TimerList.ExpiredHandle), &(pTimer->ExpiredNode));
        pTimer->ExpiredTicks = pTimer->MatchTicks;
        pTimer->Property |= TIMER_PROP_EXPIRED;
    }

    /* ����ʱ���ӻ�������Ƴ� */
    uObjListRemoveNode(pTimer->LinkNode.Handle, &(pTimer->LinkNode));

    /* ���������͵��û���ʱ�����·Żػ��ʱ�������� */
    if (pTimer->Property & TIMER_PROP_PERIODIC)
    {
      	pTimer->ExpiredTimes++;
        pTimer->MatchTicks += pTimer->PeriodTicks;
        spoke = (TBase32)(pTimer->MatchTicks % TCLC_TIMER_WHEEL_SIZE);
        uObjListAddPriorityNode(&(TimerList.ActiveHandle[spoke]), &(pTimer->LinkNode));
        pTimer->Status = eTimerActive;
    }
    else
    {
        /* �����λص���ʱ���ŵ����߶����� */
        uObjListAddNode(&(TimerList.DormantHandle), &(pTimer->LinkNode), eLinkPosHead);
        pTimer->Status = eTimerDormant;
    }
}


/*************************************************************************************************
 *  ���ܣ��ں˶�ʱ��ISR������                                                                  *
 *  ��������                                                                                     *
 *  ���أ���                                                                                     *
 *  ˵��:                                                                                        *
 *************************************************************************************************/
void uTimerTickUpdate(void)
{
    TTimer*   pTimer;
    TIndex    spoke;
    TLinkNode* pNode;
    TLinkNode* pNext;

    /* ���ݵ�ǰϵͳʱ�ӽ��ļ����������ǰ���ʱ������ */
    spoke = (TIndex)(uKernelVariable.Jiffies % TCLC_TIMER_WHEEL_SIZE);
    pNode = TimerList.ActiveHandle[spoke];

    /*
     * ��鵱ǰ���ʱ��������Ķ�ʱ����������Ķ�ʱ��������������ֵ��С�������С�
     * ���ĳ�����ж��׶�ʱ������С�ڵ�ǰϵͳʱ�ӽ��ļ�������˵���ж�ʱ���������������
     * �����ڱ�ϵͳ�У�ϵͳʱ�ӽ��ļ���Ϊ64Bits,ͬʱǿ��Ҫ��ʱ����ʱ��������С��63Bits��
     * ������ʹ��ʱ���������������Ҳ���ᶪʧ������
     */
    while (pNode != (TLinkNode*)0)
    {
        pNext = pNode->Next;
        pTimer = (TTimer*)(pNode->Owner);

        /*
         * �Ƚ϶�ʱ������ʱ�������ʹ�ʱϵͳʱ�ӽ�������
         * ���С���������ö�ʱ��;
         * �����������ö�ʱ��;
         * ����������˳���������;
         */
        if (pTimer->MatchTicks < uKernelVariable.Jiffies)
        {
            pNode = pNext;
        }
        else if (pTimer->MatchTicks == uKernelVariable.Jiffies)
        {
            DispatchExpiredTimer(pTimer);
            pNode = pNext;
        }
        else
        {
            break;
        }
    }

    /* �����Ҫ�����ں����õ��û���ʱ���ػ��߳� */
    if (TimerList.ExpiredHandle != (TLinkNode*)0)
    {
        uThreadResumeFromISR(uKernelVariable.TimerDaemon);
    }
}


/*************************************************************************************************
 *  ���ܣ��û���ʱ����ʼ������                                                                   *
 *  ������(1) pTimer   ��ʱ����ַ                                                                *
 *        (2) pName    ��ʱ������                                                                *
 *        (3) property ��ʱ������                                                                *
 *        (4) ticks    ��ʱ���δ���Ŀ                                                            *
 *        (5) pRoutine �û���ʱ���ص�����                                                        *
 *        (6) pData    �û���ʱ���ص���������                                                    *
 *        (7) priority ��ʱ�����ȼ�                                                              *
 *        (8) pError   ��ϸ���ý��                                                              *
 *  ����: (1) eSuccess �����ɹ�                                                                  *
 *        (2) eFailure ����ʧ��                                                                  *
 *  ˵��                                                                                         *
 *************************************************************************************************/
TState xTimerCreate(TTimer* pTimer, TChar* pName, TProperty property, TTimeTick ticks,
                    TTimerRoutine pRoutine, TArgument data, TPriority priority, TError* pError)
{
    TState state = eFailure;
    TError error = TIMER_ERR_FAULT;
    TReg32 imask;

    CpuEnterCritical(&imask);

    /* ��鶨ʱ���������� */
    if (!(pTimer->Property & TIMER_PROP_READY))
    {
        /* ����ʱ�����뵽�ں˶�������� */
        uKernelAddObject(&(pTimer->Object), pName, eTimer, (void*)pTimer);

        /* ��ʼ����ʱ�������ö�ʱ����Ϣ */
        pTimer->Status       = eTimerDormant;
        pTimer->Property     = (property | TIMER_PROP_READY);
        pTimer->PeriodTicks  = ticks;
        pTimer->MatchTicks   = (TTimeTick)0;
        pTimer->Routine      = pRoutine;
        pTimer->Argument     = data;
        pTimer->Priority     = priority;
        pTimer->ExpiredTicks = (TTimeTick)0;
  	    pTimer->ExpiredTimes = 0U;
		
        /* ���ö�ʱ����������ڵ���Ϣ */
        pTimer->ExpiredNode.Next   = (TLinkNode*)0;
        pTimer->ExpiredNode.Prev   = (TLinkNode*)0;
        pTimer->ExpiredNode.Handle = (TLinkNode**)0;
        pTimer->ExpiredNode.Data   = (TBase32*)(&(pTimer->Priority));
        pTimer->ExpiredNode.Owner  = (void*)pTimer;

        /* ���ö�ʱ������ڵ���Ϣ, ������ʱ���������߶����� */
        pTimer->LinkNode.Next   = (TLinkNode*)0;
        pTimer->LinkNode.Prev   = (TLinkNode*)0;
        pTimer->LinkNode.Handle = (TLinkNode**)0;
        pTimer->LinkNode.Data   = (TBase32*)(&(pTimer->MatchTicks));
        pTimer->LinkNode.Owner  = (void*)pTimer;
        uObjListAddNode(&(TimerList.DormantHandle), &(pTimer->LinkNode), eLinkPosHead);

        error = TIMER_ERR_NONE;
        state = eSuccess;

    }

    CpuLeaveCritical(imask);

    *pError = error;
    return state;
}


/*************************************************************************************************
 *  ���ܣ��ں˶�ʱ��ȡ����ʼ��                                                                   *
 *  ������(1) pTimer   ��ʱ���ṹ��ַ                                                            *
 *        (2) pError   ��ϸ���ý��                                                              *
 *  ����: (1) eSuccess �����ɹ�                                                                  *
 *        (2) eFailure ����ʧ��                                                                  *
 *  ˵��                                                                                         *
 *************************************************************************************************/
TState xTimerDelete(TTimer* pTimer, TError* pError)
{
    TState state = eFailure;
    TError error = TIMER_ERR_UNREADY;
    TReg32 imask;

    CpuEnterCritical(&imask);

    /* ��鶨ʱ���������� */
    if (pTimer->Property & TIMER_PROP_READY)
    {
        if (pTimer->Status == eTimerDormant)
        {
            /* ����ʱ�����ں˶����б����Ƴ� */
            uKernelRemoveObject(&(pTimer->Object));

            /* ����ʱ����������ʱ���������Ƴ� */
            uObjListRemoveNode(pTimer->LinkNode.Handle, &(pTimer->LinkNode));

            /* ��ն�ʱ������ */
            memset(pTimer, 0U, sizeof(TTimer));
            error = TIMER_ERR_NONE;
            state = eSuccess;
        }
        else
        {
            error = TIMER_ERR_STATUS;
        }
    }

    CpuLeaveCritical(imask);

    *pError = error;
    return state;
}


/*************************************************************************************************
 *  ���ܣ���ʱ����������                                                                         *
 *  ������(1) pTimer     ��ʱ���ṹ��ַ                                                          *
 *        (2) lagticks   ��ʱ���ӻ���ʼ����ʱ��                                                  *
 *        (3) pError     ��ϸ���ý��                                                            *
 *  ����: (1) eSuccess   �����ɹ�                                                                *
 *        (2) eFailure   ����ʧ��                                                                *
 *  ˵������                                                                                     *
 *************************************************************************************************/
TState xTimerStart(TTimer* pTimer,TTimeTick lagticks, TError* pError)
{
    TState state = eFailure;
    TError error = TIMER_ERR_UNREADY;
    TReg32 imask;
    TIndex spoke;

    CpuEnterCritical(&imask);

    /* ��鶨ʱ���������� */
    if (pTimer->Property & TIMER_PROP_READY)
    {
        if (pTimer->Status == eTimerDormant)
        {
            /* ����ʱ�������߶������Ƴ� */
            uObjListRemoveNode(pTimer->LinkNode.Handle, &(pTimer->LinkNode));

            /* ����ʱ������������ */
            pTimer->MatchTicks  = uKernelVariable.Jiffies + pTimer->PeriodTicks + lagticks;
            spoke = (TBase32)(pTimer->MatchTicks % TCLC_TIMER_WHEEL_SIZE);
            uObjListAddPriorityNode(&(TimerList.ActiveHandle[spoke]), &(pTimer->LinkNode));
            pTimer->Status = eTimerActive;
            error = TIMER_ERR_NONE;
            state = eSuccess;
        }
        else
        {
            error = TIMER_ERR_STATUS;
        }

    }

    CpuLeaveCritical(imask);

    *pError = error;
    return state;
}


/*************************************************************************************************
 *  ���ܣ�ֹͣ�û���ʱ������                                                                     *
 *  ������(1) pTimer   ��ʱ����ַ                                                                *
 *        (2) pError   ��ϸ���ý��                                                              *
 *  ����: (1) eSuccess �����ɹ�                                                                  *
 *        (2) eFailure ����ʧ��                                                                  *
 *  ˵����                                                                                       *
 *************************************************************************************************/
TState xTimerStop(TTimer* pTimer, TError* pError)
{
    TState state = eFailure;
    TError error = TIMER_ERR_UNREADY;
    TReg32 imask;

    CpuEnterCritical(&imask);

    /* ��鶨ʱ���������� */
    if (pTimer->Property & TIMER_PROP_READY)
    {
        /* ����ʱ���ӻ����/�����������Ƴ����ŵ����߶����� */
        if (pTimer->Status == eTimerActive)
        {
            if (pTimer->Property & TIMER_PROP_EXPIRED)
            {
                uObjListRemoveNode(pTimer->ExpiredNode.Handle, &(pTimer->ExpiredNode));
                pTimer->Property &= ~TIMER_PROP_EXPIRED;
            }

            uObjListRemoveNode(pTimer->LinkNode.Handle, &(pTimer->LinkNode));
            uObjListAddNode(&(TimerList.DormantHandle), &(pTimer->LinkNode), eLinkPosHead);
            pTimer->Status = eTimerDormant;

            error = TIMER_ERR_NONE;
            state = eSuccess;
        }
        else
        {
            error = TIMER_ERR_STATUS;
        }

    }

    CpuLeaveCritical(imask);

    *pError = error;
    return state;
}


/*************************************************************************************************
 *  ���ܣ����ö�ʱ�����͡���ʱʱ������ȼ�                                                       *
 *  ������(1) pTimer   ��ʱ���ṹ��ַ                                                            *
 *        (2) ticks    ��ʱ��ʱ�ӽ�����Ŀ                                                        *
 *        (3) priority ��ʱ�����ȼ�                                                              *
 *        (4) pError   ��ϸ���ý��                                                              *
 *  ����: (1) eSuccess �����ɹ�                                                                  *
 *        (2) eFailure ����ʧ��                                                                  *
 *  ˵��                                                                                         *
 *************************************************************************************************/
TState xTimerConfig(TTimer* pTimer, TTimeTick ticks, TPriority priority, TError* pError)
{
    TState state = eFailure;
    TError error = TIMER_ERR_UNREADY;
    TReg32 imask;

    CpuEnterCritical(&imask);

    /* ��鶨ʱ���������� */
    if (pTimer->Property & TIMER_PROP_READY)
    {
        if (pTimer->Status == eTimerDormant)
        {
            pTimer->PeriodTicks = ticks;
            pTimer->Priority    = priority;
            error = TIMER_ERR_NONE;
            state = eSuccess;
        }
        else
        {
            error = TIMER_ERR_STATUS;
        }
    }

    CpuLeaveCritical(imask);

    *pError = error;
    return state;
}


/* �ں˶�ʱ���ػ��̶߳����ջ���� */
static TBase32 TimerDaemonStack[TCLC_TIMER_DAEMON_STACK_BYTES >> 2];
static TThread TimerDaemonThread;

/* �ں˶�ʱ���ػ��̲߳������κ��̹߳���API���� */
#define TIMER_DAEMON_ACAPI (THREAD_ACAPI_NONE)

/*************************************************************************************************
 *  ���ܣ��ں��еĶ�ʱ���ػ��̺߳���                                                             *
 *  ������(1) argument ��ʱ���̵߳��û�����                                                      *
 *  ���أ���                                                                                     *
 *  ˵����                                                                                       *
 *************************************************************************************************/
static void xTimerDaemonEntry(TArgument argument)
{
    TBase32       imask;
    TTimer*       pTimer;
    TTimerRoutine pRoutine;
    TArgument     data;
    TTimeTick     ticks;

    /*
     * ��������û���ʱ�������̻߳����´���ʱ���ص�����
     * ���������ʱ������Ϊ���򽫶�ʱ���ػ��̹߳���
     */
    while(eTrue)
    {
        CpuEnterCritical(&imask);

        if (TimerList.ExpiredHandle == (TLinkNode*)0)
        {
            uThreadSuspendSelf();
            CpuLeaveCritical(imask);
        }
        else
        {
            /* ������������ȡ��һ����ʱ�� */
            pTimer = (TTimer*)(TimerList.ExpiredHandle->Owner);

            /*
             * ���㶨ʱ����Ư��ʱ��,�����׼��ʱ����Ư��ʱ����ڵ��ڶ�ʱ���ڣ�
             * ˵����ʱ���������ʵ��̫����~,һ�������������⡣
             */
            if (uKernelVariable.Jiffies == pTimer->ExpiredTicks)
            {
                ticks = 0U;
            }
            else if (uKernelVariable.Jiffies > pTimer->ExpiredTicks)
            {
                ticks = uKernelVariable.Jiffies - pTimer->ExpiredTicks;
            }
            else
            {
                ticks = TCLM_MAX_VALUE64 - pTimer->ExpiredTicks + uKernelVariable.Jiffies;
            }

            if (pTimer->Property & TIMER_PROP_ACCURATE)
            {
                if (ticks >= pTimer->PeriodTicks)
                {
                    uKernelVariable.Diagnosis |= KERNEL_DIAG_TIMER_ERROR;
                    pTimer->Diagnosis |= TIMER_DIAG_OVERFLOW;
                    uDebugAlarm("");
                }
            }

            /* ����ʱ���������������Ƴ� */
            uObjListRemoveNode(pTimer->ExpiredNode.Handle, &(pTimer->ExpiredNode));
            pTimer->Property &= ~TIMER_PROP_EXPIRED;

            /* ���ƶ�ʱ�������ͺ������� */
            pRoutine = pTimer->Routine;
            data = pTimer->Argument;

            CpuLeaveCritical(imask);

            /* ���̻߳�����ִ�ж�ʱ������ */
            pRoutine(data, ticks);
        }
    }
}


/*************************************************************************************************
 *  ���ܣ���ʼ���û���ʱ���ػ��߳�                                                               *
 *  ��������                                                                                     *
 *  ���أ���                                                                                     *
 *  ˵����                                                                                       *
 *************************************************************************************************/
void uTimerCreateDaemon(void)
{
    /* ����ں��Ƿ��ڳ�ʼ״̬ */
    if(uKernelVariable.State != eOriginState)
    {
        uDebugPanic("", __FILE__, __FUNCTION__, __LINE__);
    }

    /* ��ʼ���ں˶�ʱ�������߳� */
    uThreadCreate(&TimerDaemonThread,
                  "timer daemon",
                  eThreadSuspended,
                  THREAD_PROP_PRIORITY_FIXED|\
                  THREAD_PROP_CLEAN_STACK|\
                  THREAD_PROP_KERNEL_DAEMON,
                  TIMER_DAEMON_ACAPI,
                  xTimerDaemonEntry,
                  (TArgument)(0U),
                  (void*)TimerDaemonStack,
                  (TBase32)TCLC_TIMER_DAEMON_STACK_BYTES,
                  (TPriority)TCLC_TIMER_DAEMON_PRIORITY,
                  (TTimeTick)TCLC_TIMER_DAEMON_SLICE);

    /* ��ʼ����ص��ں˱��� */
    uKernelVariable.TimerDaemon = &TimerDaemonThread;
}


/*************************************************************************************************
 *  ���ܣ���ʱ��ģ���ʼ��                                                                       *
 *  ��������                                                                                     *
 *  ���أ���                                                                                     *
 *  ˵����                                                                                       *
 *************************************************************************************************/
void uTimerModuleInit(void)
{
    /* ����ں��Ƿ��ڳ�ʼ״̬ */
    if(uKernelVariable.State != eOriginState)
    {
        uDebugPanic("", __FILE__, __FUNCTION__, __LINE__);
    }

    memset(&TimerList, 0, sizeof(TimerList));

    /* ��ʼ����ص��ں˱��� */
    uKernelVariable.TimerList = &TimerList;
}
#endif

