/***
*sbheap.c -  Small-block heap code
*
*       Copyright (c) 1996-1997, Microsoft Corporation. All rights reserved.
*
*Purpose:
*       Core code for small-block heap.
*
*******************************************************************************/

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <winheap.h>
#include <windows.h>


__sbh_region_t __small_block_heap = {
        &__small_block_heap,                      /* p_next_region */
        &__small_block_heap,                      /* p_prev_region */
        &__small_block_heap.region_map[0],        /* p_starting_region_map */
        &__small_block_heap.region_map[0],        /* p_first_uncommitted */
        (__sbh_page_t *)_NO_PAGES,                /* p_pages_begin */
        (__sbh_page_t *)_NO_PAGES,                /* p_pages_end */
        { _PARAS_PER_PAGE, _NO_FAILED_ALLOC }     /* region_map[] */
};

static __sbh_region_t *__sbh_p_starting_region = &__small_block_heap;

static int __sbh_decommitable_pages = 0;

size_t __sbh_threshold = _PARASIZE * (_PARAS_PER_PAGE / 8);

/*
 * Prototypes for user functions.
 */
size_t __cdecl _get_sbh_threshold(void);
int    __cdecl _set_sbh_threshold(size_t);


/***
*size_t _get_sbh_threshold() - return small-block threshold
*
*Purpose:
*       Return the current value of __sbh_threshold
*
*Entry:
*       None.
*
*Exit:
*       See above.
*
*Exceptions:
*
*******************************************************************************/

size_t __cdecl _get_sbh_threshold (
        void
        )
{
        return __sbh_threshold;
}


/***
*int _set_sbh_threshold(size_t threshold) - set small-block heap threshold
*
*Purpose:
*       Set the upper limit for the size of an allocation which will be
*       supported from the small-block heap. It is required that at least two
*       allocations can come from a page. This imposes an upper limit on how
*       big the new threshold can  be.
*
*Entry:
*       size_t  threshold   - proposed new value for __sbh_theshold
*
*Exit:
*       Returns 1 if successful. Returns 0 if threshold was too big.
*
*Exceptions:
*
*******************************************************************************/

int __cdecl _set_sbh_threshold (
        size_t threshold
        )
{
        /*
         * Round up the proposed new value to the nearest paragraph
         */
        threshold = (threshold + _PARASIZE - 1) & ~(_PARASIZE - 1);

        /*
         * Require that at least two allocations be can be made within a
         * page.
         */
        if ( threshold <= (_PARASIZE * (_PARAS_PER_PAGE / 2)) ) {
            __sbh_threshold = threshold;
            return 1;
        }
        else
            return 0;
}


/***
*__sbh_region_t * __sbh_new_region() - get a region for the small-block heap
*
*Purpose:
*       Creates and adds a new region for the small-block heap. First, a
*       descriptor (__sbh_region_t) is obtained for the new region. Next,
*       VirtualAlloc() is used to reserved an address space of size
*       _PAGES_PER_REGION * _PAGESIZE_, and the first _PAGES_PER_COMMITTMENT
*       pages are committed.
*
*       Note that if __small_block_heap is available (i.e., the p_pages_begin
*       field is _NO_PAGES), it becomes the descriptor for the new regions. This is
*       basically the small-block heap initialization.
*
*Entry:
*       No arguments.
*
*Exit:
*       If successful, a pointer to the descriptor for the new region is
*       returned. Otherwise, NULL is returned.
*
*******************************************************************************/

