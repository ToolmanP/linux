#include <kunit/test.h>
#include <linux/mm.h>

static void runpv_alloc_page_zero_test(struct kunit *test)
{
	int order;
	unsigned long *addr;
  unsigned long array_size;
	unsigned long i;
	struct folio *folio[PMD_ORDER];

	pr_info("Running runpv_alloc_page_test\n");
	for (order = 0; order < PMD_ORDER; order++) {
		pr_info("Allocating folio of order %d\n", order);
		folio[order] = folio_alloc(
			GFP_KERNEL | __GFP_ZERO | __GFP_RUNPV, order);
	}

	for (order = 0; order < PMD_ORDER; order++) {
		addr = folio_address(folio[order]);
    array_size = (1ul << (order + PAGE_SHIFT)) / sizeof(unsigned long);
		for (i = 0; i < array_size; i++)
			KUNIT_EXPECT_EQ_MSG(test, addr[i], 0UL,
					    "Allocated memory is not zeroed\n");
	}

	for (order = 0; order < PMD_ORDER; order++) {
		folio_put(folio[order]);
	}
}

static void runpv_fill_page_test(struct kunit *test)
{
	int order;
	unsigned long *addr;
  unsigned long array_size;
	unsigned long i;
	struct folio *folio[PMD_ORDER];

	pr_info("Running fill_page_test\n");
	for (order = 0; order < PMD_ORDER; order++) {
		pr_info("Allocating folio of order %d\n", order);
		folio[order] = folio_alloc(
			GFP_KERNEL | __GFP_ZERO | __GFP_RUNPV, order);
	}

	for (order = 0; order < PMD_ORDER; order++) {
		addr = folio_address(folio[order]);
    array_size = (1 << (order + PAGE_SHIFT)) / sizeof(unsigned long);
		pr_info("Filling folio of order %d\n", order);
    for (i = 0; i < array_size; i++)
      addr[i] = 0xDEADBEEF00000000ul + i;
		for (i = 0; i < array_size; i++)
			KUNIT_EXPECT_EQ_MSG(test, addr[i], 0xDEADBEEF00000000ul + i,
					    "Allocated memory is not correct.\n");
	}

	for (order = 0; order < PMD_ORDER; order++) {
    pr_info("Releasing folio of order %d\n", order);
		folio_put(folio[order]);
	}

  pr_info("%s: OK\n", __func__);
}

static struct kunit_case runpv_alloc_test_cases[] = {
  KUNIT_CASE(runpv_fill_page_test),
  KUNIT_CASE(runpv_alloc_page_zero_test),
  {},
};

static struct kunit_suite runpv_alloc_test_suite = {
	.name = "runpv-alloc-page",
	.test_cases = runpv_alloc_test_cases,
};

kunit_test_suite(runpv_alloc_test_suite);
MODULE_LICENSE("GPL");
