#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include "xstrtod.h"
#include "parser.h"
#include "sort.h"

#ifdef DEBUG
#define debug(...) printf(__VA_ARGS__)
#else
#define debug(...) 
#endif

#ifndef bool
typedef int bool;
enum {
	false = 0,
	true = 1
};
#endif

#define DEFAULT_FILE_SOURCE 	"a.txt"
#define DEFAULT_FILE_TARGET 	"b.txt"


#define ITEM_VAILD 		(1 << 0)
#define ITEM_MATCH 		(1 << 1)

struct item {
	int index;  /* start from 1 */
	char *data;
	double val;
	int flags;

	struct item *next;
};

struct data_sample {
	char *file;
	struct item *list;
	unsigned long *sort;
	int count;
};

struct account_match {
	struct data_sample src;
	struct data_sample tgt;
};

static void *read_file(const char *fname, unsigned *_sz)
{
	char *data;
	int sz;
	int fd;
	struct stat sb;

	data = 0;
	fd = open(fname, O_RDONLY);
	if(fd < 0)
		return 0;

	if (fstat(fd, &sb) < 0) {
		printf("fstat failed for '%s'\n", fname);
		goto oops;
	}

	sz = lseek(fd, 0, SEEK_END);
	if(sz < 0)
		goto oops;

	if(lseek(fd, 0, SEEK_SET) != 0) 
		goto oops;

	data = (char*) malloc(sz + 2);
	if(data == 0)
		goto oops;

	if(read(fd, data, sz) != sz) 
		goto oops;

	close(fd);
	data[sz] = '\n';
	data[sz+1] = 0;
	if(_sz)
		*_sz = sz;

	return data;

oops:
	close(fd);
	if(data != 0)
		free(data);
	return 0;
}

static bool str_isdigit(const char *s)
{
	for(; *s != '\0'; ++s) {
		if(!isdigit(*s) && (*s != ',') && (*s != '.')) {
			return false;
		}
	}

	return true;
}

static struct item *create_item(const char *data)
{
	struct item *item;
	char *endptr;

	item = malloc(sizeof(*item));
	if(!item)
		return NULL;

	item->data = (char *)data;
	item->flags = 0;
	item->next = NULL;

	if(str_isdigit(data))
		item->flags |= ITEM_VAILD;
	else 
		item->flags &= (~ITEM_VAILD);

	item->val = xstrtod(data, &endptr);

	return item;
}


static void parse_line_no_op(struct parse_state *state, int nargs, char **args)
{
}

static struct item *reversion_list(struct item *head) {
	struct item *p = NULL;
	struct item *item = head;

	while (item) {
		head = item->next;

		item->next = p;
		p = item;

		item = head;
	}
	
	return p;
}

static void parse_items(struct item **head, const char *fn, char *s)
{
	struct parse_state state;
	struct item *item, *tmp;
	int nargs;

	nargs = 0;
	state.filename = fn;
	state.line = 0;
	state.ptr = s;
	state.nexttoken = 0;
	state.parse_line = parse_line_no_op;

	for (;;) {
		switch (next_token(&state)) {
			case T_EOF:
				debug("t_eof:%s\n", state.text);
				goto parser_done;
			case T_NEWLINE:
				debug("t_newline:%s\n", state.text);
				break;
			case T_TEXT:
				debug("t_text:%s\n", state.text);
				item = create_item(state.text);
				item->next = *head;
				*head = item;
				break;
		}
	}

parser_done:
	*head = reversion_list(*head);
	return;
}

int parse_file(struct item **head, const char *fname)
{
	char *data;

	data = read_file(fname, 0);
	if (!data)
		return -1;

	parse_items(head, fname, data);
	return 0;
}

int cmpitem(const void *a, const void *b)
{
	unsigned long x = *(unsigned long *)a;
	unsigned long y = *(unsigned long *)b;
	struct item *s = (struct item *)x;
	struct item *t = (struct item *)y;

	return s->val - t->val;
//	return *(double *)a - *(double *)b;
}


static int sort_item(struct item *head, unsigned long **out) 
{
	unsigned long *buf;
	int count = 0;
	int i = 0;
	struct item *item = head;

	while (item) {
		if(item->flags & ITEM_VAILD) {
			count++;
		}
		item = item->next;
	};

	item = head;

	buf = malloc(sizeof(void *) * count);
	if(!buf)
		return -EINVAL;
	
	while (item) {
		if(item->flags & ITEM_VAILD) {
			item->index = i + 1;
			buf[i] = (unsigned long)item;
			i++;
		}

		item = item->next;
	}

	sort(buf, count, sizeof(unsigned long), cmpitem, NULL);

	*out = buf;
	
	return count;
}


