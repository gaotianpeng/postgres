/*-------------------------------------------------------------------------
 *
 * memnodes.h
 *	  POSTGRES memory context node definitions.
 *
 *
 * Portions Copyright (c) 1996-2021, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * src/include/nodes/memnodes.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef MEMNODES_H
#define MEMNODES_H

#include "nodes/nodes.h"

/*
 * MemoryContextCounters
 *		Summarization state for MemoryContextStats collection.
 *
 * The set of counters in this struct is biased towards AllocSet; if we ever
 * add any context types that are based on fundamentally different approaches,
 * we might need more or different counters here.  A possible API spec then
 * would be to print only nonzero counters, but for now we just summarize in
 * the format historically used by AllocSet.
 */
typedef struct MemoryContextCounters
{
	Size		nblocks;		/* Total number of malloc blocks */
	Size		freechunks;		/* Total number of free chunks */
	Size		totalspace;		/* Total bytes requested from malloc */
	Size		freespace;		/* The unused portion of totalspace */
} MemoryContextCounters;

/*
 * MemoryContext
 *		A logical context in which memory allocations occur.
 *
 * MemoryContext itself is an abstract type that can have multiple
 * implementations.
 * The function pointers in MemoryContextMethods define one specific
 * implementation of MemoryContext --- they are a virtual function table
 * in C++ terms.
 *
 * Node types that are actual implementations of memory contexts must
 * begin with the same fields as MemoryContextData.
 *
 * Note: for largely historical reasons, typedef MemoryContext is a pointer
 * to the context struct rather than the struct type itself.
 */

typedef void (*MemoryStatsPrintFunc) (MemoryContext context, void *passthru,
									  const char *stats_string);

typedef struct MemoryContextMethods
{
	void	   *(*alloc) (MemoryContext context, Size size);	// 分配内存函数
	/* call this free_p in case someone #define's free() */
	void		(*free_p) (MemoryContext context, void *pointer);	// 释放内存函数
	void	   *(*realloc) (MemoryContext context, void *pointer, Size size);	// 重分配内存
	void		(*reset) (MemoryContext context);	// 重置内存上下文
	void		(*delete_context) (MemoryContext context);	// 删除内存上下文
	Size		(*get_chunk_space) (MemoryContext context, void *pointer);	// 检查内存片段的大小
	bool		(*is_empty) (MemoryContext context);	// 检查内存上下文是否为空
	void		(*stats) (MemoryContext context,	
						  MemoryStatsPrintFunc printfunc, void *passthru,
						  MemoryContextCounters *totals);	// 打印内存上下文状态
#ifdef MEMORY_CONTEXT_CHECKING
	void		(*check) (MemoryContext context);	// 检查所有内存片段
#endif
} MemoryContextMethods;
/*
	内存管理机制：运行时大多数内存分配操作在各种语义的内存上下文(MemoryContext)中进行
	内存上下文释放时将会释放在其中分配的所有内存，可以通过释放内存上下文避免内存泄露

	1) 调用 MemoryContextInit 创建 TopMemoryContext 和 ErrorContext
	2) 调用 AllocSetContextCreate 以 TopMemoryContext为根节点创建 PostmasterContext
	3) 将全局指针 CurrentMemoryContext 指向 PostmasterContext

	是一个抽象类，可以有多个实现，并不管理实际上的内存分配，仅仅是用作对 MemoryContextData 树的控制
	管理一个内存上下文中的内存块是通过 AllocSet 结构来完成的，MemoryContext 仅作为 AllocSet 的头部信息存在
*/
typedef struct MemoryContextData
{
	NodeTag		type;			/* identifies exact kind of context */
	/* these two fields are placed here to minimize alignment wastage: */
	bool		isReset;		/* T = no space alloced since last reset */
	bool		allowInCritSection; /* allow palloc in critical section */
	Size		mem_allocated;	/* track memory allocated for this context */
	const MemoryContextMethods *methods;	/* virtual function table */	// 内存处理函数指针
	MemoryContext parent;		/* NULL if no parent (toplevel context) */	// 父节点指针
	MemoryContext firstchild;	/* head of linked list of children */	// 第一个孩子节点指针
	MemoryContext prevchild;	/* previous child of same parent */	// 前一个兄弟节点指针
	MemoryContext nextchild;	/* next child of same parent */	// 后一个兄弟节点指针
	const char *name;			/* context name (just for debugging) */	// 节点的名字
	const char *ident;			/* context ID if any (just for debugging) */
	MemoryContextCallback *reset_cbs;	/* list of reset/delete callbacks */
} MemoryContextData;

/* utils/palloc.h contains typedef struct MemoryContextData *MemoryContext */


/*
 * MemoryContextIsValid
 *		True iff memory context is valid.
 *
 * Add new context types to the set accepted by this macro.
 */
#define MemoryContextIsValid(context) \
	((context) != NULL && \
	 (IsA((context), AllocSetContext) || \
	  IsA((context), SlabContext) || \
	  IsA((context), GenerationContext)))

#endif							/* MEMNODES_H */