__sbh_region_t * __cdecl __sbh_new_region(
                         void
        )
{
        __sbh_region_t * pregnew;
        __sbh_page_t *   ppage;
        int              i;

        /*
         * Get a region descriptor (__sbh_region_t). If __small_block_heap is
         * available, always use it.
         */
        if ( __small_block_heap.p_pages_begin == _NO_PAGES ) {
            pregnew = &__small_block_heap;
        }
        else {
            /*
             * Allocate space for the new __sbh_region_t structure. Note that
             * this allocation comes out of the 'big block heap.
             */
            if ( (pregnew = HeapAlloc( _crtheap, 0, sizeof(__sbh_region_t) ))
                 == NULL )
                return NULL;
        }

        /*
         * Reserve a new contiguous address range (i.e., a region).
         */
        if ( (ppage = VirtualAlloc( NULL,
                                    _PAGESIZE_ * _PAGES_PER_REGION,
                                    MEM_RESERVE,
                                    PAGE_READWRITE )) != NULL )
        {
            /*
             * Commit the first _PAGES_PER_COMMITMENT of the new region.
             */
            if ( VirtualAlloc( ppage,
                               _PAGESIZE_ * _PAGES_PER_COMMITMENT,
                               MEM_COMMIT,
                               PAGE_READWRITE ) != NULL )
            {
                /*
                 * Insert *pregnew into the linked list of regions (just
                 * before __small_block_heap)
                 */
                if ( pregnew == &__small_block_heap ) {
                    if ( __small_block_heap.p_next_region == NULL )
                        __small_block_heap.p_next_region =
                            &__small_block_heap;
                    if ( __small_block_heap.p_prev_region == NULL )
                        __small_block_heap.p_prev_region =
                            &__small_block_heap;
                }
                else {
                    pregnew->p_next_region = &__small_block_heap;
                    pregnew->p_prev_region = __small_block_heap.p_prev_region;
                    __small_block_heap.p_prev_region = pregnew;
                    pregnew->p_prev_region->p_next_region = pregnew;
                }

                /*
                 * Fill in the rest of *pregnew
                 */
                pregnew->p_pages_begin = ppage;
                pregnew->p_pages_end = ppage + _PAGES_PER_REGION;
                pregnew->p_starting_region_map = &(pregnew->region_map[0]);
                pregnew->p_first_uncommitted =
                    &(pregnew->region_map[_PAGES_PER_COMMITMENT]);

                /*
                 * Initialize pregnew->region_map[].
                 */
                for ( i = 0 ; i < _PAGES_PER_REGION ; i++ ) {

                    if ( i < _PAGES_PER_COMMITMENT )
                        pregnew->region_map[i].free_paras_in_page =
                            _PARAS_PER_PAGE;
                    else
                        pregnew->region_map[i].free_paras_in_page =
                            _UNCOMMITTED_PAGE;

                    pregnew->region_map[i].last_failed_alloc =
                        _NO_FAILED_ALLOC;
                }

                /*
                 * Initialize pages
                 */
                memset( ppage, 0, _PAGESIZE_ * _PAGES_PER_COMMITMENT );
                while ( ppage < pregnew->p_pages_begin +
                        _PAGES_PER_COMMITMENT )
                {
                    ppage->p_starting_alloc_map = &(ppage->alloc_map[0]);
                    ppage->free_paras_at_start = _PARAS_PER_PAGE;
                    (ppage++)->alloc_map[_PARAS_PER_PAGE] = (__page_map_t)-1;
                }

                /*
                 * Return success
                 */
                return pregnew;
            }
            else {
                /*
                 * Couldn't commit the pages. Release the address space .
                 */
                VirtualFree( ppage, 0, MEM_RELEASE );
            }
        }

        /*
         * Unable to create the new region. Free the region descriptor, if necessary.
         */
        if ( pregnew != &__small_block_heap )
            HeapFree(_crtheap, 0, pregnew);

        /*
         * Return failure.
         */
        return NULL;
}


/***
*void __sbh_release_region(preg) - release region
*
*Purpose:
*       Release the address space associated with the specified region
*       descriptor. Also, free the specified region descriptor and update
*       the linked list of region descriptors if appropriate.
*
*Entry:
*       __sbh_region_t *    preg    - pointer to descriptor for the region to
*                                     be released.
*
*Exit:
*       No return value.
*
*Exceptions:
*
*******************************************************************************/

void __cdecl __sbh_release_region(
        __sbh_region_t * preg
        )
{
        /*
         * Release the passed region
         */
        VirtualFree( preg->p_pages_begin, 0, MEM_RELEASE);

        /*
         * Update __sbh_p_starting_region, if necessary
         */
        if ( __sbh_p_starting_region == preg )
            __sbh_p_starting_region = preg->p_prev_region;

        if ( preg != &__small_block_heap ) {
            /*
             * Update linked list of region descriptors.
             */
            preg->p_prev_region->p_next_region = preg->p_next_region;
            preg->p_next_region->p_prev_region = preg->p_prev_region;

            /*
             * Free the region desciptor
             */
            HeapFree(_crtheap, 0, preg);
        }
        else {
            /*
             * Mark p_pages_begin as _NO_PAGES to indicate __small_block_heap
             * is not associated with any region (and can be reused). This the
             * only region descriptor for which this is supported.
             */
            __small_block_heap.p_pages_begin = _NO_PAGES;
        }
}


/***
*void __sbh_decommit_pages(count) - decommit specified number of pages
*
*Purpose:
*       Decommit count pages, if possible, in reverse (i.e., last to
*       first) order. If this results in all the pages in any region being
*       uncommitted, the region is released.
*
*Entry:
*       int count   -  number of pages to decommit
*
*Exit:
*       No return value.
*
*Exceptions:
*
*******************************************************************************/

