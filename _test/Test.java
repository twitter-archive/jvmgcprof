import java.lang.reflect.Method;
import java.util.Arrays;
import java.util.Random;

class Test{
	public static long count = 0;
	public static void main(String[] args){
		try{
			Class<?> klass;
			Method reset;
			Method dump;
			Random r = new Random();
	
			klass = Class.forName("GcProf");
			reset = klass.getDeclaredMethod("reset");
			dump = klass.getDeclaredMethod("dump");
			if(dump == null)
				throw new Exception("sigh");
			reset.invoke(null);
			dump.invoke(null);

			long start = System.currentTimeMillis();

			int i = 0;
			while(true) {
				if(false && i++ % 1000 == 0){
					long elapsed = System.currentTimeMillis() - start;
					long[] data = (long[])dump.invoke(null);
					if(data.length>1)
						Arrays.sort(data, 1, data.length-1);
					if(elapsed > 0)
						System.out.printf("total: %d (%d bpms / %dmbps)\n",
							data[0], data[0]/elapsed, (1000*data[0]>>20)/elapsed);
//					for(int j=1; j<data.length; j++)
//						System.out.printf("%d ", data[j]);
//					System.out.printf("\n");
				}
				Thread.sleep(1);
				count++;
				int[] x = new int[r.nextInt(1<<21)];
			}
		}catch(Exception e){
			System.out.println("!!!"+e+"!!!"+e.getCause());
			e.printStackTrace();
		}
	}
}
