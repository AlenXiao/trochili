/*************************************************************************************************
 *                                     Trochili RTOS Kernel                                      *
 *                                  Copyright(C) 2016 LIUXUMING                                  *
 *                                       www.trochili.com                                        *
 *************************************************************************************************/
#ifndef _TCL_TIMER_H_
#define _TCL_TIMER_H_

#include "tcl.types.h"
#include "tcl.config.h"
#include "tcl.object.h"

#if (TCLC_TIMER_ENABLE)

#define TIMER_ERR_NONE               (0x0)
#define TIMER_ERR_FAULT              (0x1<<0)           /* һ���Դ���                          */
#define TIMER_ERR_UNREADY            (0x1<<1)           /* ��ʱ������ṹδ��ʼ��              */

/* ��ʱ��״̬ö�ٶ��� */
enum TimerStatusDef
{
    eTimerDormant = 0,                                   /* ��ʱ���ĳ�ʼ״̬                   */
    eTimerActive,                                        /* ��ʱ������״̬                     */
};
typedef enum TimerStatusDef TTimerStatus;

/* ��ʱ�����Ա�Ƕ��� */
#define TIMER_PROP_DEAULT         (0x0)                 /* ��ʱ�������Ա��                     */
#define TIMER_PROP_READY          (0x1<<0)              /* ��ʱ���������                       */
#define TIMER_PROP_EXPIRED        (0x1<<1)              /* ��ʱ���������                       */
#define TIMER_PROP_PERIODIC       (0x1<<2)              /* �û����ڻص���ʱ��                   */
#define TIMER_PROP_ACCURATE       (0x1<<3)              /* �û���׼��ʱ��                       */

#define TIMER_USER_PROPERTY    (TIMER_PROP_PERIODIC| TIMER_PROP_ACCURATE)

/* ��ʱ�����д����붨�� */
#define TIMER_DIAG_NORMAL         (TBitMask)(0x0)       /* ��ʱ������                            */
#define TIMER_DIAG_OVERFLOW       (TBitMask)(0x1<<0)    /* ��ʱ���������                        */

/* �û���ʱ���ص��������Ͷ��� */
typedef void(*TTimerRoutine)(TArgument data, TTimeTick ticks);

/* ��ʱ���ṹ���� */
struct TimerDef
{
    TProperty     Property;                              /* ��ʱ������                           */
    TTimerStatus  Status;                                /* ��ʱ��״̬                           */
    TTimeTick     MatchTicks;                            /* ��ʱ����ʱʱ��                       */
    TTimeTick     PeriodTicks;                           /* ��ʱ����ʱ����                       */
    TTimerRoutine Routine;                               /* �û���ʱ���ص�����                   */
    TArgument     Argument;                              /* ��ʱ����ʱ�ص�����                   */
    TPriority     Priority;                              /* ��ʱ���ص����ȼ�                     */
    TTimeTick     ExpiredTicks;                          /* ��ʱ������ʱ��                       */
    TLinkNode     ExpiredNode;                           /* ��ʱ���������е�����ָ��             */
    TBitMask      Diagnosis;                             /* ��ʱ�����д�����                     */
    TLinkNode     LinkNode;                              /* ��ʱ�����ڶ��е�����ָ��             */
    TObject       Object;
};
typedef struct TimerDef TTimer;


/* ��ʱ�����нṹ���� */
struct TimerListDef
{
    TLinkNode*    DormantHandle;
    TLinkNode*    ActiveHandle[TCLC_TIMER_WHEEL_SIZE];
    TLinkNode*    ExpiredHandle;
};
typedef struct TimerListDef TTimerList;

#define TCLM_NODE2TIMER(NODE) ((TTimer*)((TByte*)(NODE)-OFF_SET_OF(TTimer, LinkNode)))

extern void uTimerModuleInit(void);
extern void uTimerTickUpdate(void);

extern TState xTimerCreate(TTimer* pTimer, TChar* pName, TProperty property, TTimeTick ticks,
                           TTimerRoutine pRoutine, TArgument data, TPriority priority, TError* pError);
extern TState xTimerDelete(TTimer * pTimer, TError* pError);
extern TState xTimerStart(TTimer* pTimer, TTimeTick lagticks, TError* pError);
extern TState xTimerStop(TTimer* pTimer, TError* pError);
extern TState xTimerConfig(TTimer* pTimer, TTimeTick ticks, TPriority priority, TError* pError);
extern void uTimerCreateDaemon(void);
#endif


#endif /*_TCL_TIMER_H_*/

