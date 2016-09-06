/*************************************************************************************************
 *                                     Trochili RTOS Kernel                                      *
 *                                  Copyright(C) 2016 LIUXUMING                                  *
 *                                       www.trochili.com                                        *
 *************************************************************************************************/
#include <string.h>

#include "tcl.types.h"
#include "tcl.config.h"
#include "tcl.cpu.h"
#include "tcl.debug.h"
#include "tcl.thread.h"
#include "tcl.kernel.h"
#include "tcl.ipc.h"
#include "tcl.message.h"

#if ((TCLC_IPC_ENABLE)&&(TCLC_IPC_MQUE_ENABLE))


/*************************************************************************************************
 *  ���ܣ�����Ϣ���浽��Ϣ����                                                                   *
 *  ������(1) pMsgQue ��Ϣ���нṹָ��                                                           *
 *        (2) pMsg2   ������Ϣ�ṹ��ַ��ָ�����                                                 *
 *        (3) type    ��Ϣ�����ͣ�������Ϣ����ͨ��Ϣ��                                           *
 *  ���أ���                                                                                     *
 *  ˵����(1) ������Ϣ���Ͳ�ͬ������Ϣ���浽��Ϣ���ж�ͷ���߶�β                                 *
 *        (2) ����������ܴ�����Ϣ����״̬                                                       *
 *************************************************************************************************/
static void SaveMessage(TMsgQueue* pMsgQue, void** pMsg2, TMsgType type)
{
    /* ��ͨ��Ϣֱ�ӷ��͵���Ϣ����ͷ */
    if (type == eNormalMessage)
    {
        *(pMsgQue->MsgPool + pMsgQue->Head) = *pMsg2;
        pMsgQue->Head++;
        if (pMsgQue->Head == pMsgQue->Capacity)
        {
            pMsgQue->Head = 0U;
        }
    }
    /* ������Ϣ���͵���Ϣ����β */
    else
    {
        if (pMsgQue->Tail == 0U)
        {
            pMsgQue->Tail = pMsgQue->Capacity - 1U;
        }
        else
        {
            pMsgQue->Tail--;
        }
        *(pMsgQue->MsgPool + pMsgQue->Tail) = *pMsg2;
    }

    /* ������Ϣ������Ϣ��Ŀ */
    pMsgQue->MsgEntries++;
}


/*************************************************************************************************
 *  ���ܣ�����Ϣ����Ϣ�����ж���                                                                 *
 *  ������(1) pMsgQue ��Ϣ���нṹָ��                                                           *
 *        (2) pMsg2   ������Ϣ�ṹ��ַ��ָ�����                                                 *
 *  ���أ���                                                                                     *
 *  ˵����(1) ����Ϣ�����ж�ȡ��Ϣ��ʱ��ֻ�ܴӶ�β��ȡ                                         *
 *        (2) ����������ܴ�����Ϣ����״̬                                                       *
 *************************************************************************************************/
static void ConsumeMessage(TMsgQueue* pMsgQue, void** pMsg2)
{
    /* ����Ϣ�����ж�ȡһ����Ϣ����ǰ�߳� */
    *pMsg2 = *(pMsgQue->MsgPool + pMsgQue->Tail);

    /* ������Ϣ������Ϣ��Ŀ */
    pMsgQue->MsgEntries--;

    /* ������Ϣ���е���Ϣ��д�α� */
    pMsgQue->Tail++;
    if (pMsgQue->Tail == pMsgQue->Capacity)
    {
        pMsgQue->Tail = 0U;
    }
}


/*************************************************************************************************
 *  ���ܣ��߳�/ISR���Դ���Ϣ�����ж�ȡ��Ϣ                                                       *
 *  ������(1) pMsgQue ��Ϣ���еĵ�ַ                                                             *
 *        (2) pMsg2   ������Ϣ�ṹ��ַ��ָ�����                                                 *
 *        (3) pHiRP   �Ƿ���Ҫ�̵߳��ȱ��                                                       *
 *        (4) pError  ��ϸ���ý��                                                               *
 *  ����: (1) eFailure   ����ʧ��                                                                *
 *        (2) eSuccess   �����ɹ�                                                                *
 *  ˵����                                                                                       *
 *************************************************************************************************/
