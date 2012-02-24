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

import java.lang.reflect.Method;
import java.lang.reflect.Field;
import java.lang.reflect.InvocationTargetException;
import java.util.Arrays;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicLong;

// todo: do sampling here so that hotspot can
// inline the code.
public class GcProf {
  public static native void _new(Object o);
  public static native void reset();
  public static native long[] dump();

  private static long totalAllocation = 0L;

  // This method is designed to be called from java/scala code
  // Ex: Class.forName("GcProf").getDeclaredMethod("getTotalAllocation").invoke(null)
  public static synchronized long getTotalAllocation() { return totalAllocation; }

  public static void start() {
    // Create thread, sleep every now & then, etc.
    Thread t = new Thread() {
      public void run() {
        int[] pcts = {5000, 9000, 9500, 9900, 9990, 9999};
        Integer period = Integer.getInteger("gcprof.period", 1) * 1000;
        Integer nwarmup = Integer.getInteger("gcprof.nwarmup", 0);
        String nworkpath = System.getProperty("gcprof.nwork");
        String nostrichwork = System.getProperty("gcprof.nostrichwork");
        boolean warm = false;

        long start = System.currentTimeMillis();
        while (true) {
          try {
            Thread.sleep(period);
          } catch (InterruptedException e) {
            continue;
          }

          long t = System.currentTimeMillis();
          if (t-start < 1000)
            continue;

          long nwork;
          if (nostrichwork != null)
            nwork = getnostrichwork(nostrichwork);
          else
            nwork = getnwork(nworkpath);
          if (nwork < 0) nwork = 1;

          if (!warm) {
            if (nwork > nwarmup) {
              warm = true;
              reset();
            } else
              continue;
          }

          long[] data = dump();
          long b = data[0];
          long mb = b>>20;
          long bpw = 1;
          if (nwork > 0)
            bpw = b/nwork;

          System.out.printf("%dMB w=%d (%dMB/s %dkB/w)\n",
            mb, nwork, 1000*mb/(t-start), bpw>>10);

          if (data.length <= 1)
            continue;

          int n = data.length-1;
          Arrays.sort(data, 1, 1+n);

          synchronized(this) {
            totalAllocation = mb;
          }

          for(int i=0 ; i<pcts.length ; i++) {
            int p = pcts[i];
            long allocRate = data[((n*p)/10000)+1]>>20;
            long allocRatePerReq = data[((n*p)/10000)+1]/bpw;
            System.out.printf("%02d.%02d%%\t%d\t%d\n",
              p/100,
              p-((p/100)*100),
              allocRate,
              allocRatePerReq);
          }
        }
      }
    };
    t.setDaemon(true);
    t.start();
  }

  static private long getnwork(String path) {
    if (path == null)
      return -1L;

    String[] ps = splitPath(path);
    return getlong(getClass(ps[0]), ps[1]);
  }

  static private long getnostrichwork(String name) {
    Class<?> cls = getClass("com.twitter.ostrich.stats.Stats$");

    try {
      Object o = cls.getField("MODULE$").get(null);
      Method m = o.getClass().getMethod("get", String.class);
      o = m.invoke(o, "");
      m = o.getClass().getMethod("getCounter", String.class);
      o = m.invoke(o, name);
      m = o.getClass().getMethod("apply");
      o = m.invoke(o);
      return (Long)o;
    } catch (Exception e) {
      e.printStackTrace();
      panic("failed to load ostrich "+e);
      return -1;
    }
  }

  static private void panic(String msg) {
    System.err.println(msg);
    System.exit(1);
  }

  static private long getlong(Class<?> cls, String member) {
    Object o = getMember(cls, member);

    if (o instanceof AtomicInteger)
      return ((AtomicInteger)o).get();
    if(o instanceof AtomicLong)
      return ((AtomicLong)o).get();
    if (o instanceof Number)
      return ((Number)o).longValue();

    panic("could not find object in "+member);
    return -1L;
  }

  static private String[] splitPath(String path) {
    String[] result = {};
    if (path == null)
      panic("bad path: null");
    else {
      result = path.split(":");
      if (result.length != 2)
        panic("bad path "+path);
    }
    return result;
  }

  static private Object getMember(Class<?> cls, String path) {
    String ps[] = path.split("\\.", 2);
    Object o = null;

    for (String p : ps) {
      try {
        Method m = cls.getMethod(p);
        m.setAccessible(true);
        o = m.invoke(o);
        cls = o.getClass();
        continue;
      } catch (NoSuchMethodException e) {
      } catch (IllegalAccessException e) {
        panic("IllegalAccessException ("+p+") in "+path);
      } catch (InvocationTargetException e) {
        panic("InvocationTargetException ("+p+") in "+path);
      }

      try {
        Field f = cls.getField(p);
        f.setAccessible(true);
        o = f.get(o);
        cls = o.getClass();
        continue;
      } catch (NoSuchFieldException e) {
        panic("cannot find \""+path+"\" in "+cls);
      } catch (IllegalAccessException e) {
        panic("IllegalAccessException ("+p+") in "+path);
      }

      panic("cannot find \""+p+"\" in "+cls);
    }

    return o;
  }

  static private Class<?> getClass(String fullClassName) {
    ClassLoader loader = getloader();
    Class<?> cls = null;

    try {
      cls = Class.forName(fullClassName, true, loader);
    } catch (ClassNotFoundException e) {
      panic("cannot find class \"" + fullClassName + "\"");
    }

    return cls;
  }

  static private ClassLoader getloader() {
    ThreadGroup g = Thread.currentThread().getThreadGroup();
      for (;;) {
        ThreadGroup g0 = g.getParent();
        if (g0 == null)
          break;
        g = g0;
      }

      Thread[] active = new Thread[g.activeCount()];
      g.enumerate(active);

      ClassLoader loader = ClassLoader.getSystemClassLoader();
      for (Thread t : active)
        if (t != Thread.currentThread())
          loader = t.getContextClassLoader();

    return loader;
  }
}
