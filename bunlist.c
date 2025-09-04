#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bunlist.h"

static void bunlist_resize(bunlist *arr, usize newcap);
static void bunlist_chk_resize(bunlist *arr);
static bool bunlist_chk_index(bunlist *arr, usize i);

bunlist *bunlist_create(usize isize, usize cap,
		      void (*free_fn)(usize i, void *data))
{
	return bunlist_create_ex(isize, cap, BARR_D_INCR, BARR_D_MULT, false,
				free_fn);
}

bunlist *bunlist_create_ex(usize isize, usize cap, u32 incr, bool mult,
			 bool subarr, void (*free_fn)(usize i, void *data))
{
	bunlist *arr = malloc(sizeof(bunlist));
	arr->subarr = subarr;
	arr->isize = isize;
	arr->len = 0;
	arr->cap = cap;
	arr->incr = incr <= 1 ? 1 : incr;
	arr->mult = mult;
	arr->items = NULL;
	arr->free_fn = free_fn;
	bunlist_resize(arr, arr->cap);

	return arr;
}

usize bunlist_append(bunlist *arr, void *item)
{
	bunlist_chk_resize(arr);

	memcpy(arr->items + (arr->len * arr->isize), item, arr->isize);
	arr->len++;

	return arr->len - 1;
}

usize bunlist_append_free(bunlist *arr, void *item)
{
	usize i = bunlist_append(arr, item);
	free(item);
	return i;
}

bool bunlist_remove(bunlist *arr, usize i)
{
	if (!bunlist_chk_index(arr, i)) {
		return false;
	}
	if (arr->free_fn != NULL) {
		void *itm = bunlist_get(arr, i);
		arr->free_fn(i, itm);
	}
	u8 *dest = (u8 *)(arr->items + arr->isize * (i));
	u8 *src = (u8 *)arr->items + arr->isize * (i + 1);
	usize size = (arr->isize * (arr->len - i));

	arr->len--;
	memmove(dest, src, size);

	return true;
}

bool bunlist_remove_lazy(bunlist *arr, usize i)
{
	if (!bunlist_chk_index(arr, i)) {
		return false;
	}
	--arr->len;
	u8 *item_ptr = (u8 *)arr->items + i * arr->isize;
	u8 *end_ptr = (u8 *)arr->items + arr->len * arr->isize;
	memcpy(item_ptr, end_ptr, arr->isize);

	return true;
}

bool bunlist_insert(bunlist *arr, void *item, usize i)
{
	if (!bunlist_chk_index(arr, i)) {
		return false;
	}
	bunlist_chk_resize(arr);

	u8 *dest = (arr->items + arr->isize * (i + 1));
	u8 *src = (arr->items + arr->isize * (i));
	usize size = (arr->isize * (arr->len - i));

	memcpy(dest, src, size);
	memcpy(src, item, arr->isize);
	arr->len++;
	return true;
}

bool bunlist_insert_free(bunlist *arr, void *item, usize i)
{
	bool r = bunlist_insert(arr, item, i);
	free(item);
	return r;
}

bool bunlist_destroy(bunlist *arr)
{
	if (arr->free_fn != NULL) {
		for (usize i = 0; i < arr->len; i++) {
			void *item = bunlist_get(arr, i);
			arr->free_fn(i, item);
		}
	}

	free(arr->items);
	free(arr);

	return true;
}

bool bunlist_clear(bunlist *arr)
{
	memset(arr->items, 0, arr->len * arr->isize);
	arr->len = 0;
	return true;
}

void *bunlist_get(bunlist *arr, usize i)
{
	if (!bunlist_chk_index(arr, i)) {
		return NULL;
	}
	void *item = arr->items + (i * arr->isize);
	return item;
}

void bunlist_cpy(bunlist *dst_arr, bunlist *src_arr)
{
	dst_arr->isize = src_arr->isize;
	dst_arr->len = src_arr->len;
	dst_arr->cap = src_arr->cap;
	dst_arr->incr = src_arr->incr;
	dst_arr->mult = src_arr->mult;
	bunlist_resize(dst_arr, dst_arr->cap);
	memcpy(dst_arr->items, src_arr->items, src_arr->isize * src_arr->len);
}

bunlist *bunlist_clone(bunlist *src_arr)
{
	bunlist *dst_arr = bunlist_create_ex(src_arr->isize, src_arr->cap,
					   src_arr->incr, src_arr->mult, false,
					   src_arr->free_fn);
	bunlist_resize(dst_arr, dst_arr->cap);
	memcpy(dst_arr->items, src_arr->items, src_arr->isize * src_arr->len);
	return dst_arr;
}

void bunlist_exe(bunlist *arr, void (*function)(usize, void *))
{
	for (usize i = 0; i < arr->len; i++) {
		void *item = bunlist_get(arr, i);
		function(i, item);
	}
}

void bunlist_qsort(bunlist *arr, i32 (*fn)(const void *, const void *))
{
	qsort(arr->items, arr->len, arr->isize, fn);
}

void *bunlist_bsearch(bunlist *arr, void *key,
		     i32 (*fn)(const void *, const void *))
{
	return bsearch(key, arr->items, arr->len, arr->isize, fn);
}

bunlist *bunlist_subarr(bunlist *arr, usize start, usize end, bool clone)
{
	u8 *new_items = NULL;
	bunlist *dst = NULL;
	if (clone) {
		dst = bunlist_create_ex(arr->isize, (end + 1 - start), arr->incr,
				       arr->mult, false, arr->free_fn);
		bunlist_resize(dst, dst->cap);
		memcpy(dst->items, arr->items + (arr->isize * start),
		       arr->isize * (end + 1 - start));
		dst->len = dst->cap;
	} else {
		// u8 *dest = (u8 *)(arr->items + arr->isize * (i));
		new_items = (u8 *)(arr->items + (arr->isize * start));
		dst = bunlist_create_ex(arr->isize, (end + 1 - start), arr->incr,
				       arr->mult, true, arr->free_fn);
		dst->len = dst->cap;
		dst->items = new_items;
	}
	return dst;
}

/** \brief Checks if the array has reached max capacity,
 * calculates a new capacity and 
 * resizes it accordingly to the settings */
static void bunlist_chk_resize(bunlist *arr)
{
	if (arr->cap <= arr->len) {
		if (arr->mult) {
			//roundup to avoid errors
			usize newcapacity =
				(usize)(arr->cap * (arr->incr * 0.1));

			bunlist_resize(arr, newcapacity);
		} else {
			usize newcapacity = arr->cap + arr->incr;
			bunlist_resize(arr, newcapacity);
		}
	}
}

/** \brief Updates the array len and reallocs the memory to fit the new capacity */
static void bunlist_resize(bunlist *arr, usize newcap)
{
	if (!arr->subarr) {
		arr->cap = newcap;
		void *items = realloc(arr->items, arr->isize * arr->cap);
		arr->items = items;
	}
}

/** \brief Returns true if the index is valid.
 * if it's invalid, print an error to stderr and return false. */
static bool bunlist_chk_index(bunlist *arr, usize i)
{
	if (i > arr->len) {
		fprintf(stderr, "\nInvalid Index %lu", i);
		return false;
	} else
		return true;
}