/* ������Ϣ���ж����ݵ����
   (1) ������Ϣ����Ϊ��������������������1����Ϣ���У���ȡ�����ᵼ����Ϣ����״̬����ֱ�ӵ��գ�
       ������Ϣ���н��� eMQPartial ״̬
   (2) ������Ϣ������ͨ������������Ϣ������ֻ��1����Ϣ�����ж�ȡ�������ܵ�����Ϣ���н����״̬
       ������Ϣ���б��� eMQPartial ״̬
   (3) ��������Ϣ������������Ϣ������ͨ״̬������״ֻ̬����ͨ�Ϳա�
 */
static TState ReceiveMessage(TMsgQueue* pMsgQue, void** pMsg2, TBool* pHiRP, TError* pError)
{
    TState state = eSuccess;
    TError error = IPC_ERR_NONE;
    TMsgType type;
    TIpcContext* pContext = (TIpcContext*)0;

    /* �����Ϣ����״̬ */
    if (pMsgQue->Status == eMQEmpty)
    {
        error = IPC_ERR_NORMAL;
        state = eFailure;
    }
    else if (pMsgQue->Status == eMQFull)
    {
        /* ����Ϣ�����ж�ȡһ����Ϣ����ǰ�߳� */
        ConsumeMessage(pMsgQue, pMsg2);

        /*
        	   * ����Ϣ�������������߳�������д�����е�����£�
         * ��Ҫ�����ʵ��߳̽Ӵ��������ҽ����߳�Я������Ϣд����У�
         * ���Ա�����Ϣ����״̬����
        	   */
        if (pMsgQue->Property & IPC_PROP_AUXIQ_AVAIL)
        {
            pContext = (TIpcContext*)(pMsgQue->Queue.AuxiliaryHandle->Owner);
        }
        else
        {
            if (pMsgQue->Property & IPC_PROP_PRIMQ_AVAIL)
            {
                pContext = (TIpcContext*)(pMsgQue->Queue.PrimaryHandle->Owner);
            }
        }

        if (pContext !=  (TIpcContext*)0)
        {
            uIpcUnblockThread(pContext, eSuccess, IPC_ERR_NONE, pHiRP);

            /* �����߳������ķֶ����ж���Ϣ����,���ҽ����̷߳��͵���Ϣд����Ϣ���� */
            type = ((pContext->Option) & IPC_OPT_UARGENT) ? eUrgentMessage : eNormalMessage;
            SaveMessage(pMsgQue, pContext->Data.Addr2, type);
        }
        else
        {
            pMsgQue->Status = (pMsgQue->Tail == pMsgQue->Head) ? eMQEmpty : eMQPartial;
        }
    }
    else
        /* if (mq->Status == eMQPartial) */
    {
        /* ����Ϣ�����ж�ȡһ����Ϣ����ǰ�߳� */
        ConsumeMessage(pMsgQue, pMsg2);
        pMsgQue->Status = (pMsgQue->Tail == pMsgQue->Head) ? eMQEmpty : eMQPartial;
    }

    *pError = error;
    return state;
}


/*************************************************************************************************
 *  ���ܣ��߳�/ISR��������Ϣ�����з�����Ϣ                                                       *
 *  ������(1) pMsgQue ��Ϣ���еĵ�ַ                                                             *
 *        (2) pMsg2   ������Ϣ�ṹ��ַ��ָ�����                                                 *
 *        (3) type    ��Ϣ����                                                                   *
 *        (4) pHiRP   �Ƿ���Ҫ�̵߳��ȱ��                                                       *
 *        (5) pError  ��ϸ���ý��                                                               *
 *  ����: (1) eFailure   ����ʧ��                                                                *
 *        (2) eSuccess   �����ɹ�                                                                *
 *  ˵����                                                                                       *
 *************************************************************************************************/