static int acc_match(struct account_match *acc)
{
	void *ret;
	int i, j = 0;
	unsigned long *p, *p2;
	struct item *s, *t;

	for(i=0; i<acc->src.count; i++) {
		p = acc->src.sort + i;
		s = (struct item *)(*p);

		ret = bsearch(p, acc->tgt.sort, acc->tgt.count,
			   	sizeof(unsigned long), cmpitem);
		
		if(ret == NULL) 
			continue;

		p2 = (unsigned long *)ret;
		t = (struct item *)(*p2);
		if(s->val == t->val && !(t->flags & ITEM_MATCH))
				goto found;

		do {
			t = (struct item *)(*p2);
			if(s->val == t->val && !(t->flags & ITEM_MATCH))
				goto found;

			p2--;
		} while(p2 != acc->tgt.sort);

		p2 = (unsigned long *)ret;

		do {
			t = (struct item *)(*p2);
			if(s->val == t->val && !(t->flags & ITEM_MATCH))
				goto found;

			p2++;
		} while(p2 != (acc->tgt.sort + (acc->tgt.count - 1)));

found:
		s->flags |= ITEM_MATCH;
		t->flags |= ITEM_MATCH;
	}
}

static void dump_result(struct account_match *acc)
{
	int i;
	struct item *item;
	
	printf("\n==================================================\n");

	printf("\n%s mismatch:\n", acc->src.file);
	item = acc->src.list;
	while (item) {
		if((item->flags & ITEM_VAILD) && !(item->flags & ITEM_MATCH)) {
			printf("[%d]\t%s\n", item->index, item->data);
		}

		item = item->next;
	}

	printf("\n%s mismatch:\n", acc->tgt.file);
	item = acc->tgt.list;
	while (item) {
		if((item->flags & ITEM_VAILD) && !(item->flags & ITEM_MATCH)) {
			printf("[%d]\t%s\n", item->index, item->data);
		}

		item = item->next;
	}

	printf("\n==================================================\n");
}

static void dump_details_result(struct account_match *acc)
{
	int i;
	struct item *item;
	double srcsum = 0, tgtsum = 0;
	
	printf("\n==================================================\n");

	printf("\n%s mismatch:\n", acc->src.file);
	item = acc->src.list;
	while (item) {
		if((item->flags & ITEM_VAILD)) {
			if(!(item->flags & ITEM_MATCH))
				printf("[%d]\t=>  %s\n", item->index, item->data);
			else
				printf("[%d]\t%s\n", item->index, item->data);

			srcsum += item->val;
		}

		item = item->next;
	}

	printf("\n%s mismatch:\n", acc->tgt.file);
	item = acc->tgt.list;
	while (item) {
		if((item->flags & ITEM_VAILD)) {
			if(!(item->flags & ITEM_MATCH))
				printf("[%d]\t=>  %s\n", item->index, item->data);
			else
				printf("[%d]\t%s\n", item->index, item->data);

			tgtsum += item->val;
		}

		item = item->next;
	}

	printf("\n\n%s sum:\t%.2lf\n", acc->src.file, srcsum);
	printf("%s sum:\t%.2lf\n", acc->tgt.file, tgtsum);
	printf("diff:\t%.2lf\n", srcsum - tgtsum);
	printf("\n==================================================\n");
}


int main(int argc, void **argv)
{
	struct account_match acc;
	struct item *item;
	int i;

	printf("start matching.\n");
	acc.src.list = NULL;
	acc.tgt.list = NULL;

	if(argc < 3) {
		acc.src.file = DEFAULT_FILE_SOURCE;
		acc.tgt.file = DEFAULT_FILE_TARGET;
	} else {
		acc.src.file = argv[1];
		acc.tgt.file = argv[2];
	}

	parse_file(&acc.src.list, acc.src.file);
	parse_file(&acc.tgt.list, acc.tgt.file);

	acc.src.count = sort_item(acc.src.list, &acc.src.sort);

	debug("source count:%d\n", acc.src.count);
	for(i=0; i<acc.src.count; i++) {
		item = (struct item *)acc.src.sort[i];
		debug("%.2lf\n", item->val);
	}

	acc.tgt.count = sort_item(acc.tgt.list, &acc.tgt.sort);

	debug("target count:%d\n", acc.tgt.count);
	for(i=0; i<acc.tgt.count; i++) {
		item = (struct item *)acc.tgt.sort[i];
		debug("%.2lf\n", item->val);
	}

	acc_match(&acc);

	dump_result(&acc);

	printf("press any key for get details.\n");
	getchar();

	dump_details_result(&acc);

	printf("press any key to exit..\n");
	getchar();

	return 0;
}


