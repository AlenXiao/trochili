/*************************************************************************************************
 *                                     Trochili RTOS Kernel                                      *
 *                                  Copyright(C) 2016 LIUXUMING                                  *
 *                                       www.trochili.com                                        *
 *************************************************************************************************/
#ifndef _TCL_OBJECT_H
#define _TCL_OBJECT_H
#include "tcl.types.h"
#include "tcl.config.h"

/* ��������ö�ٶ��� */
enum ObjectTypeDef
{
    eThread = 0,
    eTimer,
    eSemaphore,
    eMutex,
    eMailbox,
    eMessage,
    eFlag
};
typedef enum ObjectTypeDef TObjectType;

/* �ں˶���ڵ�ṹ���� */
struct LinkNodeDef
{
    struct LinkNodeDef*  Prev;
    struct LinkNodeDef*  Next;
    struct LinkNodeDef** Handle;
    void*    Owner;
    TBase32* Data;
};
typedef struct LinkNodeDef TLinkNode;

/* �ں˶�������������ʱ�Ľڵ�λ�� */
typedef enum LinkPosDef
{
    eLinkPosHead,
    eLinkPosTail
} TLinkPos;

/* �ں˶���ṹ���� */
struct ObjectDef
{
    TBase32       ID;                                    /* �ں˶�����     */
    TObjectType   Type;                                  /* �ں˶�������     */
    TChar         Name[TCL_OBJ_NAME_LEN];                /* �ں˶�������     */
	void*         Owner;                                 /* �ں˶�������     */
    TLinkNode     LinkNode;                              /* �ں˶������ӽڵ� */
};
typedef struct ObjectDef TObject;

extern void uObjQueueAddFifoNode(TLinkNode** pHandle2, TLinkNode* pNode, TLinkPos pos);
extern void uObjQueueAddPriorityNode(TLinkNode** pHandle2, TLinkNode* pNode);
extern void uObjQueueRemoveNode(TLinkNode** pHandle2, TLinkNode* pNode);
extern void uObjListAddNode(TLinkNode** pHandle2, TLinkNode* pNode, TLinkPos pos);
extern void uObjListRemoveNode(TLinkNode** pHandle2, TLinkNode* pNode);
extern void uObjListAddPriorityNode(TLinkNode** pHandle2, TLinkNode* pNode);
extern void uObjListAddDiffNode(TLinkNode** pHandle2, TLinkNode* pNode);
extern void uObjListRemoveDiffNode(TLinkNode** pHandle2, TLinkNode* pNode);

#endif /* _TCL_OBJECT_H */