static TState SendMessage(TMsgQueue* pMsgQue, void** pMsg2, TMsgType type, TBool* pHiRP,
                          TError* pError)
{
    TState state = eSuccess;
    TError error = IPC_ERR_NONE;
    TIpcContext* pContext = (TIpcContext*)0;

    /* �����Ϣ����״̬�������Ϣ�������򷵻�ʧ�� */
    if (pMsgQue->Status == eMQFull)
    {
        error = IPC_ERR_NORMAL;
        state = eFailure;
    }
    else if (pMsgQue->Status == eMQEmpty)
    {
        /*
         * ����Ϣ����Ϊ�յ�����£�������������̵߳ȴ�����˵���Ƕ��������У�
         * ������һ���߳̽��������������Ϣ���͸����̣߳�ͬʱ������Ϣ����״̬����
         */
        if (pMsgQue->Property & IPC_PROP_PRIMQ_AVAIL)
        {
            pContext = (TIpcContext*)(pMsgQue->Queue.PrimaryHandle->Owner);
        }
        if (pContext != (TIpcContext*)0)
        {
            uIpcUnblockThread(pContext, eSuccess, IPC_ERR_NONE, pHiRP);
            *(pContext->Data.Addr2) = *pMsg2;
        }
        else
        {
            /* ���̷߳��͵���Ϣд����Ϣ���� */
            SaveMessage(pMsgQue, pMsg2, type);
            pMsgQue->Status = (pMsgQue->Tail == pMsgQue->Head) ? eMQFull : eMQPartial;
        }
    }
    else
        /* if (mq->Status == eMQPartial) */
    {
        /* ���̷߳��͵���Ϣд����Ϣ���� */
        SaveMessage(pMsgQue, pMsg2, type);

        /* ��Ϣ���пյ�������������Ϣ���е�������1����ô����״̬�ӿ�ֱ�ӵ� eMQFull��
           ������Ϣ���н��� eMQPartial ״̬ */
        /* ��Ϣ������ͨ���������Ϣ����д�������ܵ�����Ϣ���н��� eMQFull
           ״̬���߱��� eMQPartial ״̬ */
        pMsgQue->Status = (pMsgQue->Tail == pMsgQue->Head) ? eMQFull : eMQPartial;
    }

    *pError = error;
    return state;
}

/*************************************************************************************************
 *  ����: �����߳�/ISR������Ϣ�����е���Ϣ                                                       *
 *  ����: (1) pMsgQue  ��Ϣ���нṹ��ַ                                                          *
 *        (2) pMsg2    ������Ϣ�ṹ��ַ��ָ�����                                                *
 *        (3) option   ������Ϣ���е�ģʽ                                                        *
 *        (4) timeo    ʱ������ģʽ�·��������ʱ�޳���                                          *
 *        (5) pError   ��ϸ���ý��                                                              *
 *  ����: (1) eFailure   ����ʧ��                                                                *
 *        (2) eSuccess   �����ɹ�                                                                *
 *  ˵����                                                                                       *
 *************************************************************************************************/
