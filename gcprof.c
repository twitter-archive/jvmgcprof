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
#include <jvmti.h>
#include "java_crw_demo.h"

enum
{
	Maxsample = 10000,
};

typedef struct Galloc Galloc;
struct Galloc
{
	uint64 n;
	uint64 tot;
	long rnd;
};

typedef struct Gsample Gsample;
struct Gsample
{
	long rnd;
	uint64 age;
};

static int gcprofinit();
static void lock();
static void unlock();
static void fixup(int i);
static void fixdown(int i, int j);
static int jlongcmp(const void *a, const void *b);
static inline uint64 atomicinc64(volatile uint64 *u, uint64 incr);
static inline void mfence();

JNIEXPORT void JNICALL jniNew(JNIEnv*, jclass, jobject);
JNIEXPORT void JNICALL jniReset(JNIEnv*, jclass);
JNIEXPORT jlongArray JNICALL jniDump(JNIEnv*, jclass);

static void JNICALL jvmtiVMStart(jvmtiEnv* jvmti, JNIEnv* env);
static void JNICALL jvmtiVMInit(jvmtiEnv *jvmti, JNIEnv *env, jthread thread);
static void JNICALL jvmtiVMDeath(jvmtiEnv* jvmti, JNIEnv* env);
static void JNICALL jvmtiObjectFree(jvmtiEnv* jvmti, jlong tag);
static void JNICALL jvmtiClassFileLoadHook(
	jvmtiEnv*, JNIEnv*, jclass, jobject, const char*, 
	jobject, jint, const unsigned char*,
	jint*, unsigned char**);

static jvmtiEnv *jvmti;
static const char *Gcprofclass = "GcProf";
static const char *Gcprofclasssig = "LGcProf;";
static int started = 0;
static jrawMonitorID monitor;
static struct
{
	uint64 rst;
	uint64 tot;
	uint64 ntagged;
	uint64 nrejected;
} stats;
static Gsample sample[Maxsample+1];
static int nsample = 0;

JNIEXPORT jint JNICALL 
Agent_OnLoad(JavaVM *jvm, char *options, void *_ignore)
{
	jint rv;

	rv = (*jvm)->GetEnv(jvm, (void**)&jvmti, JVMTI_VERSION_1_0);
	if(rv != JNI_OK || jvmti == nil)
		panic("failed to access JVMTIv1");

	if(gcprofinit() < 0)
		panic("failed to initialize gcprof");

	return JNI_OK;
}

static int
gcprofinit()
{
	jvmtiCapabilities c;
	jvmtiEvent events[] = {
		JVMTI_EVENT_VM_START,
		JVMTI_EVENT_VM_INIT,
		JVMTI_EVENT_VM_DEATH,
		JVMTI_EVENT_CLASS_FILE_LOAD_HOOK,
		JVMTI_EVENT_OBJECT_FREE
	};
	uint i;
	int rv;

	rv = (*jvmti)->CreateRawMonitor(jvmti, "gcprof", &monitor);
	if(rv != JVMTI_ERROR_NONE)
		panic("CreateRawMonitor");

	stats.tot = stats.rst = stats.ntagged = stats.nrejected = 0;

	memset(&c, 0, sizeof(c));
	c.can_generate_all_class_hook_events = 1;
	c.can_tag_objects = 1;
	c.can_generate_object_free_events = 1;

	if((*jvmti)->AddCapabilities(jvmti, &c) != JVMTI_ERROR_NONE)
		return -1;

	jvmtiEventCallbacks cb; 
	memset(&cb, 0, sizeof(cb));
	cb.VMStart = &jvmtiVMStart;
	cb.VMInit = &jvmtiVMInit;
	cb.VMDeath = &jvmtiVMDeath;
	cb.ObjectFree = &jvmtiObjectFree;
	cb.ClassFileLoadHook = &jvmtiClassFileLoadHook;
	rv = (*jvmti)->SetEventCallbacks(jvmti, &cb, (jint)sizeof cb);
	if(rv != JVMTI_ERROR_NONE)
		return -1;

	for(i=0; i<nelem(events); i++){
		rv = (*jvmti)->SetEventNotificationMode(
			jvmti, JVMTI_ENABLE, events[i], nil);
		if(rv != JVMTI_ERROR_NONE)
			return -1;
	}

	return 0;
}

static void
lock()
{
	if((*jvmti)->RawMonitorEnter(jvmti, monitor) != JVMTI_ERROR_NONE)
		panic("RawMonitorEnter");
}

static void
unlock()
{
	if((*jvmti)->RawMonitorExit(jvmti, monitor) != JVMTI_ERROR_NONE)
		panic("RawMonitorExit");
}

static void
fixup(int i)
{
	Gsample x;

	while(i != 1 && sample[i].rnd > sample[i/2].rnd){
		x = sample[i];
		sample[i] = sample[i/2];
		sample[i/2] = x;
		i /= 2;
	}
}

static void
fixdown(int i, int j)
{
	int m;
	Gsample x;
	
  loop:
	if(j < 2*i)
		return;

	if(j == 2*i || sample[2*i].rnd > sample[2*i+1].rnd)
		m = 2*i;
	else
		m = 2*i+1;

	if(sample[m].rnd > sample[i].rnd){
		x = sample[i];
		sample[i] = sample[m];
		sample[m] = x;
		i = m;
		goto loop;
	}
}

