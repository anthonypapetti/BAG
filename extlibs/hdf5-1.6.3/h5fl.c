/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Copyright by the Board of Trustees of the University of Illinois.         *
 * All rights reserved.                                                      *
 *                                                                           *
 * This file is part of HDF5.  The full HDF5 copyright notice, including     *
 * terms governing use, modification, and redistribution, is contained in    *
 * the files COPYING and Copyright.html.  COPYING can be found at the root   *
 * of the source code distribution tree; Copyright.html can be found at the  *
 * root level of an installed copy of the electronic HDF5 document set and   *
 * is linked from the top-level documents page.  It can also be found at     *
 * http://hdf.ncsa.uiuc.edu/HDF5/doc/Copyright.html.  If you do not have     *
 * access to either file, you may request a copy from hdfhelp@ncsa.uiuc.edu. *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/*
 * Programmer: Quincey Koziol <koziol@ncsa.uiuc.edu>
 *	       Thursday, March 23, 2000
 *
 * Purpose: Manage priority queues of free-lists (of blocks of bytes).
 *      These are used in various places in the library which allocate and
 *      free differently blocks of bytes repeatedly.  Usually the same size
 *      of block is allocated and freed repeatly in a loop, while writing out
 *      chunked data for example, but the blocks may also be of different sizes
 *      from different datasets and an attempt is made to optimize access to
 *      the proper free list of blocks by using these priority queues to
 *      move frequently accessed free lists to the head of the queue.
 */

/* Pablo information */
/* (Put before include files to avoid problems with inline functions) */
#define PABLO_MASK	H5FL_mask

/* #define H5FL_DEBUG */

#include "H5private.h"		/* Generic Functions			*/
#include "H5Eprivate.h"		/* Error handling		  	*/
#include "H5FLprivate.h"	/* Free Lists                           */
#include "H5MMprivate.h"	/* Memory management			*/

static int		interface_initialize_g = 0;
#define INTERFACE_INIT	NULL

/*
 * Private type definitions
 */

/*
    Default limits on how much memory can accumulate on each free list before
    it is garbage collected.
 */
static size_t H5FL_reg_glb_mem_lim=1*1024*1024; /* Default to 1MB limit on all regular free lists */
static size_t H5FL_reg_lst_mem_lim=1*65536;     /* Default to 64KB limit on each regular free list */
static size_t H5FL_arr_glb_mem_lim=4*1024*1024; /* Default to 4MB limit on all array free lists */
static size_t H5FL_arr_lst_mem_lim=4*65536;     /* Default to 256KB limit on each array free list */
static size_t H5FL_blk_glb_mem_lim=16*1024*1024; /* Default to 16MB limit on all block free lists */
static size_t H5FL_blk_lst_mem_lim=1024*1024;   /* Default to 1024KB (1MB) limit on each block free list */

/* A garbage collection node for regular free lists */
typedef struct H5FL_reg_gc_node_t {
    H5FL_reg_head_t *list;              /* Pointer to the head of the list to garbage collect */
    struct H5FL_reg_gc_node_t *next;    /* Pointer to the next node in the list of things to garbage collect */
} H5FL_reg_gc_node_t;

/* The garbage collection head for regular free lists */
typedef struct H5FL_reg_gc_list_t {
    size_t mem_freed;               /* Amount of free memory on list */
    struct H5FL_reg_gc_node_t *first;   /* Pointer to the first node in the list of things to garbage collect */
} H5FL_reg_gc_list_t;

/* The head of the list of things to garbage collect */
static H5FL_reg_gc_list_t H5FL_reg_gc_head={0,NULL};

/* A garbage collection node for array free lists */
typedef struct H5FL_gc_arr_node_t {
    H5FL_arr_head_t *list;              /* Pointer to the head of the list to garbage collect */
    struct H5FL_gc_arr_node_t *next;    /* Pointer to the next node in the list of things to garbage collect */
} H5FL_gc_arr_node_t;

/* The garbage collection head for array free lists */
typedef struct H5FL_gc_arr_list_t {
    size_t mem_freed;                    /* Amount of free memory on list */
    struct H5FL_gc_arr_node_t *first;    /* Pointer to the first node in the list of things to garbage collect */
} H5FL_gc_arr_list_t;

/* The head of the list of array things to garbage collect */
static H5FL_gc_arr_list_t H5FL_arr_gc_head={0,NULL};

/* A garbage collection node for blocks */
typedef struct H5FL_blk_gc_node_t {
    H5FL_blk_head_t *pq;                /* Pointer to the head of the PQ to garbage collect */
    struct H5FL_blk_gc_node_t *next;    /* Pointer to the next node in the list of things to garbage collect */
} H5FL_blk_gc_node_t;

/* The garbage collection head for blocks */
typedef struct H5FL_blk_gc_list_t {
    size_t mem_freed;                   /* Amount of free memory on list */
    struct H5FL_blk_gc_node_t *first;   /* Pointer to the first node in the list of things to garbage collect */
} H5FL_blk_gc_list_t;

/* The head of the list of PQs to garbage collect */
static H5FL_blk_gc_list_t H5FL_blk_gc_head={0,NULL};

/* Forward declarations of local static functions */
static herr_t H5FL_reg_gc(void);
static herr_t H5FL_reg_gc_list(H5FL_reg_head_t *head);
static herr_t H5FL_arr_gc(void);
static herr_t H5FL_arr_gc_list(H5FL_arr_head_t *head);
static herr_t H5FL_blk_gc(void);
static herr_t H5FL_blk_gc_list(H5FL_blk_head_t *head);

/* Declare a free list to manage the H5FL_blk_node_t struct */
H5FL_DEFINE(H5FL_blk_node_t);


/*-------------------------------------------------------------------------
 * Function:	H5FL_malloc
 *
 * Purpose:	Attempt to allocate space using malloc.  If malloc fails, garbage
 *      collect and try again.  If malloc fails again, then return NULL.
 *
 * Return:	Success:	non-NULL
 * 		Failure:	NULL
 *
 * Programmer:	Quincey Koziol
 *              Tuesday, August 1, 2000
 *
 * Modifications:
 * 	
 *-------------------------------------------------------------------------
 */
static void *
H5FL_malloc(size_t mem_size)
{
    void *ret_value=NULL;   /* return value*/

    FUNC_ENTER_NOAPI(H5FL_malloc, NULL);

    /* Attempt to allocate the memory requested */
    if(NULL==(ret_value=H5MM_malloc(mem_size))) {
        /* If we can't allocate the memory now, try garbage collecting first */
        H5FL_garbage_coll();

        /* Now try allocating the memory again */
        if(NULL==(ret_value=H5MM_malloc(mem_size)))
            HGOTO_ERROR(H5E_RESOURCE, H5E_NOSPACE, NULL, "memory allocation failed for chunk");
    } /* end if */

done:
    FUNC_LEAVE_NOAPI(ret_value);
}   /* end H5FL_malloc() */


/*-------------------------------------------------------------------------
 * Function:	H5FL_reg_init
 *
 * Purpose:	Initialize a free list for a certain type.  Right now, this just
 *      adds the free list to the list of things to garbage collect.
 *
 * Return:	Success:	Non-negative
 * 		Failure:	Negative
 *
 * Programmer:	Quincey Koziol
 *              Friday, March 24, 2000
 *
 * Modifications:
 * 	
 *-------------------------------------------------------------------------
 */