extern TState xMQReceive(TMsgQueue* pMsgQue, TMessage* pMsg2, TOption option,
                         TTimeTick timeo, TError* pError)
{
    TState state = eFailure;
    TError error = IPC_ERR_UNREADY;
    TBool  HiRP = eFalse;
    TIpcContext* pContext = (TIpcContext*)0;
    TReg32 imask;

    CpuEnterCritical(&imask);

    if (pMsgQue->Property & IPC_PROP_READY)
    {
        /*
         * ������жϳ�����ñ�������ֻ���Է�������ʽ������Ϣ,
         * ������ʱ�������̵߳������⡣
         * ���ж���,��ǰ�߳�δ������߾������ȼ��߳�,Ҳδ�ش����ں˾����̶߳��У�
         * �����ڴ˴��õ���HiRP������κ����塣
         */
        state = ReceiveMessage(pMsgQue, (void**)pMsg2, &HiRP, &error);

            if ((uKernelVariable.State == eThreadState) &&
                    (uKernelVariable.SchedLockTimes == 0U))
            {
                /* �����ǰ�߳̽���˸������ȼ��̵߳���������е��ȡ�*/
                if (state == eSuccess)
                {
                    if (HiRP == eTrue)
                    {
                        uThreadSchedule();
                    }
                }
                else
                {
                    /*
                    * �����ǰ�̲߳��ܽ�����Ϣ�����Ҳ��õ��ǵȴ���ʽ��
                    * ��ô��ǰ�̱߳�����������Ϣ������
                    */
                    if (option & IPC_OPT_WAIT)
                    {
                        /* �����ǰ�̲߳��ܱ���������ֱ�ӷ��� */
                        if (uKernelVariable.CurrentThread->ACAPI & THREAD_ACAPI_BLOCK)
                        {
                            /* �õ���ǰ�̵߳�IPC�����Ľṹ��ַ */
                            pContext = &(uKernelVariable.CurrentThread->IpcContext);

                            /* �����̹߳�����Ϣ */
                            option |= IPC_OPT_MSGQUEUE | IPC_OPT_READ_DATA;
                            uIpcSaveContext(pContext, (void*)pMsgQue, (TBase32)pMsg2, sizeof(TBase32), option,
                                            &state, &error);

                            /* ��ǰ�߳������ڸ���Ϣ���е��������У�ʱ�޻������޵ȴ�����IPC_OPT_TIMED�������� */
                            uIpcBlockThread(pContext, &(pMsgQue->Queue), timeo);

                            /* ��ǰ�̱߳������������̵߳���ִ�� */
                            uThreadSchedule();

                            CpuLeaveCritical(imask);
                            /*
                                * ��Ϊ��ǰ�߳��Ѿ�������IPC������߳��������У����Դ�������Ҫִ�б���̡߳�
                                * ���������ٴδ����߳�ʱ���ӱ����������С�
                                */
                            CpuEnterCritical(&imask);

                            /* ����̹߳�����Ϣ */
                            uIpcCleanContext(pContext);
                        }
                        else
                        {
                            error = IPC_ERR_ACAPI;
                        }
                    }
                }
        }
    }

    CpuLeaveCritical(imask);

    *pError = error;
    return state;
}


/*************************************************************************************************
 *  ����: �����߳�/ISR����Ϣ�����з�����Ϣ                                                       *
 *  ����: (1) pMsgQue  ��Ϣ���нṹ��ַ                                                          *
 *        (2) pMsg2    ������Ϣ�ṹ��ַ��ָ�����                                                *
 *        (3) option   ������Ϣ���е�ģʽ                                                        *
 *        (4) timeo    ʱ������ģʽ�·��������ʱ�޳���                                          *
 *        (5) pError   ��ϸ���ý��                                                              *
 *  ����: (1) eFailure ����ʧ��                                                                  *
 *        (2) eSuccess �����ɹ�                                                                  *
 *  ˵����                                                                                       *
 *************************************************************************************************/
