/* SPDX-License-Identifier: GPL-2.0-only */
#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef uint32_t u32;
typedef uint64_t u64;
typedef struct { int pins; } ka_page_t;
#define KA_FOLL_WRITE 1
#define KA_MM_PAGE_SIZE 4096
#define DEVMM_GET_2M_PAGE_NUM 512
static unsigned long ka_jiffies;
static ka_page_t backing[1024];
static int results[8], count, calls, released, logged_result;
static int partial_result, expected_flags;

static int ka_mm_pin_user_pages_fast(unsigned long start, int nr_pages,
                                    int flags, ka_page_t **pages)
{
    int ret, i;
    assert(calls < count);
    assert(flags == expected_flags);
    ret = results[calls++];
    assert(ret <= nr_pages);
    for (i = 0; i < ret; i++) {
        pages[i] = &backing[start / KA_MM_PAGE_SIZE + i];
        pages[i]->pins++;
    }
    return ret;
}

static void devmm_unpin_user_pages(ka_page_t **pages, u64 total, u64 got)
{
    u64 i;
    assert(got <= total);
    for (i = 0; i < got; i++) {
        assert(pages[i] && pages[i]->pins > 0);
        pages[i]->pins--;
        pages[i] = NULL;
        released++;
    }
}

static void devmm_try_cond_resched(u32 *stamp) { (void)stamp; }
static void devmm_drv_err(const char *format, unsigned long va, int expected, int actual)
{
    (void)va;
    (void)expected;
    if (strstr(format, "Repeated partial")) partial_result = actual;
    else logged_result = actual;
}

#include "svm_pin_test.inc"

static void setup(const int *sequence, int length)
{
    int i;
    for (i = 0; i < 1024; i++) assert(backing[i].pins == 0);
    memcpy(results, sequence, length * sizeof(*sequence));
    count = length;
    calls = released = logged_result = 0;
    partial_result = -1;
    expected_flags = KA_FOLL_WRITE;
}

int main(void)
{
    ka_page_t *pages[1024] = {0};
    int errors[] = {-EFAULT, -ENOMEM, -EINVAL, -EAGAIN};
    unsigned int i;

    for (i = 0; i < sizeof(errors) / sizeof(errors[0]); i++) {
        setup(&errors[i], 1);
        assert(devmm_pin_user_pages_fast(0, 65, 1, pages) == -EFAULT);
        assert(logged_result == errors[i]);
        assert(calls == 1 && released == 0);
    }
    setup((int[]){32, 65}, 2);
    assert(devmm_pin_user_pages_fast(0, 65, 1, pages) == 0);
    assert(calls == 2 && released == 32);
    devmm_unpin_user_pages(pages, 65, 65);

    /* The observed read-only failure: 19 of 65 pages on all five attempts. */
    setup((int[]){19, 19, 19, 19, 19}, 5);
    expected_flags = 0;
    assert(devmm_pin_user_pages_fast(0, 65, 0, pages) == -EFAULT);
    assert(logged_result == 0 && calls == 5 && released == 95);
    assert(partial_result == 19);

    setup((int[]){0, 0, 0, 0, 0}, 5);
    assert(devmm_pin_user_pages_fast(0, 65, 1, pages) == -EFAULT);
    assert(partial_result == 0 && calls == 5 && released == 0);

    setup((int[]){512, 17, -ENOMEM}, 3);
    assert(devmm_pin_user_pages_fast(0, 600, 1, pages) == -EFAULT);
    assert(logged_result == -ENOMEM && released == 529);

    setup((int[]){512, 88}, 2);
    assert(devmm_pin_user_pages_fast(0, 600, 1, pages) == 0);
    devmm_unpin_user_pages(pages, 600, 600);
    setup((int[]){0}, 1); /* Also verifies all preceding pins were released. */
    puts("SVM pin error and ownership tests passed");
    return 0;
}