static herr_t
H5FL_reg_init(H5FL_reg_head_t *head)
{
    H5FL_reg_gc_node_t *new_node;   /* Pointer to the node for the new list to garbage collect */
    herr_t ret_value=SUCCEED;   /* return value*/

    FUNC_ENTER_NOAPI_NOINIT(H5FL_reg_init);

    /* Allocate a new garbage collection node */
    if (NULL==(new_node = H5MM_malloc(sizeof(H5FL_reg_gc_node_t))))
        HGOTO_ERROR (H5E_RESOURCE, H5E_NOSPACE, FAIL, "memory allocation failed");

    /* Initialize the new garbage collection node */
    new_node->list=head;

    /* Link in to the garbage collection list */
    new_node->next=H5FL_reg_gc_head.first;
    H5FL_reg_gc_head.first=new_node;

    /* Indicate that the free list is initialized */
    head->init=1;

    /* Make certain that the space allocated is large enough to store a free list pointer (eventually) */
    if(head->size<sizeof(void *))
        head->size=sizeof(void *);

done:
    FUNC_LEAVE_NOAPI(ret_value);
}   /* end H5FL_reg_init() */


/*-------------------------------------------------------------------------
 * Function:	H5FL_reg_free
 *
 * Purpose:	Release an object & put on free list
 *
 * Return:	Success:	Non-negative
 * 		Failure:	Negative
 *
 * Programmer:	Quincey Koziol
 *              Friday, March 24, 2000
 *
 * Modifications:
 * 	
 *-------------------------------------------------------------------------
 */
void *
H5FL_reg_free(H5FL_reg_head_t *head, void *obj)
{
    H5FL_reg_node_t *temp;      /* Temp. ptr to the new free list node allocated */
    void *ret_value=NULL;       /* Return value */

    FUNC_ENTER_NOAPI(H5FL_reg_free, NULL);

    /* Double check parameters */
    assert(head);
    assert(obj);

#ifdef H5FL_DEBUG
    HDmemset(obj,255,head->size);
#endif /* H5FL_DEBUG */

    /* Make certain that the free list is initialized */
    assert(head->init);

    /* Alias the pointer to the block to free into a H5FL_reg_node_t node */
    temp=(H5FL_reg_node_t *)obj;

    /* Link into the free list */
    temp->next=head->list;

    /* Point free list at the node freed */
    head->list=temp;

    /* Increment the number of blocks & memory on free list */
    head->onlist++;
    head->list_mem+=head->size;

    /* Increment the amount of "regular" freed memory globally */
    H5FL_reg_gc_head.mem_freed+=head->size;

    /* Check for exceeding free list memory use limits */
    /* First check this particular list */
    if(head->list_mem>H5FL_reg_lst_mem_lim)
        H5FL_reg_gc_list(head);

    /* Then check the global amount memory on regular free lists */
    if(H5FL_reg_gc_head.mem_freed>H5FL_reg_glb_mem_lim)
        H5FL_reg_gc();

done:
    FUNC_LEAVE_NOAPI(ret_value);
}   /* end H5FL_reg_free() */


/*-------------------------------------------------------------------------
 * Function:	H5FL_reg_malloc
 *
 * Purpose:	Allocate a block on a free list
 *
 * Return:	Success:	Pointer to a valid object
 * 		Failure:	NULL
 *
 * Programmer:	Quincey Koziol
 *              Friday, March 24, 2000
 *
 * Modifications:
 * 	
 *-------------------------------------------------------------------------
 */
void *
H5FL_reg_malloc(H5FL_reg_head_t *head)
{
    void *ret_value;        /* Pointer to object to return */

    FUNC_ENTER_NOAPI(H5FL_reg_malloc, NULL);

    /* Double check parameters */
    assert(head);

    /* Make certain the list is initialized first */
    if(!head->init)
        H5FL_reg_init(head);

    /* Check for nodes available on the free list first */
    if(head->list!=NULL) {
        /* Get a pointer to the block on the free list */
        ret_value=(void *)(head->list);

        /* Remove node from free list */
        head->list=head->list->next;

        /* Decrement the number of blocks & memory on free list */
        head->onlist--;
        head->list_mem-=head->size;

        /* Decrement the amount of global "regular" free list memory in use */
        H5FL_reg_gc_head.mem_freed-=(head->size);

    } /* end if */
    /* Otherwise allocate a node */
    else {
        if (NULL==(ret_value = H5FL_malloc(head->size)))
            HGOTO_ERROR (H5E_RESOURCE, H5E_NOSPACE, NULL, "memory allocation failed");

        /* Increment the number of blocks allocated in list */
        head->allocated++;
    } /* end else */

done:
    FUNC_LEAVE_NOAPI(ret_value);
}   /* end H5FL_reg_malloc() */


/*-------------------------------------------------------------------------
 * Function:	H5FL_reg_calloc
 *
 * Purpose:	Allocate a block on a free list and clear it to zeros
 *
 * Return:	Success:	Pointer to a valid object
 * 		Failure:	NULL
 *
 * Programmer:	Quincey Koziol
 *              Monday, December 23, 2002
 *
 * Modifications:
 * 	
 *-------------------------------------------------------------------------
 */
void *
H5FL_reg_calloc(H5FL_reg_head_t *head)
{
    void *ret_value;        /* Pointer to object to return */

    FUNC_ENTER_NOAPI(H5FL_reg_calloc, NULL);

    /* Double check parameters */
    assert(head);

    /* Allocate the block */
    if (NULL==(ret_value = H5FL_reg_malloc(head)))
        HGOTO_ERROR (H5E_RESOURCE, H5E_NOSPACE, NULL, "memory allocation failed");

    /* Clear to zeros */
    HDmemset(ret_value,0,head->size);

done:
    FUNC_LEAVE_NOAPI(ret_value);
}   /* end H5FL_reg_calloc() */


/*-------------------------------------------------------------------------
 * Function:	H5FL_reg_gc_list
 *
 * Purpose:	Garbage collect on a particular object free list
 *
 * Return:	Success:	Non-negative
 * 		Failure:	Negative
 *
 * Programmer:	Quincey Koziol
 *              Tuesday, July 25, 2000
 *
 * Modifications:
 * 	
 *-------------------------------------------------------------------------
 */
static herr_t
H5FL_reg_gc_list(H5FL_reg_head_t *head)
{
    H5FL_reg_node_t *free_list; /* Pointer to nodes in free list being garbage collected */
    void *tmp;          /* Temporary node pointer */
    size_t total_mem;   /* Total memory used on list */
    
    FUNC_ENTER_NOAPI_NOINIT_NOFUNC(H5FL_reg_gc_list);

    /* Calculate the total memory used on this list */
    total_mem=head->onlist*head->size;

    /* For each free list being garbage collected, walk through the nodes and free them */
    free_list=head->list;
    while(free_list!=NULL) {
        tmp=free_list->next;

        /* Decrement the count of nodes allocated and free the node */
        head->allocated--;

        /* Decrement count of free memory on this list */
        head->list_mem-=head->size;

        H5MM_xfree(free_list);

        free_list=tmp;
    } /* end while */

    /* Double check that all the memory on this list is recycled */
    assert(head->list_mem==0);

    /* Indicate no free nodes on the free list */
    head->list=NULL;
    head->onlist=0;

    /* Decrement global count of free memory on "regular" lists */
    H5FL_reg_gc_head.mem_freed-=total_mem;

    FUNC_LEAVE_NOAPI(SUCCEED);
}   /* end H5FL_reg_gc_list() */


/*-------------------------------------------------------------------------
 * Function:	H5FL_reg_gc
 *
 * Purpose:	Garbage collect on all the object free lists
 *
 * Return:	Success:	Non-negative
 * 		Failure:	Negative
 *
 * Programmer:	Quincey Koziol
 *              Friday, March 24, 2000
 *
 * Modifications:
 *  Broke into two parts, one for looping over all the free lists and
 *      another for freeing each list - QAK 7/25/00
 * 	
 *-------------------------------------------------------------------------
 */
