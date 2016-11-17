/*************************************************************************************************
 *                                     Trochili RTOS Kernel                                      *
 *                                  Copyright(C) 2016 LIUXUMING                                  *
 *                                       www.trochili.com                                        *
 *************************************************************************************************/
#include <string.h>

#include "tcl.types.h"
#include "tcl.config.h"
#include "tcl.object.h"
#include "tcl.cpu.h"
#include "tcl.kernel.h"
#include "tcl.thread.h"
#include "tcl.debug.h"
#include "tcl.irq.h"

#if (TCLC_IRQ_ENABLE)

/* �ж��������������� */
#define IRQ_VECTOR_PROP_NONE   (TProperty)(0x0)
#define IRQ_VECTOR_PROP_READY  (TProperty)(0x1<<0)
#define IRQ_VECTOR_PROP_LOCKED (TProperty)(0x1<<1)

#if (TCLC_IRQ_DAEMON_ENABLE)
/* IRQ����������Ͷ��� */
typedef struct IrqListDef
{
    TLinkNode* Handle;
} TIrqList;
#endif

/* �ں��ж������� */
static TIrqVector IrqVectorTable[TCLC_IRQ_VECTOR_NUM];

/* MCU�жϺŵ��ں��ж�������ת���� */
static TAddr32 IrqMapTable[TCLC_CPU_IRQ_NUM];


/*************************************************************************************************
 *  ���ܣ��жϴ����������                                                                     *
 *  ������(1) irqn �жϺ�                                                                        *
 *  ���أ���                                                                                     *
 *  ˵����                                                                                       *
 *************************************************************************************************/
void xIrqEnterISR(TIndex irqn)
{
    TReg32      imask;

    TIrqVector* pVector;
    TISR        pISR;
    TArgument   data;
    TBitMask    retv = IRQ_ISR_DONE;

    KNL_ASSERT((irqn < TCLC_CPU_IRQ_NUM), "");
    CpuEnterCritical(&imask);

    /* ��ú��жϺŶ�Ӧ���ж����� */
    pVector = (TIrqVector*)(IrqMapTable[irqn]);
    if ((pVector != (TIrqVector*)0) &&
            (pVector->Property & IRQ_VECTOR_PROP_READY))
    {
        /* �ڴ����ж϶�Ӧ������ʱ����ֹ���������޸����� */
        pVector->Property |= IRQ_VECTOR_PROP_LOCKED;

        /* ���жϻ����µ��õͼ��жϴ����� */
        if (pVector->ISR != (TISR)0)
        {
            pISR = pVector->ISR;
            data = pVector->Argument;
            CpuLeaveCritical(imask);
            retv = pISR(data);
            CpuEnterCritical(&imask);
        }

        /* �����Ҫ������жϴ����߳�DAEMON(�û��жϴ����̻߳����ں��ж��ػ��߳�),
           ע���ʱDAEMON���ܴ���eThreadReady״̬ */
#if (TCLC_IRQ_DAEMON_ENABLE)
        if (retv & IRQ_CALL_DAEMON)
        {
            uThreadResumeFromISR(uKernelVariable.IrqDaemon);

        }
#endif
        pVector->Property &= (~IRQ_VECTOR_PROP_LOCKED);
    }

    CpuLeaveCritical(imask);
}


/*************************************************************************************************
 *  ���ܣ������ж���������                                                                       *
 *  ������(1) irqn     �жϺ�                                                                    *
 *        (2) pISR     ISR������                                                               *
 *        (3) data     Ӧ���ṩ�Ļص�����                                                        *
 *        (4) pError   ��ϸ���ý��                                                              *
 *  ����: (1) eFailure ����ʧ��                                                                  *
 *        (2) eSuccess �����ɹ�                                                                  *
 *  ˵����                                                                                       *
 *************************************************************************************************/