void __cdecl __sbh_decommit_pages(
        int              count
        )
{
        __sbh_region_t * preg1;
        __sbh_region_t * preg2;
        __region_map_t * pregmap;
        int              page_decommitted_flag;
        int              i;

        /*
         * Scan the regions of the small-block heap, in reverse order. looking
         * for pages which can be decommitted.
         */
        preg1 = __small_block_heap.p_prev_region;
        do {
            if ( preg1->p_pages_begin != _NO_PAGES ) {
                /*
                 * Scan the pages in *preg1, in reverse order, looking for
                 * pages which can be decommitted.
                 */
                for ( i = _PAGES_PER_REGION - 1, page_decommitted_flag = 0,
                        pregmap = &(preg1->region_map[i]) ;
                      i >= 0 ; i--, pregmap-- )
                {
                    /*
                     * Check if the pool page is unused and, if so, decommit it.
                     */
                    if ( pregmap->free_paras_in_page == _PARAS_PER_PAGE ) {
                        if ( VirtualFree((preg1->p_pages_begin) + i, _PAGESIZE_,
                                         MEM_DECOMMIT) )
                        {
                            /*
                             * Mark the page as uncommitted, update the count
                             * (global) decommitable pages, update the
                             * first_uncommitted_index field of the region
                             * descriptor, set the flag indicating at least
                             * one page has been decommitted in the region,
                             * and decrement count.
                             */
                            pregmap->free_paras_in_page = _UNCOMMITTED_PAGE;

                            __sbh_decommitable_pages--;

                            if ( (preg1->p_first_uncommitted == NULL)
                                 || (preg1->p_first_uncommitted > pregmap) )
                                preg1->p_first_uncommitted = pregmap;

                            page_decommitted_flag++;
                            if ( --count == 0 )
                                break;
                        }
                    }
                }

                /*
                 * 'Decrement' the preg1 pointer, but save a copy in preg2 in
                 * case the region needs to be released.
                 */
                preg2 = preg1;
                preg1 = preg1->p_prev_region;

                /*
                 * If appropriate, determine if all the pages in the region
                 * are uncommitted so that the region can be released.
                 */
                if ( page_decommitted_flag &&
                    (preg2->region_map[0].free_paras_in_page ==
                    _UNCOMMITTED_PAGE) )
                {

                    for ( i = 1, pregmap = &(preg2->region_map[1]) ;
                          (i < _PAGES_PER_REGION) &&
                            (pregmap->free_paras_in_page ==
                            _UNCOMMITTED_PAGE) ;
                          i++, pregmap++ );

                    if ( i == _PAGES_PER_REGION )
                        __sbh_release_region(preg2);
                }
            }
        }
        while ( (preg1 != __small_block_heap.p_prev_region) && (count > 0) );
}


/***
*__page_map_t *__sbh_find_block(pblck, ppreg, pppage) - find block in
*       small-block heap
*
*Purpose:
*       Determine if the specified allocation block lies in the small-block
*       heap and, if so, return the region, page and starting paragraph index
*       of the block.
*
*Entry:
*       void *              pblck   - pointer to block to be freed
*       __sbh_region_t **   ppreg   - pointer to a pointer to the region
*                                     holding *pblck, if found
*       __sbh_page_t **     pppage  - pointer to a pointer to the page holding
*                                     *pblck, if found
*
*Exit:
*       If successful, a pointer to the starting alloc_map[] entry for the
*       allocation block is returned.
*       If unsuccessful, NULL is returned.
*
*Exceptions:
*
*******************************************************************************/

__page_map_t * __cdecl __sbh_find_block (
        void *            pblck,
        __sbh_region_t ** ppreg,
        __sbh_page_t   ** pppage
        )
{
        __sbh_region_t *  preg;
        __sbh_page_t *    ppage;

        preg = &__small_block_heap;
        do
        {
            /*
             * Does the block lie within this small heap region?
             */
            if ( (pblck > (void *)preg->p_pages_begin) &&
                 (pblck < (void *)preg->p_pages_end) )
            {
                /*
                 * pblck lies within the region! Carry out a couple of
                 * important validity checks.
                 */
                if ( (((unsigned)pblck & (_PARASIZE - 1)) == 0) &&
                     (((unsigned)pblck & (_PAGESIZE_ - 1)) >=
                        offsetof(struct __sbh_page_struct, alloc_blocks[0])) )
                {
                    /*
                     * Copy region and page pointers back through the passed
                     * pointers.
                     */
                    *ppreg = preg;
                    *pppage = ppage = (__sbh_page_t *)((unsigned)pblck &
                                      ~(_PAGESIZE_ - 1));

                    /*
                     * Return pointer to the alloc_map[] entry of the block.
                     */
                    return ( &(ppage->alloc_map[0]) + ((__para_t *)pblck -
                                &(ppage->alloc_blocks[0])) );
                }

                return NULL;
            }
        }
        while ( (preg = preg->p_next_region) != &__small_block_heap );

        return NULL;
}


/***
*void __sbh_free_block(preg, ppage, pmap) - free block
*
*Purpose:
*       Free the specified block from the small-block heap.
*
*Entry:
*       __sbh_region_t *preg        - pointer to the descriptor for the
*                                     region containing the block
*       __sbh_page_t *  ppage       - pointer to the page containing the
*                                     block
*       __page_map_t *  pmap        - pointer to the initial alloc_map[]
*                                     entry for the allocation block
*
*Exit:
*       No return value.
*
*Exceptions:
*
*******************************************************************************/