static herr_t
H5FL_reg_gc(void)
{
    H5FL_reg_gc_node_t *gc_node;    /* Pointer into the list of things to garbage collect */
    
    FUNC_ENTER_NOAPI_NOINIT_NOFUNC(H5FL_reg_gc);

    /* Walk through all the free lists, free()'ing the nodes */
    gc_node=H5FL_reg_gc_head.first;
    while(gc_node!=NULL) {
        /* Release the free nodes on the list */
        H5FL_reg_gc_list(gc_node->list);

        /* Go on to the next free list to garbage collect */
        gc_node=gc_node->next;
    } /* end while */

    /* Double check that all the memory on the free lists is recycled */
    assert(H5FL_reg_gc_head.mem_freed==0);

    FUNC_LEAVE_NOAPI(SUCCEED);
}   /* end H5FL_reg_gc() */


/*--------------------------------------------------------------------------
 NAME
    H5FL_reg_term
 PURPOSE
    Terminate various H5FL object free lists
 USAGE
    int H5FL_term()
 RETURNS
    Success:	Positive if any action might have caused a change in some
                other interface; zero otherwise.
   	Failure:	Negative
 DESCRIPTION
    Release any resources allocated.
 GLOBAL VARIABLES
 COMMENTS, BUGS, ASSUMPTIONS
     Can't report errors...
 EXAMPLES
 REVISION LOG
        Robb Matzke, 2000-04-25
        If a list cannot be freed because something is using it then return
        zero (failure to free a list doesn't affect any other part of the
        library). If some other layer frees something during its termination
        it will return non-zero, which will cause this function to get called
        again to reclaim this layer's memory.
--------------------------------------------------------------------------*/
static int
H5FL_reg_term(void)
{
    H5FL_reg_gc_node_t *left;   /* pointer to garbage collection lists with work left */
    H5FL_reg_gc_node_t *tmp;    /* Temporary pointer to a garbage collection node */

    FUNC_ENTER_NOAPI_NOINIT_NOFUNC(H5FL_reg_term);

    if (interface_initialize_g) {
        /* Free the nodes on the garbage collection list, keeping nodes with allocations outstanding */
        left=NULL;
        while(H5FL_reg_gc_head.first!=NULL) {
            tmp=H5FL_reg_gc_head.first->next;

#ifdef H5FL_DEBUG
            printf("H5FL_reg_term: head->name=%s, head->allocated=%d\n", H5FL_reg_gc_head.first->list->name,(int)H5FL_reg_gc_head.first->list->allocated);
#endif /* H5FL_DEBUG */
            /* Check if the list has allocations outstanding */
            if(H5FL_reg_gc_head.first->list->allocated>0) {
                /* Add free list to the list of nodes with allocations open still */
                H5FL_reg_gc_head.first->next=left;
                left=H5FL_reg_gc_head.first;
            } /* end if */
            /* No allocations left open for list, get rid of it */
            else {
                /* Reset the "initialized" flag, in case we restart this list somehow (I don't know how..) */
                H5FL_reg_gc_head.first->list->init=0;

                /* Free the node from the garbage collection list */
                H5MM_xfree(H5FL_reg_gc_head.first);
            } /* end else */

            H5FL_reg_gc_head.first=tmp;
        } /* end while */

        /* Point to the list of nodes left with allocations open, if any */
        H5FL_reg_gc_head.first=left;
        if (!left)
            interface_initialize_g = 0; /*this layer has reached its initial state*/
    }

    /* Terminating this layer never affects other layers; rather, other layers affect
     * the termination of this layer. */
    FUNC_LEAVE_NOAPI(0);
}   /* end H5FL_reg_term() */


/*-------------------------------------------------------------------------
 * Function:	H5FL_blk_find_list
 *
 * Purpose:	Finds the free list for blocks of a given size.  Also moves that
 *      free list node to the head of the priority queue (if it isn't there
 *      already).  This routine does not manage the actual free list, it just
 *      works with the priority queue.
 *
 * Return:	Success:	valid pointer to the free list node
 *
 *		Failure:	NULL
 *
 * Programmer:	Quincey Koziol
 *		Thursday, March  23, 2000
 *
 * Modifications:
 *
 *-------------------------------------------------------------------------
 */
static H5FL_blk_node_t *
H5FL_blk_find_list(H5FL_blk_node_t **head, size_t size)
{
    H5FL_blk_node_t *temp;  /* Temp. pointer to node in the native list */

    FUNC_ENTER_NOAPI_NOINIT_NOFUNC(H5FL_blk_find_list)

    /* Find the correct free list */
    temp=*head;

    /* Check if the node is at the head of the list */
    if(temp && temp->size!=size) {
        temp=temp->next;

        while(temp!=NULL) {
            /* Check if we found the correct node */
            if(temp->size==size) {
                /* Take the node found out of it's current position */
                if(temp->next==NULL) {
                    temp->prev->next=NULL;
                } /* end if */
                else {
                    temp->prev->next=temp->next;
                    temp->next->prev=temp->prev;
                } /* end else */

                /* Move the found node to the head of the list */
                temp->prev=NULL;
                temp->next=*head;
                (*head)->prev=temp;
                *head=temp;

                /* Get out */
                break;
            } /* end if */

            temp=temp->next;
        } /* end while */
    } /* end if */
    
    FUNC_LEAVE_NOAPI(temp)
} /* end H5FL_blk_find_list() */


/*-------------------------------------------------------------------------
 * Function:	H5FL_blk_create_list
 *
 * Purpose:	Creates a new free list for blocks of the given size at the
 *      head of the priority queue.
 *
 * Return:	Success:	valid pointer to the free list node
 *
 *		Failure:	NULL
 *
 * Programmer:	Quincey Koziol
 *		Thursday, March  23, 2000
 *
 * Modifications:
 *
 *-------------------------------------------------------------------------
 */
static H5FL_blk_node_t *
H5FL_blk_create_list(H5FL_blk_node_t **head, size_t size)
{
    H5FL_blk_node_t *temp;  /* Temp. pointer to node in the list */
    H5FL_blk_node_t *ret_value;

    FUNC_ENTER_NOAPI_NOINIT(H5FL_blk_create_list);

    /* Allocate room for the new free list node */
    if(NULL==(temp=H5FL_MALLOC(H5FL_blk_node_t)))
        HGOTO_ERROR(H5E_RESOURCE, H5E_NOSPACE, NULL, "memory allocation failed for chunk info");
    
    /* Set the correct values for the new free list */
    temp->size=size;
    temp->list=NULL;

    /* Attach to head of priority queue */
    if(*head==NULL) {
        *head=temp;
        temp->next=temp->prev=NULL;
    } /* end if */
    else {
        temp->next=*head;
        (*head)->prev=temp;
        temp->prev=NULL;
        *head=temp;
    } /* end else */

    ret_value=temp;

done:
    FUNC_LEAVE_NOAPI(ret_value);
} /* end H5FL_blk_create_list() */


/*-------------------------------------------------------------------------
 * Function:	H5FL_blk_init
 *
 * Purpose:	Initialize a priority queue of a certain type.  Right now, this just
 *      adds the PQ to the list of things to garbage collect.
 *
 * Return:	Success:	Non-negative
 * 		Failure:	Negative
 *
 * Programmer:	Quincey Koziol
 *              Saturday, March 25, 2000
 *
 * Modifications:
 * 	
 *-------------------------------------------------------------------------
 */
static herr_t
H5FL_blk_init(H5FL_blk_head_t *head)
{
    H5FL_blk_gc_node_t *new_node;   /* Pointer to the node for the new list to garbage collect */
    herr_t ret_value=SUCCEED;       /* return value*/

    FUNC_ENTER_NOAPI_NOINIT(H5FL_blk_init);

    /* Allocate a new garbage collection node */
    if (NULL==(new_node = H5MM_malloc(sizeof(H5FL_blk_gc_node_t))))
        HGOTO_ERROR (H5E_RESOURCE, H5E_NOSPACE, FAIL, "memory allocation failed");

    /* Initialize the new garbage collection node */
    new_node->pq=head;

    /* Link in to the garbage collection list */
    new_node->next=H5FL_blk_gc_head.first;
    H5FL_blk_gc_head.first=new_node;

    /* Indicate that the PQ is initialized */
    head->init=1;

done:
    FUNC_LEAVE_NOAPI(ret_value);
}   /* end H5FL_blk_init() */