TState xIrqSetVector(TIndex irqn, TISR pISR, TArgument data, TError* pError)
{
    TState state = eFailure;
    TError error = IRQ_ERR_FAULT;
    TReg32 imask;
    TIndex index;
    TIrqVector* pVector;

    CpuEnterCritical(&imask);

    /* ���ָ�����жϺ��Ѿ�ע����ж���������ôֱ�Ӹ��� */
    if (IrqMapTable[irqn] != (TAddr32)0)
    {
        pVector = (TIrqVector*)(IrqMapTable[irqn]);

        /* ����֮ǰȷ��û�б����� */
        if ((pVector->Property & IRQ_VECTOR_PROP_LOCKED))
        {
            error = IRQ_ERR_LOCKED;
        }
        else
        {
            error = IRQ_ERR_NONE;
            state = eSuccess;
        }
    }
    else
    {
        /* Ϊ���жϺ������ж������� */
        for (index = 0; index < TCLC_IRQ_VECTOR_NUM; index++)
        {
            pVector = (TIrqVector*)IrqVectorTable + index;
            if (!(pVector->Property & IRQ_VECTOR_PROP_READY))
            {
                /* �����жϺźͶ�Ӧ���ж���������ϵ */
                IrqMapTable[irqn] = (TAddr32)pVector;
                pVector->IRQn       = irqn;
                pVector->Property   = IRQ_VECTOR_PROP_READY;

                error = IRQ_ERR_NONE;
                state = eSuccess;
                break;
            }
        }
    }

    /* �����ж�������Ӧ���жϷ�������жϷ����߳�(���û����Ĭ��ΪIrqDaemon�߳�) */
    if (state == eSuccess)
    {
        pVector->ISR      = pISR;
        pVector->Argument = data;
    }

    CpuLeaveCritical(imask);

    *pError = error;
    return state;
}


/*************************************************************************************************
 *  ���ܣ�����ж���������                                                                       *
 *  ������(1) irqn �жϱ��                                                                      *
 *  ���أ���                                                                                     *
 *  ˵����                                                                                       *
 *************************************************************************************************/
TState xIrqCleanVector(TIndex irqn, TError* pError)
{
    TState state = eFailure;
    TError error = IRQ_ERR_FAULT;
    TReg32 imask;
    TIrqVector* pVector;

    CpuEnterCritical(&imask);

    /* �ҵ����ж�����������������Ϣ */
    if (IrqMapTable[irqn] != (TAddr32)0)
    {
        pVector = (TIrqVector*)(IrqMapTable[irqn]);
        if ((pVector->Property & IRQ_VECTOR_PROP_READY) &&
                (pVector->IRQn == irqn))
        {
            if (!(pVector->Property & IRQ_VECTOR_PROP_LOCKED))
            {
                IrqMapTable[irqn] = (TAddr32)0;
                memset(pVector, 0, sizeof(TIrqVector));
                error = IRQ_ERR_NONE;
                state = eSuccess;
            }
            else
            {
                error = IRQ_ERR_LOCKED;
            }
        }
    }

    CpuLeaveCritical(imask);

    *pError = error;
    return state;
}

#if (TCLC_IRQ_DAEMON_ENABLE)

/* IRQ�ػ��̶߳����ջ���� */
static TThread IrqDaemonThread;
static TBase32 IrqDaemonStack[TCLC_IRQ_DAEMON_STACK_BYTES >> 2];

/* IRQ�ػ��̲߳������κ��̹߳���API���� */
#define IRQ_DAEMON_ACAPI (THREAD_ACAPI_NONE)

/* IRQ������� */
static TIrqList IrqReqList;

/*************************************************************************************************
 *  ���ܣ��ύ�ж�����                                                                           *
 *  ������(1) pIRQ      �ж�����ṹ��ַ                                                         *
 *        (2) priority  �ж��������ȼ�                                                           *
 *        (3) pEntry    �жϴ���ص�����                                                         *
 *        (4) data      �жϴ���ص�����                                                         *
 *  ����: (1) eFailure  ����ʧ��                                                                 *
 *        (2) eSuccess  �����ɹ�                                                                 *
 *  ˵����                                                                                       *
 *************************************************************************************************/
TState xIrqPostRequest(TIrq* pIRQ, TPriority priority, TIrqEntry pEntry, TArgument data,
                       TError* pError)
{
    TState state = eFailure;
    TError error = IRQ_ERR_FAULT;
    TReg32 imask;

    CpuEnterCritical(&imask);

    if (!(pIRQ->Property & IRQ_PROP_READY))
    {
        pIRQ->Property       = IRQ_PROP_READY;
        pIRQ->Entry          = pEntry;
        pIRQ->Argument       = data;
        pIRQ->Priority       = priority;
        pIRQ->LinkNode.Next   = (TLinkNode*)0;
        pIRQ->LinkNode.Prev   = (TLinkNode*)0;
        pIRQ->LinkNode.Handle = (TLinkNode**)0;
        pIRQ->LinkNode.Data   = (TBase32*)(&(pIRQ->Priority));
        pIRQ->LinkNode.Owner  = (void*)pIRQ;
        uObjListAddPriorityNode(&(IrqReqList.Handle), &(pIRQ->LinkNode));

        error = IRQ_ERR_NONE;
        state = eSuccess;
    }

    CpuLeaveCritical(imask);

    *pError = error;
    return state;
}