JNIEXPORT void JNICALL 
jniNew(JNIEnv* env, jclass class, jobject o)
{
	jlong n;
	long rnd;
	Galloc *g;

	if((*jvmti)->GetObjectSize(jvmti, o, &n) != JVMTI_ERROR_NONE)
		panic("GetObjectSize");


	// keep track of nallocations as well.

	atomicinc64(&stats.tot, n);
	rnd = rand();

	// Because the max(heap) is monotonically decreasing,
	// this test is safe.
	if(nsample > 0 && rnd > sample[1].rnd)
		return;

	// Slow path: we're sampled!
	g = emalloc(sizeof *g);
	g->rnd = rnd;
	g->n = n;
	g->tot = stats.tot;
	(*jvmti)->SetTag(jvmti, o, (long)g);
	atomicinc64(&stats.ntagged, 1);
}

JNIEXPORT void JNICALL
jniReset(JNIEnv *env, jclass class)
{
	lock();
	nsample = 0;
	stats.rst = stats.tot;
	unlock();
}

JNIEXPORT jlongArray JNICALL
jniDump(JNIEnv *env, jclass class)
{
	jlongArray a;
	jlong tot, *snap;
	int i, n;

	lock();
	n = nsample;
	snap = emalloc(n*sizeof(jlong));

	for(i=0; i<n; i++)
		snap[i] = sample[i+1].age;

	tot = stats.tot - stats.rst;
	unlock();

	//qsort(snap, n, sizeof(jlong), jlongcmp);
	a = (*env)->NewLongArray(env, n+1);
	(*env)->SetLongArrayRegion(env, a, 0, 1, &tot);
	if(n > 0)
		(*env)->SetLongArrayRegion(env, a, 1, n, snap);

	free(snap);

	return a;
}

static void JNICALL 
jvmtiVMStart(jvmtiEnv* jvmti, JNIEnv* env)
{
	jmethodID mid;

	JNINativeMethod registry[] = {
		{"_new", 	"(Ljava/lang/Object;)V",	&jniNew},
		{"reset", 	"()V",				&jniReset},
		{"dump", 	"()[J",				&jniDump},
	};
	jclass class;

	if((class=(*env)->FindClass(env, Gcprofclass)) == nil)
		panic("Failed to find helper class %s", Gcprofclass);

	// TODO: does this need to be protected?
	if((*env)->RegisterNatives(env, class, registry, nelem(registry)) != JNI_OK)
		panic("Failed to register natives");

	started = 1;
	mfence();
}

static void
JNICALL jvmtiVMInit(jvmtiEnv *jvmti, JNIEnv *env, jthread thread)
{
	jmethodID mid;
	jclass class;
	
	if((class=(*env)->FindClass(env, Gcprofclass)) == nil)
		panic("Failed to find helper class %s", Gcprofclass);
		
	if((mid=(*env)->GetStaticMethodID(env, class, "start", "()V")) == nil)
		panic("GetStaticMethodID");

	(*env)->CallStaticVoidMethod(env, class, mid);
}

static void JNICALL 
jvmtiVMDeath(jvmtiEnv* jvmti, JNIEnv* env)
{}

static void JNICALL
jvmtiObjectFree(jvmtiEnv* jvmti, jlong tag)
{
	Galloc *g;
	long rnd;
	uint64 age;

	g = (Galloc*)tag;
	age = stats.tot - g->tot;

	lock();
	if(g->tot < stats.rst){
	}else if(nsample<Maxsample){
		sample[++nsample].rnd = g->rnd;
		sample[nsample].age = age;
		fixup(nsample);
	}else if(g->rnd < sample[1].rnd){
		sample[1].rnd = g->rnd;
		sample[1].age = age;
		fixdown(1, nsample);
	}else
		atomicinc64(&stats.nrejected, 1);

	unlock();
	//mfence();
	free(g);
}

static void JNICALL 
jvmtiClassFileLoadHook(
	jvmtiEnv* jvmti, JNIEnv* env,
	jclass class, jobject loader,
	const char* _name, jobject protdomain,
	jint nclassdata, const unsigned char* classdata,
	jint* nnewclassdata, unsigned char** newclassdata)
{
	uchar *image;
	long nimage;
	char *name;
	char *p;

	if (_name == nil)
		name = java_crw_demo_classname(
			classdata, nclassdata, nil);
	else
		name = estrdup((char*)_name);
		
	if(strcmp(name, Gcprofclass) == 0)
		goto done;

	java_crw_demo(
		0,
		name,
		classdata, nclassdata,
		!started,  // system class
		(char*)Gcprofclass,
		(char*)Gcprofclasssig,
		nil, nil,
		nil, nil,
		"_new", "(Ljava/lang/Object;)V",
		"_new", "(Ljava/lang/Object;)V",
		&image, &nimage,
		(void*)nil, (void*)nil);

	if(nimage > 0L){
		if((*jvmti)->Allocate(jvmti, nimage, (uchar**)&p) != JVMTI_ERROR_NONE)
			panic("Allocate");
		memcpy(p, image, nimage);
		*nnewclassdata = nimage;
		*newclassdata = (uchar*)p;
	}

	if(image != nil)
		free(image);

  done:
  	free(name);
}

static int
jlongcmp(const void *a, const void *b)
{
	uint64 ua, ub;
	ua = *(jlong*)a;
	ub = *(jlong*)b;

	if(ua < ub)
		return -1;
	else if(ub < ua)
		return 1;
	else
		return 0;
}

static inline uint64
atomicinc64(volatile uint64 *u, uint64 incr)
{
	uint64 x;
	x = incr;

	__asm__ __volatile__(
		"lock;"
		"xaddq %0, %1"
		: "+r" (x), "+m" (*u)
		:
		: "memory"
	);

	return x+incr;
}

static inline void
mfence()
{
	__asm__ __volatile__(
		"mfence"
		: 
		: 
		: "memory"
	);
}