# gcprof

gcprof is a simple utility for profile allocation and garbage collection activity in the JVM

## Usage

gcprof [-p period] [-n class:fieldpath] [-no ostrichcounter] [-w nwarmup] java ...

  Profile allocation and garbage collection activity in the JVM.  The
  gcprof command runs a java command under profiling.  Allocation and
  collection statistics are printed periodically.  If -n or -no are
  provided, statistics are also reported in terms of the given
  application metric.  Total allocation, allocation rate, and a
  survival histogram is given.  The intended use for this tool is
  twofold: (1) monitor and test garbage allocation and GC behavior,
  and (2) inform GC tuning.

  For example:

	903MB w=5432 (12MB/s 170kB/w)
	50.00%	8	48
	90.00%	14	88
	95.00%	15	92
	99.00%	16	97
	99.90%	30	182
	99.99%	47	288

  Reports 903MB of total allocation, with the amount of work being
  5432 (in this example, requests processed by a server).  The
  allocation rate is 12 MB/s, or 170kB per request.  50.00% of
  allocated objects survive only for 8MB of allocation (or processing
  48 requests).  By 47 MB of allocation or 288 requests processed
  99.99% of the objects have died.

  An application metric can be integrated through the use of "-n".
  Given a class:fieldpath.  -n names a longs, AtomicInteger or
  AtomicLong.  The components in the fieldpath can either be fields or
  nullary methods.  For example
  
	package foo.bar;
	class Foo{
		public static Baz baz = …
	}
	class Baz{
		Biz biz() {
		  …
		}
	}
	class Biz{
		AtomicInteger count = …
	}
	
  ``count'' is named by the string "foo.bar.Foo.baz.biz.count".  If
  you are using Ostrich[1], an application metric can be provided by
  naming an ostrich counter with the -no flag.
  
  The reporting period (in seconds) is specified with -p, defaulting
  to 1.
  
  If -w is given, data is gathered only after that amount of work has
  been done according to the work metric.
  
  The statistics are subject to the current garbage collector (and its
  settings).

## Examples
  
  A new load balancer in finagle[2] drastically improved allocation
  behavior when load balancing over large numbers of hosts.  To
  observe this, we ran a stress test under gcprof with the old & new
  balancers:

	% gcprof -n com.twitter.finagle.stress.LoadBalancerTest$:MODULE$.totalRequests\
	  $s/bin/scala -i finagle/finagle-stress com.twitter.finagle.stress.LoadBalancerTest
  
  Old:

	6335MB w=5348 (369MB/s 1213kB/w)
	50.00%	10		8
	90.00%	18		15
	95.00%	25		21
	99.00%	453		382
	99.90%	813		687
	99.99%	4966	4192

  New:

	2797MB w=101223 (231MB/s 28kB/w)
	50.00%	8	297
	90.00%	14	542
	95.00%	15	572
	99.00%	61	2237
	99.90%	2620	94821
	99.99%	2652	95974

  The new load balancer results in two orders of magnitude smaller
  allocation rate per request (it is also faster, hence the observe
  the total memory throughput is only slightly smaller).  The survival
  distribution also changed: the tail is longer in requests processed
  but shorter in allocation.  Presumably this is due to the improved
  throughput of the balancer.

## Bugs

In the remote possibility that there exist bugs in this code, please report them to:
<https://github.com/twitter/jvmgcprof/issues>

* Note: currently works only on x86_64

## TODO

* maintain windowed averages
* nallocations (though somewhat less relevant--fun to know?)

[1] http://github.com/twitter/ostrich
[2] http://github.com/twitter/finagle

## Authors
* Marius Eriksen <http://twitter.com/marius>

Thanks for assistance and contributions:

* Steve Gury <http://twitter.com/SteveGury>

## License
Copyright 2012 Twitter, Inc.

Licensed under the Apache License, Version 2.0: http://www.apache.org/licenses/LICENSE-2.0