/*************************************************************************************************
 *  ���ܣ������ж�����                                                                           *
 *  ������(1) pIRQ      �ж�����ṹ��ַ                                                         *
 *  ����: (1) eFailure  ����ʧ��                                                                 *
 *        (2) eSuccess  �����ɹ�                                                                 *
 *  ˵����                                                                                       *
 *************************************************************************************************/
TState xIrqCancelRequest(TIrq* pIRQ, TError* pError)
{
    TState state = eFailure;
    TError error = IRQ_ERR_UNREADY;
    TReg32 imask;

    CpuEnterCritical(&imask);
    if (pIRQ->Property & IRQ_PROP_READY)
    {
        uObjListRemoveNode( pIRQ->LinkNode.Handle, &(pIRQ->LinkNode));
        memset(pIRQ, 0, sizeof(TIrq));

        error = IRQ_ERR_NONE;
        state = eSuccess;
    }
    CpuLeaveCritical(imask);

    *pError = error;
    return state;
}


/*************************************************************************************************
 *  ���ܣ��ں��е�IRQ�ػ��̺߳���                                                                *
 *  ������(1) argument IRQ�ػ��̵߳��û�����                                                     *
 *  ���أ���                                                                                     *
 *  ˵����                                                                                       *
 *************************************************************************************************/
static void xIrqDaemonEntry(TArgument argument)
{
    TReg32    imask;
    TIrq*     pIRQ;
    TIrqEntry pEntry;
    TArgument data;

    /*
     * �Ӷ�����������IRQ��������̻߳����´����IRQ�ص�����
     * ���IRQ�������Ϊ����IRQ�ػ��̹߳���
     */
    while(eTrue)
    {
        CpuEnterCritical(&imask);
        if (IrqReqList.Handle == (TLinkNode*)0)
        {
            uThreadSuspendSelf();
            CpuLeaveCritical(imask);
        }
        else
        {
            pIRQ   = (TIrq*)(IrqReqList.Handle->Owner);
            pEntry = pIRQ->Entry;
            data   = pIRQ->Argument;
            uObjListRemoveNode(pIRQ->LinkNode.Handle, &(pIRQ->LinkNode));
            memset(pIRQ, 0, sizeof(TIrq));
            CpuLeaveCritical(imask);

            pEntry(data);
        }
    }
}


/*************************************************************************************************
 *  ���ܣ���ʼ��IRQ�ػ��߳�                                                                      *
 *  ��������                                                                                     *
 *  ���أ���                                                                                     *
 *  ˵����                                                                                       *
 *************************************************************************************************/
void uIrqCreateDaemon(void)
{
    /* ����ں��Ƿ��ڳ�ʼ״̬ */
    if(uKernelVariable.State != eOriginState)
    {
        uDebugPanic("", __FILE__, __FUNCTION__, __LINE__);
    }

    /* ��ʼ���ں��жϷ����߳� */
    uThreadCreate(&IrqDaemonThread,
                  "irq daemon",
                  eThreadSuspended,
                  THREAD_PROP_PRIORITY_FIXED | \
                  THREAD_PROP_CLEAN_STACK | \
                  THREAD_PROP_KERNEL_DAEMON,
                  IRQ_DAEMON_ACAPI,
                  xIrqDaemonEntry,
                  (TArgument)0,
                  (void*)IrqDaemonStack,
                  (TBase32)TCLC_IRQ_DAEMON_STACK_BYTES,
                  (TPriority)TCLC_IRQ_DAEMON_PRIORITY,
                  (TTimeTick)TCLC_IRQ_DAEMON_SLICE);

    /* ��ʼ����ص��ں˱��� */
    uKernelVariable.IrqDaemon = &IrqDaemonThread;
}

#endif


/*************************************************************************************************
 *  ���ܣ���ʱ��ģ���ʼ��                                                                       *
 *  ��������                                                                                     *
 *  ���أ���                                                                                     *
 *  ˵����                                                                                       *
 *************************************************************************************************/
void uIrqModuleInit(void)
{
    /* ����ں��Ƿ��ڳ�ʼ״̬ */
    if(uKernelVariable.State != eOriginState)
    {
        uDebugPanic("", __FILE__, __FUNCTION__, __LINE__);
    }

    memset(IrqMapTable, 0, sizeof(IrqMapTable));
    memset(IrqVectorTable, 0, sizeof(IrqVectorTable));

#if (TCLC_IRQ_DAEMON_ENABLE)
    memset(&IrqReqList, 0, sizeof(IrqReqList));
#endif

    /* ��ʼ����ص��ں˱��� */
    uKernelVariable.IrqMapTable    = IrqMapTable;
    uKernelVariable.IrqVectorTable = IrqVectorTable;
}

#endif

