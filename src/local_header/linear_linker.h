/**
 *	\file		linear_linker_h
 *	\brief		Helper library for building Singly linked list.
 *	\remarks	Be careful to use this because of the lack of memory safety checking implementation.
 */

/*
300@divide_distance=1, skip_for_presorted->
INFO : linear_linker_sort_test: compare_count[random->ordered]: 2438
INFO : linear_linker_sort_test: compare_count[ordered->ordered]: 299
INFO : linear_linker_sort_test: compare_count[ordered->reverse]: 1479

3000@divide_distance=1, skip_for_presorted->
INFO : linear_linker_sort_test: compare_count[random->ordered]: 33129
INFO : linear_linker_sort_test: compare_count[ordered->ordered]: 2999
INFO : linear_linker_sort_test: compare_count[ordered->reverse]: 19827

30000@divide_distance=1, skip_for_presorted->
INFO : linear_linker_sort_test: compare_count[random->ordered]: 430146
INFO : linear_linker_sort_test: compare_count[ordered->ordered]: 29999
INFO : linear_linker_sort_test: compare_count[ordered->reverse]: 249503

300@divide_distance=16, skip_for_presorted->
INFO : linear_linker_sort_test: compare_count[random->ordered]: 2533
INFO : linear_linker_sort_test: compare_count[ordered->ordered]: 299
INFO : linear_linker_sort_test: compare_count[ordered->reverse]: 1164

3000@divide_distance=16, skip_for_presorted->
INFO : linear_linker_sort_test: compare_count[random->ordered]: 34546
INFO : linear_linker_sort_test: compare_count[ordered->ordered]: 2999
INFO : linear_linker_sort_test: compare_count[ordered->reverse]: 16643

30000@divide_distance=16, skip_for_presorted->
INFO : linear_linker_sort_test: compare_count[random->ordered]: 444414
INFO : linear_linker_sort_test: compare_count[ordered->ordered]: 29999
INFO : linear_linker_sort_test: compare_count[ordered->reverse]: 217628

300@pure merge sort->
INFO : linear_linker_sort_test: compare_count[random->ordered]: 2228
INFO : linear_linker_sort_test: compare_count[ordered->ordered]: 1416
INFO : linear_linker_sort_test: compare_count[ordered->reverse]: 1180

3000@pure merge sort->
INFO : linear_linker_sort_test: compare_count[random->ordered]: 31149
INFO : linear_linker_sort_test: compare_count[ordered->ordered]: 18156
INFO : linear_linker_sort_test: compare_count[ordered->reverse]: 16828

30000@pure merge sort->
INFO : linear_linker_sort_test: compare_count[random->ordered]: 410299
INFO : linear_linker_sort_test: compare_count[ordered->ordered]: 228752
INFO : linear_linker_sort_test: compare_count[ordered->reverse]: 219504

300@divide_distance=16->
INFO : linear_linker_sort_test: compare_count[random->ordered]: 2545
INFO : linear_linker_sort_test: compare_count[ordered->ordered]: 1097
INFO : linear_linker_sort_test: compare_count[ordered->reverse]: 1146

3000@divide_distance=16->
INFO : linear_linker_sort_test: compare_count[random->ordered]: 34141
INFO : linear_linker_sort_test: compare_count[ordered->ordered]: 14972
INFO : linear_linker_sort_test: compare_count[ordered->reverse]: 16456

30000@divide_distance=16->
INFO : linear_linker_sort_test: compare_count[random->ordered]: 442347
INFO : linear_linker_sort_test: compare_count[ordered->ordered]: 196877
INFO : linear_linker_sort_test: compare_count[ordered->reverse]: 215754

 */

#ifndef	LINEAR_LINKER_H_
#define	LINEAR_LINKER_H_

#include <stddef.h>
#include <assert.h>

#define LINEAR_LINKER_ASSERTION_ 0


#define	LL_DBG_PRINTF_LEVEL_ 1

