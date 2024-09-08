#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bunarr.h"

static void bunarr_resize(bunarr *arr, usize newcap);
static void bunarr_check_resize(bunarr *arr);
static bool bunarr_chk_index(bunarr *arr, usize i);

bunarr *bunarr_create(usize isize, usize cap,
		      void (*free_fn)(usize i, void *data))
{
	return bunarr_create_ex(isize, cap, BARR_D_INCR, BARR_D_MULT, false,
				free_fn);
}

bunarr *bunarr_create_ex(usize isize, usize cap, u32 incr, bool mult,
			 bool subarr, void (*free_fn)(usize i, void *data))
{
	bunarr *arr = malloc(sizeof(bunarr));
	arr->subarr = subarr;
	arr->isize = isize;
	arr->len = 0;
	arr->cap = cap;
	arr->incr = incr <= 1 ? 1 : incr;
	arr->mult = mult;
	arr->items = NULL;
	arr->free_fn = free_fn;
	bunarr_resize(arr, arr->cap);

	return arr;
}

usize bunarr_append(bunarr *arr, void *item)
{
	bunarr_check_resize(arr);

	memcpy(arr->items + (arr->len * arr->isize), item, arr->isize);
	arr->len++;

	return arr->len - 1;
}

usize bunarr_append_free(bunarr *arr, void *item)
{
	usize i = bunarr_append(arr, item);
	free(item);
	return i;
}

bool bunarr_remove(bunarr *arr, usize i)
{
	if (!bunarr_chk_index(arr, i)) {
		return false;
	}
	if (arr->free_fn != NULL) {
		void *itm = bunarr_get(arr, i);
		arr->free_fn(i, itm);
	}
	u8 *dest = (u8 *)(arr->items + arr->isize * (i));
	u8 *src = (u8 *)arr->items + arr->isize * (i + 1);
	usize size = (arr->isize * (arr->len - i));

	arr->len--;
	memmove(dest, src, size);

	return true;
}

bool bunarr_remove_lazy(bunarr *arr, usize i)
{
	if (!bunarr_chk_index(arr, i)) {
		return false;
	}
	--arr->len;
	u8 *item_ptr = (u8 *)arr->items + i * arr->isize;
	u8 *end_ptr = (u8 *)arr->items + arr->len * arr->isize;
	memcpy(item_ptr, end_ptr, arr->isize);

	return 0;
}

usize bunarr_insert(bunarr *arr, void *item, usize i)
{
	if (!bunarr_chk_index(arr, i)) {
		return false;
	}
	bunarr_check_resize(arr);

	u8 *dest = (arr->items + arr->isize * (i + 1));
	u8 *src = (arr->items + arr->isize * (i));
	usize size = (arr->isize * (arr->len - i));

	memcpy(dest, src, size);
	memcpy(src, item, arr->isize);
	arr->len++;
	return i;
}

usize bunarr_insert_free(bunarr *arr, void *item, usize i)
{
	usize r = bunarr_insert(arr, item, i);
	free(item);
	return r;
}

bool bunarr_destroy(bunarr *arr)
{
	if (arr->free_fn != NULL) {
		for (usize i = 0; i < arr->len; i++) {
			void *item = bunarr_get(arr, i);
			arr->free_fn(i, item);
		}
	}

	free(arr->items);
	free(arr);

	return true;
}

bool bunarr_clear(bunarr *arr)
{
	memset(arr->items, 0, arr->len * arr->isize);
	arr->len = 0;
	return true;
}

void *bunarr_get(bunarr *arr, usize i)
{
	if (!bunarr_chk_index(arr, i)) {
		return NULL;
	}
	void *item = arr->items + (i * arr->isize);
	return item;
}

void bunarr_cpy(bunarr *dst_arr, bunarr *src_arr)
{
	dst_arr->isize = src_arr->isize;
	dst_arr->len = src_arr->len;
	dst_arr->cap = src_arr->cap;
	dst_arr->incr = src_arr->incr;
	dst_arr->mult = src_arr->mult;
	bunarr_resize(dst_arr, dst_arr->cap);
	memcpy(dst_arr->items, src_arr->items, src_arr->isize * src_arr->len);
}

void *bunarr_clone(bunarr *src_arr)
{
	bunarr *dst_arr = bunarr_create_ex(src_arr->isize, src_arr->cap,
					   src_arr->incr, src_arr->mult, false,
					   src_arr->free_fn);
	bunarr_resize(dst_arr, dst_arr->cap);
	memcpy(dst_arr->items, src_arr->items, src_arr->isize * src_arr->len);
	return dst_arr;
}

void bunarr_exe(bunarr *arr, void (*function)(usize, void *))
{
	for (usize i = 0; i < arr->len; i++) {
		void *item = bunarr_get(arr, i);
		function(i, item);
	}
}

void bunarr_qsort(bunarr *arr, i32 (*fn)(const void *, const void *))
{
	qsort(arr->items, arr->len, arr->isize, fn);
}

void *bunarr_bsearch(bunarr *arr, void *key,
		     i32 (*fn)(const void *, const void *))
{
	return bsearch(key, arr->items, arr->len, arr->isize, fn);
}

bunarr *bunarr_subarr(bunarr *arr, usize start, usize end, bool clone)
{
	u8 *new_items = NULL;
	bunarr *dst = NULL;
	if (clone) {
		dst = bunarr_create_ex(arr->isize, (end + 1 - start), arr->incr,
				       arr->mult, false, arr->free_fn);
		bunarr_resize(dst, dst->cap);
		memcpy(dst->items, arr->items + (arr->isize * start),
		       arr->isize * (end + 1 - start));
		dst->len = dst->cap;
	} else {
		// u8 *dest = (u8 *)(arr->items + arr->isize * (i));
		new_items = (u8 *)(arr->items + (arr->isize * start));
		dst = bunarr_create_ex(arr->isize, (end + 1 - start), arr->incr,
				       arr->mult, true, arr->free_fn);
		dst->len = dst->cap;
		dst->items = new_items;
	}
	return dst;
}

/** \brief Checks if the array has reached max capacity,
 * calculates a new capacity and 
 * resizes it accordingly to the settings*/
static void bunarr_check_resize(bunarr *arr)
{
	if (arr->cap <= arr->len) {
		if (arr->mult) {
			//roundup to avoid errors
			usize newcapacity =
				(usize)(arr->cap * (arr->incr * 0.1));

			bunarr_resize(arr, newcapacity);
		} else {
			usize newcapacity = arr->cap + arr->incr;
			bunarr_resize(arr, newcapacity);
		}
	}
}

/** \brief Updates the array len and reallocs the memory to fit the new capacity */
static void bunarr_resize(bunarr *arr, usize newcap)
{
	if (!arr->subarr) {
		arr->cap = newcap;
		void *items = realloc(arr->items, arr->isize * arr->cap);
		arr->items = items;
	}
}

/** \brief Returns true if the index is valid.
 * if it's invalid, print an error to stderr and return false. */
static bool bunarr_chk_index(bunarr *arr, usize i)
{
	if (i > arr->len) {
		fprintf(stderr, "\nInvalid Index %lu", i);
		return false;
	} else
		return true;
}