/*-------------------------------------------------------------------------
 * Function:	H5FL_blk_free_block_avail
 *
 * Purpose:	Checks if a free block of the appropriate size is available
 *      for a given list.
 *
 * Return:	Success:	non-negative
 *		Failure:	negative
 *
 * Programmer:	Quincey Koziol
 *		Monday, December 16, 2002
 *
 * Modifications:
 *
 *-------------------------------------------------------------------------
 */
htri_t
H5FL_blk_free_block_avail(H5FL_blk_head_t *head, size_t size)
{
    H5FL_blk_node_t *free_list;  /* The free list of nodes of correct size */
    htri_t ret_value;   /* Return value */

    FUNC_ENTER_NOAPI(H5FL_blk_free_block_avail, FAIL);

    /* Double check parameters */
    assert(head);

    /* check if there is a free list for blocks of this size */
    /* and if there are any blocks available on the list */
    if((free_list=H5FL_blk_find_list(&(head->head),size))!=NULL && free_list->list!=NULL)
        ret_value=TRUE;
    else
        ret_value=FALSE;
done:
    FUNC_LEAVE_NOAPI(ret_value);
} /* end H5FL_blk_free_block_avail() */


/*-------------------------------------------------------------------------
 * Function:	H5FL_blk_malloc
 *
 * Purpose:	Allocates memory for a block.  This routine is used
 *      instead of malloc because the block can be kept on a free list so
 *      they don't thrash malloc/free as much.
 *
 * Return:	Success:	valid pointer to the block
 *
 *		Failure:	NULL
 *
 * Programmer:	Quincey Koziol
 *		Thursday, March  23, 2000
 *
 * Modifications:
 *
 *-------------------------------------------------------------------------
 */
void *
H5FL_blk_malloc(H5FL_blk_head_t *head, size_t size)
{
    H5FL_blk_node_t *free_list;  /* The free list of nodes of correct size */
    H5FL_blk_list_t *temp;  /* Temp. ptr to the new native list allocated */
    void *ret_value;    /* Pointer to the block to return to the user */

    FUNC_ENTER_NOAPI(H5FL_blk_malloc, NULL);

    /* Double check parameters */
    assert(head);
    assert(size);

    /* Make certain the list is initialized first */
    if(!head->init)
        H5FL_blk_init(head);

    /* check if there is a free list for blocks of this size */
    /* and if there are any blocks available on the list */
    if((free_list=H5FL_blk_find_list(&(head->head),size))!=NULL && free_list->list!=NULL) {
        /* Remove the first node from the free list */
        temp=free_list->list;
        free_list->list=free_list->list->next;

        /* Restore the size of the block */
        temp->size=size;        /* Overwrites the 'next' field */

        /* Return the pointer to the data portion */
        ret_value=((char *)temp)+sizeof(H5FL_blk_list_t);

        /* Decrement the number of blocks & memory used on free list */
        head->onlist--;
        head->list_mem-=size;

        /* Decrement the amount of global "block" free list memory in use */
        H5FL_blk_gc_head.mem_freed-=size;

    } /* end if */
    /* No free list available, or there are no nodes on the list, allocate a new node to give to the user */
    else { 
        /* Allocate new node, with room for the page info header and the actual page data */
        if(NULL==(temp=H5FL_malloc(sizeof(H5FL_blk_list_t)+size)))
            HGOTO_ERROR(H5E_RESOURCE, H5E_NOSPACE, NULL, "memory allocation failed for chunk");

        /* Increment the number of blocks allocated */
        head->allocated++;

        /* Initialize the block allocated */
        temp->size=size;

        /* Set the return value to the block itself */
        ret_value=((char *)temp)+sizeof(H5FL_blk_list_t);
    } /* end else */

done:
    FUNC_LEAVE_NOAPI(ret_value);
} /* end H5FL_blk_malloc() */


/*-------------------------------------------------------------------------
 * Function:	H5FL_blk_calloc
 *
 * Purpose:	Allocates memory for a block and clear it to zeros.
 *      This routine is used
 *      instead of malloc because the block can be kept on a free list so
 *      they don't thrash malloc/free as much.
 *
 * Return:	Success:	valid pointer to the block
 *
 *		Failure:	NULL
 *
 * Programmer:	Quincey Koziol
 *		Monday, December 23, 2002
 *
 * Modifications:
 *
 *-------------------------------------------------------------------------
 */
void *
H5FL_blk_calloc(H5FL_blk_head_t *head, size_t size)
{
    void *ret_value;    /* Pointer to the block to return to the user */

    FUNC_ENTER_NOAPI(H5FL_blk_calloc, NULL);

    /* Double check parameters */
    assert(head);
    assert(size);

    /* Allocate the block */
    if (NULL==(ret_value = H5FL_blk_malloc(head,size)))
        HGOTO_ERROR (H5E_RESOURCE, H5E_NOSPACE, NULL, "memory allocation failed");

    /* Clear the block to zeros */
    HDmemset(ret_value,0,size);

done:
    FUNC_LEAVE_NOAPI(ret_value);
} /* end H5FL_blk_calloc() */


/*-------------------------------------------------------------------------
 * Function:	H5FL_blk_free
 *
 * Purpose:	Releases memory for a block.  This routine is used
 *      instead of free because the blocks can be kept on a free list so
 *      they don't thrash malloc/free as much.
 *
 * Return:	Success:	NULL
 *
 *		Failure:	never fails
 *
 * Programmer:	Quincey Koziol
 *		Thursday, March  23, 2000
 *
 * Modifications:
 *
 *-------------------------------------------------------------------------
 */
void *
H5FL_blk_free(H5FL_blk_head_t *head, void *block)
{
    H5FL_blk_node_t *free_list;      /* The free list of nodes of correct size */
    H5FL_blk_list_t *temp;      /* Temp. ptr to the new free list node allocated */
    size_t free_size;           /* Size of the block freed */
    void *ret_value=NULL;       /* Return value */

    FUNC_ENTER_NOAPI(H5FL_blk_free, NULL);

    /* Double check parameters */
    assert(head);
    assert(block);

    /* Get the pointer to the native block info header in front of the native block to free */
    temp=(H5FL_blk_list_t *)((unsigned char *)block-sizeof(H5FL_blk_list_t));

    /* Save the block's size for later */
    free_size=temp->size;

    /* check if there is a free list for native blocks of this size */
    if((free_list=H5FL_blk_find_list(&(head->head),temp->size))==NULL) {
        /* No free list available, create a new list node and insert it to the queue */
        free_list=H5FL_blk_create_list(&(head->head),temp->size);
    } /* end if */

    /* Prepend the free'd native block to the front of the free list */
    if(free_list!=NULL) {
        temp->next=free_list->list; /* Overwrites the size field in union */
        free_list->list=temp;
    } /* end if */

    /* Increment the number of blocks on free list */
    head->onlist++;
    head->list_mem+=free_size;

    /* Increment the amount of "block" freed memory globally */
    H5FL_blk_gc_head.mem_freed+=free_size;

    /* Check for exceeding free list memory use limits */
    /* First check this particular list */
    if(head->list_mem>H5FL_blk_lst_mem_lim)
        H5FL_blk_gc_list(head);

    /* Then check the global amount memory on block free lists */
    if(H5FL_blk_gc_head.mem_freed>H5FL_blk_glb_mem_lim)
        H5FL_blk_gc();

done:
    FUNC_LEAVE_NOAPI(ret_value);
} /* end H5FL_blk_free() */


