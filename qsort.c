/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)qsort.c	8.1 (Berkeley) 6/4/93";
#endif /* LIBC_SCCS and not lint */

#include <sys/types.h>
#include <stdlib.h>

#if !defined (WIN32) || !defined (min)
#define min(a, b)	(a) < (b) ? a : b
#endif

/*
 * Quicksort routine from Bentley & McIlroy's "Engineering a Sort Function".
 */
#define swapcode(TYPE, parmi, parmj, n) { 		\
	long i = (n) / sizeof (TYPE); 			\
	register TYPE *pi = (TYPE *) (parmi); 		\
	register TYPE *pj = (TYPE *) (parmj); 		\
	do { 						\
		register TYPE	t = *pi;		\
		*pi++ = *pj;				\
		*pj++ = t;				\
        } while (--i > 0);				\
}

#define SWAPINIT(a, es) swaptype = ((char *)a - (char *)0) % sizeof(long) || \
	es % sizeof(long) ? 2 : es == sizeof(long)? 0 : 1;

static void
swapfunc(a, b, n, swaptype)
     char *a, *b;
	int n, swaptype;
{
  if(swaptype <= 1)
    swapcode(long, a, b, n)
      else
	swapcode(char, a, b, n)
}

#define swap(a, b)					\
	if (swaptype == 0) {				\
		long t = *(long *)(a);			\
		*(long *)(a) = *(long *)(b);		\
		*(long *)(b) = t;			\
	} else						\
	swapfunc(a, b, es, swaptype)

#define vecswap(a, b, n) 	if ((n) > 0) swapfunc(a, b, n, swaptype)

static char* med3(a, b, c, cmp)
     char *a, *b, *c;
     int (*cmp)();
{
  return cmp(a, b) < 0 ?
    (cmp(b, c) < 0 ? b : (cmp(a, c) < 0 ? c : a ))
    :(cmp(b, c) > 0 ? b : (cmp(a, c) < 0 ? a : c ));
}

void quicksort(void *a, size_t n, size_t es,
	       int (*compare_function) (const void *p1, const void *p2))
{
  char *pa, *pb, *pc, *pd, *pl, *pm, *pn;
  int d, r, swaptype, swap_cnt;
  
 loop:	SWAPINIT(a, es);
	swap_cnt = 0;
	if (n < 7) {
	  for (pm = (char*)((unsigned long)a + es); 
	       pm < (char *) ((unsigned long)a + n * es); 
	       pm += es)
	    for (pl = pm; pl > (char *) a && compare_function(pl - es, pl) > 0;
		 pl -= es)
	      swap(pl, pl - es);
	  return;
	}
	pm = (char*)((unsigned long)a + (n / 2) * es);
	if (n > 7) {
	  pl = a;
	  pn = (char*)((unsigned long)a + (n - 1) * es);
	  if (n > 40) {
	    d = (n / 8) * es;
	    pl = med3(pl, pl + d, pl + 2 * d, compare_function);
	    pm = med3(pm - d, pm, pm + d, compare_function);
	    pn = med3(pn - 2 * d, pn - d, pn, compare_function);
	  }
	  pm = med3(pl, pm, pn, compare_function);
	}
	swap(a, pm);
	pa = pb = (char*)((unsigned long)a + es);

	pc = pd = (char*)((unsigned long)a + (n - 1) * es);
	for (;;) {
	  while (pb <= pc && (r = compare_function(pb, a)) <= 0) {
	    if (r == 0) {
	      swap_cnt = 1;
	      swap(pa, pb);
	      pa += es;
	    }
	    pb += es;
	  }
	  while (pb <= pc && (r = compare_function(pc, a)) >= 0) {
	    if (r == 0) {
	      swap_cnt = 1;
	      swap(pc, pd);
	      pd -= es;
	    }
	    pc -= es;
	  }
	  if (pb > pc)
	    break;
	  swap(pb, pc);
	  swap_cnt = 1;
	  pb += es;
	  pc -= es;
	}
	if (swap_cnt == 0) {  /* Switch to insertion sort */
	  for (pm = (char*)((unsigned long)a + es); pm < (char *) a + n * es; pm += es)
	    for (pl = pm; pl > (char *) a && compare_function(pl - es, pl) > 0;
		 pl -= es)
	      swap(pl, pl - es);
	  return;
	}

	pn = (char*)((unsigned long)a + n * es);
	r = min(pa - (char *)a, pb - pa);
	vecswap(a, pb - r, r);
	r = min(pd - pc, pn - pd - es);
	vecswap(pb, pn - r, r);
	if ((r = pb - pa) > es)
	  quicksort(a, r / es, es, compare_function);
	if ((r = pd - pc) > es) {
	  /* Iterate rather than recurse to save stack space */
	  a = pn - r;
	  n = r / es;
	  goto loop;
	}
	/*		quicksort(pn - r, r / es, es, compare_function);*/
}