void __cdecl __sbh_free_block (
        __sbh_region_t * preg,
        __sbh_page_t *   ppage,
        __page_map_t *   pmap
        )
{
        __region_map_t * pregmap;

        pregmap = &(preg->region_map[0]) + (ppage - preg->p_pages_begin);

        /*
         * Update the region_map[] entry.
         */
        pregmap->free_paras_in_page += (int)*pmap;

        /*
         * Mark the alloc_map[] entry as free
         */
        *pmap = _FREE_PARA;

        /*
         * Clear the last_failed_alloc[] entry for the page.
         */
        pregmap->last_failed_alloc = _NO_FAILED_ALLOC;

        /*
         * Check if the count of decommitable pages needs to be updated, and
         * if some pages need to be decommited.
         */
        if ( pregmap->free_paras_in_page == _PARAS_PER_PAGE )
            if ( ++__sbh_decommitable_pages == (2 * _PAGES_PER_COMMITMENT) )
                __sbh_decommit_pages(_PAGES_PER_COMMITMENT);
}


/***
*void * __sbh_alloc_block(para_req) - allocate a block
*
*Purpose:
*       Allocate a block from the small-block heap, the specified number of
*       paragraphs in size.
*
*Entry:
*       size_t  para_req    - size of the allocation request in paragraphs.
*
*Exit:
*       Returns a pointer to the newly allocated block, if successful.
*       Returns NULL, if failure.
*
*Exceptions:
*
*******************************************************************************/

void * __cdecl __sbh_alloc_block (
        size_t          para_req
        )
{
        __sbh_region_t * preg;
        __sbh_page_t *   ppage;
        __sbh_page_t *   ppage2;
        __region_map_t * pregmap;
        __region_map_t * pregmap2;
        void *           retp;
        int              i, j;

        /*
         * First pass through the small-block heap. Try to satisfy the current
         * request from already committed pages.
         */
        preg = __sbh_p_starting_region;

        do {
            if ( preg->p_pages_begin != _NO_PAGES ) {
                /*
                 * Search from *p_starting_region_map to the end of the
                 * region_map[] array.
                 */
                for ( pregmap = preg->p_starting_region_map,
                        pregmap2 = &(preg->region_map[_PAGES_PER_REGION]),
                        ppage = preg->p_pages_begin +
                                (int)(pregmap - &(preg->region_map[0])) ;
                      pregmap < pregmap2 ;
                      pregmap++, ppage++ )
                {
                    /*
                     * If the page has at least para_req free paragraphs, try
                     * to satisfy the request in this page.
                     */
                    if ( (pregmap->free_paras_in_page >= (int)para_req) &&
                         (pregmap->last_failed_alloc > para_req) )
                    {
                        if ( (retp = __sbh_alloc_block_from_page(
                                        ppage,
                                        pregmap->free_paras_in_page,
                                        para_req)) != NULL )
                        {
                            /*
                             * Success.
                             *  Update __sbh_p_starting_region.
                             *  Update free_paras_in_page field for the page.
                             *  Update the p_starting_region_map field in the
                             *  region.
                             *  Return a pointer to the allocated block.
                             */
                            __sbh_p_starting_region = preg;
                            pregmap->free_paras_in_page -= para_req;
                            preg->p_starting_region_map = pregmap;
                            return retp;
                        }
                        else {
                            /*
                             * Update last_failed_alloc field.
                             */
                            pregmap->last_failed_alloc = para_req;
                        }
                    }
                }

                /*
                 * If necessary, search from 0 page to search_start_index.
                 */
                for ( pregmap = &(preg->region_map[0]),
                        pregmap2 = preg->p_starting_region_map,
                        ppage = preg->p_pages_begin ;
                      pregmap < pregmap2 ;
                      pregmap++, ppage++ )
                {
                    /*
                     * If the page has at least para_req free paragraphs, try
                     * to satisfy the request in this page.
                     */
                    if ( (pregmap->free_paras_in_page >= (int)para_req) &&
                         (pregmap->last_failed_alloc > para_req) )
                    {
                        if ( (retp = __sbh_alloc_block_from_page(
                                        ppage,
                                        pregmap->free_paras_in_page,
                                        para_req)) != NULL )
                        {
                            /*
                             * Success.
                             *  Update __sbh_p_starting_region.
                             *  Update free_paras_in_page field for the page.
                             *  Update the p_starting_region_map field in the
                             *  region.
                             *  Return a pointer to the allocated block.
                             */
                            __sbh_p_starting_region = preg;
                            pregmap->free_paras_in_page -= para_req;
                            preg->p_starting_region_map = pregmap;
                            return retp;
                        }
                        else {
                            /*
                             * Update last_failed_alloc field.
                             */
                            pregmap->last_failed_alloc = para_req;
                        }
                    }
                }
            }
        }
        while ( (preg = preg->p_next_region) != __sbh_p_starting_region );

        /*
         * Second pass through the small-block heap. This time, look for an
         * uncommitted page. Also, start at __small_block_heap rather than at
         * *__sbh_p_starting_region.
         */
        preg = &__small_block_heap;

        do
        {
            if ( (preg->p_pages_begin != _NO_PAGES) &&
                 (preg->p_first_uncommitted != NULL) )
            {
                pregmap = preg->p_first_uncommitted;

                ppage = preg->p_pages_begin +
                        (pregmap - &(preg->region_map[0]));

                /*
                 * Determine how many adjacent pages, up to
                 * _PAGES_PER_COMMITMENT, are uncommitted (and can now be
                 * committed)
                 */
                for ( i = 0, pregmap2 = pregmap ;
                      (pregmap2->free_paras_in_page == _UNCOMMITTED_PAGE) &&
                        (i < _PAGES_PER_COMMITMENT) ;
                      pregmap2++, i++ ) ;

                /*
                 * Commit the pages.
                 */
                if ( VirtualAlloc( (void *)ppage,
                                   i * _PAGESIZE_,
                                   MEM_COMMIT,
                                   PAGE_READWRITE ) == ppage )
                {
                    /*
                     * Initialize the committed pages.
                     */
                    memset(ppage, i * _PAGESIZE_, 0);

                    for ( j = 0, ppage2 = ppage, pregmap2 = pregmap ;
                          j < i ;
                          j++, ppage2++, pregmap2++ )
                    {
                        /*
                         * Initialize fields in the page header
                         */
                        ppage2->p_starting_alloc_map = &(ppage2->alloc_map[0]);
                        ppage2->free_paras_at_start = _PARAS_PER_PAGE;
                        ppage2->alloc_map[_PARAS_PER_PAGE] = (__page_map_t)(-1);

                        /*
                         * Initialize region_map[] entry for the page.
                         */
                        pregmap2->free_paras_in_page = _PARAS_PER_PAGE;
                        pregmap2->last_failed_alloc = _NO_FAILED_ALLOC;
                    }

                    /*
                     * Update __sbh_p_starting_region
                     */
                    __sbh_p_starting_region = preg;

                    /*
                     * Update the p_first_uncommitted for the region.
                     */
                    while ( (pregmap2 < &(preg->region_map[_PAGES_PER_REGION]))
                            && (pregmap2->free_paras_in_page
                                != _UNCOMMITTED_PAGE) )
                        pregmap2++;

                    preg->p_first_uncommitted = (pregmap2 <
                        &(preg->region_map[_PAGES_PER_REGION])) ? pregmap2 :
                        NULL;

                    /*
                     * Fulfill the allocation request using the first of the
                     * newly committed pages.
                     */
                    ppage->alloc_map[0] = (__page_map_t)para_req;

                    /*
                     * Update the p_starting_region_map field in the region
                     * descriptor and region_map[] entry for the page.
                     */
                    preg->p_starting_region_map = pregmap;
                    pregmap->free_paras_in_page -= para_req;

                    /*
                     * Update the p_starting_alloc_map and free_paras_at_start
                     * fields of the page.
                     */
                    ppage->p_starting_alloc_map = &(ppage->alloc_map[para_req]);
                    ppage->free_paras_at_start -= para_req;

                    /*
                     * Return pointer to allocated paragraphs.
                     */
                    return (void *)&(ppage->alloc_blocks[0]);
                }
                else {
                    /*
                     * Attempt to commit the pages failed. Return failure, the
                     * allocation will be attempted in the Win32 heap manager.
                     */
                    return NULL;
                }
            }
        }
        while ( (preg = preg->p_next_region) != &__small_block_heap );

        /*
         * Failure so far. None of the pages have a big enough free area to
         * fulfill the pending request. All of the pages in all of the current
         * regions are committed. Therefore, try to create a new region.
         */
        if ( (preg = __sbh_new_region()) != NULL ) {
            /*
             * Success! A new region has been created and the first few pages
             * (_PAGES_PER_COMMITMENT to be exact) have been committed.
             * satisfy the request out of the first page of the new region.
             */
            ppage = preg->p_pages_begin;
            ppage->alloc_map[0] = (__page_map_t)para_req;

            __sbh_p_starting_region = preg;
            ppage->p_starting_alloc_map = &(ppage->alloc_map[para_req]);
            ppage->free_paras_at_start = _PARAS_PER_PAGE - para_req;
            (preg->region_map[0]).free_paras_in_page -= (__page_map_t)para_req;
            return (void *)&(ppage->alloc_blocks[0]);
        }

        /*
         * Everything has failed, return NULL
         */
        return NULL;
}