TState xMQSend(TMsgQueue* pMsgQue, TMessage* pMsg2, TOption option, TTimeTick timeo,
               TError* pError)
{
    TState state = eFailure;
    TError error = IPC_ERR_UNREADY;
    TBool HiRP = eFalse;
    TIpcContext* pContext;
    TMsgType type;
    TReg32 imask;

    CpuEnterCritical(&imask);
    if (pMsgQue->Property & IPC_PROP_READY)
    {
        /*
         * ������жϳ�����ñ�������ֻ���Է�������ʽ������Ϣ,
         * ������ʱ�������̵߳������⡣
         * ���ж���,��ǰ�߳�δ������߾������ȼ��߳�,Ҳδ�ش����ں˾����̶߳��У�
         * �����ڴ˴��õ���HiRP������κ����塣
         */
        type = (option & IPC_OPT_UARGENT) ? eUrgentMessage : eNormalMessage;
        state = SendMessage(pMsgQue, (void**)pMsg2, type, &HiRP, &error);

        if ((uKernelVariable.State == eThreadState) &&
                (uKernelVariable.SchedLockTimes == 0U))
        {
            /* �����ǰ�߳̽���˸������ȼ��̵߳���������е��ȡ�*/
            if (state == eSuccess)
            {
                if (HiRP == eTrue)
                {
                    uThreadSchedule();
                }
            }
            else
            {
                /*
                * �����ǰ�̲߳��ܷ�����Ϣ�����Ҳ��õ��ǵȴ���ʽ��
                * ��ô��ǰ�̱߳�����������Ϣ������
                */
                if (option & IPC_OPT_WAIT)
                {
                    /* �����ǰ�̲߳��ܱ���������ֱ�ӷ��� */
                    if (uKernelVariable.CurrentThread->ACAPI & THREAD_ACAPI_BLOCK)
                    {
                        if (option & IPC_OPT_UARGENT)
                        {
                            option |= IPC_OPT_USE_AUXIQ;
                        }

                        /* �õ���ǰ�̵߳�IPC�����Ľṹ��ַ */
                        pContext = &(uKernelVariable.CurrentThread->IpcContext);

                        /* �����̹߳�����Ϣ */
                        option |= IPC_OPT_MSGQUEUE | IPC_OPT_WRITE_DATA;
                        uIpcSaveContext(pContext, (void*)pMsgQue, (TBase32)pMsg2,  sizeof(TBase32), option,
                                        &state, &error);

                        /* ��ǰ�߳������ڸ���Ϣ���е��������У�ʱ�޻������޵ȴ�����IPC_OPT_TIMED�������� */
                        uIpcBlockThread(pContext, &(pMsgQue->Queue), timeo);

                        /* ��ǰ�̱߳������������̵߳���ִ�� */
                        uThreadSchedule();

                        CpuLeaveCritical(imask);
                        /*
                         * ��Ϊ��ǰ�߳��Ѿ�������IPC������߳��������У����Դ�������Ҫִ�б���̡߳�
                         * ���������ٴδ����߳�ʱ���ӱ����������С�
                         */
                        CpuEnterCritical(&imask);

                        /* ����̹߳�����Ϣ */
                        uIpcCleanContext(pContext);
                    }
                    else
                    {
                        error = IPC_ERR_ACAPI;
                    }
                }
            }
        }
    }
    CpuLeaveCritical(imask);

    *pError = error;
    return state;
}


/*************************************************************************************************
 *  ���ܣ���Ϣ���г�ʼ������                                                                     *
 *  ���룺(1) pMsgQue   ��Ϣ���нṹ��ַ                                                         *
 *        (2) pPool2    ��Ϣ�����ַ                                                             *
 *        (3) capacity  ��Ϣ��������������Ϣ�����С                                             *
 *        (4) policy    ��Ϣ�����̵߳��Ȳ���                                                     *
 *        (5) pError    ��ϸ���ý��                                                             *
 *  ���أ�(1) eSuccess  �����ɹ�                                                                 *
 *        (2) eFailure  ����ʧ��                                                                 *
 *  ˵����                                                                                       *
 *************************************************************************************************/
TState xMQCreate(TMsgQueue* pMsgQue, void** pPool2, TBase32 capacity, TProperty property,
                 TError* pError)
{
    TState state = eFailure;
    TError error = IPC_ERR_FAULT;
    TReg32 imask;

    CpuEnterCritical(&imask);

    if (!(pMsgQue->Property & IPC_PROP_READY))
    {
        property |= IPC_PROP_READY;
        pMsgQue->Property = property;
        pMsgQue->Capacity = capacity;
        pMsgQue->MsgPool = pPool2;
        pMsgQue->MsgEntries = 0U;
        pMsgQue->Head = 0U;
        pMsgQue->Tail = 0U;
        pMsgQue->Status = eMQEmpty;

        pMsgQue->Queue.PrimaryHandle   = (TObjNode*)0;
        pMsgQue->Queue.AuxiliaryHandle = (TObjNode*)0;
        pMsgQue->Queue.Property        = &(pMsgQue->Property);

        error = IPC_ERR_NONE;
        state = eSuccess;
    }

    CpuLeaveCritical(imask);

    *pError = error;
    return state;
}