/*-------------------------------------------------------------------------
 * Function:	H5FL_blk_realloc
 *
 * Purpose:	Resizes a block.  This does things the straightforward, simple way,
 *      not actually using realloc.
 *
 * Return:	Success:	NULL
 *
 *		Failure:	never fails
 *
 * Programmer:	Quincey Koziol
 *		Thursday, March  23, 2000
 *
 * Modifications:
 *
 *-------------------------------------------------------------------------
 */
void *
H5FL_blk_realloc(H5FL_blk_head_t *head, void *block, size_t new_size)
{
    void *ret_value=NULL;       /* Return value */

    FUNC_ENTER_NOAPI(H5FL_blk_realloc, NULL);

    /* Double check parameters */
    assert(head);
    assert(new_size);

    /* Check if we are actually re-allocating a block */
    if(block!=NULL) {
        H5FL_blk_list_t *temp;      /* Temp. ptr to the new block node allocated */

        /* Get the pointer to the chunk info header in front of the chunk to free */
        temp=(H5FL_blk_list_t *)((unsigned char *)block-sizeof(H5FL_blk_list_t));

        /* check if we are actually changing the size of the buffer */
        if(new_size!=temp->size) {
            size_t blk_size;           /* Temporary block size */

            if((ret_value=H5FL_blk_malloc(head,new_size))==NULL)
                HGOTO_ERROR(H5E_RESOURCE, H5E_NOSPACE, NULL, "memory allocation failed for block");
            blk_size=MIN(new_size,temp->size);
            HDmemcpy(ret_value,block,blk_size);
            H5FL_blk_free(head,block);
        } /* end if */
        else
            ret_value=block;
    } /* end if */
    /* Not re-allocating, just allocate a fresh block */
    else
        ret_value=H5FL_blk_malloc(head,new_size);

done:
    FUNC_LEAVE_NOAPI(ret_value);
} /* end H5FL_blk_realloc() */


/*-------------------------------------------------------------------------
 * Function:	H5FL_blk_gc_list
 *
 * Purpose:	Garbage collect a priority queue
 *
 * Return:	Success:	Non-negative
 * 		Failure:	Negative
 *
 * Programmer:	Quincey Koziol
 *              Thursday, March 23, 2000
 *
 * Modifications:
 * 	
 *-------------------------------------------------------------------------
 */
static herr_t
H5FL_blk_gc_list(H5FL_blk_head_t *head)
{
    H5FL_blk_list_t *list; /* The free list of native nodes of a particular size */
    void *next;     /* Temp. ptr to the free list list node */
    void *temp;     /* Temp. ptr to the free list page node */
    
    FUNC_ENTER_NOAPI_NOINIT_NOFUNC(H5FL_blk_gc_list);

    /* Loop through all the nodes in the block free list queue */
    while(head->head!=NULL) {
        temp=head->head->next;

        /* Loop through all the blocks in the free list, freeing them */
        list=head->head->list;
        while(list!=NULL) {
            next=list->next;

            /* Decrement the number of blocks & memory allocated from this PQ */
            head->allocated--;
            head->list_mem-=head->head->size;

            /* Decrement global count of free memory on "block" lists */
            H5FL_blk_gc_head.mem_freed-=head->head->size;

            /* Free the block */
            H5MM_xfree(list);

            list=next;
        } /* end while */

        /* Free the free list node */
        H5FL_FREE(H5FL_blk_node_t,head->head);

        /* Advance to the next free list */
        head->head=temp;
    } /* end while */

    /* Indicate no free nodes on the free list */
    head->head=NULL;
    head->onlist=0;

    /* Double check that all the memory on this list is recycled */
    assert(head->list_mem==0);

    FUNC_LEAVE_NOAPI(SUCCEED);
}   /* end H5FL_blk_gc_list() */


/*-------------------------------------------------------------------------
 * Function:	H5FL_blk_gc
 *
 * Purpose:	Garbage collect on all the priority queues
 *
 * Return:	Success:	Non-negative
 * 		Failure:	Negative
 *
 * Programmer:	Quincey Koziol
 *              Saturday, March 25, 2000
 *
 * Modifications:
 * 	
 *-------------------------------------------------------------------------
 */
static herr_t
H5FL_blk_gc(void)
{
    H5FL_blk_gc_node_t *gc_node;    /* Pointer into the list of things to garbage collect */
    
    FUNC_ENTER_NOAPI_NOINIT_NOFUNC(H5FL_blk_gc);

    /* Walk through all the free lists, free()'ing the nodes */
    gc_node=H5FL_blk_gc_head.first;
    while(gc_node!=NULL) {
        /* For each free list being garbage collected, walk through the nodes and free them */
        H5FL_blk_gc_list(gc_node->pq);

        /* Go on to the next free list to garbage collect */
        gc_node=gc_node->next;
    } /* end while */

    /* Double check that all the memory on the free lists are recycled */
    assert(H5FL_blk_gc_head.mem_freed==0);

    FUNC_LEAVE_NOAPI(SUCCEED);
}   /* end H5FL_blk_gc() */


/*--------------------------------------------------------------------------
 NAME
    H5FL_blk_term
 PURPOSE
    Terminate various H5FL_blk objects
 USAGE
    void H5FL_blk_term()
 RETURNS
    Success:	Positive if any action might have caused a change in some
                other interface; zero otherwise.
   	Failure:	Negative
 DESCRIPTION
    Release any resources allocated.
 GLOBAL VARIABLES
 COMMENTS, BUGS, ASSUMPTIONS
     Can't report errors...
 EXAMPLES
 REVISION LOG
--------------------------------------------------------------------------*/
static int
H5FL_blk_term(void)
{
    H5FL_blk_gc_node_t *left;   /* pointer to garbage collection lists with work left */
    H5FL_blk_gc_node_t *tmp;    /* Temporary pointer to a garbage collection node */

    FUNC_ENTER_NOAPI_NOINIT_NOFUNC(H5FL_blk_term);
    
    /* Free the nodes on the garbage collection list, keeping nodes with allocations outstanding */
    left=NULL;
    while(H5FL_blk_gc_head.first!=NULL) {
        tmp=H5FL_blk_gc_head.first->next;

#ifdef H5FL_DEBUG
printf("H5FL_blk_term: head->name=%s, head->allocated=%d\n", H5FL_blk_gc_head.first->pq->name,(int)H5FL_blk_gc_head.first->pq->allocated);
#endif /* H5FL_DEBUG */

        /* Check if the list has allocations outstanding */
        if(H5FL_blk_gc_head.first->pq->allocated>0) {
            /* Add free list to the list of nodes with allocations open still */
            H5FL_blk_gc_head.first->next=left;
            left=H5FL_blk_gc_head.first;
        } /* end if */
        /* No allocations left open for list, get rid of it */
        else {
            /* Reset the "initialized" flag, in case we restart this list somehow (I don't know how..) */
            H5FL_blk_gc_head.first->pq->init=0;

            /* Free the node from the garbage collection list */
            H5MM_xfree(H5FL_blk_gc_head.first);
        } /* end else */

        H5FL_blk_gc_head.first=tmp;
    } /* end while */

    /* Point to the list of nodes left with allocations open, if any */
    H5FL_blk_gc_head.first=left;
    
    FUNC_LEAVE_NOAPI(H5FL_blk_gc_head.first!=NULL ? 1 : 0);
}   /* end H5FL_blk_term() */


/*-------------------------------------------------------------------------
 * Function:	H5FL_arr_init
 *
 * Purpose:	Initialize a free list for a arrays of certain type.  Right now,
 *      this just adds the free list to the list of things to garbage collect.
 *
 * Return:	Success:	Non-negative
 * 		Failure:	Negative
 *
 * Programmer:	Quincey Koziol
 *              Saturday, March 25, 2000
 *
 * Modifications:
 * 	
 *-------------------------------------------------------------------------
 */
