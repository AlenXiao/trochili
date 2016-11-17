/*************************************************************************************************
 *                                     Trochili RTOS Kernel                                      *
 *                                  Copyright(C) 2016 LIUXUMING                                  *
 *                                       www.trochili.com                                        *
 *************************************************************************************************/
#include "tcl.types.h"
#include "tcl.config.h"
#include "tcl.object.h"
#include "tcl.debug.h"
#include "tcl.kernel.h"
#include "tcl.timer.h"
#include "tcl.thread.h"
#include "tcl.ipc.h"

#if (TCLC_IPC_ENABLE)

/*************************************************************************************************
 *  ���ܣ����̼߳��뵽ָ����IPC�߳�����������                                                    *
 *  ������(1) pQueue   IPC���е�ַ                                                               *
 *        (2) pThread  �߳̽ṹ��ַ                                                              *
 *  ���أ���                                                                                     *
 *  ˵����                                                                                       *
 *************************************************************************************************/
static void EnterBlockedQueue(TIpcQueue* pQueue, TIpcContext* pContext)
{
    TProperty property;

    property = *(pQueue->Property);
    if ((pContext->Option) & IPC_OPT_USE_AUXIQ)
    {
        if (property &IPC_PROP_PREEMP_AUXIQ)
        {
            uObjQueueAddPriorityNode(&(pQueue->AuxiliaryHandle), &(pContext->LinkNode));
        }
        else
        {
            uObjQueueAddFifoNode(&(pQueue->AuxiliaryHandle), &(pContext->LinkNode), eLinkPosTail);
        }
        property |= IPC_PROP_AUXIQ_AVAIL;
    }
    else
    {
        if (property &IPC_PROP_PREEMP_PRIMIQ)
        {
            uObjQueueAddPriorityNode(&(pQueue->PrimaryHandle), &(pContext->LinkNode));
        }
        else
        {
            uObjQueueAddFifoNode(&(pQueue->PrimaryHandle), &(pContext->LinkNode), eLinkPosTail);
        }
        property |= IPC_PROP_PRIMQ_AVAIL;
    }

    *(pQueue->Property) = property;

    /* �����߳��������� */
    pContext->Queue = pQueue;
}


/*************************************************************************************************
 *  ���ܣ����̴߳�ָ�����̶߳������Ƴ�                                                           *
 *  ������(1) pQueue   IPC���е�ַ                                                               *
 *        (2) pThread  �߳̽ṹ��ַ                                                              *
 *  ���أ���                                                                                     *
 *  ˵����                                                                                       *
 *************************************************************************************************/
static void LeaveBlockedQueue(TIpcQueue* pQueue, TIpcContext* pContext)
{
    TProperty property;

    property = *(pQueue->Property);

    /* ���̴߳�ָ���ķֶ�����ȡ�� */
    if ((pContext->Option) & IPC_OPT_USE_AUXIQ)
    {
        uObjQueueRemoveNode(&(pQueue->AuxiliaryHandle), &(pContext->LinkNode));
        if (pQueue->AuxiliaryHandle == (TLinkNode*)0)
        {
            property &= ~IPC_PROP_AUXIQ_AVAIL;
        }
    }
    else
    {
        uObjQueueRemoveNode(&(pQueue->PrimaryHandle), &(pContext->LinkNode));
        if (pQueue->PrimaryHandle == (TLinkNode*)0)
        {
            property &= ~IPC_PROP_PRIMQ_AVAIL;
        }
    }

    *(pQueue->Property) = property;

    /* �����߳��������� */
    pContext->Queue = (TIpcQueue*)0;
}


/*************************************************************************************************
 *  ���ܣ����̷߳�����Դ��������                                                                 *
 *  ������(1) pContext���������ַ                                                               *
 *        (2) pQueue  �̶߳��нṹ��ַ                                                           *
 *        (3) ticks   ��Դ�ȴ�ʱ��                                                               *
 *  ���أ���                                                                                     *
 *  ˵���������߳̽�����ض��еĲ��Ը��ݶ��в�������������                                       *
 *************************************************************************************************/
void uIpcBlockThread(TIpcContext* pContext, TIpcQueue* pQueue, TTimeTick ticks)
{
    TThread* pThread;

    KNL_ASSERT((uKernelVariable.State != eIntrState), "");

    /* ����̵߳�ַ */
    pThread = (TThread*)(pContext->Owner);

    /* ֻ�д��ھ���״̬���̲߳ſ��Ա����� */
    if (pThread->Status != eThreadRunning)
    {
        uKernelVariable.Diagnosis |= KERNEL_DIAG_THREAD_ERROR;
        pThread->Diagnosis |= THREAD_DIAG_INVALID_STATE;
        uDebugPanic("", __FILE__, __FUNCTION__, __LINE__);
    }

    /* ���̷߳����ں��̸߳������� */
    uThreadLeaveQueue(uKernelVariable.ThreadReadyQueue, pThread);
    uThreadEnterQueue(uKernelVariable.ThreadAuxiliaryQueue, pThread, eLinkPosTail);
    pThread->Status = eThreadBlocked;

    /* ���̷߳����������� */
    EnterBlockedQueue(pQueue, pContext);

    /* �����Ҫ�������߳����ڷ�����Դ��ʱ�޶�ʱ�� */
    if ((pContext->Option & IPC_OPT_TIMEO) && (ticks > 0U))
    {
        pThread->Timer.RemainTicks = ticks;
        uObjListAddDiffNode(&(uKernelVariable.ThreadTimerList),
                            &(pThread->Timer.LinkNode));
    }
}