/***
*void * __sbh_alloc_block_from_page(ppage, free_para_count, para_req) -
*       allocate a block from the given page.
*
*Purpose:
*       Allocate a block from the specified page of the small-block heap, of
*       the specified number of paragraphs in size.
*
*Entry:
*       __sbh_page_t *  ppage           - pointer to a page in the small-block
*                                         heap
*       int             free_para_count - number of free paragraphs in *ppage
*       size_t          para_req        - size of the allocation request in
*                                         paragraphs.
*
*Exit:
*       Returns a pointer to the newly allocated block, if successful.
*       Returns NULL, otherwise.
*
*Exceptions:
*       It is assumed that free_para_count >= para_req on entry. This must be
*       guaranteed by the caller. The behavior is undefined if this condition
*       is violated.
*
*******************************************************************************/

void * __cdecl __sbh_alloc_block_from_page (
        __sbh_page_t * ppage,
        size_t         free_para_count,
        size_t         para_req
        )
{
        __page_map_t * pmap1;
        __page_map_t * pmap2;
        __page_map_t * pstartmap;
        __page_map_t * pendmap;
        size_t         contiguous_free;

        pmap1 = pstartmap = ppage->p_starting_alloc_map;
        pendmap = &(ppage->alloc_map[_PARAS_PER_PAGE]);

        /*
         * Start at start_para_index and walk towards the end of alloc_map[],
         * looking for a string of free paragraphs big enough to satisfy the
         * the current request.
         *
         * Check if there are enough free paragraphs are p_starting_alloc_map
         * to satisfy the pending allocation request.
         */
        if ( ppage->free_paras_at_start >= para_req ) {
            /*
             * Success right off!
             * Mark the alloc_map entry with the size of the allocation
             * request.
             */
            *pmap1 = (__page_map_t)para_req;

            /*
             * Update the p_starting_alloc_map and free_paras_at_start fields
             * in the page.
             */
            if ( (pmap1 + para_req) < pendmap ) {
                ppage->p_starting_alloc_map += para_req;
                ppage->free_paras_at_start -= para_req;
            }
            else {
                ppage->p_starting_alloc_map = &(ppage->alloc_map[0]);
                ppage->free_paras_at_start = 0;
            }

            /*
             * Derive and return a pointer to the newly allocated
             * paragraphs.
             */
            return (void *)&(ppage->alloc_blocks[pmap1 -
                &(ppage->alloc_map[0])]);
        }

        /*
         * See if the search loop can be started just beyond the paragraphs
         * examined above. Note, this test assumes alloc_map[_PARAS_PER_PAGE]
         * != _FREE_PARA!
         */
        if ( *(pmap1 + ppage->free_paras_at_start) != _FREE_PARA )
            pmap1 += ppage->free_paras_at_start;

        while ( pmap1 + para_req < pendmap ) {

            if ( *pmap1 == _FREE_PARA ) {
                /*
                 * pmap1 refers to a free paragraph. Determine if there are
                 * enough free paragraphs contiguous with it to satisfy the
                 * allocation request. Note that the loop below requires that
                 * alloc_map[_PARAS_PER_PAGE] != _FREE_PARA to guarantee
                 * termination.
                 */
                for ( pmap2 = pmap1 + 1, contiguous_free = 1 ;
                      *pmap2 == _FREE_PARA ;
                      pmap2++, contiguous_free++ );

                if ( contiguous_free < para_req ) {
                    /*
                     * There were not enough contiguous free paragraphs. Do
                     * a little bookkeeping before going on to the next
                     * interation.
                     */

                    /* If pmap1 != pstartmap then these free paragraphs
                     * cannot be revisited.
                     */
                    if ( pmap1 == pstartmap ) {
                        /*
                         * Make sure free_paras_at_start is up-to-date.
                         */
                         ppage->free_paras_at_start = contiguous_free;
                    }
                    else {
                        /*
                         * These free paragraphs will not be revisited!
                         */
                        if ( (free_para_count -= contiguous_free) < para_req )
                            /*
                             * There are not enough unvisited free paragraphs
                             * to satisfy the current request. Return failure
                             * to the caller.
                             */
                            return NULL;
                    }

                    /*
                     * Update pmap1 for the next iteration of the loop.
                     */
                    pmap1 = pmap2;
                }
                else {
                    /*
                     * Success!
                     *
                     * Update the p_starting_alloc_map and free_paras_at_start
                     * fields in the page.
                     */
                    if ( (pmap1 + para_req) < pendmap ) {
                        ppage->p_starting_alloc_map = pmap1 + para_req;
                        ppage->free_paras_at_start = contiguous_free -
                                                     para_req;
                    }
                    else {
                        ppage->p_starting_alloc_map = &(ppage->alloc_map[0]);
                        ppage->free_paras_at_start = 0;
                    }

                    /*
                     * Mark the alloc_map entry with the size of the
                     * allocation request.
                     */
                    *pmap1 = (__page_map_t)para_req;

                    /*
                     * Derive and return a pointer to the newly allocated
                     * paragraphs.
                     */
                    return (void *)&(ppage->alloc_blocks[pmap1 -
                        &(ppage->alloc_map[0])]);
                }
            }
            else {
                /*
                 * pmap1 points to start of an allocated block in alloc_map[].
                 * Skip over it.
                 */
                pmap1 = pmap1 + *pmap1;
            }
        }

        /*
         * Now start at index 0 in alloc_map[] and walk towards, but not past,
         * index starting_para_index, looking for a string of free paragraphs
         * big enough to satisfy the allocation request.
         */
        pmap1 = &(ppage->alloc_map[0]);

        while ( (pmap1 < pstartmap) &&
                (pmap1 + para_req < pendmap) )
        {
            if ( *pmap1 == _FREE_PARA ) {
                /*
                 * pmap1 refers to a free paragraph. Determine if there are
                 * enough free paragraphs contiguous with it to satisfy the
                 * allocation request.
                 */
                for ( pmap2 = pmap1 + 1, contiguous_free = 1 ;
                      *pmap2 == _FREE_PARA ;
                      pmap2++, contiguous_free++ );

                if ( contiguous_free < para_req ) {
                    /*
                     * There were not enough contiguous free paragraphs.
                     *
                     * Update the count of unvisited free paragraphs.
                     */
                    if ( (free_para_count -= contiguous_free) < para_req )
                        /*
                         * There are not enough unvisited free paragraphs
                         * to satisfy the current request. Return failure
                         * to the caller.
                         */
                        return NULL;

                    /*
                     * Update pmap1 for the next iteration of the loop.
                     */
                    pmap1 = pmap2;
                }
                else {
                    /*
                     * Success!
                     *
                     * Update the p_starting_alloc_map and free_paras_at_start
                     * fields in the page..
                     */
                    if ( (pmap1 + para_req) < pendmap ) {
                        ppage->p_starting_alloc_map = pmap1 + para_req;
                        ppage->free_paras_at_start = contiguous_free -
                                                     para_req;
                    }
                    else {
                        ppage->p_starting_alloc_map = &(ppage->alloc_map[0]);
                        ppage->free_paras_at_start = 0;
                    }

                    /*
                     * Mark the alloc_map entry with the size of the
                     * allocation request.
                     */
                    *pmap1 = (__page_map_t)para_req;

                    /*
                     * Derive and return a pointer to the newly allocated
                     * paragraphs.
                     */
                    return (void *)&(ppage->alloc_blocks[pmap1 -
                        &(ppage->alloc_map[0])]);
                }
            }
            else {
                /*
                 * pmap1 points to start of an allocated block in alloc_map[].
                 * Skip over it.
                 */
                pmap1 = pmap1 + *pmap1;
            }
        }

        /*
         * Return failure.
         */
        return NULL;
}