static herr_t
H5FL_arr_init(H5FL_arr_head_t *head)
{
    H5FL_gc_arr_node_t *new_node;   /* Pointer to the node for the new list to garbage collect */
    size_t u;                       /* Local index variable */
    herr_t ret_value=SUCCEED;       /* return value*/

    FUNC_ENTER_NOAPI_NOINIT(H5FL_arr_init);

    /* Allocate a new garbage collection node */
    if (NULL==(new_node = H5MM_malloc(sizeof(H5FL_gc_arr_node_t))))
        HGOTO_ERROR (H5E_RESOURCE, H5E_NOSPACE, FAIL, "memory allocation failed");

    /* Initialize the new garbage collection node */
    new_node->list=head;

    /* Link in to the garbage collection list */
    new_node->next=H5FL_arr_gc_head.first;
    H5FL_arr_gc_head.first=new_node;

    /* Allocate room for the free lists */
    if (NULL==(head->list_arr = H5MM_calloc((size_t)head->maxelem*sizeof(H5FL_arr_node_t))))
        HGOTO_ERROR (H5E_RESOURCE, H5E_NOSPACE, FAIL, "memory allocation failed")

    /* Initialize the size of each array */
    for(u=1; u<(size_t)head->maxelem; u++)
        head->list_arr[u].size=head->size*u;
    
    /* Indicate that the free list is initialized */
    head->init=1;

done:
    FUNC_LEAVE_NOAPI(ret_value);
}   /* end H5FL_arr_init() */


/*-------------------------------------------------------------------------
 * Function:	H5FL_arr_free
 *
 * Purpose:	Release an array of objects & put on free list
 *
 * Return:	Success:	Non-negative
 * 		Failure:	Negative
 *
 * Programmer:	Quincey Koziol
 *              Friday, March 24, 2000
 *
 * Modifications:
 * 	
 *-------------------------------------------------------------------------
 */
void *
H5FL_arr_free(H5FL_arr_head_t *head, void *obj)
{
    H5FL_arr_list_t *temp;  /* Temp. ptr to the new free list node allocated */
    size_t mem_size;        /* Size of memory being freed */
    size_t free_nelem;      /* Number of elements in node being free'd */
    void *ret_value=NULL;   /* Return value */

    FUNC_ENTER_NOAPI(H5FL_arr_free, NULL);

    /* The H5MM_xfree code allows obj to null */
    if (!obj)
        HGOTO_DONE (NULL);

    /* Double check parameters */
    assert(head);

    /* Make certain that the free list is initialized */
    assert(head->init);

    /* Get the pointer to the info header in front of the block to free */
    temp=(H5FL_arr_list_t *)((unsigned char *)obj-sizeof(H5FL_arr_list_t)); /*lint !e826 Pointer-to-pointer cast is appropriate here */

    /* Get the number of elements */
    free_nelem=temp->nelem;

    /* Double-check that there is enough room for arrays of this size */
    assert((int)free_nelem<=head->maxelem);

    /* Link into the free list */
    temp->next=head->list_arr[free_nelem].list;

    /* Point free list at the node freed */
    head->list_arr[free_nelem].list=temp;

    /* Get the size of arrays with this many elements */
    mem_size=head->list_arr[free_nelem].size;

    /* Increment the number of blocks & memory used on free list */
    head->list_arr[free_nelem].onlist++;
    head->list_mem+=mem_size;

    /* Increment the amount of "array" freed memory globally */
    H5FL_arr_gc_head.mem_freed+=mem_size;

    /* Check for exceeding free list memory use limits */
    /* First check this particular list */
    if(head->list_mem>H5FL_arr_lst_mem_lim)
        H5FL_arr_gc_list(head);

    /* Then check the global amount memory on array free lists */
    if(H5FL_arr_gc_head.mem_freed>H5FL_arr_glb_mem_lim)
        H5FL_arr_gc();

done:
    FUNC_LEAVE_NOAPI(ret_value);
}   /* end H5FL_arr_free() */


/*-------------------------------------------------------------------------
 * Function:	H5FL_arr_malloc
 *
 * Purpose:	Allocate an array of objects
 *
 * Return:	Success:	Pointer to a valid array object
 * 		Failure:	NULL
 *
 * Programmer:	Quincey Koziol
 *              Saturday, March 25, 2000
 *
 * Modifications:
 * 	
 *-------------------------------------------------------------------------
 */
void *
H5FL_arr_malloc(H5FL_arr_head_t *head, size_t elem)
{
    H5FL_arr_list_t *new_obj;   /* Pointer to the new free list node allocated */
    void *ret_value;        /* Pointer to object to return */
    size_t mem_size;        /* Size of memory block being recycled */

    FUNC_ENTER_NOAPI(H5FL_arr_malloc, NULL);

    /* Double check parameters */
    assert(head);
    assert(elem);

    /* Make certain the list is initialized first */
    if(!head->init)
        H5FL_arr_init(head);

    /* Get the set of the memory block */
    mem_size=head->list_arr[elem].size;

    /* Sanity check that the number of elements is supported */
    assert((int)elem<=head->maxelem);

    /* Check for nodes available on the free list first */
    if(head->list_arr[elem].list!=NULL) {
        /* Get a pointer to the block on the free list */
        new_obj=head->list_arr[elem].list;

        /* Remove node from free list */
        head->list_arr[elem].list=head->list_arr[elem].list->next;

        /* Decrement the number of blocks & memory used on free list */
        head->list_arr[elem].onlist--;
        head->list_mem-=mem_size;

        /* Decrement the amount of global "array" free list memory in use */
        H5FL_arr_gc_head.mem_freed-=mem_size;

    } /* end if */
    /* Otherwise allocate a node */
    else {
        if (NULL==(new_obj = H5FL_malloc(sizeof(H5FL_arr_list_t)+mem_size)))
            HGOTO_ERROR (H5E_RESOURCE, H5E_NOSPACE, NULL, "memory allocation failed")

        /* Increment the number of blocks allocated in list */
        head->allocated++;
    } /* end else */

    /* Initialize the new object */
    new_obj->nelem=elem;

    /* Get a pointer to the new block */
    ret_value=((char *)new_obj)+sizeof(H5FL_arr_list_t);

done:
    FUNC_LEAVE_NOAPI(ret_value);
}   /* end H5FL_arr_malloc() */


/*-------------------------------------------------------------------------
 * Function:	H5FL_arr_calloc
 *
 * Purpose:	Allocate an array of objects and clear it to zeros
 *
 * Return:	Success:	Pointer to a valid array object
 * 		Failure:	NULL
 *
 * Programmer:	Quincey Koziol
 *              Monday, December 23, 2002
 *
 * Modifications:
 * 	
 *-------------------------------------------------------------------------
 */
void *
H5FL_arr_calloc(H5FL_arr_head_t *head, size_t elem)
{
    void *ret_value;        /* Pointer to object to return */

    FUNC_ENTER_NOAPI(H5FL_arr_calloc, NULL);

    /* Double check parameters */
    assert(head);
    assert(elem);

    /* Allocate the array */
    if (NULL==(ret_value = H5FL_arr_malloc(head,elem)))
        HGOTO_ERROR (H5E_RESOURCE, H5E_NOSPACE, NULL, "memory allocation failed");

    /* Clear to zeros */
    HDmemset(ret_value,0,head->list_arr[elem].size);

done:
    FUNC_LEAVE_NOAPI(ret_value);
}   /* end H5FL_arr_calloc() */


/*-------------------------------------------------------------------------
 * Function:	H5FL_arr_realloc
 *
 * Purpose:	Reallocate an array of objects
 *
 * Return:	Success:	Pointer to a valid array object
 * 		Failure:	NULL
 *
 * Programmer:	Quincey Koziol
 *              Saturday, March 25, 2000
 *
 * Modifications:
 * 	
 *-------------------------------------------------------------------------
 */