/*************************************************************************************************
 *  ���ܣ�����IPC����������ָ�����߳�                                                            *
 *  ������(1) pContext���������ַ                                                               *
 *        (2) state   �߳���Դ���ʷ��ؽ��                                                       *
 *        (3) error   ��ϸ���ý��                                                               *
 *        (4) pHiRP   �Ƿ����Ѹ������ȼ���������Ҫ�����̵߳��ȵı��                           *
 *  ���أ���                                                                                     *
 *  ˵����                                                                                       *
 *************************************************************************************************/
void uIpcUnblockThread(TIpcContext* pContext, TState state, TError error, TBool* pHiRP)
{
    TThread* pThread;
    pThread = (TThread*)(pContext->Owner);

    /* ֻ�д�������״̬���̲߳ſ��Ա�������� */
    if (pThread->Status != eThreadBlocked)
    {
        uKernelVariable.Diagnosis |= KERNEL_DIAG_THREAD_ERROR;
        pThread->Diagnosis |= THREAD_DIAG_INVALID_STATE;
        uDebugPanic("", __FILE__, __FUNCTION__, __LINE__);
    }

    /*
     * �����̣߳�����̶߳��к�״̬ת��,ע��ֻ���жϴ���ʱ��
     * ��ǰ�̲߳Żᴦ���ں��̸߳���������(��Ϊ��û���ü��߳��л�)
     * ��ǰ�̷߳��ؾ�������ʱ��һ��Ҫ�ص���Ӧ�Ķ���ͷ
     * ���߳̽�����������ʱ������Ҫ�����̵߳�ʱ�ӽ�����
     */
    uThreadLeaveQueue(uKernelVariable.ThreadAuxiliaryQueue, pThread);
    if (pThread == uKernelVariable.CurrentThread)
    {
        uThreadEnterQueue(uKernelVariable.ThreadReadyQueue,
                          pThread, eLinkPosHead);
        pThread->Status = eThreadRunning;
    }
    else
    {
        uThreadEnterQueue(uKernelVariable.ThreadReadyQueue,
                          pThread, eLinkPosTail);
        pThread->Status = eThreadReady;
    }

    /* ���̴߳����������Ƴ� */
    LeaveBlockedQueue(pContext->Queue, pContext);

    /* �����̷߳�����Դ�Ľ���ʹ������ */
    *(pContext->State) = state;
    *(pContext->Error) = error;

    /* ����߳�����ʱ�޷�ʽ������Դ��رո��̵߳�ʱ�޶�ʱ�� */
    if (pContext->Option & IPC_OPT_TIMEO)
    {
        uObjListRemoveDiffNode(&(uKernelVariable.ThreadTimerList),
                               &(pThread->Timer.LinkNode));
    }

    /* �����̵߳���������,�˱��ֻ���̻߳�������Ч��
     * ��ISR���ǰ�߳̿������κζ��������ǰ�߳���Ƚ����ȼ�Ҳ��������ġ�
     * ���̻߳����£������ǰ�̵߳����ȼ��Ѿ��������߳̾������е�������ȼ���
     * �����ں˴�ʱ��û�йر��̵߳��ȣ���ô����Ҫ����һ���߳���ռ
     */
    if (pThread->Priority < uKernelVariable.CurrentThread->Priority)
    {
        *pHiRP = eTrue;
    }
}


/*************************************************************************************************
 *  ���ܣ�ѡ�������������е�ȫ���߳�                                                           *
 *  ������(1) pQueue  �̶߳��нṹ��ַ                                                           *
 *        (2) state   �߳���Դ���ʷ��ؽ��                                                       *
 *        (3) error   ��ϸ���ý��                                                               *
 *        (4) pData   �̷߳���IPC�õ�������                                                      *
 *        (5) pHiRP  �߳��Ƿ���Ҫ���ȵı��                                                      *
 *  ���أ�                                                                                       *
 *  ˵����ֻ���������Ϣ���й㲥ʱ�Żᴫ��pData2����                                             *
 *************************************************************************************************/