#define		LL_DBG_PRINTF_VERBOSE_(...)\
	do{\
		if(LL_DBG_PRINTF_LEVEL_ < 0){printf("VERB : ");printf(__VA_ARGS__);}\
	}while(0)

#define		LL_DBG_PRINTF_DEBUG_(...)\
	do{\
		if(LL_DBG_PRINTF_LEVEL_ < 1){printf("DEBUG: ");printf(__VA_ARGS__);}\
	}while(0)

#define		LL_DBG_PRINTF_INFO_(...)\
	do{\
		if(LL_DBG_PRINTF_LEVEL_ < 2){printf("INFO : ");printf(__VA_ARGS__);}\
	}while(0)

#define		LL_DBG_PRINTF_WARN_(...)\
	do{\
		if(LL_DBG_PRINTF_LEVEL_ < 3){printf("WARN : ");printf(__VA_ARGS__);}\
	}while(0)

#define		LL_DBG_PRINTF_ERROR_(...)\
	do{\
		if(LL_DBG_PRINTF_LEVEL_ < 4){printf("ERROR: ");printf(__VA_ARGS__);}\
	}while(0)

#define		LL_DBG_PRINTF_FATAL_(...)\
	do{\
		if(LL_DBG_PRINTF_LEVEL_ < 5){printf("FATAL: ");printf(__VA_ARGS__);}\
	}while(0)

/**
 *	\brief	Unnamed-structure indicates the link for making linke list.
 *	\arg	type_s	The structure you want to treat.
 */
#define LINEAR_LINKER_S_(type_s)\
	struct {\
		type_s		*next;		/*Pointer to the next element.*/\
		type_s		**to_self;	/* (*next) of the previous element(=shows the node). */\
	}\

/**
 *	\brief	Add the node to the tail of the linked list.
 *	\arg	pheadptr	(type_s**)	Pointer to the head node ptr.(=Treat it as the list);
 *	\arg	pnode		(type_s*)	The node you want to add.
 *	\arg	type_s					Target structure.
 *	\arg	link_member				Target link member which you want to treat as link.
 */
#define	LINEAR_LINKER_ADD_(pheadptr, pnode, type_s, link_member)\
	do{\
		type_s **cursor = pheadptr;\
		while(*cursor != NULL){\
			cursor = &((*cursor)->link_member.next);\
		}\
		*cursor = pnode;\
		(pnode)->link_member.to_self = cursor;\
		(pnode)->link_member.next = NULL;\
		if(LINEAR_LINKER_ASSERTION_){\
			\
		}\
	}while(0)

/**
 *	\brief	Remove the node from the linked list.
 *	\arg	pheadptr	(type_s**)	Pointer to the head node ptr.(=Treat it as the list);
 *	\arg	pnode		(type_s*)	The node you want to remove.
 *	\arg	type_s					Target structure.
 *	\arg	link_member				Target link member which you want to treat as link.
 *	\remarks			pheadptr is for just showing the list you are treating. If NULL, it works correctly.
 *	\warning			Do not use this while iterating with LINEAR_LINKER_NEXT_ directly.
 */
#define	LINEAR_LINKER_DEL_(pheadptr, pnode, type_s, link_member)\
	do{\
		type_s **cursor;\
		cursor = (pnode)->link_member.to_self;\
		*cursor = (pnode)->link_member.next;\
		if(*cursor != NULL){\
			(*cursor)->link_member.to_self = cursor;\
		}\
		(pnode)->link_member.to_self = NULL;\
		(pnode)->link_member.next = NULL;\
	}while(0)

/**
 *	\brief	Refer the next node.
 *	\arg	pnode		(type_s*)	Current node.
 *	\arg	type_s					Target structure.
 *	\arg	link_member				Target link member which you want to treat as link.
 *	\return			(type_s*)	The next of pnode.
 *	\remarks						If pnode indicates the tail, this returns NULL
 */
#define	LINEAR_LINKER_NEXT_(pnode, type_s, link_member)\
	(pnode == NULL ? NULL : ((type_s *)pnode)->link_member.next)
	