void *
H5FL_arr_realloc(H5FL_arr_head_t *head, void * obj, size_t new_elem)
{
    void *ret_value;        /* Pointer to object to return */

    FUNC_ENTER_NOAPI(H5FL_arr_realloc, NULL);

    /* Double check parameters */
    assert(head);
    assert(new_elem);

    /* Check if we are really allocating the object */
    if(obj==NULL)
        ret_value=H5FL_arr_malloc(head,new_elem);
    else {
        H5FL_arr_list_t *temp;  /* Temp. ptr to the new free list node allocated */

        /* Sanity check that the number of elements is supported */
        assert((int)new_elem<=head->maxelem);

        /* Get the pointer to the info header in front of the block to free */
        temp=(H5FL_arr_list_t *)((unsigned char *)obj-sizeof(H5FL_arr_list_t)); /*lint !e826 Pointer-to-pointer cast is appropriate here */

        /* Check if the size is really changing */
        if(temp->nelem!=new_elem) {
            size_t blk_size;       /* Size of block */

            /* Get the new array of objects */
            ret_value=H5FL_arr_malloc(head,new_elem);

            /* Copy the appropriate amount of elements */
            blk_size=head->size*MIN(temp->nelem,new_elem);
            HDmemcpy(ret_value,obj,blk_size);

            /* Free the old block */
            H5FL_arr_free(head,obj);
        } /* end if */
        else
            ret_value=obj;
    } /* end else */

done:
    FUNC_LEAVE_NOAPI(ret_value);
}   /* end H5FL_arr_realloc() */


/*-------------------------------------------------------------------------
 * Function:	H5FL_arr_gc_list
 *
 * Purpose:	Garbage collect on an array object free list
 *
 * Return:	Success:	Non-negative
 * 		Failure:	Negative
 *
 * Programmer:	Quincey Koziol
 *              Tuesday, July 25, 2000
 *
 * Modifications:
 * 	
 *-------------------------------------------------------------------------
 */
static herr_t
H5FL_arr_gc_list(H5FL_arr_head_t *head)
{
    H5FL_arr_list_t *arr_free_list; /* Pointer to nodes in free list being garbage collected */
    void *tmp;      /* Temporary node pointer */
    unsigned u;     /* Counter for array of free lists */
    size_t total_mem;   /* Total memory used on list */
    
    FUNC_ENTER_NOAPI_NOINIT_NOFUNC(H5FL_arr_gc_list);

    /* Walk through the array of free lists */
    for(u=0; u<(unsigned)head->maxelem; u++) {
        if(head->list_arr[u].onlist>0) {
            /* Calculate the total memory used on this list */
            total_mem=head->list_arr[u].onlist*head->list_arr[u].size;

            /* For each free list being garbage collected, walk through the nodes and free them */
            arr_free_list=head->list_arr[u].list;
            while(arr_free_list!=NULL) {
                tmp=arr_free_list->next;

                /* Decrement the count of nodes allocated and free the node */
                head->allocated--;
                H5MM_xfree(arr_free_list);

                arr_free_list=tmp;
            } /* end while */

            /* Indicate no free nodes on the free list */
            head->list_arr[u].list=NULL;
            head->list_arr[u].onlist=0;

            /* Decrement count of free memory on this "array" list */
            head->list_mem-=total_mem;

            /* Decrement global count of free memory on "array" lists */
            H5FL_arr_gc_head.mem_freed-=total_mem;
        } /* end if */
    } /* end for */

    /* Double check that all the memory on this list is recycled */
    assert(head->list_mem==0);

    FUNC_LEAVE_NOAPI(SUCCEED);
}   /* end H5FL_arr_gc_list() */


/*-------------------------------------------------------------------------
 * Function:	H5FL_arr_gc
 *
 * Purpose:	Garbage collect on all the array object free lists
 *
 * Return:	Success:	Non-negative
 * 		Failure:	Negative
 *
 * Programmer:	Quincey Koziol
 *              Saturday, March 25, 2000
 *
 * Modifications:
 * 	
 *-------------------------------------------------------------------------
 */
static herr_t
H5FL_arr_gc(void)
{
    H5FL_gc_arr_node_t *gc_arr_node;    /* Pointer into the list of things to garbage collect */
    
    FUNC_ENTER_NOAPI_NOINIT_NOFUNC(H5FL_arr_gc);

    /* Walk through all the free lists, free()'ing the nodes */
    gc_arr_node=H5FL_arr_gc_head.first;
    while(gc_arr_node!=NULL) {
        /* Release the free nodes on the list */
        H5FL_arr_gc_list(gc_arr_node->list);

        /* Go on to the next free list to garbage collect */
        gc_arr_node=gc_arr_node->next;
    } /* end while */

    /* Double check that all the memory on the free lists are recycled */
    assert(H5FL_arr_gc_head.mem_freed==0);

    FUNC_LEAVE_NOAPI(SUCCEED);
}   /* end H5FL_arr_gc() */


/*--------------------------------------------------------------------------
 NAME
    H5FL_arr_term
 PURPOSE
    Terminate various H5FL array object free lists
 USAGE
    int H5FL_arr_term()
 RETURNS
    Success:	Positive if any action might have caused a change in some
                other interface; zero otherwise.
   	Failure:	Negative
 DESCRIPTION
    Release any resources allocated.
 GLOBAL VARIABLES
 COMMENTS, BUGS, ASSUMPTIONS
     Can't report errors...
 EXAMPLES
 REVISION LOG
--------------------------------------------------------------------------*/
static int
H5FL_arr_term(void)
{
    H5FL_gc_arr_node_t *left;   /* pointer to garbage collection lists with work left */
    H5FL_gc_arr_node_t *tmp;    /* Temporary pointer to a garbage collection node */
    
    FUNC_ENTER_NOAPI_NOINIT_NOFUNC(H5FL_arr_term);

    /* Free the nodes on the garbage collection list, keeping nodes with allocations outstanding */
    left=NULL;
    while(H5FL_arr_gc_head.first!=NULL) {
        tmp=H5FL_arr_gc_head.first->next;

        /* Check if the list has allocations outstanding */
#ifdef H5FL_DEBUG
printf("H5FL_arr_term: head->name=%s, head->allocated=%d\n", H5FL_arr_gc_head.first->list->name,(int)H5FL_arr_gc_head.first->list->allocated);
#endif /* H5FL_DEBUG */
        if(H5FL_arr_gc_head.first->list->allocated>0) {
            /* Add free list to the list of nodes with allocations open still */
            H5FL_arr_gc_head.first->next=left;
            left=H5FL_arr_gc_head.first;
        } /* end if */
        /* No allocations left open for list, get rid of it */
        else {
            /* Free the array of free lists */
            H5MM_xfree(H5FL_arr_gc_head.first->list->list_arr);

            /* Reset the "initialized" flag, in case we restart this list somehow (I don't know how..) */
            H5FL_arr_gc_head.first->list->init=0;

            /* Free the node from the garbage collection list */
            H5MM_xfree(H5FL_arr_gc_head.first);
        } /* end else */

        H5FL_arr_gc_head.first=tmp;
    } /* end while */

    /* Point to the list of nodes left with allocations open, if any */
    H5FL_arr_gc_head.first=left;
    
    FUNC_LEAVE_NOAPI(H5FL_arr_gc_head.first!=NULL ? 1 : 0);
}   /* end H5FL_arr_term() */


/*-------------------------------------------------------------------------
 * Function:	H5FL_seq_free
 *
 * Purpose:	Release a sequence of objects & put on free list
 *
 * Return:	Success:	Non-negative
 * 		Failure:	Negative
 *
 * Programmer:	Quincey Koziol
 *              Saturday, April 3, 2004
 *
 * Modifications:
 * 	
 *-------------------------------------------------------------------------
 */
