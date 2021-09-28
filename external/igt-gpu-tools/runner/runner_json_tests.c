#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <json.h>

#include "igt.h"
#include "resultgen.h"

static char testdatadir[] = JSON_TESTS_DIRECTORY;

static struct json_object *read_json(int fd)
{
	struct json_object *obj;
	struct json_tokener *tok = json_tokener_new();
	enum json_tokener_error err;
	char buf[512];
	ssize_t s;

	do {
		s = read(fd, buf, sizeof(buf));
		obj = json_tokener_parse_ex(tok, buf, s);
	} while ((err = json_tokener_get_error(tok)) == json_tokener_continue);

	igt_assert_eq(err, json_tokener_success);

	json_tokener_free(tok);
	return obj;
}

static void compare(struct json_object *one,
		    struct json_object *two);

static void compare_objects(struct json_object *one,
			    struct json_object *two)
{
	json_object_iter iter;
	struct json_object *subobj;

	json_object_object_foreachC(one, iter) {
		igt_debug("Key %s\n", iter.key);

		igt_assert(json_object_object_get_ex(two, iter.key, &subobj));

		compare(iter.val, subobj);
	}
}

static void compare_arrays(struct json_object *one,
			   struct json_object *two)
{
	size_t i;

	for (i = 0; i < json_object_array_length(one); i++) {
		igt_debug("Array index %zd\n", i);
		compare(json_object_array_get_idx(one, i),
			json_object_array_get_idx(two, i));
	}
}

static bool compatible_types(struct json_object *one,
			     struct json_object *two)
{
	/*
	 * A double of value 0.0 gets written as "0", which gets read
	 * as an int.
	 */
	json_type onetype = json_object_get_type(one);
	json_type twotype = json_object_get_type(two);

	switch (onetype) {
	case json_type_boolean:
	case json_type_string:
	case json_type_object:
	case json_type_array:
	case json_type_null:
		return onetype == twotype;
		break;
	case json_type_double:
	case json_type_int:
		return twotype == json_type_double || twotype == json_type_int;
		break;
	}

	igt_assert(!"Cannot be reached");
	return false;
}

static void compare(struct json_object *one,
		    struct json_object *two)
{
	igt_assert(compatible_types(one, two));

	switch (json_object_get_type(one)) {
	case json_type_boolean:
		igt_assert_eq(json_object_get_boolean(one), json_object_get_boolean(two));
		break;
	case json_type_double:
	case json_type_int:
		/*
		 * A double of value 0.0 gets written as "0", which
		 * gets read as an int. Both yield 0.0 with
		 * json_object_get_double(). Comparing doubles with ==
		 * considered crazy but it's good enough.
		 */
		igt_assert(json_object_get_double(one) == json_object_get_double(two));
		break;
	case json_type_string:
		igt_assert(!strcmp(json_object_get_string(one), json_object_get_string(two)));
		break;
	case json_type_object:
		igt_assert_eq(json_object_object_length(one), json_object_object_length(two));
		compare_objects(one, two);
		break;
	case json_type_array:
		igt_assert_eq(json_object_array_length(one), json_object_array_length(two));
		compare_arrays(one, two);
		break;
	case json_type_null:
		break;
	default:
		igt_assert(!"Cannot be reached");
	}
}

static void run_results_and_compare(int dirfd, const char *dirname)
{
	int testdirfd = openat(dirfd, dirname, O_RDONLY | O_DIRECTORY);
	int reference;
	struct json_object *resultsobj, *referenceobj;

	igt_assert_fd(testdirfd);

	igt_assert((resultsobj = generate_results_json(testdirfd)) != NULL);

	reference = openat(testdirfd, "reference.json", O_RDONLY);
	close(testdirfd);

	igt_assert_fd(reference);
	referenceobj = read_json(reference);
	close(reference);
	igt_assert(referenceobj != NULL);

	igt_debug("Root object\n");
	compare(resultsobj, referenceobj);
	igt_assert_eq(json_object_put(resultsobj), 1);
	igt_assert_eq(json_object_put(referenceobj), 1);
}

static const char *dirnames[] = {
	"normal-run",
	"warnings",
	"warnings-with-dmesg-warns",
	"piglit-style-dmesg",
	"incomplete-before-any-subtests",
	"dmesg-results",
	"aborted-on-boot",
	"aborted-after-a-test",
	"dmesg-escapes",
	"notrun-results",
	"notrun-results-multiple-mode",
	"dmesg-warn-level",
	"dmesg-warn-level-piglit-style",
	"dmesg-warn-level-one-piglit-style"
};

igt_main
{
	int dirfd = open(testdatadir, O_RDONLY | O_DIRECTORY);
	size_t i;

	igt_assert_fd(dirfd);

	for (i = 0; i < ARRAY_SIZE(dirnames); i++) {
		igt_subtest(dirnames[i]) {
			run_results_and_compare(dirfd, dirnames[i]);
		}
	}
}