/**
 *	\brief	Refer the nth next from the node.
 *	\arg	pnode		(type_s*)	Current node.
 *	\arg	ret_ptr		(type_s**)	Returned addr will filled to this.
 *	\arg	type_s					Target structure.
 *	\arg	link_member				Target link member which you want to treat as link.
 */
#define	LINEAR_LINKER_NTH_NEXT_(pnode, ret_ptr, n, type_s, link_member)\
	do{\
		int i;\
		type_s *ret = pnode;\
		for(i = 0; i < n; i++){\
			if((ret = LINEAR_LINKER_NEXT_(ret, type_s, link_member)) == NULL){\
				break;\
			}\
		}\
		*ret_ptr = ret;\
	}while(0)\


/**
 *	\brief	Get and delete the first node from the list.
 *	\arg	pheadptr	(type_s**)	Pointer to the head node ptr.(=Treat it as the list);
 *	\arg	retptr		(type_s**)	Returned addr will filled to this.
 *	\arg	type_s					Target structure.
 *	\arg	link_member				Target link member which you want to treat as link.
 */
#define	LINEAR_LINKER_POP_(pheadptr, retptr, type_s, link_member)\
	do{\
		/*先頭に配置されたインスタンスを返す*/\
		*retptr = *pheadptr;\
		if(*retptr != NULL){\
			/*先頭が誰かいた→pheadptrをその次に移し、そのto_selfをpheadptrに向ける*/\
			*pheadptr = (*retptr)->link_member.next;\
			if(*pheadptr != NULL){\
				(*pheadptr)->link_member.to_self = pheadptr;\
			}\
			/*リストからいなくなるので後始末*/\
			(*retptr)->link_member.next = NULL;\
			(*retptr)->link_member.to_self = NULL;\
		}\
	}while(0)
	
/**
 *	\brief	Insert the node as the after of specified node.
 *	\arg	pheadptr	(type_s**)	Pointer to the head node ptr.(=Treat it as the list);
 *	\arg	pnode		(type_s*)	The node you want to insert.
 *	\arg	pindex		(type_s*)	pnode will be inserted after this. If NULL, that means insertion to the head.
 *	\arg	type_s					Target structure.
 *	\arg	link_member				Target link member which you want to treat as link.
 */
#define	LINEAR_LINKER_INSERT_AFTER_(pheadptr, pnode, pindex, type_s, link_member)\
	do{\
		type_s **cursor = pheadptr;\
		/*pindex == NULL => Insert to the head.*/\
		if(pindex != NULL){\
			/*If you indicate the pindex as Non-NULL, insert the node after this.*/\
			cursor = &(((type_s *)pindex)->link_member.next);\
		}\
		if(*cursor != NULL){\
			/*カーソルを合わせたところ(pindexの次/pindex=NULLなら先頭)に誰かがいる => その前に挿入されることになる。*/\
			(*cursor)->link_member.to_self = &((pnode)->link_member.next);\
		}\
		(pnode)->link_member.to_self = cursor;\
		(pnode)->link_member.next = *cursor;\
		*cursor = pnode;\
		\
	}while(0)\

/**
 *	\brief	Insert the node with the order.
 *	\arg	pheadptr	(type_s**)	Pointer to the head node ptr.(=Treat it as the list);
 *	\arg	pnode		(type_s*)	The node you want to insert.
 *	\arg	comp		*int(type_s*, type_s*)	Comparison function name meaning "node_a - node_b".
 *	\arg	type_s					Target structure.
 *	\arg	link_member				Target link member which you want to treat as link.
 */
#define	LINEAR_LINKER_INSERT_INORDER_(pheadptr, pnode, comp, type_s, link_member)\
	do{\
		type_s *index;\
		type_s *before = NULL;\
		for(index = *pheadptr; index != NULL; index = LINEAR_LINKER_NEXT_(index, type_s, link_member)){\
			int ret = comp(pnode, index);\
			/*If the same value, the insert after the same value node.*/\
			if(ret < 0){\
				break;\
			}\
			before = index;\
		}\
		LINEAR_LINKER_INSERT_AFTER_(pheadptr, pnode, before, type_s, link_member);\
		\
	}while(0)\