/***
*size_t __sbh_resize_block(preg, ppage, pmap, new_para_sz) -
*       resize block
*
*Purpose:
*       Resize the specified block from the small-block heap. The allocation
*       block is not moved.
*
*Entry:
*       __sbh_region_t *preg        - pointer to the descriptor for the
*                                     region containing the block
*       __sbh_page_t *  ppage       - pointer to the page containing the
*                                     block
*       __page_map_t *  pmap        - pointer to the initial alloc_map[]
*                                     entry for the allocation block
*       size_t          new_para_sz - requested new size for the allocation
*                                     block, in paragraphs.
*
*Exit:
*       Returns 1, if successful. Otherwise, 0 is returned.
*
*Exceptions:
*
*******************************************************************************/

int __cdecl __sbh_resize_block (
        __sbh_region_t * preg,
        __sbh_page_t *   ppage,
        __page_map_t *   pmap,
        size_t           new_para_sz
        )
{
        __page_map_t *   pmap2;
        __page_map_t *   pmap3;
        __region_map_t * pregmap;
        size_t           old_para_sz;
        size_t           free_para_count;
        int              retval = 0;

        pregmap = &(preg->region_map[ppage - preg->p_pages_begin]);

        if ( (old_para_sz = *pmap) > new_para_sz ) {
            /*
             *  The allocation block is to be shrunk.
             */
            *pmap = new_para_sz;

            pregmap->free_paras_in_page += old_para_sz - new_para_sz;

            pregmap->last_failed_alloc = _NO_FAILED_ALLOC;

            retval++;
        }
        else if ( old_para_sz < new_para_sz ) {
            /*
             * The allocation block is to be grown to new_para_sz paragraphs
             * (if possible).
             */
            if ( (pmap + new_para_sz) <= &(ppage->alloc_map[_PARAS_PER_PAGE]) )
            {
                /*
                 * Determine if there are sufficient free paragraphs to
                 * expand the block to the desired new size.
                 */
                for ( pmap2 = pmap + old_para_sz,
                        pmap3 = pmap + new_para_sz ;
                      (pmap2 < pmap3) && (*pmap2 == _FREE_PARA) ;
                      pmap2++ ) ;

                if ( pmap2 == pmap3 ) {
                    /*
                     * Success, mark the resized allocation
                     */
                    *pmap = (__page_map_t)new_para_sz;

                    /*
                     * Check whether the p_starting_alloc_map and the
                     * free_paras_at_start fields need to be updated.
                     */
                    if ( (pmap <= ppage->p_starting_alloc_map) &&
                         (pmap3 > ppage->p_starting_alloc_map) )
                    {
                        if ( pmap3 < &(ppage->alloc_map[_PARAS_PER_PAGE]) ) {
                            ppage->p_starting_alloc_map = pmap3;
                            /*
                             * Determine how many contiguous free paragraphs
                             * there are starting a *pmap3. Note, this assumes
                             * that alloc_map[_PARAS_PER_PAGE] != _FREE_PARA.
                             */
                            for ( free_para_count = 0 ; *pmap3 == _FREE_PARA ;
                                  free_para_count++, pmap3++ ) ;
                            ppage->free_paras_at_start = free_para_count;
                        }
                        else {
                            ppage->p_starting_alloc_map = &(ppage->alloc_map[0]);
                            ppage->free_paras_at_start = 0;
                        }
                    }

                    /*
                     * Update the region_map[] entry.
                     */
                    pregmap->free_paras_in_page += (old_para_sz - new_para_sz);

                    retval++;
                }
            }
        }

        return retval;
}


