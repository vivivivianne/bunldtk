#pragma once
#include "buntypes.h"

#define BARR_D_INCR 15
#define BARR_D_MULT false

typedef struct bunarr {
	void *items; //points to the start of the memory block where the items of the array are stored
	usize isize; //the size of each item
	usize len; //the number of items currently held by the array
	usize cap; //the number of items the array can hold
	u32 incr; //how much the list will be increased by when it reaches maxsize
	bool mult; // percentage - if true the increase value will be used as a multiplyer of the array capacity: newcap = cap * (incr/10)
	bool subarr; // set to true if this is a subarray
	void (*free_fn)(usize i, void *itm);

} bunarr;

/** \brief creates an dynamic array with the given item_size, initial capacity and a callback function to free your items. 
 * \param isize The size in bytes of each item,
 * \param cap The initial capacity of the array,
 * \param free_fn An optional function used to properly free items with malloc'ed memory when they are removed from the array.
 * \sa bunarr_create_ex */
bunarr *bunarr_create(usize isize, usize cap,
		      void (*free_fn)(usize i, void *itm));

/** \brief creates an dynamic array with the given item_size , initial capacity and resize_factor 
 * \param isize - The size in bytes of each item,
 * \param cap - The initial capacity of the array
 * \param incr - 'increase' how much the array will grow once it's resized
 * \param mult - 'percentage' if true the incr value will be treated as a percentage of the array size. 
 * \param subarray - if true this will be treated as a subarray, which means it will not allow memory reallocations */
bunarr *bunarr_create_ex(usize isize, usize cap, u32 incr, bool mult,
			 bool subarr, void (*free_fn)(usize i, void *itm));

/** \brief Destroys the array */
bool bunarr_destroy(bunarr *arr);

/** \brief Appends item to bunarr by copying*/
usize bunarr_append(bunarr *arr, void *itm);

/** \brief Appends item to bunarr by copying, and then runs free(itm) */
usize bunarr_append_free(bunarr *arr, void *itm);

/** \brief inserts an item at the index moves the memory block from index + 1 
 * to array->len one item fowards, overwriting the item we 
 * wish to remove, and then we update the overall array size
 * \param *item - Pointer to the item we wish to insert.
 * \param i - index of insertion */
usize bunarr_insert(bunarr *arr, void *itm, usize i);

/** \brief same as bunarr_insert , but runs free(itm) to free the original memory afterwards */
usize bunarr_insert_free(bunarr *arr, void *itm, usize i);

/** \brief removes item i from the array 
 * \param i - the index of the item we wish to remove. */
bool bunarr_remove(bunarr *arr, usize i);

/** \brief lazy remove, copys last item into the index and reduces the list size */
bool bunarr_remove_lazy(bunarr *arr, usize i);

/** \brief Clears the array by setting everything to 0 */
bool bunarr_clear(bunarr *arr);

/** \brief Returns the item of the given index from the array */
void *bunarr_get(bunarr *arr, usize i);

/** \brief copy src array to dst array */
void bunarr_cpy(bunarr *dst_arr, bunarr *scr_arr);

/** \brief clones the src array and returns the new dst array */
void *bunarr_clone(bunarr *src_arr);

/** \brief Loops trough the array and executes the given function on every item, 
 * also passes the index to the function */
void bunarr_exe(bunarr *arr, void (*function)(usize i, void *itm));

/** \brief runs bsearch on the array and returns the requested key, 
 * \param key the desired item you want to find
 * \param compar_fn_t comparison function
 * please ensure the array is sorted by the variable you want to search before doing so. */
void *bunarr_bsearch(bunarr *arr, void *key,
		     i32 (*fn)(const void *, const void *));

/** \brief Runs quick sort on the array using the given compare function
 * \param  i32 (*fn)(const void *, const void *), function that receives pointers to two items of the arr and returns the result of the comparison */
void bunarr_qsort(bunarr *arr, i32 (*fn)(const void *, const void *));

/** \brief creates a subarray from the given array, optionally clone it instead of creating a pointer */
bunarr *bunarr_subarr(bunarr *arr, usize start, usize end, bool clone);