void *
H5FL_seq_free(H5FL_seq_head_t *head, void *obj)
{
    void *ret_value=NULL;   /* Return value */

    FUNC_ENTER_NOAPI(H5FL_seq_free, NULL)

    /* The H5MM_xfree code allows obj to null */
    if (!obj)
        HGOTO_DONE (NULL)

    /* Double check parameters */
    assert(head);

    /* Make certain that the free list is initialized */
    assert(head->queue.init);

    /* Use block routine */
    H5FL_blk_free(&(head->queue),obj);

done:
    FUNC_LEAVE_NOAPI(ret_value)
}   /* end H5FL_seq_free() */


/*-------------------------------------------------------------------------
 * Function:	H5FL_seq_malloc
 *
 * Purpose:	Allocate a sequence of objects
 *
 * Return:	Success:	Pointer to a valid sequence object
 * 		Failure:	NULL
 *
 * Programmer:	Quincey Koziol
 *              Saturday, April 3, 2004
 *
 * Modifications:
 * 	
 *-------------------------------------------------------------------------
 */
void *
H5FL_seq_malloc(H5FL_seq_head_t *head, size_t elem)
{
    void *ret_value;        /* Pointer to object to return */

    FUNC_ENTER_NOAPI(H5FL_seq_malloc, NULL)

    /* Double check parameters */
    assert(head);
    assert(elem);

    /* Use block routine */
    ret_value=H5FL_blk_malloc(&(head->queue),head->size*elem);

done:
    FUNC_LEAVE_NOAPI(ret_value)
}   /* end H5FL_seq_malloc() */


/*-------------------------------------------------------------------------
 * Function:	H5FL_seq_calloc
 *
 * Purpose:	Allocate a sequence of objects and clear it to zeros
 *
 * Return:	Success:	Pointer to a valid array object
 * 		Failure:	NULL
 *
 * Programmer:	Quincey Koziol
 *              Saturday, April 3, 2004
 *
 * Modifications:
 * 	
 *-------------------------------------------------------------------------
 */
void *
H5FL_seq_calloc(H5FL_seq_head_t *head, size_t elem)
{
    void *ret_value;        /* Pointer to object to return */

    FUNC_ENTER_NOAPI(H5FL_seq_calloc, NULL)

    /* Double check parameters */
    assert(head);
    assert(elem);

    /* Use block routine */
    ret_value=H5FL_blk_calloc(&(head->queue),head->size*elem);

done:
    FUNC_LEAVE_NOAPI(ret_value)
}   /* end H5FL_seq_calloc() */


/*-------------------------------------------------------------------------
 * Function:	H5FL_seq_realloc
 *
 * Purpose:	Reallocate a sequence of objects
 *
 * Return:	Success:	Pointer to a valid sequence object
 * 		Failure:	NULL
 *
 * Programmer:	Quincey Koziol
 *              Saturday, April 3, 2004
 *
 * Modifications:
 * 	
 *-------------------------------------------------------------------------
 */
void *
H5FL_seq_realloc(H5FL_seq_head_t *head, void * obj, size_t new_elem)
{
    void *ret_value;        /* Pointer to object to return */

    FUNC_ENTER_NOAPI(H5FL_seq_realloc, NULL)

    /* Double check parameters */
    assert(head);
    assert(new_elem);

    /* Use block routine */
    ret_value=H5FL_blk_realloc(&(head->queue),obj,head->size*new_elem);

done:
    FUNC_LEAVE_NOAPI(ret_value)
}   /* end H5FL_seq_realloc() */


/*-------------------------------------------------------------------------
 * Function:	H5FL_garbage_coll
 *
 * Purpose:	Garbage collect on all the free lists
 *
 * Return:	Success:	Non-negative
 * 		Failure:	Negative
 *
 * Programmer:	Quincey Koziol
 *              Friday, March 24, 2000
 *
 * Modifications:
 * 	
 *-------------------------------------------------------------------------
 */
herr_t
H5FL_garbage_coll(void)
{
    FUNC_ENTER_NOAPI_NOINIT_NOFUNC(H5FL_garbage_coll);

    /* Garbage collect the free lists for array objects */
    H5FL_arr_gc();

    /* Garbage collect free lists for blocks */
    H5FL_blk_gc();

    /* Garbage collect the free lists for regular objects */
    H5FL_reg_gc();

    FUNC_LEAVE_NOAPI(SUCCEED);
}   /* end H5FL_garbage_coll() */


/*-------------------------------------------------------------------------
 * Function:	H5FL_set_free_list_limits
 *
 * Purpose:	Sets limits on the different kinds of free lists.  Setting a value
 *      of -1 for a limit means no limit of that type.  These limits are global
 *      for the entire library.  Each "global" limit only applies to free lists
 *      of that type, so if an application sets a limit of 1 MB on each of the
 *      global lists, up to 3 MB of total storage might be allocated (1MB on
 *      each of regular, array and block type lists).
 *
 * Parameters:
 *  int reg_global_lim;  IN: The limit on all "regular" free list memory used
 *  int reg_list_lim;    IN: The limit on memory used in each "regular" free list
 *  int arr_global_lim;  IN: The limit on all "array" free list memory used
 *  int arr_list_lim;    IN: The limit on memory used in each "array" free list
 *  int blk_global_lim;  IN: The limit on all "block" free list memory used
 *  int blk_list_lim;    IN: The limit on memory used in each "block" free list
 *
 * Return:	Success:	non-negative
 *
 *		Failure:	negative
 *
 * Programmer:	Quincey Koziol
 *              Wednesday, August 2, 2000
 *
 * Modifications:
 *
 *-------------------------------------------------------------------------
 */
herr_t 
H5FL_set_free_list_limits(int reg_global_lim, int reg_list_lim, int arr_global_lim,
    int arr_list_lim, int blk_global_lim, int blk_list_lim)
{
    herr_t                  ret_value = SUCCEED;

    FUNC_ENTER_NOAPI(H5FL_set_free_list_limits, FAIL);

    /* Set the limit variables */
    /* limit on all regular free lists */
    H5FL_reg_glb_mem_lim=(reg_global_lim==-1 ? UINT_MAX : (size_t)reg_global_lim);
    /* limit on each regular free list */
    H5FL_reg_lst_mem_lim=(reg_list_lim==-1 ? UINT_MAX : (size_t)reg_list_lim);
    /* limit on all array free lists */
    H5FL_arr_glb_mem_lim=(arr_global_lim==-1 ? UINT_MAX : (size_t)arr_global_lim);
    /* limit on each array free list */
    H5FL_arr_lst_mem_lim=(arr_list_lim==-1 ? UINT_MAX : (size_t)arr_list_lim);
    /* limit on all block free lists */
    H5FL_blk_glb_mem_lim=(blk_global_lim==-1 ? UINT_MAX : (size_t)blk_global_lim);
    /* limit on each block free list */
    H5FL_blk_lst_mem_lim=(blk_list_lim==-1 ? UINT_MAX : (size_t)blk_list_lim);

done:
    FUNC_LEAVE_NOAPI(ret_value);
}   /* end H5FL_set_free_list_limits() */


/*--------------------------------------------------------------------------
 NAME
    H5FL_term_interface
 PURPOSE
    Terminate various H5FL objects
 USAGE
    void H5FL_term_interface()
 RETURNS
    Success:	Positive if any action might have caused a change in some
                other interface; zero otherwise.
   	Failure:	Negative
 DESCRIPTION
    Release any resources allocated.
 GLOBAL VARIABLES
 COMMENTS, BUGS, ASSUMPTIONS
     Can't report errors...
 EXAMPLES
 REVISION LOG
--------------------------------------------------------------------------*/
int
H5FL_term_interface(void)
{
    int ret_value=0;
    
    FUNC_ENTER_NOAPI_NOINIT_NOFUNC(H5FL_term_interface);

    /* Garbage collect any nodes on the free lists */
    H5FL_garbage_coll();

    ret_value=H5FL_reg_term()+H5FL_arr_term()+H5FL_blk_term();

    FUNC_LEAVE_NOAPI(ret_value);
}