/**
 *	\brief	Sort the list with the order specified.
 *	\arg	pheadptr	(type_s**)	Pointer to the head node ptr.(=Treat it as the list);
 *	\arg	comp		*int(type_s*, type_s*)	Comparison function name meaning "node_a - node_b".
 *	\arg	type_s					Target structure.
 *	\arg	link_member				Target link member which you want to treat as link.
 *	\remarks			The used algorithm is Non-recursion merge sort.
 */
#define	LINEAR_LINKER_SORT_(pheadptr, comp, type_s, link_member)\
	do{\
		type_s *left_ptr, *left_last;\
		type_s *right_ptr, *right_end;\
		type_s *dest_list = NULL;\
		\
		int divide_distance = 16;\
		const int skip_for_presorted = 1;\
		\
		type_s *work;\
		type_s *ins_ptr = NULL;\
		\
		type_s **tmp_cursor = &dest_list;\
		if(divide_distance != 1){\
			/*If the first "divide_distance" is not "1", it means this is a hybrid algorithm of merge and insertion.*/\
			for(;;){\
				/*Perform Insertion-sort for each block having "divide_distance" elems. */\
				int i;\
				type_s *cached_index;\
				cached_index = *tmp_cursor;\
				for(i = 0; i < divide_distance; i++){\
					LINEAR_LINKER_POP_(pheadptr, &work, type_s, link_member);\
					if(work == NULL){\
						break;\
					}\
					\
					type_s **tmp_head = tmp_cursor;\
					if(cached_index != NULL && comp(cached_index, work) <= 0){\
						/*if cache is smaller than work, then search from the next of cached_index.*/\
						tmp_head = &(cached_index->link_member.next);\
					}\
					LINEAR_LINKER_INSERT_INORDER_(tmp_head, work, comp, type_s, link_member);\
					cached_index = work;\
					\
				}\
				if(work == NULL){\
					/*No more elems in the original list.->End of the first insertion sorting phase.*/\
					*pheadptr = dest_list;\
					if(dest_list != NULL){\
						dest_list->link_member.to_self = pheadptr;\
					}\
					break;\
				}\
				/*May still having next node->Prepare for the next block.*/\
				while(work->link_member.next != NULL){\
					work = work->link_member.next;\
				}\
				tmp_cursor = &(work->link_member.next);\
			}\
		}\
		\
		for(;;){\
			/*Merge Left section(left_ptr to right_ptr--) and Right section(right_ptr to right_end--) */\
			/*Both sections are already sorted.*/\
			dest_list = NULL;	/*head of the temporary list.*/\
			ins_ptr = NULL;		/*tail of &dest_list*/\
			left_ptr = *pheadptr;\
			LINEAR_LINKER_NTH_NEXT_(left_ptr, &left_last, divide_distance - 1, type_s, link_member);\
			right_ptr = LINEAR_LINKER_NEXT_(left_last, type_s, link_member);\
			if(right_ptr == NULL){\
				/*ALL ELEM ARE BELONG TO LEFT->the end of the sorting.*/\
				break;\
			}\
			LINEAR_LINKER_NTH_NEXT_(right_ptr, &right_end, divide_distance, type_s, link_member);\
			\
			/*printf("divide_distance=%d(0x%04x)\n", divide_distance, divide_distance);*/\
			while(left_ptr != NULL){\
				\
				/*printf("left[%p->%p], right[%p->%p]\n", left_ptr, right_ptr, right_ptr, right_end);*/\
				\
				/*printf("left[@%p to %p]\n", left_ptr, right_ptr);*/\
				/*printf("right[@%p to %p]\n", right_ptr, right_end);*/\
				if(right_ptr == NULL || (skip_for_presorted && comp(left_last, right_ptr) <= 0)){\
					/*NOT NECESSARY TO MERGE FOR THIS SECTION.*/\
					right_ptr = right_end;\
				}\
				else{\
					/*STILL NEEDED TO BE MERGED CAUSE THESE SECTIONS SEEM NOT TO BE SEQUENCED.*/\
					while(right_ptr != right_end && left_ptr != right_ptr){\
						/*left section and right section still have nodes.->pop lesser and append it to the temp list.*/\
						if(comp(left_ptr, right_ptr) <= 0){\
							work = left_ptr;\
							left_ptr = LINEAR_LINKER_NEXT_(left_ptr, type_s, link_member);\
						}\
						else{\
							work = right_ptr;\
							right_ptr = LINEAR_LINKER_NEXT_(right_ptr, type_s, link_member);\
						}\
						LINEAR_LINKER_DEL_(pheadptr, work, type_s, link_member);\
						/*this means LINEAR_LINKER_ADD_(&dest_list, work, type_s, link_member);*/\
						LINEAR_LINKER_INSERT_AFTER_(&dest_list, work, ins_ptr, type_s, link_member);\
						ins_ptr = work;\
						\
					}\
				}\
				/*One section has no more node.*/\
				/*.=>Whole of the another section can be treated as sorted.*/\
				type_s **cursor = (ins_ptr == NULL) ? &dest_list : &(ins_ptr->link_member.next);\
				\
				/*check if right section is out. If so, concat from left ptr.*/\
				work = (right_ptr == right_end) ? left_ptr : right_ptr;\
				/*assert(work->link_member.to_self == pheadptr);*/\
				\
				*pheadptr = right_end;\
				if(right_end != NULL){\
					*(right_end->link_member.to_self) = NULL;\
					right_end->link_member.to_self = pheadptr;\
				}\
				*cursor = work;\
				work->link_member.to_self = cursor;\
				\
				while(work->link_member.next != NULL){\
					work = work->link_member.next;\
				}\
				\
				ins_ptr = work;\
				\
				/*To next merge.*/\
				left_ptr = right_end;\
				LINEAR_LINKER_NTH_NEXT_(left_ptr, &left_last, divide_distance - 1, type_s, link_member);\
				right_ptr = LINEAR_LINKER_NEXT_(left_last, type_s, link_member);\
				LINEAR_LINKER_NTH_NEXT_(right_ptr, &right_end, divide_distance, type_s, link_member);\
				/*printf("next section: left[%p->%p], right[%p->%p]\n", left_ptr, right_ptr, right_ptr, right_end);*/\
			}\
			/*printf("merge(%d) end. left[%p->%p], right[%p->%p]\n", divide_distance, left_ptr, right_ptr, right_ptr, right_end);*/\
			divide_distance *= 2;\
			\
			*pheadptr = dest_list;\
			dest_list->link_member.to_self = pheadptr;\
			/*printf("\n\n\n\n\n\n");*/\
			/*usleep(500*1000);*/\
		}\
	}while(0)\

#define	LINEAR_LINKER_WHOLE_ASSERT_(pheadptr, type_s, link_member)\
	do{\
		type_s *tmp;\
		type_s **cursor;\
		cursor = pheadptr;\
		int i = 0;\
		for(tmp = *pheadptr; tmp != NULL; tmp = LINEAR_LINKER_NEXT_(tmp, type_s, link_member)){\
			LL_DBG_PRINTF_DEBUG_("%s[%p], %dth\n", __func__, tmp, i++);\
			LL_DBG_PRINTF_VERBOSE_("%s[%p] checking...\n", __func__, tmp);\
			assert(*cursor == tmp);\
			LL_DBG_PRINTF_VERBOSE_("%s[%p] checking cursor...\n", __func__, tmp);\
			assert(cursor == tmp->link_member.to_self);\
			cursor = &(tmp->link_member.next);\
		}\
	}while(0)

#endif	/* !LINEAR_LINKER_H_ */