/*************************************************************************************************
 *  ���ܣ���Ϣ�������ú���                                                                       *
 *  ���룺(1) pMsgQue   ��Ϣ���нṹ��ַ                                                         *
 *        (2) pError    ��ϸ���ý��                                                             *
 *  ���أ�(1) eSuccess  �����ɹ�                                                                 *
 *        (2) eFailure  ����ʧ��                                                                 *
 *  ˵����                                                                                       *
 *************************************************************************************************/
TState xMQDelete(TMsgQueue* pMsgQue, TError* pError)
{
    TState state = eFailure;
    TError error = IPC_ERR_UNREADY;
    TBool HiRP = eFalse;
    TReg32 imask;

    CpuEnterCritical(&imask);

    if (pMsgQue->Property & IPC_PROP_READY)
    {
        /* �����������е��̷ַ߳���Ϣ */
        uIpcUnblockAll(&(pMsgQue->Queue), eFailure, IPC_ERR_DELETE, (void**)0, &HiRP);

        /* �����Ϣ���ж����ȫ������ */
        memset(pMsgQue, 0U, sizeof(TMsgQueue));

        /*
         * ���̻߳����£������ǰ�̵߳����ȼ��Ѿ��������߳̾������е�������ȼ���
         * �����ں˴�ʱ��û�йر��̵߳��ȣ���ô����Ҫ����һ���߳���ռ
         */
        if ((uKernelVariable.State == eThreadState) &&
                (uKernelVariable.SchedLockTimes == 0U) &&
                (HiRP == eTrue))
        {
            uThreadSchedule();
        }
        error = IPC_ERR_NONE;
        state = eSuccess;
    }

    CpuLeaveCritical(imask);

    *pError = error;
    return state;
}


/*************************************************************************************************
 *  ����: �����Ϣ������������                                                                   *
 *  ����: (1) pMsgQue   ��Ϣ���нṹ��ַ                                                         *
 *        (2) pError    ��ϸ���ý��                                                             *
 *  ���أ�(1) eSuccess  �����ɹ�                                                                 *
 *        (2) eFailure  ����ʧ��                                                                 *
 *  ˵����                                                                                       *
 *************************************************************************************************/
TState xMQReset(TMsgQueue* pMsgQue, TError* pError)
{
    TState state = eFailure;
    TError error = IPC_ERR_UNREADY;
    TBool HiRP = eFalse;
    TReg32 imask;

    CpuEnterCritical(&imask);

    if (pMsgQue->Property & IPC_PROP_READY)
    {
        /* �����������ϵ����еȴ��̶߳��ͷţ������̵߳ĵȴ��������TCLE_IPC_RESET    */
        uIpcUnblockAll(&(pMsgQue->Queue), eFailure, IPC_ERR_RESET, (void**)0, &HiRP);

        /* ����������Ϣ���нṹ */
        pMsgQue->Property &= IPC_RESET_MQUE_PROP;
        pMsgQue->MsgEntries = 0U;
        pMsgQue->Head = 0U;
        pMsgQue->Tail = 0U;
        pMsgQue->Status = eMQEmpty;

        /*
         * ���̻߳����£������ǰ�̵߳����ȼ��Ѿ��������߳̾������е�������ȼ���
         * �����ں˴�ʱ��û�йر��̵߳��ȣ���ô����Ҫ����һ���߳���ռ
         */
        if ((uKernelVariable.State == eThreadState) &&
                (uKernelVariable.SchedLockTimes == 0U) &&
                (HiRP == eTrue))
        {
            uThreadSchedule();
        }
        error = IPC_ERR_NONE;
        state = eSuccess;
    }

    CpuLeaveCritical(imask);

    *pError = error;
    return state;
}