/***
*void * __sbh_heap_check() - check small-block heap
*
*Purpose:
*       Perform validity checks on the small-block heap.
*
*Entry:
*       There are no arguments.
*
*Exit:
*       Returns 0 if the small-block is okay.
*       Returns < 0 if the small-block heap has an error. The exact value
*       identifies where, in the source code below, the error was detected.
*
*Exceptions:
*       There is no protection against memory access error (exceptions).
*
*******************************************************************************/

int __cdecl __sbh_heap_check (
        void
        )
{
        __sbh_region_t *    preg;
        __sbh_page_t *      ppage;
        int                 uncommitted_pages;
        int                 free_paras_in_page;
        int                 contiguous_free_paras;
        int                 starting_region_found;
        int                 p_starting_alloc_map_found;
        int                 i, j, k;

        starting_region_found = 0;
        preg = &__small_block_heap;
        do {
            if ( __sbh_p_starting_region == preg )
                starting_region_found++;

            if ( (ppage = preg->p_pages_begin) != _NO_PAGES ) {
                /*
                 * Scan the pages of the region looking for
                 * inconsistencies.
                 */
                for ( i = 0, uncommitted_pages = 0,
                        ppage = preg->p_pages_begin ;
                      i < _PAGES_PER_REGION ;
                      i++, ppage++ )
                {
                    if ( preg->region_map[i].free_paras_in_page ==
                         _UNCOMMITTED_PAGE )
                    {
                        /*
                         * Verify the first_uncommitted_index field.
                         */
                        if ( (uncommitted_pages == 0) &&
                             (preg->p_first_uncommitted !=
                                &(preg->region_map[i])) )
                            /*
                             * Bad first_uncommitted_index field!
                             */
                            return -1;

                        uncommitted_pages++;
                    }
                    else {

                        if ( ppage->p_starting_alloc_map >=
                             &(ppage->alloc_map[_PARAS_PER_PAGE]) )
                            /*
                             * Bad p_starting_alloc_map field
                             */
                            return -2;

                        if ( ppage->alloc_map[_PARAS_PER_PAGE] !=
                             (__page_map_t)-1 )
                            /*
                             * Bad alloc_map[_PARAS_PER_PAGE] field
                             */
                            return -3;

                        /*
                         * Scan alloc_map[].
                         */
                        j  = 0;
                        p_starting_alloc_map_found = 0;
                        free_paras_in_page = 0;
                        contiguous_free_paras = 0;

                        while ( j < _PARAS_PER_PAGE ) {
                            /*
                             * Look for the *p_starting_alloc_map.
                             */
                            if ( &(ppage->alloc_map[j]) ==
                                 ppage->p_starting_alloc_map )
                                p_starting_alloc_map_found++;

                            if ( ppage->alloc_map[j] == _FREE_PARA ) {
                                /*
                                 * Free paragraph, increment the count.
                                 */
                                free_paras_in_page++;
                                contiguous_free_paras++;
                                j++;
                            }
                            else {
                                /*
                                 * First paragraph of an allocated block.
                                 */

                                /*
                                 * Make sure the preceding free block, if any,
                                 * was smaller than the last_failed_alloc[]
                                 * entry for the page.
                                 */
                                if ( contiguous_free_paras >=
                                     (int)preg->region_map[i].last_failed_alloc )
                                     /*
                                      * last_failed_alloc[i] was mismarked!
                                      */
                                     return -4;

                                /*
                                 * If this is the end of the string of free
                                 * paragraphs starting at *p_starting_alloc_map,
                                 * verify that free_paras_at_start is
                                 * reasonable.
                                 */
                                if ( p_starting_alloc_map_found == 1 ) {
                                    if ( contiguous_free_paras <
                                         (int)ppage->free_paras_at_start )
                                         return -5;
                                    else
                                        /*
                                         * Set flag to 2 so the check is not
                                         * repeated.
                                         */
                                        p_starting_alloc_map_found++;
                                }

                                contiguous_free_paras = 0;

                                /*
                                 * Scan the remaining paragraphs and make
                                 * sure they are marked properly (they should
                                 * look like free paragraphs).
                                 */
                                for ( k = j + 1 ;
                                      k < j + ppage->alloc_map[j] ; k++ )
                                {
                                    if ( ppage->alloc_map[k] != _FREE_PARA )
                                        /*
                                         * alloc_map[k] is mismarked!
                                         */
                                        return -6;
                                }

                                j = k;
                            }
                        }

                        if ( free_paras_in_page !=
                             preg->region_map[i].free_paras_in_page )
                            /*
                             * region_map[i] does not match the number of
                             * free paragraphs in the page!
                             */
                             return -7;

                        if ( p_starting_alloc_map_found == 0 )
                            /*
                             * Bad p_starting_alloc_map field!
                             */
                            return -8;

                    }
                }
            }
        }
        while ( (preg = preg->p_next_region) != &__small_block_heap );

        if ( starting_region_found == 0 )
            /*
             * Bad __sbh_p_starting_region!
             */
            return -9;

        return 0;
}