void uIpcUnblockAll(TIpcQueue* pQueue, TState state, TError error, void** pData2, TBool* pHiRP)
{
    TIpcContext* pContext;

    /* ���������е��߳����ȱ�������� */
    while (pQueue->AuxiliaryHandle != (TLinkNode*)0)
    {
        pContext = (TIpcContext*)(pQueue->AuxiliaryHandle->Owner);
        uIpcUnblockThread(pContext, state, error, pHiRP);

        if ((pData2 != (void**)0) && (pContext->Data.Addr2 != (void**)0))
        {
            *(pContext->Data.Addr2) = *pData2;
        }
    }

    /* ���������е��߳���󱻽������ */
    while (pQueue->PrimaryHandle != (TLinkNode*)0)
    {
        pContext = (TIpcContext*)(pQueue->PrimaryHandle->Owner);
        uIpcUnblockThread(pContext, state, error, pHiRP);

        if ((pData2 != (void**)0) && (pContext->Data.Addr2 != (void**)0))
        {
            *(pContext->Data.Addr2) = *pData2;
        }
    }
}


/*************************************************************************************************
 *  ���ܣ��ı䴦��IPC���������е��̵߳����ȼ�                                                    *
 *  ������(1) pContext ���������ַ                                                              *
 *        (2) priority ��Դ�ȴ�ʱ��                                                              *
 *  ���أ���                                                                                     *
 *  ˵��������߳������������в������ȼ����ԣ����̴߳������������������Ƴ���Ȼ���޸��������ȼ�,*
 *        ����ٷŻ�ԭ���С�����������ȳ������򲻱ش���                                       *
 *************************************************************************************************/
void uIpcSetPriority(TIpcContext* pContext, TPriority priority)
{
    TProperty property;
    TIpcQueue* pQueue;

    pQueue = pContext->Queue;

    /* ����ʵ����������°����߳���IPC�����������λ�� */
    property = *(pContext->Queue->Property);
    if (pContext->Option & IPC_OPT_USE_AUXIQ)
    {
        if (property & IPC_PROP_PREEMP_AUXIQ)
        {
            uObjQueueRemoveNode(&(pQueue->AuxiliaryHandle), &(pContext->LinkNode));
            uObjQueueAddPriorityNode(&(pQueue->AuxiliaryHandle), &(pContext->LinkNode));
        }
    }
    else
    {
        if (property & IPC_PROP_PREEMP_PRIMIQ)
        {
            uObjQueueRemoveNode(&(pQueue->PrimaryHandle), &(pContext->LinkNode));
            uObjQueueAddPriorityNode(&(pQueue->PrimaryHandle), &(pContext->LinkNode));
        }
    }
}


/*************************************************************************************************
 *  ���ܣ��趨�����̵߳�IPC�������Ϣ                                                            *
 *  ������(1) pContext���������ַ                                                               *
 *        (2) pIpc    ���ڲ�����IPC����ĵ�ַ                                                    *
 *        (3) data    ָ������Ŀ�����ָ���ָ��                                                 *
 *        (4) len     ���ݵĳ���                                                                 *
 *        (5) option  ����IPC����ʱ�ĸ��ֲ���                                                    *
 *        (6) state   IPC������ʽ��                                                            *
 *        (7) pError  ��ϸ���ý��                                                               *
 *  ���أ���                                                                                     *
 *  ˵����dataָ���ָ�룬������Ҫͨ��IPC���������ݵ��������߳̿ռ��ָ��                        *
 *************************************************************************************************/
void uIpcInitContext(TIpcContext* pContext, void* pIpc, TBase32 data, TBase32 len,
                     TOption option, TState* pState, TError* pError)
{
    TThread* pThread;

    pThread = uKernelVariable.CurrentThread;
    pThread->IpcContext = pContext;

    pContext->Owner      = (void*)pThread;
    pContext->Object     = pIpc;
    pContext->Queue      = (TIpcQueue*)0;
    pContext->Data.Value = data;
    pContext->Length     = len;
    pContext->Option     = option;
    pContext->State      = pState;
    pContext->Error      = pError;

    pContext->LinkNode.Next   = (TLinkNode*)0;
    pContext->LinkNode.Prev   = (TLinkNode*)0;
    pContext->LinkNode.Handle = (TLinkNode**)0;
    pContext->LinkNode.Data   = (TBase32*)(&(pThread->Priority));
    pContext->LinkNode.Owner  = (void*)pContext;

    *pState              = eError;
    *pError              = IPC_ERR_FAULT;
}


/*************************************************************************************************
 *  ���ܣ���������̵߳�IPC�������Ϣ                                                            *
 *  ������(1) pContext ���������ַ                                                              *
 *  ���أ���                                                                                     *
 *  ˵����                                                                                       *
 *************************************************************************************************/
void uIpcCleanContext(TIpcContext* pContext)
{
    TThread* pThread;

    pThread = (TThread*)(pContext->Owner);
    pThread->IpcContext = (TIpcContext*)0;

    pContext->Object     = (void*)0;
    pContext->Queue      = (TIpcQueue*)0;
    pContext->Data.Value = 0U;
    pContext->Length     = 0U;
    pContext->Option     = IPC_OPT_DEFAULT;
    pContext->State      = (TState*)0;
    pContext->Error      = (TError*)0;
}

#endif