/*************************************************************************************************
 *  ���ܣ���Ϣ����������ֹ����,��ָ�����̴߳���Ϣ���е�������������ֹ����������                  *
 *  ������(1) pMsgQue  ��Ϣ���нṹ��ַ                                                          *
 *        (2) option   ����ѡ��                                                                  *
 *        (3) pThread  �̵߳�ַ                                                                  *
 *        (4) pError   ��ϸ���ý��                                                              *
 *  ���أ�(1) eSuccess �ɹ�                                                                      *
 *        (2) eFailure ʧ��                                                                      *
 *  ˵����                                                                                       *
 *************************************************************************************************/
TState xMQFlush(TMsgQueue* pMsgQue, TError* pError)
{
    TState state = eFailure;
    TError error = IPC_ERR_UNREADY;
    TReg32 imask;
    TBool HiRP = eFalse;

    CpuEnterCritical(&imask);

    if (pMsgQue->Property & IPC_PROP_READY)
    {
        /* ����Ϣ�������������ϵ����еȴ��̶߳��ͷţ������̵߳ĵȴ��������TCLE_IPC_FLUSH  */
        uIpcUnblockAll(&(pMsgQue->Queue), eFailure, IPC_ERR_FLUSH, (void**)0, &HiRP);

        /*
         * ���̻߳����£������ǰ�̵߳����ȼ��Ѿ��������߳̾������е�������ȼ���
         * �����ں˴�ʱ��û�йر��̵߳��ȣ���ô����Ҫ����һ���߳���ռ
         */
        if ((uKernelVariable.State == eThreadState) &&
                (uKernelVariable.SchedLockTimes == 0U) &&
                (HiRP == eTrue))
        {
            uThreadSchedule();
        }
        state = eSuccess;
        error = IPC_ERR_NONE;
    }

    CpuLeaveCritical(imask);

    *pError = error;
    return state;
}


/*************************************************************************************************
 *  ���ܣ���Ϣ���й㲥����,�����ж����������е��̹߳㲥��Ϣ                                      *
 *  ������(1) pMsgQue    ��Ϣ���нṹ��ַ                                                        *
 *        (2) pMsg2      ������Ϣ�ṹ��ַ��ָ�����                                              *
 *        (3) pError     ��ϸ���ý��                                                            *
 *  ���أ�(1) eSuccess   �ɹ��㲥������Ϣ                                                        *
 *        (2) eFailure   �㲥������Ϣʧ��                                                        *
 *  ˵����ֻ�ж����ж���Ϣ���е�ʱ�򣬲��ܰ���Ϣ���͸������е��߳�                               *
 *************************************************************************************************/
TState xMQBroadcast(TMsgQueue* pMsgQue, TMessage* pMsg2, TError* pError)
{
    TState state = eFailure;
    TError error = IPC_ERR_UNREADY;
    TReg32 imask;
    TBool HiRP = eFalse;

    CpuEnterCritical(&imask);

    if (pMsgQue->Property & IPC_PROP_READY)
    {
        /* �ж���Ϣ�����Ƿ���ã�ֻ����Ϣ���пղ������̵߳ȴ���ȡ��Ϣ��ʱ����ܽ��й㲥 */
        if (pMsgQue->Status == eMQEmpty)
        {
            /* ����Ϣ���еĶ����������е��̹߳㲥���� */
            uIpcUnblockAll(&(pMsgQue->Queue), eSuccess, IPC_ERR_NONE, (void**)pMsg2, &HiRP);

            /*
             * ���̻߳����£������ǰ�̵߳����ȼ��Ѿ��������߳̾������е�������ȼ���
             * �����ں˴�ʱ��û�йر��̵߳��ȣ���ô����Ҫ����һ���߳���ռ
             */
            if ((uKernelVariable.State == eThreadState) &&
                    (uKernelVariable.SchedLockTimes == 0U) &&
                    (HiRP == eTrue))
            {
                uThreadSchedule();
            }
            error = IPC_ERR_NONE;
            state = eSuccess;
        }
        else
        {
            error = IPC_ERR_NORMAL;
        }
    }

    CpuLeaveCritical(imask);

    *pError = error;
    return state;
}


#endif

