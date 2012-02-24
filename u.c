// Copyright 2011 Twitter, Inc.
// 
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// 
// http://www.apache.org/licenses/LICENSE-2.0
// 
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "u.h"
#include <stdio.h>
#include <stdarg.h>

void
panic(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	fprintf(stderr, "\n");
	fflush(stderr);
	va_end(ap);
	exit(1);
}

void*
emalloc(uint n)
{
	void *v;

	v = calloc(n, 1);
	if(v == nil)
		panic("out of memory");
	return v;
}

void*
erealloc(void *v, uint n)
{
	v = realloc(v, n);
	if(v == nil)
		panic("out of memory");
	return v;
}

char*
estrdup(char *s)
{
	s = strdup(s);
	if(s == nil)
		panic("out of memory");
	return s;
}
